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

#define AFTER 8000  // number of samples to take after sample containing trigger

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

    // loop through entries in the array
    for (uint32_t i = 0; i < numfilledentries; i ++) {

        // read next entry from array
        uint32_t index = (earliestentry + i) & (ILACTL_DEPTH - 1);
        ZWR(pdpat[ILACTL], index * ILACTL_INDEX0);
        uint64_t thisentry = ((uint64_t) ZRD(pdpat[ILADAT+1]) << 32) | ZRD(pdpat[ILADAT+0]);

        printf ("[%5u]  %06o %o %02o %02o %o %06o  %o %o %o  %o %o %o  %o %o %o\n",
            i,                                      // 10nS per tick

            (unsigned) (thisentry >> 36) & 0777777, // dev_a_h
            (unsigned) (thisentry >> 35) & 1,       // dev_bbsy_h
            (unsigned) (thisentry >> 31) & 15,      // dev_bg_l
            (unsigned) (thisentry >> 27) & 15,      // dev_br_h
            (unsigned) (thisentry >> 25) & 3,       // dev_c_h
            (unsigned) (thisentry >>  9) & 0177777, // dev_d_h

            (unsigned) (thisentry >>  8) & 1,       // dev_init_h
            (unsigned) (thisentry >>  7) & 1,       // dev_intr_h
            (unsigned) (thisentry >>  6) & 1,       // dev_del_msyn_h

            (unsigned) (thisentry >>  5) & 1,       // dev_syn_msyn_h
            (unsigned) (thisentry >>  4) & 1,       // dev_npg_l
            (unsigned) (thisentry >>  3) & 1,       // dev_npr_h

            (unsigned) (thisentry >>  2) & 1,       // dev_sack_h
            (unsigned) (thisentry >>  1) & 1,       // dev_del_ssyn_h
            (unsigned) (thisentry >>  0) & 1        // dev_syn_ssyn_h
        );
    }
    return 0;
}

static void siginthand (int signum)
{
    if (write (STDOUT_FILENO, "\n", 1) < 0) { }
    ctrlcflag = true;
}
