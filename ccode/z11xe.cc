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
// also there's DELUA User's Guide Apr 86
// http://www.bitsavers.org/pdf/dec/unibus/

// normally run as daemon:
//  ./z11xe -daemon
// run as command for debugging:
//  ./z11xe

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "z11defs.h"
#include "z11util.h"

#define MINPKTLEN 64

#define SERI 0x8000     // PCSR0 status error interrupt
#define PCEI 0x4000     // PCSR0 port command error interrupt
#define RXI  0x2000     // PCSR0 receive ring interrupt
#define TXI  0x1000     // PCSR0 transmit ring interrupt
#define DNI  0x0800     // PCSR0 done interrupt
#define RCBI 0x0400     // PCSR0 receive buffer unavailable interrupt
#define USCI 0x0100     // PCSR0 unsolicited state change interrupt
#define INTR 0x0080     // PCSR0 interrupt summary (automatically kept up-to-date by fpga)
#define RSET 0x0020     // PCSR0 deuna reset
#define HIJK 0x0010     // PCSR0 hi-jacked go bit (set by fpga when RSET set or PCMD written)
#define PCMD 0x000F     // PCSR0 port command

#define RUN  0x0001     // PCSR1 running
#define RDY  0x0002     // PCSR1 ready
#define DELU 0x0010     // PCSR1 is a DELUA
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

union Packet { uint16_t w[757]; uint8_t b[1514]; };

static uint16_t const bdcastaddr[3] = { 0xFFFFU, 0xFFFFU, 0xFFFFU };
static uint16_t defethaddr[3] = { 0x0008U, 0x002BU, 0 };

static bool rcbisetinh;
static Counters counters;
static int debug;
static int sockfd;
static Packet rcvp, xmtp;
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
static uint32_t volatile *xeat;         // fpga xe11.v registers
static uint32_t pcbaddr;
static uint32_t rdrca;                  // receive descriptor ring current address
static uint32_t tdrca;                  // transmit descriptor ring current address

static void xeiothread ();
static void xeiothread_locked ();
static void doreset ();
static void getcommand_locked ();
static void getcommand_dmalkd ();
static void *receivethread (void *dummy);
static void *transmithread (void *dummy);
static void transmithread_locked ();
static bool transmithread_dmalkd ();
static void transmithread_dmaulk (int xmtlen);
static void gotincoming_locked (int rc);
static uint16_t gotincoming_dmalkd (uint16_t rcvlen);
static bool matchesincoming (uint16_t const *rcvwrd);
static void writepcsr0 (uint16_t pcsr0);
static void lockit ();
static void unlkit ();
static void waketransmit ();
static void waittransmit ();

int main (int argc, char **argv)
{
    setlinebuf (stderr);
    setlinebuf (stdout);

    bool daemfl = false;
    bool killit = false;
    bool macopt = false;
    int xgid = -1;
    int xuid = -1;
    char const *ethdev = "eth0";
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("  Handle DEUNA/DELUA ethernet I/O");
            puts ("");
            puts ("    sudo ./z11xe [-daemon] [-eth <device>] [-gid <gid>] [-killit] [-mac <address>] [-uid <uid>]");
            puts ("");
            puts ("      -daemon = daemonize, redirect log to /tmp/z11xe.log.(time)");
            puts ("      -eth    = use given ethernet device (default eth0)");
            puts ("      -gid    = set group id after opening ethernet");
            puts ("      -killit = kill other instance already running");
            puts ("      -mac    = use given mac address");
            puts ("      -uid    = set user id after opening ethernet");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-daemon") == 0) {
            daemfl = true;
            continue;
        }
        if (strcasecmp (argv[i], "-eth") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing device after -eth\n");
                return 1;
            }
            ethdev = argv[i];
            continue;
        }
        if (strcasecmp (argv[i], "-gid") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing group id after -gid\n");
                return 1;
            }
            xgid = atoi (argv[i]);
            continue;
        }
        if (strcasecmp (argv[i], "-killit") == 0) {
            killit = true;
            continue;
        }
        if (strcasecmp (argv[i], "-mac") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing address after -mac\n");
                return 1;
            }
            uint8_t addr[6];
            char *p = argv[i];
            int j;
            for (j = 0; j < 6;) {
                uint32_t v = strtoul (p, &p, 16);
                if (v > 255) goto badmacaddr;
                addr[j++] = v;
                if (*p == 0) goto gotmacaddr;
                if (*(p ++) != ':') goto badmacaddr;
            }
        badmacaddr:;
            fprintf (stderr, "bad mac address %s\n", argv[i]);
            return 1;
        gotmacaddr:;
            memcpy (((uint8_t *)defethaddr) + 6 - j, addr, j);
            macopt = true;
            continue;
        }
        if (strcasecmp (argv[i], "-uid") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing user id after -uid\n");
                return 1;
            }
            xuid = atoi (argv[i]);
            continue;
        }
        fprintf (stderr, "unknown option/argument %s\n", argv[i]);
        return 1;
    }

    // if -mac not given, use first xor last 3 bytes of hardware eth0 as last 3 bytes of DEUNA
    if (! macopt) {

        // get ethdev MAC address
        char macname[strlen(ethdev)+26];
        sprintf (macname, "/sys/class/net/%s/address", ethdev);
        FILE *macfile = fopen (macname, "r");
        if (macfile == NULL) {
            fprintf (stderr, "z11xe: error opening %s: %m\n", macname);
            ABORT ();
        }
        char macline[40];
        if (fgets (macline, sizeof macline, macfile) == NULL) macline[0] = 0;
        fclose (macfile);
        unsigned int macaddr[6];
        if (sscanf (macline, "%x:%x:%x:%x:%x:%x", &macaddr[0], &macaddr[1],
                &macaddr[2], &macaddr[3], &macaddr[4], &macaddr[5]) != 6) {
            fprintf (stderr, "z11xe: bad %s mac address %s\n", macname, macline);
            ABORT ();
        }

        // save it in low 3 bytes of our MAC address
        // top 3 bytes retain 08:00:2B
        ((uint8_t *)defethaddr)[3] = macaddr[3] ^ macaddr[0];
        ((uint8_t *)defethaddr)[4] = macaddr[4] ^ macaddr[1];
        ((uint8_t *)defethaddr)[5] = macaddr[5] ^ macaddr[2];
    }

    // set up to transmit out using the given ethernet device
    sendtoaddr.sll_family = AF_PACKET;
    sendtoaddr.sll_ifindex = if_nametoindex (ethdev);
    if (sendtoaddr.sll_ifindex <= 0) {
        fprintf (stderr, "z11xe: unknown ethernet device %s: %m\n", ethdev);
        return 1;
    }

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

    // maybe set uid & gid now that we don't need root access any more
    if (((xgid >= 0) && (setresgid (xgid, xgid, xgid) < 0)) ||
        ((xuid >= 0) && (setresuid (xuid, xuid, xuid) < 0))) {
        fprintf (stderr, "z11xe: failed to set uid,gid to %d,%d: %m\n", xuid, xgid);
        ABORT ();
    }

    // set up default mac address to be current mac address
    memcpy (curethaddr, defethaddr, 6);

    fprintf (stderr, "z11xe: mac %02X:%02X:%02X:%02X:%02X:%02X on line %s\n",
        curethaddr[0] & 0xFF, curethaddr[0] >> 8, curethaddr[1] & 0xFF, curethaddr[1] >> 8,
        curethaddr[2] & 0xFF, curethaddr[2] >> 8, ethdev);

    debug = 0;
    char const *dbgenv = getenv ("z11xe_debug");
    if (dbgenv != NULL) debug = atoi (dbgenv);

    // maybe daemonize
    if (daemfl) {

        // open /dev/null for stdin and /tmp/z11xe.log.(time) for stdout,stderr
        int nulfd = open ("/dev/null", O_RDONLY);
        if (nulfd < 0) ABORT ();
        char logname[30];
        sprintf (logname, "/tmp/z11xe.log.%u", (unsigned) time (NULL));
        int logfd = open (logname, O_WRONLY | O_CREAT, 0666);
        if (logfd < 0) {
            fprintf (stderr, "z11xe: error creating %s: %m\n", logname);
            ABORT ();
        }

        // fork/exit to create detached process
        if (daemon (0, 1) < 0) {
            fprintf (stderr, "z11xe: error daemonizing: %m\n");
            ABORT ();
        }

        // redirect stdin,stdout,stderr
        dup2 (nulfd, STDIN_FILENO);
        dup2 (logfd, STDOUT_FILENO);
        dup2 (logfd, STDERR_FILENO);
        close (nulfd);
        close (logfd);
    }

    // access fpga register set for the DEUNA controller
    // lock it so we are only process accessing it
    z11page = new Z11Page ();
    xeat = z11page->findev ("XE", NULL, NULL, true, killit);

    // enable board to process io instructions
    pcsr1 = DELU;   // 0=DEUNA - rsx4.5 decnet works with DELUA, not DEUNA (tries to upload mystery microcode)
    ZWR(xeat[1], pcsr1 << 16);
    ZWR(xeat[3], XE3_ENAB);

    pthread_t tid;
    int rc = pthread_create (&tid, NULL, receivethread, NULL);
    if (rc != 0) ABORT ();
    rc = pthread_create (&tid, NULL, transmithread, NULL);
    if (rc != 0) ABORT ();

    xeiothread ();

    return 0;
}

static char const* const portcommands[] = {
    "NOP", "GET_PCBB", "GET_CMD", "SELF_TEST", "START", "BOOT", "NOT_USED_6", "NOT_USED_7",
    "PDMD", "NOT_USED_9", "NOT_USED_10", "NOT_USED_11", "NOT_USED_12", "NOT_USED_13", "NOT_USED_14", "STOP" };

// process functions in PCSR0<03:00> in response to PDP writes
static void xeiothread ()
{
    while (true) {

        // wait for PDP to set RSET or write PCMD
        // FPGA hijacks PCSR0<04>, setting it whenever PDP sets RSET or writes PCMD
        // ...which wakes up the arm
        z11page->waitint (ZGINT_XE);

        lockit ();
        xeiothread_locked ();
        unlkit ();
    }
}

// process function (reset or port command) posted to registers by PDP
// 'themutex' locked on entry and exit
static void xeiothread_locked ()
{
    uint16_t pcsr0 = ZRD(xeat[1]);          // read PCSR0
    if (debug > 0) fprintf (stderr, "xeiothread: PCSR0=%06o=%04X\n", pcsr0, pcsr0);

    if (! (pcsr0 & HIJK)) return;           // make sure hijack bit is set, not spurious wakeup

    if (pcsr0 & RSET) {                     // check for reset, can also be from bus_init just released
        if (debug > 0) fprintf (stderr, "xeiothread:   reset\n");
        doreset ();
        writepcsr0 (DNI | RSET | HIJK);     // set done bit, clear reset and hijack bits
        return;
    }

    if (debug > 0) fprintf (stderr, "xeiothread:   portcmd=%s\n", portcommands[pcsr0&PCMD]);
    switch (pcsr0 & PCMD) {                 // decode port command
        case 0:
        case 6:
        case 7: {   // nop without setting DNI
            writepcsr0 (HIJK);
            return;
        }
        case 1: {   // get pcbb
            pcbaddr = ZRD(xeat[2]) & 0777776;
            break;
        }
        case 2: {   // get cmd
            getcommand_locked ();
            return;
        }
        case 3: {   // self test
            doreset ();
            break;
        }
        case 4: {   // start
            if (! (pcsr1 & RUN)) {          // nop if already running
                rcbisetinh = false;
                pcsr1 |=   RUN;             // set running bit (state = 3)
                rdrca = ringfmt.rdrb;       // reset rings
                tdrca = ringfmt.tdrb;
                waketransmit ();            // see if anything ready to transmit
            }
            break;
        }
        case 5: {   // boot
            break;
        }
        case 8: {   // polling demand
            rcbisetinh = false;
            waketransmit ();                // see if anything ready to transmit
            break;
        }
        case 15: {  // stop
            pcsr1 &= ~ RUN;                 // clear running bit (state = 2)
            break;
        }
    }

    writepcsr0 (DNI | HIJK);                // set port command done bit, clear hijack bit
}

// bus_init just released, RSET just set, or self-test command
// p131/v4-75
static void doreset ()
{
    pcsr1 = (pcsr1 & DELU) | RDY;                // set ready state, not running
    modebits = 0;
    rcbisetinh = false;
    curmlt = 0;
    memcpy (curethaddr, defethaddr, 6);

    memset (&counters, 0, sizeof counters);
    counterszeroed = time (NULL);
}

static char const *const pcbfuncs[] = {
    "NOP", "STUC", "RDPA", "NOP_3", "RPA", "WPA", "RMAL", "WMAL",
    "RRF", "WRF", "RC", "RCC", "RDMOD", "WRMOD", "RPS", "RCPS",
    "RDUC", "LDUC", "RSID", "WSID", "RLSA", "WLSA" };

// process command in PCB
// sets DNI or PCEI bit when done (p61/v4-5)
// 'themutex' locked on entry and exit
static void getcommand_locked ()
{
    z11page->dmalock ();
    getcommand_dmalkd ();
    z11page->dmaunlk ();
}

// 'themutex' and 'dmamutex' locked on entry and exit
static void getcommand_dmalkd ()
{
    uint16_t pcbdata[4];
    uint16_t pcbfc;

    pcsr1 &= ~ PCTO;

    if (z11page->dmareadlocked (pcbaddr, &pcbdata[0]) != 0) goto dmaerror;
    pcbfc = pcbdata[0] & 0xFF;
    if (debug > 0) fprintf (stderr, "gotcommand: func=%03o  %s\n", pcbfc, ((pcbfc < 026) ? pcbfuncs[pcbfc] : ""));
    switch (pcbfc) {

        // nop
        case 000: {
            break;
        }

        // get default ethernet address
        case 002: {
            if (! z11page->dmawritelocked (pcbaddr + 2, defethaddr[0])) goto dmaerror;
            if (! z11page->dmawritelocked (pcbaddr + 4, defethaddr[1])) goto dmaerror;
            if (! z11page->dmawritelocked (pcbaddr + 6, defethaddr[2])) goto dmaerror;
            break;
        }

        // get current ethernet address
        case 004: {
            if (! z11page->dmawritelocked (pcbaddr + 2, curethaddr[0])) goto dmaerror;
            if (! z11page->dmawritelocked (pcbaddr + 4, curethaddr[1])) goto dmaerror;
            if (! z11page->dmawritelocked (pcbaddr + 6, curethaddr[2])) goto dmaerror;
            break;
        }

        // set current ethernet address
        case 005: {
            if (z11page->dmareadlocked (pcbaddr + 2, &curethaddr[0]) != 0) goto dmaerror;
            curethaddr[0] &= 0xFFFEU;
            if (z11page->dmareadlocked (pcbaddr + 4, &curethaddr[1]) != 0) goto dmaerror;
            if (z11page->dmareadlocked (pcbaddr + 6, &curethaddr[2]) != 0) goto dmaerror;
            break;
        }

        // get multicast table (p80/v4-24)
        case 006: {
            uint16_t udbblo, udbbhi;
            if (z11page->dmareadlocked (pcbaddr + 2, &udbblo) != 0) goto dmaerror;
            if (z11page->dmareadlocked (pcbaddr + 4, &udbbhi) != 0) goto dmaerror;
            uint32_t udbb = (((uint32_t) udbbhi << 16) | udbblo) & 0777776;
            uint16_t mltlen = udbbhi >> 8;
            if (mltlen > MAXMLT) goto funceror;
            for (uint16_t i = 0; (i < curmlt) && (i < mltlen); i ++) {
                if (! z11page->dmawritelocked (udbb + i * 6 + 0, mltaddrtable[i*3+0])) goto dmaerror;
                if (! z11page->dmawritelocked (udbb + i * 6 + 2, mltaddrtable[i*3+1])) goto dmaerror;
                if (! z11page->dmawritelocked (udbb + i * 6 + 4, mltaddrtable[i*3+2])) goto dmaerror;
            }
            break;
        }

        // set multicast table
        case 007: {
            uint16_t udbblo, udbbhi;
            if (z11page->dmareadlocked (pcbaddr + 2, &udbblo) != 0) goto dmaerror;
            if (z11page->dmareadlocked (pcbaddr + 4, &udbbhi) != 0) goto dmaerror;
            uint32_t udbb = (((uint32_t) udbbhi << 16) | udbblo) & 0777776;
            uint16_t mltlen = udbbhi >> 8;
            if (mltlen > MAXMLT) goto funceror;
            curmlt = 0;
            for (uint16_t i = 0; i < mltlen; i ++) {
                if (z11page->dmareadlocked (udbb + i * 6 + 0, &mltaddrtable[i*3+0])) goto dmaerror;
                if (z11page->dmareadlocked (udbb + i * 6 + 2, &mltaddrtable[i*3+1])) goto dmaerror;
                if (z11page->dmareadlocked (udbb + i * 6 + 4, &mltaddrtable[i*3+2])) goto dmaerror;
                if (! (mltaddrtable[i*3+0] & 1)) goto funceror;
                if (debug > 1) fprintf (stderr, "gotcommand:  mltaddrtable[%u] = %02X:%02X:%02X:%02X:%02X:%02X\n",
                    i, mltaddrtable[i*3+0] & 0xFF, mltaddrtable[i*3+0] >> 8,
                       mltaddrtable[i*3+1] & 0xFF, mltaddrtable[i*3+1] >> 8,
                       mltaddrtable[i*3+2] & 0xFF, mltaddrtable[i*3+2] >> 8);
                curmlt ++;
            }
            break;
        }

        // get ring format
        case 010: {
            uint16_t rfbuf[6], udbblo, udbbhi;
            if (z11page->dmareadlocked (pcbaddr + 2, &udbblo) != 0) goto dmaerror;
            if (z11page->dmareadlocked (pcbaddr + 4, &udbbhi) != 0) goto dmaerror;
            uint32_t udbb = (((uint32_t) udbbhi << 16) | udbblo) & 0777776;
            rfbuf[0] = ringfmt.tdrb;
            rfbuf[1] = ((uint16_t) ringfmt.telen << 8) | (ringfmt.tdrb >> 16);
            rfbuf[2] = ringfmt.trlen;
            rfbuf[3] = ringfmt.rdrb;
            rfbuf[4] = ((uint16_t) ringfmt.relen << 8) | (ringfmt.rdrb >> 16);
            rfbuf[5] = ringfmt.rrlen;
            for (uint32_t i = 0; i < 6; i ++) {
                if (! z11page->dmawritelocked ((udbb + i * 2) & 0777776, rfbuf[i])) goto dmaerror;
            }
            break;
        }

        // set ring format
        case 011: {
            if (pcsr1 & RUN) goto funceror;
            uint16_t rfbuf[6], udbblo, udbbhi;
            if (z11page->dmareadlocked (pcbaddr + 2, &udbblo) != 0) goto dmaerror;
            if (z11page->dmareadlocked (pcbaddr + 4, &udbbhi) != 0) goto dmaerror;
            uint32_t udbb = (((uint32_t) udbbhi << 16) | udbblo) & 0777776;
            for (uint32_t i = 0; i < 6; i ++) {
                if (z11page->dmareadlocked ((udbb + i * 2) & 0777776, &rfbuf[i]) != 0) goto dmaerror;
            }
            ringfmt.rdrb  = (((uint32_t) rfbuf[4] << 16) | rfbuf[3]) & 0777776;
            ringfmt.tdrb  = (((uint32_t) rfbuf[1] << 16) | rfbuf[0]) & 0777776;
            ringfmt.rrlen = rfbuf[5];
            ringfmt.trlen = rfbuf[2];
            ringfmt.relen = rfbuf[4] >> 8;
            ringfmt.telen = rfbuf[1] >> 8;
            if (debug > 1) fprintf (stderr, "getcommand:   rrlen=%u trlen=%u relen=%u telen=%u\n",
                ringfmt.rrlen, ringfmt.trlen, ringfmt.relen, ringfmt.telen);

            rdrca = ringfmt.rdrb;
            tdrca = ringfmt.tdrb;
            break;
        }

        // read/read+clear counters
        case 012:
        case 013: {
            uint16_t ctrlen, udbblo, udbbhi;
            if (z11page->dmareadlocked (pcbaddr + 2, &udbblo) != 0) goto dmaerror;
            if (z11page->dmareadlocked (pcbaddr + 4, &udbbhi) != 0) goto dmaerror;
            if (z11page->dmareadlocked (pcbaddr + 6, &ctrlen) != 0) goto dmaerror;
            uint32_t udbb = (((uint32_t) udbbhi << 16) | udbblo) & 0777776;

            counters.ubusdblen = ctrlen / 2;
            if (counters.ubusdblen > sizeof counters / 2) counters.ubusdblen = sizeof counters / 2;

            time_t dt = time (NULL) - counterszeroed;
            if (dt > 65535) dt = 65535;
            counters.secsincezer = dt;

            for (uint16_t i = 0; counters.ubusdblen; i ++) {
                if (! z11page->dmawritelocked (udbb + i * 2, ((uint16_t *)&counters)[i])) goto dmaerror;
            }
            if (pcbdata[0] & 1) {
                memset (&counters, 0, sizeof counters);
                counterszeroed = time (NULL);
            }
            break;
        }

        // read/write mode (p92/v4-36)
        case 014: {
            if (! z11page->dmawritelocked (pcbaddr + 2, modebits)) goto dmaerror;
            break;
        }
        case 015: {
            uint16_t mode;
            if (z11page->dmareadlocked (pcbaddr + 2, &mode) != 0) goto dmaerror;
            if (mode & 0x05F2) goto funceror;
            modebits = mode;
            if (debug > 1) fprintf (stderr, "getcommand:   modebits=%04X\n", modebits);
            break;
        }

        // read/read+clear port status
        case 016:
        case 017: {
            uint16_t portst2 = ((uint16_t) curmlt << 8) | MAXMLT;
            if (! z11page->dmawritelocked (pcbaddr + 2, portst1 | RREV)) goto dmaerror;
            if (! z11page->dmawritelocked (pcbaddr + 4, portst2)) goto dmaerror;
            if (! z11page->dmawritelocked (pcbaddr + 6, MAXCTR)) goto dmaerror;
            if (pcbdata[0] & 1) portst1 = 0;
            break;
        }

        // all others give error status
        default: goto funceror;
    }
    writepcsr0 (DNI | HIJK);
    return;

dmaerror:;
    pcsr1 |= PCTO;                  // set PCTO - set port command dma timeout
funceror:;
    writepcsr0 (PCEI | HIJK);
}

///////////////////////
//  TRANSMIT THREAD  //
///////////////////////

// wait for PDP to put buffers in ring then send out over ethernet
static void *transmithread (void *dummy)
{
    lockit ();
    transmithread_locked ();
    unlkit ();
    return NULL;
}

// process messages in transmit ring
// 'themutex' locked on entry and exit
static void transmithread_locked ()
{
    bool didsomething = false;

    while (true) {

        // p84
        if (! (pcsr1 & RUN) || (ringfmt.trlen == 0) || ! didsomething) {

            // wait for something in ring
            waittransmit ();

            didsomething = true;
        } else {
            if (debug > 0) fprintf (stderr, "transmithread: trlen=%u\n", ringfmt.trlen);

            z11page->dmalock ();
            didsomething = transmithread_dmalkd ();
            z11page->dmaunlk ();
        }
    }
}

// process messages in transmit ring
// 'themutex' and 'dmamutex' locked on entry and exit
// returns:
//  false: queue was empty, nothing processed
//   true: something was in queue, processed
static bool transmithread_dmalkd ()
{
    int xmtlen = -1;
    uint16_t tdrb[4];
    uint32_t segb;
    uint32_t tdrpa;

    // get descriptor
    // p108
    for (int i = 0; i < 4; i ++) {
        if (z11page->dmareadlocked ((tdrca + i * 2) & 0777776, &tdrb[i]) != 0) goto dmaerror;
    }

    // OWN - stop if owned by the PDP
    if (! (tdrb[2] & OWN)) {

        // set TXI (transmit ring interrupt)
        // p63/v4-7
        writepcsr0 (TXI);       // transmitter going idle
        return false;           // do the waittransmit() call above
    }

    // increment to next ring entry, wrapping if necessary
    tdrpa = tdrca;
    tdrca = (tdrca + ringfmt.telen * 2) & 0777776;
    if (((tdrca - ringfmt.tdrb) & 0777776) >= (ringfmt.trlen * ringfmt.telen * 2)) {
        tdrca = ringfmt.tdrb;
    }

    // see how many bytes in buffer and check for overflow
    if (tdrb[2] & STP) xmtlen = 0;          // STP - start of buffer

    if (xmtlen < 0) {                       // looking for start of buffer (STP)
        if (! z11page->dmawritelocked (tdrpa + 6, tdrb[3] | BUFL)) goto dmaerror;
        return true;                        // not found, set BUFL then check next descriptor
    }

    if (xmtlen + tdrb[0] > (int) sizeof xmtp) { // see if message too int
        if (! z11page->dmawritelocked (tdrpa + 6, tdrb[3] | BUFL)) goto dmaerror;
        xmtlen = -1;                        // search for STP
        return true;                        // if so, set BUFL then check next descriptor
    }

    // transfer from unibus to xmtp.b
    segb = ((uint32_t) tdrb[2] << 16) | tdrb[1];
    while (tdrb[0] > 0) {
        uint16_t word;
        if (z11page->dmareadlocked (segb & 0777776, &word) != 0) {
            if (! z11page->dmawritelocked (tdrpa + 6, tdrb[3] | UBTO)) goto dmaerror;
            xmtlen = -1;                    // timeout reading memory, abandon this packet
            goto relpacket;
        }
        if (segb & 1) {
            xmtp.b[xmtlen++] = word >> 8;
            segb ++;
            tdrb[0] --;
            continue;
        }
        if (tdrb[0] == 1) {
            xmtp.b[xmtlen++] = word;
            break;
        }
        xmtp.b[xmtlen++] = word;
        xmtp.b[xmtlen++] = word >> 8;
        segb += 2;
        tdrb[0] -= 2;
    }

    // if ENP (end of packet), transmit packet
    if (tdrb[2] & ENP) {

        // transmit xmtp (xmtlen bytes)
        z11page->dmaunlk ();
        transmithread_dmaulk (xmtlen);
        z11page->dmalock ();

        // looking for an STP (start of packet) flag for another packet to send
        xmtlen = -1;
    }

    // mark packet owned by PDP
relpacket:;
    if (z11page->dmawritelocked (tdrpa + 4, tdrb[2] & ~ OWN)) return true;

dmaerror:;
    portst1 |= ERRS | TMOT | TRNG;  // transmit ring error (p96)
    writepcsr0 (SERI);              // port status error
    return true;
}

// send the packet out
// 'themutex' locked on entry and exit
static void transmithread_dmaulk (int xmtlen)
{
    // fill in source mac address
    memcpy (xmtp.b + 6, curethaddr, 6);

    // pad to minimum packet length
    if ((xmtlen < MINPKTLEN) && (modebits & MODE_TPAD)) {
        memset (xmtp.b + xmtlen, 0, MINPKTLEN - xmtlen);
        xmtlen = MINPKTLEN;
    }

    // maybe print out some bytes
    if (debug > 1) {
        fprintf (stderr, "transmithread: xmtlen=%d:", xmtlen);
        for (int i = 0; (i < 20) && (i < xmtlen); i ++) {
            fprintf (stderr, " %02X", xmtp.b[i]);
        }
        fputc ('\n', stderr);
    }

    // if loopback mode, just copy to receive queue
    if (modebits & MODE_LOOP) {
        memcpy (rcvp.b, xmtp.b, xmtlen);
        gotincoming_locked (xmtlen);
    } else {

        // real transmit, releae 'themutex' (so receiver can run) while sending it
        unlkit ();

        int rc = sendto (sockfd, xmtp.b, xmtlen, 0, (sockaddr const *) &sendtoaddr, sizeof sendtoaddr);
        if (rc != xmtlen) {
            if (rc < 0) {
                fprintf (stderr, "z11xe: error transmitting: %m\n");
            } else {
                fprintf (stderr, "z11xe: only sent %d bytes of %d byte packet\n", rc, xmtlen);
            }
            ABORT ();
        }

        lockit ();
    }

    // increment counters
    uint32_t packetsxmtd   = counters.packetsxmtd   + 1;        // p89
    uint32_t databytesxmtd = counters.databytesxmtd + xmtlen - 14;
    if (packetsxmtd   < counters.packetsxmtd)   packetsxmtd   = 0xFFFFFFFFU;
    if (databytesxmtd < counters.databytesxmtd) databytesxmtd = 0xFFFFFFFFU;
    counters.packetsxmtd   = packetsxmtd;
    counters.databytesxmtd = databytesxmtd;
}

//////////////////////
//  RECEIVE THREAD  //
//////////////////////

// process incoming packets, passing them along to the PDP
static void *receivethread (void *dummy)
{
    while (true) {
        int rc = read (sockfd, rcvp.b, sizeof rcvp.b);
        if (rc < 0) {
            fprintf (stderr, "z11xe: error receiving packet: %m\n");
            ABORT ();
        }
        if (rc < 14) {
            fprintf (stderr, "z11xe: runt %d-byte packet received\n", rc);
            continue;
        }

        if (rc < MINPKTLEN) {
            memset (rcvp.b + rc, 0, MINPKTLEN - rc);
            rc = MINPKTLEN;
        }

        lockit ();
        gotincoming_locked (rc);
        unlkit ();
    }
}

// got an incoming packet, copy to receive ring and possibly send interrupt to pdp
// 'themutex' locked on entry and exit
static void gotincoming_locked (int rc)
{
    if (debug > 2) {
        uint8_t *rcvbyt = rcvp.b;
        if (rcvbyt[12] != 0x08) {   // don't dump IPs and ARPs
            fprintf (stderr, "gotincoming: rc=%05d:", rc);
            for (int i = 0; (i < 20) & (i < rc); i ++) fprintf (stderr, " %02X", ((uint8_t *)rcvbyt)[i]);
            fprintf (stderr, "\n");
        }
    }

    if ((pcsr1 & RUN) && matchesincoming (rcvp.w)) {
        if (ringfmt.rrlen == 0) {
            if (debug > 1) fprintf (stderr, "gotincoming: ring empty\n");
            if (! rcbisetinh) writepcsr0 (RCBI);
            rcbisetinh = true;  // don't set again until told to rescan ring
        } else {
            uint32_t packetsrcvd   = counters.packetsrcvd   + 1;    // p89
            uint32_t databytesrcvd = counters.databytesrcvd + rc - 14;
            if (packetsrcvd   < counters.packetsrcvd)   packetsrcvd   = 0xFFFFFFFFU;
            if (databytesrcvd < counters.databytesrcvd) databytesrcvd = 0xFFFFFFFFU;
            counters.packetsrcvd   = packetsrcvd;
            counters.databytesrcvd = databytesrcvd;

            z11page->dmalock ();
            uint16_t status = gotincoming_dmalkd (rc);
            z11page->dmaunlk ();

            // set RXI (receive ring interrupt) or RCBI (receive buffer unavailable) plus INTR
            // p63
            writepcsr0 (status);
        }
    }
}

// copy received packet to receive ring
// 'themutex' and 'dmamutex' locked on entry and exit
static uint16_t gotincoming_dmalkd (uint16_t rcvlen)
{
    uint16_t numbytestogo = rcvlen;
    uint16_t index  = 0;
    uint16_t status = 0;
    do {

        // hopefully descriptor entry pointed to by rdrca is owned by DEUNA
        // p113/v4-57
        uint16_t rdrb[3];
        for (int i = 0; i < 3; i ++) {
            if (z11page->dmareadlocked ((rdrca + i * 2) & 0777776, &rdrb[i]) != 0) goto dmaerror;
        }
        if (! (rdrb[2] & OWN)) {
            if (debug > 1) fprintf (stderr, "gotincoming: ring overflow\n");
            if (! rcbisetinh) status |= RCBI;       // set RCBI - receive buffer unabailable interrupt
            rcbisetinh = true;                      // don't set again until told to rescan ring
            break;                                  // abandon incoming packet
        }
        if (debug > 1) fprintf (stderr, "gotincoming: packet accepted rdrca=%u index=%u\n", rdrca, index);

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
            if (! z11page->dmawritelocked (segb, rcvp.w[index])) {
                word6 |= UBTO;
                goto badbuf;
            }
            segb += 2;
            index ++;
        }
        if (amountfits & 1) {
            ASSERT (amountfits == numbytestogo);
            if (! z11page->dmawbytelocked (segb, rcvp.w[index])) {
                word6 |= UBTO;
            }
        }
    badbuf:;

        // see if that was last of message
        uint16_t nextowner;
        if (amountfits == numbytestogo) {
            byte5 |= ENP >> 8;                      // ENP - end of packet
            word6 |= rcvlen & 0x0FFFU;              // MLEN - total message length in bytes
        }

        // if it didn't all fit, peek at owner of next descriptor in ring
        // if no further entry, assume it is owned by the PDP
        else if ((modebits & MODE_DRDC) ||
                 (z11page->dmareadlocked ((rdrna + 4) & 0777776, &nextowner) != 0) ||
                 ! (nextowner & OWN)) {
            word6 |= BUFL;                          // BUFL - buffer length error
            numbytestogo = amountfits;              // chop!
        }

        // write status out to memory, turning ownership over to PDP
        if (! z11page->dmawritelocked (rdrca + 6, word6)) goto dmaerror;
        if (! z11page->dmawbytelocked (rdrca + 5, byte5)) goto dmaerror;

        // set RXI - receive ring interrupt
        status |= RXI;

        // advance on to next ring entry
        rdrca = rdrna;

        // maybe there is more of this packet to process
        numbytestogo -= amountfits;
    } while (numbytestogo > 0);
    return status;

dmaerror:;
    portst1 |= ERRS | TMOT | RRNG;  // receive ring error (p96)
    return SERI;                    // port status error
}

// see if incoming packet matches receive filters
static bool maceq (uint16_t const *a, uint16_t const *b)
{
    return (a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]);
}
static bool matchesincoming (uint16_t const *rcvwrd)
{
    if (modebits & MODE_PROM) return true;
    if (! (rcvwrd[0] & 1)) return maceq (rcvwrd, curethaddr);
    if (modebits & MODE_ENAL) return true;
    if (maceq (rcvwrd, bdcastaddr)) return true;
    for (uint8_t i = 0; i < curmlt; i ++) {
        if (maceq (rcvwrd, &mltaddrtable[i*3])) return true;
    }
    return false;
}

/////////////////
//  UTILITIES  //
/////////////////

static void writepcsr0 (uint16_t pcsr0)
{
    ZWR(xeat[1], ((uint32_t) pcsr1 << 16) | pcsr0);
}

static void lockit ()
{
    if (pthread_mutex_lock (&themutex) != 0) ABORT ();
}

static void unlkit ()
{
    if (pthread_mutex_unlock (&themutex) != 0) ABORT ();
}

static void waketransmit ()
{
    if (pthread_cond_broadcast (&thecond) != 0) ABORT ();
}

static void waittransmit ()
{
    if (pthread_cond_wait (&thecond, &themutex) != 0) ABORT ();
}
