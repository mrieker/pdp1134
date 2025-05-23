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

// Arm then dump zynq.v ilaarray when triggered

//  ./z11ila.armv7l

#include <alloca.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "z11defs.h"
#include "z11util.h"

#define AFTER 96  // number of samples to take after sample containing trigger

static bool volatile ctrlcflag;

static void siginthand (int signum);

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    bool asisflag = false;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("  arm then dump zynq.v ilaarray when triggered");
            puts ("");
            puts ("    ./z11ila [-asis]");
            puts ("");
            puts ("      -asis = don't arm and wait, just dump as is");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-asis") == 0) {
            asisflag = true;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    Z11Page z11page;
    uint32_t volatile *pdpat = z11page.findev ("11", NULL, NULL, false);

    uint32_t ctl;
    if (asisflag) {
        ctl = ZRD(pdpat[ILACTL]);
    } else {

        // tell zynq.v to start collecting samples
        // tell it to stop when collected trigger sample plus AFTER thereafter
        ZWR(pdpat[ILACTL], ILACTL_ARMED | AFTER * ILACTL_AFTER0);
        printf ("armed\n");

        if (signal (SIGINT, siginthand) != SIG_DFL) ABORT ();

        // wait for sampling to stop
        while (true) {
            ctl = ZRD(pdpat[ILACTL]);
            if ((ctl & (ILACTL_ARMED | ILACTL_AFTER)) == 0) break;
            if (ctrlcflag) break;
            usleep (10000);
        }
    }

    // stop collection if not already
    ZWR(pdpat[ILACTL], 0);

    // get limits of entries to print
    uint32_t earliestentry = (ctl & ILACTL_OFLOW) ? (ctl & ILACTL_INDEX) / ILACTL_INDEX0 : 0;
    uint32_t numfilledentries = (ctl & ILACTL_OFLOW) ? ILACTL_DEPTH : (ctl & ILACTL_INDEX) / ILACTL_INDEX0;
    uint32_t numaftertrigger = AFTER - (ctl & ILACTL_AFTER) / ILACTL_AFTER0;
    printf ("ctl=%08X  earliestentry=%u  numfilledentries=%u  numaftertrigger=%u\n", ctl, earliestentry, numfilledentries, numaftertrigger);

    bool dmxhltrq = false;
    bool dmxnpr   = false;
    uint32_t dmxa = 0;
    uint32_t dmxc = 0;
    uint32_t dmxd = 0;
    uint32_t lastrsel = 0;
    uint32_t lastmuxn = 0;

    // loop through entries in the array
    for (uint32_t i = 0; i < numfilledentries; i ++) {

        // read next entry from array
        uint32_t index = (earliestentry + i) & (ILACTL_DEPTH - 1);
        ZWR(pdpat[ILACTL], index * ILACTL_INDEX0);
        uint64_t thisentry = ((uint64_t) ZRD(pdpat[ILADAT+1]) << 32) | ZRD(pdpat[ILADAT+0]);
        uint32_t thisentex = ZRD(pdpat[ILATIM]);

        uint32_t thisrsel = (thisentry >> 47) & 3;
        uint32_t thismuxn = (thisentry >> 49) & 077777;
        if (thisrsel != lastrsel) switch (lastrsel) {
            case 1: {
                dmxd &= ~ 0177662;
                dmxhltrq = false;
                if (lastmuxn & 020000) dmxd |= 0004000; // b
                if (lastmuxn & 010000) dmxhltrq = true; // c
                if (lastmuxn & 002000) dmxd |= 0100000; // e
                if (lastmuxn & 001000) dmxd |= 0040000; // f
                if (lastmuxn & 000400) dmxd |= 0020000; // h
                if (lastmuxn & 000200) dmxd |= 0010000; // j
                if (lastmuxn & 000100) dmxd |= 0002000; // k
                if (lastmuxn & 000040) dmxd |= 0001000; // l
                if (lastmuxn & 000020) dmxd |= 0000400; // m
                if (lastmuxn & 000010) dmxd |= 0000200; // n
                if (lastmuxn & 000004) dmxd |= 0000020; // p
                if (lastmuxn & 000002) dmxd |= 0000040; // r
                if (lastmuxn & 000001) dmxd |= 0000002; // s
                break;
            }
            case 2: {
                dmxa &= ~ 0710004;
                dmxc &= ~ 2;
                dmxd &= ~ 0000115;
                if (lastmuxn & 040000) dmxa |= 0010000; // a
                if (lastmuxn & 020000) dmxa |= 0400000; // b
                if (lastmuxn & 010000) dmxa |= 0000004; // c
                if (lastmuxn & 004000) dmxd |= 0000001; // d
                if (lastmuxn & 002000) dmxd |= 0000010; // e
                if (lastmuxn & 001000) dmxd |= 0000004; // f
                if (lastmuxn & 000400) dmxd |= 0000100; // h
                if (lastmuxn & 000010) dmxa |= 0100000; // n
                if (lastmuxn & 000004) dmxa |= 0200000; // p
                if (lastmuxn & 000002) dmxc |= 2;       // r
                break;
            }
            case 3: {
                dmxa  &= ~ 0067773;
                dmxc  &= ~ 1;
                dmxnpr = false;
                if (lastmuxn & 040000) dmxa |= 0000002; // a
                if (lastmuxn & 020000) dmxa |= 0040000; // b
                if (lastmuxn & 010000) dmxa |= 0004000; // c
                if (lastmuxn & 004000) dmxa |= 0002000; // d
                if (lastmuxn & 002000) dmxa |= 0001000; // e
                if (lastmuxn & 001000) dmxa |= 0000100; // f
                if (lastmuxn & 000400) dmxa |= 0000040; // h
                if (lastmuxn & 000200) dmxnpr = true;   // j
                if (lastmuxn & 000100) dmxa |= 0000001; // k
                if (lastmuxn & 000040) dmxc |= 1;       // l
                if (lastmuxn & 000020) dmxa |= 0020000; // m
                if (lastmuxn & 000010) dmxa |= 0000400; // n
                if (lastmuxn & 000004) dmxa |= 0000200; // p
                if (lastmuxn & 000002) dmxa |= 0000020; // r
                if (lastmuxn & 000001) dmxa |= 0000010; // s
                break;
            }
        }

        printf ("[%5u]  %06o %o %o %06o  %05o %o  %o %o %o %o %o  %o %o %o %o %o %o  %06o %o %06o  %06o %o %06o %o %o\n",
            i,                                      // 10nS per tick

            (unsigned) (thisentex >> 16) & 0177774, // dmx_a_in_h[15:02]
            (unsigned) (thisentex >> 17) & 1,       // del_msyn_in_h
            (unsigned) (thisentex >> 16) & 1,       // del_ssyn_in_h
            (unsigned) (thisentex >>  0) & 0177777, // dma_d_in_h

            (unsigned) thismuxn,                    // muxn
            (unsigned) thisrsel,                    // rsel

            (unsigned) (thisentry >> 46) & 1,       // bbsy_in_h,
            (unsigned) (thisentry >> 45) & 1,       // msyn_in_h,
            (unsigned) (thisentry >> 44) & 1,       // npg_in_l,
            (unsigned) (thisentry >> 43) & 1,       // sack_in_h,
            (unsigned) (thisentry >> 42) & 1,       // ssyn_in_h

            (unsigned) (thisentry >> 41) & 1,       // bbsy_out_h,
            (unsigned) (thisentry >> 40) & 1,       // msyn_out_h,
            (unsigned) (thisentry >> 39) & 1,       // npg_out_l,
            (unsigned) (thisentry >> 38) & 1,       // npr_out_h,
            (unsigned) (thisentry >> 37) & 1,       // sack_out_h,
            (unsigned) (thisentry >> 36) & 1,       // ssyn_out_h
            (unsigned) (thisentry >> 18) & 0777777, // a_out_h
            (unsigned) (thisentry >> 16) & 3,       // c_out_h,
            (unsigned) (thisentry >>  0) & 0177777, // d_out_h,

            (unsigned) dmxa,
            (unsigned) dmxc,
            (unsigned) dmxd,
            (unsigned) dmxhltrq,
            (unsigned) dmxnpr
        );

        lastmuxn = thismuxn;
        lastrsel = thisrsel;
    }
    return 0;
}

static void siginthand (int signum)
{
    if (write (STDOUT_FILENO, "\n", 1) < 0) { }
    ctrlcflag = true;
}
