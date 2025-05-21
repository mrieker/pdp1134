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
        ctl = pdpat[ILACTL];
    } else {

        // tell zynq.v to start collecting samples
        // tell it to stop when collected trigger sample plus AFTER thereafter
        pdpat[ILACTL] = CTL_ARMED | AFTER * CTL_AFTER0;
        printf ("armed\n");

        if (signal (SIGINT, siginthand) != SIG_DFL) ABORT ();

        // wait for sampling to stop
        while (true) {
            ctl = pdpat[ILACTL];
            if ((ctl & (CTL_ARMED | CTL_AFTER)) == 0) break;
            if (ctrlcflag) break;
            usleep (10000);
        }
    }
    pdpat[ILACTL] = 0;

    // get limits of entries to print
    uint32_t earliestentry = (ctl & CTL_INDEX) / CTL_INDEX0;
    uint32_t numfilledentries = CTL_DEPTH;
    if (! (ctl & CTL_OFLOW)) {
        earliestentry = 0;
        numfilledentries = (ctl & CTL_INDEX) / CTL_INDEX0;
    }
    uint32_t numaftertrigger = AFTER - (ctl & CTL_AFTER) / CTL_AFTER0;
    printf ("ctl=%08X  earliestentry=%u  numfilledentries=%u  numaftertrigger=%u\n", ctl, earliestentry, numfilledentries, numaftertrigger);

    // loop through entries in the array
    uint32_t basetime = 0;
    for (uint32_t i = 0; i < numfilledentries; i ++) {

        // read next entry from array
        uint32_t index = (earliestentry + i) & (CTL_DEPTH - 1);
        pdpat[ILACTL] = index * CTL_INDEX0;
        if (i == 0) basetime = pdpat[ILATIM];
        uint32_t deltatime  = pdpat[ILATIM] - basetime;
        uint64_t thisentry  = ((uint64_t) pdpat[ILADAT+1] << 32) | pdpat[ILADAT+0];

        printf ("[%04o] %2u.%08u0  %03o %06o %o %o %02o %02o %o %06o %o %o %o %o %o %o %o %o %o %o\n",
            index, deltatime / 100000000, deltatime % 100000000,   // 10nS per tick

            (unsigned) (thisentry >> 56) & 0377,    // muxcount
            (unsigned) (thisentry >> 38) & 0777777, // dev_a_in_h
            (unsigned) (thisentry >> 37) & 1,       // dev_ac_lo_in_h,
            (unsigned) (thisentry >> 36) & 1,       // dev_bbsy_in_h,
            (unsigned) (thisentry >> 32) & 017,     // dev_bg_in_l,
            (unsigned) (thisentry >> 28) & 017,     // dmx_br_in_h,
            (unsigned) (thisentry >> 26) & 3,       // dev_c_in_h,
            (unsigned) (thisentry >> 10) & 0177777, // dev_d_in_h,
            (unsigned) (thisentry >>  9) & 1,       // dev_dc_lo_in_h,
            (unsigned) (thisentry >>  8) & 1,       // dev_hltgr_in_l,
            (unsigned) (thisentry >>  7) & 1,       // dev_hltrq_in_h,
            (unsigned) (thisentry >>  6) & 1,       // dev_init_in_h,
            (unsigned) (thisentry >>  5) & 1,       // dev_intr_in_h,
            (unsigned) (thisentry >>  4) & 1,       // dev_msyn_in_h,
            (unsigned) (thisentry >>  3) & 1,       // dev_npg_in_l,
            (unsigned) (thisentry >>  2) & 1,       // dmx_npr_in_h,
            (unsigned) (thisentry >>  1) & 1,       // dev_sack_in_h,
            (unsigned) (thisentry >>  0) & 1        // dev_ssyn_in_h
        );
    }
    return 0;
}

static void siginthand (int signum)
{
    if (write (STDOUT_FILENO, "\n", 1) < 0) { }
    ctrlcflag = true;
}
