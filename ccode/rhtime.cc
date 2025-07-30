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

// Test RP04/RP06 timing
// Assumes media loaded in drive 0
// Can do pin set rh_fastio 1 or 0 to test fast & slow timing
// make rhtime.armv7l
// ./rhtime.armv7l

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "z11defs.h"
#include "z11util.h"

static uint64_t getnowns ()
{
    struct timespec nowts;
    if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
    return nowts.tv_sec * 1000000000ULL + nowts.tv_nsec;
}

int main (int argc, char **argv)
{
    setlinebuf (stderr);
    setlinebuf (stdout);

    z11page = new Z11Page ();
    uint32_t volatile *rhat = z11page->findev ("RH", NULL, NULL, false);

    // rpcs2 - select drive 0
    if (! z11page->dmawrite (0776710, 0)) ABORT ();

    // recal - should be 500mS
    printf ("recal:\n");
    for (int i = 0; i < 8; i ++) {
        uint64_t startns = getnowns ();
        if (! z11page->dmawrite (0776700, 007)) ABORT ();
        while (! (rhat[5] & RH5_DRYS0)) { }
        uint64_t stopns = getnowns ();
        printf (" %llu\n", stopns - startns);
    }

    // return to centerline - should be 10mS
    printf ("return to centerline:\n");
    for (int i = 0; i < 8; i ++) {
        uint64_t startns = getnowns ();
        if (! z11page->dmawrite (0776700, 017)) ABORT ();
        while (! (rhat[5] & RH5_DRYS0)) { }
        uint64_t stopns = getnowns ();
        printf (" %llu\n", stopns - startns);
    }

    // various seeks - 7mS 1st cyl, 170uS each cyl after
    for (int j = 0; j < 128;) {
        printf ("seek %d:\n", j);
        for (int i = 0; i < 8; i ++) {
            uint16_t rpcc;
            if (z11page->dmaread (0776736, &rpcc) != 0) ABORT ();
            if (! z11page->dmawrite (0776734, rpcc ^ j)) ABORT ();
            uint64_t startns = getnowns ();
            if (! z11page->dmawrite (0776700, 005)) ABORT ();
            while (! (rhat[5] & RH5_DRYS0)) { }
            uint64_t stopns = getnowns ();
            printf (" %llu\n", stopns - startns);
        }
        j = (j == 0) ? 1 : j * 2;
    }

    return 0;
}
