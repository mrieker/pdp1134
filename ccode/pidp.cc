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

// Run on a RasPI plugged into PiDP-11 panel
//   ./pidp client
// Uses UDP to communicate with ZTurn to control/monitor the PDP
// Must be running the pidp server on the ZTurn:
//   ./pidp server

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "z11defs.h"
#include "z11util.h"

// gpio pins

                    // LED0 LED1 LED2 LED3 LED4 LED5 ROW0 ROW1 ROW2
#define COL00   26  //  A00  A12 AD22  D00  D12      SR00 SR12 TEST
#define COL01   27  //  A01  A13 AD18  D01  D13      SR01 SR13 LDAD
#define COL02    4  //  A02  A14 AD16  D02  D14      SR02 SR14 EXAM
#define COL03    5  //  A03  A15 DATA  D03  D15      SR03 SR15 DEP
#define COL04    6  //  A04  A16 KERN  D04 PARL      SR04 SR16 CONT
#define COL05    7  //  A05  A17 SUPR  D05 PARH      SR05 SR17 HALT
#define COL06    8  //  A06  A18 USER  D06 R1UD R1UI SR06 SR18 SINS
#define COL07    9  //  A07  A19 MAST  D07 R1SD R1SI SR07 SR19 STRT
#define COL08   10  //  A08  A20 PAUS  D08 R1KD R1KI SR08 SR20
#define COL09   11  //  A09  A21 RUN   D09 R1CP R1PP SR09 SR21
#define COL10   12  //  A10      AERR  D10 R2DP MUFP SR10
#define COL11   13  //  A11      PERR  D11 R2BR R2DR SR11

#define LED0 20
#define LED1 21
#define LED2 22
#define LED3 23
#define LED4 24
#define LED5 25

#define ROW0 16
#define ROW1 17
#define ROW2 18

#define L0_A1100 0007777
#define L1_A2112 0001777
#define L2_RUN   0001000
#define L2_AERR  0002000
#define L2_PERR  0004000
#define L3_D1100 0007777
#define L4_D1512 0000017

#define R0_SR1100 0007777
#define R1_SR2112 0001777
#define R2_TEST_  0000001
#define R2_LDAD   0000002
#define R2_EXAM   0000004
#define R2_DEP    0000010
#define R2_CONT   0000020
#define R2_HALT   0000040
#define R2_STRT   0000200

#define BB(m) (m & - m)

#define IGO 1

#define GPIO_FSEL0 0
#define GPIO_SET0 (0x1C/4) //  7
#define GPIO_CLR0 (0x28/4) // 10
#define GPIO_LEV0 (0x34/4) // 13

#define GPIO_PUD     (0x94/4) // 37
#define GPIO_PUDCLK0 (0x98/4) // 38

#define GPIO_PUD_OFF  0
#define GPIO_PUD_DOWN 1
#define GPIO_PUD_UP   2

#define PIDPUDPPORT 5826

struct PiDPUDPPkt {
    uint64_t seq;
    uint16_t leds[6];
    uint16_t rows[3];
};

static uint32_t const colbits[] = {
    1U << COL00, 1U << COL01, 1U << COL02, 1U << COL03, 1U << COL04, 1U << COL05,
    1U << COL06, 1U << COL07, 1U << COL08, 1U << COL09, 1U << COL10, 1U << COL11 };

static uint32_t const colbitsall =
    (1U << COL00) | (1U << COL01) | (1U << COL02) | (1U << COL03) | (1U << COL04) | (1U << COL05) |
    (1U << COL06) | (1U << COL07) | (1U << COL08) | (1U << COL09) | (1U << COL10) | (1U << COL11);

static uint32_t const ledbits[] = { 1U << LED0, 1U << LED1, 1U << LED2, 1U << LED3, 1U << LED4, 1U << LED5 };
static uint32_t const rowbits[] = { 1U << ROW0, 1U << ROW1, 1U << ROW2 };

static void client (int argc, char **argv);
static void writeleds (uint32_t volatile *gpiopage, uint16_t *leds);
static void server ();

static int openclientsocket (struct sockaddr_in *cliaddr);
static int openserversocket (struct sockaddr_in *svraddr);
static void getserveripaddr (struct sockaddr_in *cliaddr, char const *server);

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    if ((argc >= 2) && (strcasecmp (argv[1], "client") == 0)) client (argc - 1, argv + 1);
    if ((argc == 2) && (strcasecmp (argv[1], "server") == 0)) server ();
    return 0;
}

// run on raspi plugged into pidp-11 panel
//  pidp client [ -test | <serveraddr> ]
static void client (int argc, char **argv)
{
    bool test = false;
    char const *server = NULL;
    for (int i = 0; ++ i < argc;) {
        if (strcasecmp (argv[i], "-test") == 0) {
            test = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            ABORT ();
        }
        if (server != NULL) {
            fprintf (stderr, "unknown argument %s\n", argv[i]);
            ABORT ();
        }
        server = argv[i];
    }
    if (! test && (server == NULL)) {
        fprintf (stderr, "missing server address\n");
        ABORT ();
    }

    // access gpio page to access leds and switches
    int gpiofd = open ("/dev/gpiomem", O_RDWR);
    if (gpiofd < 0) {
        fprintf (stderr, "error opening /dev/gpiomem: %m\n");
        ABORT ();
    }
    void *gpioptr = mmap (NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, gpiofd, 0);
    if (gpioptr == MAP_FAILED) {
        fprintf (stderr, "error mmapping /dev/gpiomem: %m\n");
        ABORT ();
    }
    uint32_t volatile *gpiopage = (uint32_t volatile *) gpioptr;

    // configure:
    //  columns with pullup
    //  rows,leds,columns all set to inputs for now
    gpiopage[GPIO_FSEL0+0] = 0;
    gpiopage[GPIO_FSEL0+1] = 0;
    gpiopage[GPIO_FSEL0+2] = 0;

    for (int g = 4; g <= 27; g ++) {
        gpiopage[GPIO_PUD] = (colbitsall & (1U << g)) ? GPIO_PUD_UP : GPIO_PUD_OFF;
        usleep (20);
        gpiopage[GPIO_PUDCLK0] = 1U << g;
        usleep (20);
        gpiopage[GPIO_PUD] = 0;
        gpiopage[GPIO_PUDCLK0] = 0;
    }

    // set up sequence number gt anything previously used
    PiDPUDPPkt pidpmsg;
    memset (&pidpmsg, 0, sizeof pidpmsg);
    struct timespec nowts;
    if (clock_gettime (CLOCK_REALTIME, &nowts) < 0) ABORT ();
    uint64_t sendseq = nowts.tv_sec * 1000000000ULL + nowts.tv_nsec;

    // -test : test LED access
    while (test) {
        for (int i = 0; i < 64; i ++) {
            pidpmsg.leds[0] = ((i >=  0) && (i < 12)) ? (1U <<  i)       : 0;
            pidpmsg.leds[1] = ((i >= 12) && (i < 22)) ? (1U << (i - 12)) : 0;
            pidpmsg.leds[2] = ((i >= 22) && (i < 34)) ? (1U << (i - 22)) : 0;
            pidpmsg.leds[3] = ((i >= 34) && (i < 46)) ? (1U << (i - 34)) : 0;
            pidpmsg.leds[4] = ((i >= 46) && (i < 58)) ? (1U << (i - 46)) : 0;
            pidpmsg.leds[5] = ((i >= 58) && (i < 64)) ? (1U << (i - 52)) : 0;
            writeleds (gpiopage, pidpmsg.leds);
        }
    }

    // open socket to communicate to server with
    struct sockaddr_in cliaddr;
    int udpfd = openclientsocket (&cliaddr);

    // set up to send to GPIOUDPPORT on server
    struct sockaddr_in svraddr;
    memset (&svraddr, 0, sizeof svraddr);
    svraddr.sin_family = AF_INET;
    svraddr.sin_port = htons (PIDPUDPPORT);
    getserveripaddr (&svraddr, server);

    while (true) {

        // write leds - leds,cols = outputs; rows = inputs (hi-Z)
        writeleds (gpiopage, pidpmsg.leds);

        // read switches -
        // - led rows = inputs (hi-Z)
        // - active switch row: output low
        //              others: input (hi-Z)
        // - cols = inputs with pullups
        gpiopage[GPIO_FSEL0+0] = 0;
        gpiopage[GPIO_FSEL0+1] = 0;
        gpiopage[GPIO_FSEL0+2] = 0;
        gpiopage[GPIO_CLR0] = -1;

        for (int r = 0; r <= 2; r ++) {
            ASSERT ((ROW0 >= 10) && (ROW2 <= 19) && (ROW1 == ROW0 + 1) && (ROW2 == ROW0 + 2));
            gpiopage[GPIO_FSEL0+1] = IGO << ((r + ROW0 - 10) * 3);
            usleep (1);
            uint32_t gpin = ~ gpiopage[GPIO_LEV0];  // active low inputs
            uint16_t row  = 0;
            for (int c = 0; c <= 11; c ++) {
                if (gpin & colbits[c]) row |= 1U << c;
            }
            pidpmsg.rows[r] = row;
        }

        // send switches to server, trigger receiving leds
        pidpmsg.seq = ++ sendseq;
        int rc = sendto (udpfd, &pidpmsg, sizeof pidpmsg, 0, (struct sockaddr *) &svraddr, sizeof svraddr);
        if (rc != (int) sizeof pidpmsg) {
            if (rc < 0) {
                fprintf (stderr, "error sending udp packet: %m\n");
            } else {
                fprintf (stderr, "only sent %d of %d bytes over udp\n", rc, (int) sizeof pidpmsg);
            }
            ABORT ();
        }

        // receive leds from server
        do {
            struct sockaddr_in cliaddr;
            socklen_t addrlen = sizeof cliaddr;
            int rc = recvfrom (udpfd, &pidpmsg, sizeof pidpmsg, 0, (struct sockaddr *) &cliaddr, &addrlen);
            if (rc != (int) sizeof pidpmsg) {
                if (rc < 0) {
                    if (errno == EAGAIN) break;
                    fprintf (stderr, "error receiving udp packet: %m\n");
                } else {
                    fprintf (stderr, "only received %d of %d bytes over udp\n", rc, (int) sizeof pidpmsg);
                }
                ABORT ();
            }
        } while (pidpmsg.seq < sendseq);
    }
}

static void writeleds (uint32_t volatile *gpiopage, uint16_t *leds)
{
    gpiopage[GPIO_FSEL0+0] = IGO * 01111110000; // COL07..COL02
    gpiopage[GPIO_FSEL0+1] = IGO * 00000001111; // COL11..COL08
    gpiopage[GPIO_FSEL0+2] = IGO * 00011111111; // COL01..COL00,LED5..LED0

    for (int r = 0; r <= 5; r ++) {

        // compute row of leds
        // line for selected led row is high, other led rows low
        // column bit is active low
        uint32_t gpout = ledbits[r];            // set LEDn bit to activate the led row
        uint16_t ledn = ~ leds[r];              // led bits for the row
        for (int c = 0; c <= 11; c ++) {
            if (ledn & 1) gpout |= colbits[c];  // active low column bit
            ledn >>= 1;
        }
        gpiopage[GPIO_CLR0] = ~ gpout;
        gpiopage[GPIO_SET0] =   gpout;

        // leave led row on for 3.333333mS
        int const ledonns = 3333333;
        struct timespec ts;
        if (clock_gettime (CLOCK_MONOTONIC, &ts) < 0) ABORT ();
        ts.tv_nsec = ledonns - ts.tv_nsec % ledonns;
        ts.tv_sec  = 0;
        if (nanosleep (&ts, NULL) < 0) ABORT ();
    }
}

static void server ()
{
    struct sockaddr_in svraddr;
    int udpfd = openserversocket (&svraddr);

    // access fpga state page
    z11page = new Z11Page ();
    uint32_t volatile *pdpat = z11page->findev ("11", NULL, NULL, false);
    uint32_t volatile *kyat  = z11page->findev ("KY", NULL, NULL, false);

    bool     adderror   = false;
    bool     lastdep    = false;
    bool     lastexam   = false;
    bool     parerror   = false;
    int      testled    = 0;
    int      oldrown    = 0;
    uint16_t memorydata = 0;
    uint32_t loadedaddr = 0;
    uint64_t recvseq    = 0;

    uint16_t lastrows[3], oldrows[3][4];
    memset (lastrows, 0, sizeof lastrows);
    memset (oldrows,  0, sizeof oldrows);

    while (true) {

        // receive switches from client
        PiDPUDPPkt pidpmsg;
        struct sockaddr_in cliaddr;
        do {
            socklen_t addrlen = sizeof cliaddr;
            int rc = recvfrom (udpfd, &pidpmsg, sizeof pidpmsg, 0, (struct sockaddr *) &cliaddr, &addrlen);
            if (rc != (int) sizeof pidpmsg) {
                if (rc < 0) {
                    fprintf (stderr, "error receiving udp packet: %m\n");
                } else {
                    fprintf (stderr, "only received %d of %d bytes over udp\n", rc, (int) sizeof pidpmsg);
                }
                ABORT ();
            }
        } while (pidpmsg.seq <= recvseq);
        recvseq = pidpmsg.seq;

        // SR<21> has to be set to override SR<17:00>
        uint32_t sr = (((pidpmsg.rows[1] & R1_SR2112) / BB(R1_SR2112)) << 12) |
                       ((pidpmsg.rows[0] & R0_SR1100) / BB(R0_SR1100));
        if (sr & 010000000) {
            z11page->dmalock ();    // make sure snapregs() isn't messing with KY regs
            ZWR(kyat[1], (ZRD(kyat[1]) & ~ KY_SWITCHES) | (sr * BB(KY_SWITCHES) & KY_SWITCHES));
            ZWR(kyat[2], (ZRD(kyat[2]) & ~ KY2_SR1716) | ((sr >> 16) * BB(KY2_SR1716) & KY2_SR1716));
            z11page->dmaunlk ();
        }

        // if HALT swith set, set the KY_HALTREQ line
        if (pidpmsg.rows[2] & R2_HALT) {
            z11page->dmalock ();    // make sure snapregs() isn't messing with KY regs
            ZWR(kyat[2], ZRD(kyat[2]) | KY2_HALTREQ);
            z11page->dmaunlk ();
        }

        // debounce momentary switches and detect positive edges
        uint16_t posedges[3];
        for (int i = 0; i < 3; i ++) {
            oldrows[i][oldrown] = pidpmsg.rows[i];
            for (int j = 0; j < 4; j ++) {
                pidpmsg.rows[i] &= oldrows[i][j];
            }
            posedges[i] = ~ lastrows[i] & pidpmsg.rows[i];
            lastrows[i] = pidpmsg.rows[i];
        }
        if (++ oldrown == 4) oldrown = 0;

        // momentaries work only when processor halted
        bool running = ! (ZRD(kyat[2]) & KY2_HALTED);
        if (running) {

            // running, display address bus and data bus
            // also clear any loadaddress/examine/deposit context
            loadedaddr = (ZRD(pdpat[Z_RK]) & k_lataddr) / BB(k_lataddr);
            memorydata = (ZRD(pdpat[Z_RL]) & l_latdata) / BB(l_latdata);
            adderror = false;
            lastdep  = false;
            lastexam = false;
            parerror = false;
        } else {

            // load address, examine, deposit
            if (posedges[2] & R2_LDAD) {
                loadedaddr = sr & 0777777;
                if ((loadedaddr < 0777700) || (loadedaddr > 0777717)) loadedaddr &= -2;
                memorydata = 0;
                adderror = false;
                lastdep  = false;
                lastexam = false;
                parerror = false;
            }
            if (posedges[2] & R2_EXAM) {
                uint32_t addrinc = ((loadedaddr < 0777700) || (loadedaddr > 0777717)) ? 2 : 1;
                if (lastexam) loadedaddr = (loadedaddr + addrinc) & 0777777;
                uint32_t rc = z11page->dmaread (loadedaddr, &memorydata);
                adderror = (rc & KY3_DMATIMO);
                lastdep  = false;
                lastexam = true;
                parerror = (rc & KY3_DMAPERR);
            }
            if (posedges[2] & R2_DEP) {
                memorydata = sr;
                uint32_t addrinc = ((loadedaddr < 0777700) || (loadedaddr > 0777717)) ? 2 : 1;
                if (lastdep) loadedaddr = (loadedaddr + addrinc) & 0777777;
                adderror = ! z11page->dmawrite (loadedaddr, memorydata);
                lastdep  = true;
                lastexam = false;
                parerror = false;
            }

            // posedge CONT : clear KY2_HALTREQ line
            if (posedges[2] & R2_CONT) {
                z11page->dmalock ();
                ZWR(kyat[2], ZRD(kyat[2]) & ~ KY2_HALTREQ);
                z11page->dmaunlk ();
                adderror = false;
                lastdep  = false;
                lastexam = false;
                parerror = false;
            }

            // posedge START : reset processor, load program counter, clear KY2_HALTREQ line
            if (posedges[2] & R2_STRT) {
                z11page->resetit ();
                if (! z11page->dmawrite (0777707, loadedaddr)) {
                    memorydata = loadedaddr;
                    loadedaddr = 0777707;
                    adderror = true;
                } else {
                    z11page->dmalock ();
                    ZWR(kyat[2], ZRD(kyat[2]) & ~ KY2_HALTREQ);
                    z11page->dmaunlk ();
                    adderror = false;
                }
                adderror = false;
                lastdep  = false;
                lastexam = false;
                parerror = false;
            }
        }

        // fill in leds from fpga state
        if (false && ! (lastrows[2] & R2_TEST_)) {
            pidpmsg.leds[0] = ((testled >=  0) && (testled < 12)) ? (1U <<  testled)       : 0;
            pidpmsg.leds[1] = ((testled >= 12) && (testled < 22)) ? (1U << (testled - 12)) : 0;
            pidpmsg.leds[2] = ((testled >= 22) && (testled < 34)) ? (1U << (testled - 22)) : 0;
            pidpmsg.leds[3] = ((testled >= 34) && (testled < 46)) ? (1U << (testled - 34)) : 0;
            pidpmsg.leds[4] = ((testled >= 46) && (testled < 58)) ? (1U << (testled - 46)) : 0;
            pidpmsg.leds[5] = ((testled >= 58) && (testled < 64)) ? (1U << (testled - 52)) : 0;
            if (++testled == 64) testled = 0;
        } else {
            pidpmsg.leds[0] =  (loadedaddr        * BB(L0_A1100)) & L0_A1100;
            pidpmsg.leds[1] = ((loadedaddr >> 12) * BB(L1_A2112)) & L1_A2112;
            pidpmsg.leds[2] =  (running  ? L2_RUN  : 0) |
                               (adderror ? L2_AERR : 0) |
                               (parerror ? L2_PERR : 0);
            pidpmsg.leds[3] =  (memorydata        * BB(L3_D1100)) & L3_D1100;
            pidpmsg.leds[4] = ((memorydata >> 12) * BB(L4_D1512)) & L4_D1512;
            pidpmsg.leds[5] = 0;
        }

        // send leds to client
        int rc = sendto (udpfd, &pidpmsg, sizeof pidpmsg, 0, (struct sockaddr *) &cliaddr, sizeof cliaddr);
        if (rc != (int) sizeof pidpmsg) {
            if (rc < 0) {
                fprintf (stderr, "error sending udp packet: %m\n");
            } else {
                fprintf (stderr, "only sent %d of %d bytes over udp\n", rc, (int) sizeof pidpmsg);
            }
            ABORT ();
        }
    }
}

static int openclientsocket (struct sockaddr_in *cliaddr)
{
    int udpfd = socket (AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) ABORT ();

    // set up ephemeral port for this end
    memset (cliaddr, 0, sizeof *cliaddr);
    cliaddr->sin_family = AF_INET;

    if (bind (udpfd, (struct sockaddr *) cliaddr, sizeof *cliaddr) < 0) {
        fprintf (stderr, "error binding to ephemeral port: %m\n");
        ABORT ();
    }

    struct timeval rcvtmo;
    memset (&rcvtmo, 0, sizeof rcvtmo);
    rcvtmo.tv_usec = 20000;
    if (setsockopt (udpfd, SOL_SOCKET, SO_RCVTIMEO, &rcvtmo, sizeof rcvtmo) < 0) ABORT ();

    return udpfd;
}

static int openserversocket (struct sockaddr_in *svraddr)
{
    int udpfd = socket (AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) ABORT ();

    // set up server port for this end
    memset (svraddr, 0, sizeof *svraddr);
    svraddr->sin_family = AF_INET;
    svraddr->sin_port = htons (PIDPUDPPORT);

    if (bind (udpfd, (struct sockaddr *) svraddr, sizeof *svraddr) < 0) {
        fprintf (stderr, "error binding to ephemeral port: %m\n");
        ABORT ();
    }

    return udpfd;
}

static void getserveripaddr (struct sockaddr_in *cliaddr, char const *server)
{
    if (! inet_aton (server, &cliaddr->sin_addr)) {
        struct hostent *he = gethostbyname (server);
        if (he == NULL) {
            fprintf (stderr, "bad server address %s\n", server);
            ABORT ();
        }
        if ((he->h_addrtype != AF_INET) || (he->h_length != 4)) {
            fprintf (stderr, "server %s not IP v4\n", server);
            ABORT ();
        }
        cliaddr->sin_addr = *(struct in_addr *)he->h_addr;
    }
}
