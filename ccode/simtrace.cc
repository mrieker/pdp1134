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

// trace sim memory cycles

#include <signal.h>
#include <stdio.h>

#include "z11defs.h"
#include "z11util.h"

static bool volatile ctrlcflag;

static void sighandler (int signum);

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    signal (SIGINT, sighandler);

    z11page = new Z11Page ();
    uint32_t volatile *pdpat = z11page->findev ("11", NULL, NULL, false);

    pdpat[Z_RL] |= l_stepenable;

    while (! ctrlcflag) {
        while (! (pdpat[Z_RL] & l_stephalted)) { if (ctrlcflag) goto done; }
        uint32_t addr = (pdpat[Z_RD] & d_dev_a_h) / (d_dev_a_h & - d_dev_a_h);
        if ((addr & 0777770) == 0774510) {
            uint32_t simst = (pdpat[Z_RK] & k_simst)   / (k_simst   & - k_simst);
            uint32_t ctrl  = (pdpat[Z_RE] & e_dev_c_h) / (e_dev_c_h & - e_dev_c_h);
            uint32_t data  = (pdpat[Z_RG] & g_dev_d_h) / (g_dev_d_h & - g_dev_d_h);
            if (ctrl != 3) {
                printf (" %02u  %06o  %o  %06o\n", simst, addr, ctrl, data);
            } else {
                if (addr & 1) printf (" %02u  %06o  %o  %04o  \n",  simst, addr, ctrl, (data >> 6) & 01774);
                         else printf (" %02u  %06o  %o     %03o\n", simst, addr, ctrl, data & 0377);
            }
        }
        pdpat[Z_RL] |= l_stepsingle;
        while (pdpat[Z_RL] & l_stepsingle) { if (ctrlcflag) goto done; }
    }

done:;
    pdpat[Z_RL] &= ~ l_stepenable;

    return 0;
}

static void sighandler (int signum)
{
    ctrlcflag = true;
}
