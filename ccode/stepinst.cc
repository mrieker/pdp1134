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

// single-step the processor (real or sim)
// customize as needed for conditions and prints

//  make stepinst.armv7l
//  ./stepinst.armv7l

#include <signal.h>
#include <stdio.h>

#include "z11defs.h"
#include "z11util.h"

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    z11page = new Z11Page ();
    uint32_t volatile *kyat = z11page->findev ("KY", NULL, NULL, false);

    z11page->haltreq ();
    for (int i = 0; ! (ZRD(kyat[2]) & KY2_HALTED); i ++) {
        if (i > 10000000) {
            fprintf (stderr, "failed to initially halt\n");
            ABORT ();
        }
    }

    while (true) {
        uint16_t pc, ps, mmr0, mmr2;
        if (z11page->dmaread (0777707, &pc) != 0) pc = 0177777;
        if ((pc >= 031570) && (pc <= 032112)) {
            if (z11page->dmaread (0777776, &ps)   != 0) ps   = 0177777;
            if (z11page->dmaread (0777572, &mmr0) != 0) mmr0 = 0177777;
            if (z11page->dmaread (0777576, &mmr2) != 0) mmr2 = 0177777;
            printf ("PC=%06o PS=%06o MMR0=%06o MMR2=%06o\n", pc, ps, mmr0, mmr2);
        }
        z11page->stepreq ();
        for (int i = 0; ! (ZRD(kyat[2]) & KY2_HALTED); i ++) {
            if (i > 10000000) {
                fprintf (stderr, "failed to halt after step\n");
                ABORT ();
            }
        }
    }

    return 0;
}
