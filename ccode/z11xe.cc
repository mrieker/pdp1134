//    Copyright (C) Mike Rieker, Beverly, MA USA
//    www.outerworldapps.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//    http://www.gnu.org/licenses/gpl-2.0.html

// Performs DEUNA ethernet I/O for the PDP-11 Zynq I/O board
// Runs as a background daemon

// page references DEUNA User's Guide 1983

//  ./z11xe

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "z11defs.h"
#include "z11util.h"

#define SERI 0x8000     // PCSR0 status error interrupt
#define PCEI 0x4000     // PCSR0 port command error interrupt
#define RXI  0x2000     // PCSR0 receive ring interrupt
#define TXI  0x1000     // PCSR0 transmit ring interrupt
#define DNI  0x0800     // PCSR0 done interrupt
#define RCBI 0x0400     // PCSR0 receive buffer unavailable interrupt
#define USCI 0x0100     // PCSR0 unsolicited state change interrupt
#define INTR 0x0080     // PCSR0 interrupt summary
#define RSET 0x0020     // PCSR0 deuna reset
#define HIJK 0x0010     // PCSR0 hi-jacked go bit
#define PCMD 0x000F     // PCSR0 port command

#define RUN  0x0001     // PCSR1 running
#define RDY  0x0002     // PCSR1 ready
#define PCTO 0x0080     // PCSR1 port command timeout

#define ERRS 0x8000     // PORTST1 error summary (p96)
#define TMOT 0x0800     // PORTST1 unibus timeout error
#define RRNG 0x0200     // PORTST1 receive ring error
#define TRNG 0x0100     // PORTST1 transmit ring error
#define RREV 0x0002     // PORTST1 rom revision number

#define MAXMLT 10       // PORTST2 max number of multicast ids
#define MAXCTR 32       // PORTST3 max length of counter data block (words)

#define OWN  0x8000     // rdrb[2],tdrb[2] owned by deuna
#define STP  0x0200     // rdrb[2],tdrb[2] start of packet
#define ENP  0x0100     // rdrb[2],tdrb[2] end of packet
#define BUFL 0x8000     // rdrb[3],tdrb[3] buffer length error
#define UBTO 0x4000     // rdrb[3],tdrb[3] unibus timeout
#define NCHN 0x2000     // rdrb[3] was in non-chaining mode

#define MODE_PROM 0x8000    // promiscuous mode
#define MODE_ENAL 0x4000    // enable all multicast
#define MODE_DRDC 0x2000    // disable data chaining on receive
#define MODE_TPAD 0x1000    // transmit message pad enable
#define MODE_ECT  0x0800    // enable collision test
#define MODE_DMNT 0x0200    // disable maintenance message
#define MODE_DTCR 0x0008    // disable transmit crc
#define MODE_LOOP 0x0004    // internal loopback mode
#define MODE_HDPX 0x0001    // half-duplex mode

struct Counters {       // p87
    uint16_t ubusdblen;
    uint16_t secsincezer;
    uint32_t packetsrcvd;
    uint32_t mltpktsrcvd;
    uint16_t bits_14;
    uint16_t pktsrcvderr;
    uint32_t databytesrcvd;
    uint32_t mltbytesrcvd;
    uint16_t rcvpktlostibe;
    uint16_t rcvpktlostlbe;
    uint32_t packetsxmtd;
    uint32_t mltpktsxmtd;
    uint32_t pkts3attempt;
    uint32_t pkts2attempt;
    uint32_t databytesxmtd;
    uint32_t mltbytesxmtd;
    uint16_t bits_70;
    uint16_t xmtdaborted;
    uint16_t xmtdcolfail;
    uint16_t zero_76;
};

struct RingFmt {
    uint32_t rdrb;
    uint32_t tdrb;
    uint16_t rrlen;
    uint16_t trlen;
    uint8_t  relen;
    uint8_t  telen;
};

static uint16_t const bdcastaddr[3] = { 0xFFFFU, 0xFFFFU, 0xFFFFU };
static uint16_t defethaddr[3] = { 0x2342U, 0xAEDEU, 0xEA56U };

static bool rcbisetinh;
static Counters counters;
static int debug;
static int sockfd;
static pthread_cond_t thecond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t themutex = PTHREAD_MUTEX_INITIALIZER;
static RingFmt ringfmt;
static struct sockaddr_ll sendtoaddr;
static time_t counterszeroed;
static uint8_t curmlt;                  // current number entries in mltaddrtable
static uint16_t curethaddr[3];
static uint16_t mltaddrtable[3*MAXMLT];
static uint16_t modebits;
static uint16_t pcsr1;
static uint16_t portst1;                // port status (p96)
static uint32_t volatile *xeat;
static uint32_t rdrca;                  // receive descriptor ring current address
static uint32_t tdrca;                  // transmit descriptor ring current address
static Z11Page *z11p;

static void xeiothread ();
static void doreset ();
static void getcommand (uint32_t pcbaddr);
static void *receivethread (void *dummy);
static bool matchesincoming (uint16_t const *rcvbuf);
static void *transmithread (void *dummy);
static void writepcsr0 (uint16_t pcsr0);
static void lockit (int line);
static void unlkit (int line);
static void waketransmit ();
static void waittransmit ();

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    char const *ethdev = "eth0";
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("  Handle DEUNA ethernet I/O");
            puts ("");
            puts ("    sudo ./z11xe [-eth <device>] [-mac <address>]");
            puts ("");
            puts ("      -eth = use given ethernet device (default eth0)");
            puts ("      -mac = use given mac address");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-eth") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing device after -eth\n");
                return 1;
            }
            ethdev = argv[i];
            continue;
        }
        if (strcasecmp (argv[i], "-mac") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing address after -mac\n");
                return 1;
            }
            uint8_t *addr = (uint8_t *) defethaddr;
            char *p = argv[i];
            for (int j = 0; j < 6; j ++) {
                uint32_t v = strtoul (p, &p, 16);
                if ((*p != ((j == 5) ? 0 : ':')) || (v > 255) || ((j == 0) && (v & 1))) {
                    fprintf (stderr, "bad mac address %s\n", argv[i]);
                    return 1;
                }
                addr[j] = v;
                p ++;
            }
            continue;
        }
        fprintf (stderr, "unknown option/argument %s\n", argv[i]);
        return 1;
    }

    // set up to transmit out using the given ethernet device
    sendtoaddr.sll_family = AF_PACKET;
    sendtoaddr.sll_ifindex = if_nametoindex (ethdev);
    if (sendtoaddr.sll_ifindex <= 0) {
        fprintf (stderr, "z11xe: unknown ethernet device %s: %m\n", ethdev);
        return 1;
    }

    // set up default mac address to be current mac address
    memcpy (curethaddr, defethaddr, 6);

    // access fpga register set for the DEUNA controller
    // lock it so we are only process accessing it
    z11p = new Z11Page ();
    xeat = z11p->findev ("XE", NULL, NULL, true, false);

    // man 7 packet
    sockfd = socket (AF_PACKET, SOCK_RAW, htons (ETH_P_ALL));
    if (sockfd < 0) {
        fprintf (stderr, "z11xe: error creating socket: %m\n");
        ABORT ();
    }

    // set ethdev promiscuous mode so we will get packets for our mac address
    struct packet_mreq packetmreq;
    memset (&packetmreq, 0, sizeof packetmreq);
    packetmreq.mr_ifindex = sendtoaddr.sll_ifindex;
    packetmreq.mr_type    = PACKET_MR_PROMISC;
    if (setsockopt (sockfd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &packetmreq, sizeof packetmreq) < 0) {
        fprintf (stderr, "z11xe: error setting promiscuous mode: %m\n");
        ABORT ();
    }

    // enable board to process io instructions
    ZWR(xeat[3], XE3_ENAB);

    debug = 0;
    char const *dbgenv = getenv ("z11xe_debug");
    if (dbgenv != NULL) debug = atoi (dbgenv);

    pthread_t tid;
    int rc = pthread_create (&tid, NULL, receivethread, NULL);
    if (rc != 0) ABORT ();
    rc = pthread_create (&tid, NULL, transmithread, NULL);
    if (rc != 0) ABORT ();

    xeiothread ();

    return 0;
}

// process functions in PCSR0<03:00> in response to PDP writes
static void xeiothread ()
{
    uint32_t pcbaddr = 0;
    while (true) {

        // wait for PDP to set RSET or write PCMD
        // FPGA hijacks PCSR0<04>, setting it whenever PDP sets RSET or writes PCMD
        // ...which wakes up the arm
        z11p->waitint (ZGINT_XE);

        lockit (__LINE__);

        uint16_t pcsr0 = ZRD(xeat[1]);          // read PCSR0
        if (debug > 0) fprintf (stderr, "xeiothread: PCSR0=%06o\n", pcsr0);

        if (! (pcsr0 & HIJK)) goto done;        // make sure hijack bit is set, not spurious wakeup

        if (pcsr0 & RSET) {                     // check for reset, can also be from bus_init just released
            doreset ();
            writepcsr0 (RSET | HIJK);           // clear reset and hijack bits
        }

        else {
            switch (pcsr0 & PCMD) {             // decode port command
                case 1: {   // get pcbb
                    pcbaddr = ZRD(xeat[2]) & 0777776;
                    break;
                }
                case 2: {   // get cmd
                    getcommand (pcbaddr);
                    goto done;
                }
                case 3: {   // self test
                    doreset ();
                    break;
                }
                case 4: {   // start
                    if (! (pcsr1 & RUN)) {      // nop if already running
                        rcbisetinh = false;
                        pcsr1 |=   RUN;         // set running bit (state = 3)
                        rdrca = ringfmt.rdrb;
                        tdrca = ringfmt.tdrb;
                        waketransmit ();        // see if anything ready to transmit
                    }
                    break;
                }
                case 5: {   // boot
                    break;
                }
                case 8: {   // polling demand
                    rcbisetinh = false;
                    waketransmit ();            // see if anything ready to transmit
                    break;
                }
                case 15: {  // stop
                    pcsr1 &= ~ RUN;             // clear running bit (state = 2)
                    break;
                }
            }

            writepcsr0 (DNI | HIJK);            // set port command done bit, clear hijack bit
        }

    done:;
        unlkit (__LINE__);
    }
}

// bus_init just released, RSET just set, or self-test command
static void doreset ()
{
    pcsr1 = RDY;                    // set ready state, not running
    modebits = 0;
    rcbisetinh = false;

    memset (&counters, 0, sizeof counters);
    counterszeroed = time (NULL);
}

// process commands in PCB
static void getcommand (uint32_t pcbaddr)
{
    uint16_t pcbdata[4];

    z11p->dmalock ();

    pcsr1 &= ~ PCTO;

    if (z11p->dmareadlocked (pcbaddr, &pcbdata[0]) != 0) goto dmaerror;
    switch (pcbdata[0] & 0xFF) {

        // get default physical address
        case 002: {
            if (! z11p->dmawritelocked (pcbaddr + 2, defethaddr[0])) goto dmaerror;
            if (! z11p->dmawritelocked (pcbaddr + 4, defethaddr[1])) goto dmaerror;
            if (! z11p->dmawritelocked (pcbaddr + 6, defethaddr[2])) goto dmaerror;
            break;
        }

        // get current physical address
        case 004: {
            if (! z11p->dmawritelocked (pcbaddr + 2, curethaddr[0])) goto dmaerror;
            if (! z11p->dmawritelocked (pcbaddr + 4, curethaddr[1])) goto dmaerror;
            if (! z11p->dmawritelocked (pcbaddr + 6, curethaddr[2])) goto dmaerror;
            break;
        }

        // set current physical address
        case 005: {
            if (z11p->dmareadlocked (pcbaddr + 2, &curethaddr[0]) != 0) goto dmaerror;
            curethaddr[0] &= 0xFFFEU;
            if (z11p->dmareadlocked (pcbaddr + 4, &curethaddr[1]) != 0) goto dmaerror;
            if (z11p->dmareadlocked (pcbaddr + 6, &curethaddr[2]) != 0) goto dmaerror;
            break;
        }

        // get multicast table
        case 006: {
            uint16_t udbblo, udbbhi;
            if (z11p->dmareadlocked (pcbaddr + 2, &udbblo) != 0) goto dmaerror;
            if (z11p->dmareadlocked (pcbaddr + 4, &udbbhi) != 0) goto dmaerror;
            uint32_t udbb = (((uint32_t) udbbhi << 16) | udbblo) & 0777776;
            uint16_t mltlen = udbbhi >> 8;
            for (uint16_t i = 0; (i < 10) && (i < mltlen); i ++) {
                if (! z11p->dmawritelocked (udbb + i * 6 + 0, mltaddrtable[i*3+0])) goto dmaerror;
                if (! z11p->dmawritelocked (udbb + i * 6 + 2, mltaddrtable[i*3+1])) goto dmaerror;
                if (! z11p->dmawritelocked (udbb + i * 6 + 4, mltaddrtable[i*3+2])) goto dmaerror;
            }
            break;
        }

        // set multicast table
        case 007: {
            uint16_t udbblo, udbbhi;
            if (z11p->dmareadlocked (pcbaddr + 2, &udbblo) != 0) goto dmaerror;
            if (z11p->dmareadlocked (pcbaddr + 4, &udbbhi) != 0) goto dmaerror;
            uint32_t udbb = (((uint32_t) udbbhi << 16) | udbblo) & 0777776;
            uint16_t mltlen = udbbhi >> 8;
            for (uint16_t i = 0; (i < 10) && (i < mltlen); i ++) {
                if (z11p->dmareadlocked (udbb + i * 6 + 0, &mltaddrtable[i*3+0])) goto dmaerror;
                mltaddrtable[i*3+0] |= 1;
                if (z11p->dmareadlocked (udbb + i * 6 + 2, &mltaddrtable[i*3+1])) goto dmaerror;
                if (z11p->dmareadlocked (udbb + i * 6 + 4, &mltaddrtable[i*3+2])) goto dmaerror;
            }
            break;
        }

        // get ring format
        case 010: {
            uint16_t rfbuf[6], udbblo, udbbhi;
            if (z11p->dmareadlocked (pcbaddr + 2, &udbblo) != 0) goto dmaerror;
            if (z11p->dmareadlocked (pcbaddr + 4, &udbbhi) != 0) goto dmaerror;
            uint32_t udbb = (((uint32_t) udbbhi << 16) | udbblo) & 0777776;
            rfbuf[0] = ringfmt.tdrb;
            rfbuf[1] = ((uint16_t) ringfmt.telen << 8) | (ringfmt.tdrb >> 16);
            rfbuf[2] = ringfmt.trlen;
            rfbuf[3] = ringfmt.rdrb;
            rfbuf[4] = ((uint16_t) ringfmt.relen << 8) | (ringfmt.rdrb >> 16);
            rfbuf[5] = ringfmt.rrlen;
            for (uint32_t i = 0; i < 6; i ++) {
                if (! z11p->dmawritelocked ((udbb + i * 2) & 0777776, rfbuf[i])) goto dmaerror;
            }
            break;
        }

        // set ring format
        case 011: {
            uint16_t rfbuf[6], udbblo, udbbhi;
            if (z11p->dmareadlocked (pcbaddr + 2, &udbblo) != 0) goto dmaerror;
            if (z11p->dmareadlocked (pcbaddr + 4, &udbbhi) != 0) goto dmaerror;
            uint32_t udbb = (((uint32_t) udbbhi << 16) | udbblo) & 0777776;
            for (uint32_t i = 0; i < 6; i ++) {
                if (z11p->dmareadlocked ((udbb + i * 2) & 0777776, &rfbuf[i]) != 0) goto dmaerror;
            }
            ringfmt.rdrb  = (((uint32_t) rfbuf[4] << 16) | rfbuf[3]) & 0777776;
            ringfmt.tdrb  = (((uint32_t) rfbuf[1] << 16) | rfbuf[0]) & 0777776;
            ringfmt.rrlen = rfbuf[5];
            ringfmt.trlen = rfbuf[2];
            ringfmt.relen = rfbuf[4] >> 8;
            ringfmt.telen = rfbuf[1] >> 8;

            rdrca = ringfmt.rdrb;
            tdrca = ringfmt.tdrb;
            break;
        }

        // read/read+clear counters
        case 012:
        case 013: {
            uint16_t ctrlen, udbblo, udbbhi;
            if (z11p->dmareadlocked (pcbaddr + 2, &udbblo) != 0) goto dmaerror;
            if (z11p->dmareadlocked (pcbaddr + 4, &udbbhi) != 0) goto dmaerror;
            if (z11p->dmareadlocked (pcbaddr + 6, &ctrlen) != 0) goto dmaerror;
            uint32_t udbb = (((uint32_t) udbbhi << 16) | udbblo) & 0777776;

            counters.ubusdblen = ctrlen / 2;
            if (counters.ubusdblen > sizeof counters / 2) counters.ubusdblen = sizeof counters / 2;

            time_t dt = time (NULL) - counterszeroed;
            if (dt > 65535) dt = 65535;
            counters.secsincezer = dt;

            for (uint16_t i = 0; counters.ubusdblen; i ++) {
                if (! z11p->dmawritelocked (udbb + i * 2, ((uint16_t *)&counters)[i])) goto dmaerror;
            }
            if (pcbdata[0] & 1) {
                memset (&counters, 0, sizeof counters);
                counterszeroed = time (NULL);
            }
            break;
        }

        // read/write mode (p92/v4-36)
        case 014: {
            if (! z11p->dmawritelocked (pcbaddr + 2, modebits)) goto dmaerror;
            break;
        }
        case 015: {
            if (z11p->dmareadlocked (pcbaddr + 2, &modebits) != 0) goto dmaerror;
            modebits &= 0xFA0F;
            break;
        }

        // read/read+clear port status
        case 016:
        case 017: {
            uint16_t portst2 = ((uint16_t) curmlt << 8) | MAXMLT;
            if (! z11p->dmawritelocked (pcbaddr + 2, portst1 | RREV)) goto dmaerror;
            if (! z11p->dmawritelocked (pcbaddr + 4, portst2)) goto dmaerror;
            if (! z11p->dmawritelocked (pcbaddr + 6, MAXCTR)) goto dmaerror;
            if (pcbdata[0] & 1) portst1 = 0;
            break;
        }
    }
    writepcsr0 (DNI | HIJK);
    goto done;
dmaerror:;
    pcsr1 |= PCTO;                  // set PCTO - set port command timeout
    writepcsr0 (PCEI | DNI | HIJK);
done:;
    z11p->dmaunlk ();
}

// process incoming packets, passing them along to the PDP
static void *receivethread (void *dummy)
{
    while (true) {
        uint16_t rcvbuf[757];

        int rc = read (sockfd, rcvbuf, sizeof rcvbuf);
        if (rc < 0) {
            fprintf (stderr, "z11xe: error receiving packet: %m\n");
            ABORT ();
        }
        if (rc < 14) {
            fprintf (stderr, "z11xe: runt %d-byte packet received\n", rc);
            continue;
        }
        lockit (__LINE__);
        if (debug > 2) {
            fprintf (stderr, "receivethread: rc=%05d:", rc);
            for (int i = 0; (i < 20) & (i < rc); i ++) fprintf (stderr, " %02X", ((uint8_t *)rcvbuf)[i]);
            fprintf (stderr, "\n");
        }

        if ((pcsr1 & RUN) && matchesincoming (rcvbuf)) {
            if (ringfmt.rrlen == 0) {
                if (! rcbisetinh) writepcsr0 (RCBI);
                rcbisetinh = true;  // don't set again until told to rescan ring
            } else {
                uint32_t packetsrcvd   = counters.packetsrcvd   + 1;    // p89
                uint32_t databytesrcvd = counters.databytesrcvd + rc - 14;
                if (packetsrcvd   < counters.packetsrcvd)   packetsrcvd   = 0xFFFFFFFFU;
                if (databytesrcvd < counters.databytesrcvd) databytesrcvd = 0xFFFFFFFFU;
                counters.packetsrcvd   = packetsrcvd;
                counters.databytesrcvd = databytesrcvd;

                z11p->dmalock ();

                uint16_t numbytestogo = (uint16_t) rc;
                uint16_t index  = 0;
                uint16_t status = 0;
                do {

                    // hopefully descriptor entry pointed to by rdrca is owned by DEUNA
                    // p113
                    uint16_t rdrb[3];
                    for (int i = 0; i < 3; i ++) {
                        if (z11p->dmareadlocked ((rdrca + i * 2) & 0777776, &rdrb[i]) != 0) goto dmaerror;
                    }
                    if (! (rdrb[2] & OWN)) {
                        if (! rcbisetinh) status |= RCBI;       // set RCBI - receive buffer unabailable interrupt
                        rcbisetinh = true;                      // don't set again until told to rescan ring
                        break;
                    }

                    // compute address of next ring entry, possibly wrapping
                    uint32_t rdrna = (rdrca + ringfmt.relen * 2) & 0777776;
                    if (((rdrna - ringfmt.rdrb) & 0777776) >= (ringfmt.rrlen * ringfmt.relen * 2)) {
                        rdrna = ringfmt.rdrb;
                    }

                    // set up status values
                    uint8_t  byte5 = 0;
                    uint16_t word6 = 0;

                    if (index == 0) byte5 |= STP >> 8;          // STP - start of packet
                    if (modebits & MODE_DRDC) word6 |= NCHN;    // NCHN - not in chaining mode

                    // copy as much as we can to this descriptor's buffer
                    uint16_t amountfits = (numbytestogo < (rdrb[0] & 0xFFFEU)) ? numbytestogo : (rdrb[0] & 0xFFFEU);
                    uint32_t segb = (((uint32_t) rdrb[2] << 16) | rdrb[1]) & 0777776;
                    for (uint16_t i = 0; i + 2 <= amountfits; i += 2) {
                        if (! z11p->dmawritelocked (segb, rcvbuf[index])) {
                            word6 |= UBTO;
                            goto badbuf;
                        }
                        segb += 2;
                        index ++;
                    }
                    if (amountfits & 1) {
                        ASSERT (amountfits == numbytestogo);
                        if (! z11p->dmawbytelocked (segb, rcvbuf[index])) {
                            word6 |= UBTO;
                            goto badbuf;
                        }
                        segb ++;
                    }
                badbuf:;

                    // see if that was last of message
                    uint16_t nextowner;
                    if (amountfits == numbytestogo) {
                        byte5 |= ENP >> 8;                      // ENP - end of packet
                        word6 |= rc & 0x0FFFU;                  // MLEN - total message length in bytes
                    }

                    // if it didn't all fit, peek at owner of next descriptor in ring
                    // if no further entry, assume it is owned by the PDP
                    else if ((modebits & MODE_DRDC) ||
                             (z11p->dmareadlocked ((rdrna + 4) & 0777776, &nextowner) != 0) ||
                             ! (nextowner & OWN)) {
                        word6 |= BUFL;                          // BUFL - buffer length error
                        numbytestogo = amountfits;              // chop!
                    }

                    // write status out to memory, turning ownership over to PDP
                    if (! z11p->dmawritelocked (rdrca + 6, word6)) goto dmaerror;
                    if (! z11p->dmawbytelocked (rdrca + 5, byte5)) goto dmaerror;

                    // set RXI - receive ring interrupt
                    status |= RXI;

                    // advance on to next ring entry
                    rdrca = rdrna;

                    // maybe there is more of this packet to process
                    numbytestogo -= amountfits;
                } while (numbytestogo > 0);

                z11p->dmaunlk ();

                // set RXI (receive ring interrupt) or RCBI (receive buffer unavailable) plus INTR
                // p63
                writepcsr0 (status);
            }
            goto done;
        dmaerror:;
            z11p->dmaunlk ();
            portst1 |= ERRS | TMOT | RRNG;  // receive ring error (p96)
            writepcsr0 (SERI);              // port status error
        done:;
        }
        unlkit (__LINE__);
    }
}

// see if incoming packet matches receive filters
static bool matchesincoming (uint16_t const *rcvbuf)
{
    if (modebits & MODE_PROM) return true;
    if (! (rcvbuf[0] & 1)) return memcmp (rcvbuf, curethaddr, 6) == 0;
    return memcmp (rcvbuf, bdcastaddr, 6) == 0;
}

static void *transmithread (void *dummy)
{
    bool didsomething = false;
    uint8_t xmtbuf[1514];
    uint32_t index = 0xFFFFFFFFU;

    lockit (__LINE__);
    while (true) {

        // p84
        if (! (pcsr1 & RUN) || (ringfmt.trlen == 0) || ! didsomething) {

            // wait for something in ring
            waittransmit ();

            didsomething = true;
        } else {
            uint16_t tdrb[4];
            uint32_t segb;
            uint32_t tdrpa;

            if (debug > 0) fprintf (stderr, "transmithread: trlen=%u\n", ringfmt.trlen);

            z11p->dmalock ();

            // get descriptor
            // p108
            for (int i = 0; i < 4; i ++) {
                if (z11p->dmareadlocked ((tdrca + i * 2) & 0777776, &tdrb[i]) != 0) goto dmaerror;
            }

            // OWN - stop if owned by the PDP
            if (! (tdrb[2] & OWN)) {

                // set TXI (transmit ring interrupt)
                // p63/v4-7
                writepcsr0 (TXI);       // transmitter going idle
                didsomething = false;   // do the waittransmit() call above
                goto dmaunlok;
            }

            // increment to next ring entry, wrapping if necessary
            tdrpa = tdrca;
            tdrca = (tdrca + ringfmt.telen * 2) & 0777776;
            if (((tdrca - ringfmt.tdrb) & 0777776) >= (ringfmt.trlen * ringfmt.telen * 2)) {
                tdrca = ringfmt.tdrb;
            }

            // see how many bytes in buffer and check for overflow
            if (tdrb[2] & 0x200) index = 0;         // STP - start of buffer

            if (index == 0xFFFFFFFFU) {             // looking for start of buffer (STP)
                if (! z11p->dmawritelocked (tdrpa + 6, tdrb[3] | BUFL)) goto dmaerror;
                goto dmaunlok;                      // not found, set BUFL then check next descriptor
            }

            if (index + tdrb[0] > sizeof xmtbuf) {  // see if message too int
                if (! z11p->dmawritelocked (tdrpa + 6, tdrb[3] | BUFL)) goto dmaerror;
                index = 0xFFFFFFFFU;
                goto dmaunlok;                      // if so, set BUFL then check next descriptor
            }

            // transfer from unibus to xmtbuf
            segb = ((uint32_t) tdrb[2] << 16) | tdrb[1];
            while (tdrb[0] > 0) {
                uint16_t word;
                if (z11p->dmareadlocked (segb & 0777776, &word) != 0) {
                    if (! z11p->dmawritelocked (tdrpa + 6, tdrb[3] | UBTO)) goto dmaerror;
                    goto relpacket;
                }
                if (segb & 1) {
                    xmtbuf[index++] = word >> 8;
                    segb ++;
                    tdrb[0] --;
                    continue;
                }
                if (tdrb[0] == 1) {
                    xmtbuf[index++] = word;
                    break;
                }
                xmtbuf[index++] = word;
                xmtbuf[index++] = word >> 8;
                segb += 2;
                tdrb[0] -= 2;
            }

            // if ENP (end of packet), transmit packet
            if (tdrb[2] & ENP) {

                z11p->dmaunlk ();
                unlkit (__LINE__);

                int rc = sendto (sockfd, xmtbuf, index, 0, (sockaddr const *) &sendtoaddr, sizeof sendtoaddr);
                if (rc < 0) {
                    fprintf (stderr, "z11xe: error transmitting: %m\n");
                    ABORT ();
                }
                if (rc < (int) index) {
                    fprintf (stderr, "z11xe: only sent %d bytes of %u byte packet\n", rc, index);
                    ABORT ();
                }

                lockit (__LINE__);

                // increment counters
                uint32_t packetsxmtd   = counters.packetsxmtd   + 1;        // p89
                uint32_t databytesxmtd = counters.databytesxmtd + rc - 14;
                if (packetsxmtd   < counters.packetsxmtd)   packetsxmtd   = 0xFFFFFFFFU;
                if (databytesxmtd < counters.databytesxmtd) databytesxmtd = 0xFFFFFFFFU;
                counters.packetsxmtd   = packetsxmtd;
                counters.databytesxmtd = databytesxmtd;

                // looking for an STP (start of packet) flag for another packet to send
                index = 0xFFFFFFFFU;

                z11p->dmalock ();
            }

            // mark packet owned by PDP
        relpacket:;
            if (z11p->dmawritelocked (tdrpa + 4, tdrb[2] & ~ OWN)) goto dmaunlok;

        dmaerror:;
            portst1 |= ERRS | TMOT | TRNG;  // transmit ring error (p96)
            writepcsr0 (SERI);              // port status error
        dmaunlok:;
            z11p->dmaunlk ();
        }
    }
    unlkit (__LINE__);
}

static void writepcsr0 (uint16_t pcsr0)
{
    ZWR(xeat[1], ((uint32_t) pcsr1 << 16) | pcsr0);
}

static void lockit (int line)
{
    if (pthread_mutex_lock (&themutex) != 0) ABORT ();
    if (debug > 1) fprintf (stderr, "lockit: line %d resumed\n", line);
}

static void unlkit (int line)
{
    if (debug > 1) fprintf (stderr, "unlkit: line %d\n", line);
    if (pthread_mutex_unlock (&themutex) != 0) ABORT ();
}

static void waketransmit ()
{
    if (pthread_cond_broadcast (&thecond) != 0) ABORT ();
}

static void waittransmit ()
{
    if (debug > 1) fprintf (stderr, "waittransmit: waiting\n");
    if (pthread_cond_wait (&thecond, &themutex) != 0) ABORT ();
    if (debug > 1) fprintf (stderr, "waittransmit: resumed\n");
}
