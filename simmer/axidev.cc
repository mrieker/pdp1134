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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "axidev.h"

AxiDev *AxiDev::allaxidevs;
AxiDev *AxiDev::axidevtable[1024];
uint32_t AxiDev::axidevmasks[1024];

//////////////////////
//  static methods  //
//////////////////////

void AxiDev::axiassign ()
{
    memset (axidevtable, 0, sizeof axidevtable);
    uint32_t nextavail = 0;
    for (uint32_t thissize = 16; thissize > 0;) {
        -- thissize;
        for (AxiDev *ad = allaxidevs; ad != NULL; ad = ad->nextaxidev) {
            uint32_t size = (ad->axirdslv (0) >> 12) & 15;
            if (size == thissize) {
                for (uint32_t i = 0; i < 2U << size; i ++) {
                    if (nextavail > 1023) abort ();
                    axidevtable[nextavail] = ad;
                    axidevmasks[nextavail] = (2U << size) - 1;
                    nextavail ++;
                }
            }
        }
    }
}

uint32_t AxiDev::axirdmas (uint32_t index)
{
    AxiDev *ad = axidevtable[index];
    return (ad == NULL) ? 0xDEADBEEF : ad->axirdslv (index & axidevmasks[index]);
}

void AxiDev::axiwrmas (uint32_t index, uint32_t data)
{
    AxiDev *ad = axidevtable[index];
    if (ad != NULL) ad->axiwrslv (index & axidevmasks[index], data);
}

/////////////////////////
//  protected methods  //
/////////////////////////

AxiDev::AxiDev ()
{
    this->nextaxidev = allaxidevs;
    allaxidevs = this;
}
