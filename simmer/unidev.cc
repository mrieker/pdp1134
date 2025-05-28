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

#include "unidev.h"

UniDev *UniDev::allunidevs;
UniDev *UniDev::unidevmemory;
UniDev *UniDev::unidevtable[4096];

//////////////////////
//  static methods  //
//////////////////////

void UniDev::resetmaster ()
{
    for (UniDev *ud = allunidevs; ud != NULL; ud = ud->nextunidev) {
        ud->resetslave ();
    }
}

uint8_t UniDev::getintmaster (uint16_t level)
{
    for (UniDev *ud = allunidevs; ud != NULL; ud = ud->nextunidev) {
        uint8_t v = ud->getintslave (level);
        if (v != 0) return v;
    }
    return 0;
}

bool UniDev::rdmaster (uint32_t addr, uint16_t *data)
{
    UniDev *ud = (addr < 0760000) ? unidevmemory : unidevtable[(addr>>1)&4095];
    return (ud != NULL) && ud->rdslave (addr, data);
}

bool UniDev::wrmaster (uint32_t addr, uint16_t data, bool byte)
{
    UniDev *ud = (addr < 0760000) ? unidevmemory : unidevtable[(addr>>1)&4095];
    return (ud != NULL) && ud->wrslave (addr, data, byte);
}

/////////////////////////
//  protected methods  //
/////////////////////////

UniDev::UniDev ()
{
    this->nextunidev = allunidevs;
    allunidevs = this;
}
