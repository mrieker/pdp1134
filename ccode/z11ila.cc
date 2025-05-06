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

#define DEPTH 4096  // total number of elements in ilaarray
#define AFTER 4000  // number of samples to take after sample containing trigger
#define DIVID 1     // sample at 10MHz rate

#define ILACTL 021
#define ILADAT 022

#define CTL_ARMED  0x80000000U
#define CTL_AFTER0 0x00010000U
#define CTL_DIVID0 0x00001000U
#define CTL_INDEX0 0x00000001U
#define CTL_AFTER  (CTL_AFTER0 * (DEPTH - 1))
#define CTL_DIVID  (CTL_DIVID0 * (DIVID - 1))
#define CTL_INDEX  (CTL_INDEX0 * (DEPTH - 1))

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
        ctl = pdpat[ILACTL] & CTL_INDEX;
    } else {

        // tell zynq.v to start collecting samples
        // tell it to stop when collected trigger sample plus AFTER thereafter
        pdpat[ILACTL] = CTL_ARMED | CTL_DIVID | AFTER * CTL_AFTER0;
        printf ("armed\n");

        // wait for sampling to stop
        while (((ctl = pdpat[ILACTL]) & (CTL_ARMED | CTL_AFTER)) != 0) sleep (1);
    }

    // read array[index] = next entry to be overwritten = oldest entry
    pdpat[ILACTL] = ctl & CTL_INDEX;
    uint64_t thisentry = (((uint64_t) pdpat[ILADAT+1]) << 32) | (uint64_t) pdpat[ILADAT+0];

    // loop through all entries in the array
    bool nodots = false;
    bool indotdotdot = false;
    uint64_t preventry = 0;
    for (int i = 0; i < DEPTH; i ++) {

        // read array[index+i+1] = array entry after thisentry
        pdpat[ILACTL] = (ctl + i * CTL_INDEX0 + CTL_INDEX0) & CTL_INDEX;
        uint64_t nextentry = (((uint64_t) pdpat[ILADAT+1]) << 32) | (uint64_t) pdpat[ILADAT+0];

        // print thisentry - but use ... if same as prev and next
        if (nodots || (i == 0) || (i == DEPTH - 1) ||
                (thisentry != preventry) || (thisentry != nextentry)) {

            printf ("%7.2f %o %o %06o %06o %06o\n",
                (i - DEPTH + AFTER + 1) * DIVID / 100.0,// trigger shows as 0.00uS

                (unsigned) (thisentry >> 55) & 1,       // enab
                (unsigned) (thisentry >> 53) & 3,       // wena
                (unsigned) (thisentry >> 35) & 0777776, // addr
                (unsigned) (thisentry >> 18) & 0777777, // dout
                (unsigned) (thisentry >>  0) & 0777777  // din
            );
            indotdotdot = false;
        } else if (! indotdotdot) {
            printf ("    ...\n");
            indotdotdot = true;
        }

        // shuffle entries for next time through
        preventry = thisentry;
        thisentry = nextentry;
    }
    return 0;
}
