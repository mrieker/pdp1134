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

// PDP-11 RL01/2 disk interface

#include "rl11.h"

#define INTVEC 0160
#define BUSADD 0774400

RL11::RL11 ()
{
    unidevtable[(BUSADD&017770)/2+0] = this;
    unidevtable[(BUSADD&017770)/2+1] = this;
    unidevtable[(BUSADD&017770)/2+2] = this;
    unidevtable[(BUSADD&017770)/2+3] = this;
}

/////////////////////
//  axibus access  //
/////////////////////

uint32_t RL11::axirdslv (uint32_t index)
{
    updaterlcs ();

    switch (index) {
        case 0: return 0x524C2002;  // "RL"; size; version
        case 1: return ((uint32_t) rlba  << 16) | rlcs;
        case 2: return ((uint32_t) rlmp1 << 16) | rlda;
        case 3: return ((uint32_t) rlmp3 << 16) | rlmp2;
        case 4: return (driveerrors << 4) | drivereadys;
        case 5: return (enable << 31) | (INTVEC << 18) | BUSADD;
    }
    return 0xDEADBEEF;
}

void RL11::axiwrslv (uint32_t index, uint32_t data)
{
    switch (index) {
        case 1: {
            rlcs  = data;
            rlba  = data >> 16;
            break;
        }
        case 2: {
            rlda  = data;
            rlmp1 = data >> 16;
            break;
        }
        case 3: {
            rlmp2 = data;
            rlmp3 = data >> 16;
            break;
        }
        case 4: {
            drivereadys = data & 15;
            driveerrors = (data >> 4) & 15;
            break;
        }
        case 5: {
            enable = (data >> 31) & 1;
            break;
        }
    }
}

/////////////////////
//  unibus access  //
/////////////////////

void RL11::resetslave ()
{

    rlcs  = 0200;
    rlba  = 0;
    rlda  = 0;
    rlmp1 = 0;
    rlmp2 = 0;
    rlmp3 = 0;
}

uint8_t RL11::getintslave (uint16_t level)
{
    if ((level == 5) && ((rlcs & 0300) == 0300)) return INTVEC;
    return 0;
}

bool RL11::rdslave (uint32_t physaddr, uint16_t *data)
{
    if (! enable) return false;
    switch (physaddr & 6) {
        case 0: updaterlcs (); *data = rlcs; break;
        case 2: *data = rlba; break;
        case 4: *data = rlda; break;
        case 6: *data = rlmp1; rlmp1 = rlmp2; rlmp2 = rlmp3; rlmp3 = *data; break;
    }
    return true;
}

bool RL11::wrslave (uint32_t physaddr, uint16_t data, bool byte)
{
    if (! enable) return false;
    if (byte) {
        switch (physaddr & 7) {
            case 0: rlcs  = (rlcs & 0177400) | (data & 0377); break;
            case 1: rlcs  = (data << 8) | (rlcs & 0377); break;
            case 2: rlba  = (rlba & 0177400) | (data & 0377); break;
            case 3: rlba  = (data << 8) | (rlba & 0377); break;
            case 4: rlda  = (rlba & 0177400) | (data & 0377); break;
            case 5: rlda  = (data << 8) | (rlda & 0377); break;
            case 6: rlmp3 = rlmp2 = rlmp1 = (rlmp1 & 0177400) | (data & 0377); break;
            case 7: rlmp3 = rlmp2 = rlmp1 = (data << 8) | (rlmp1 & 0377); break;
        }
    } else {
        switch (physaddr & 6) {
            case 0: rlcs  = data; break;
            case 2: rlba  = data; break;
            case 4: rlda  = data; break;
            case 6: rlmp3 = rlmp2 = rlmp1 = data; break;
        }
    }
    return true;
}

void RL11::updaterlcs ()
{
    uint16_t dsel = (rlcs >> 8) & 3;
    uint16_t drdy = (drivereadys >> dsel) & 1;
    uint16_t derr = (driveerrors >> dsel) & 1;
    uint16_t cerr = derr || (rlcs & 036000);
    rlcs = (rlcs & 037776) | (derr << 14) | (cerr << 15) | drdy;
}
