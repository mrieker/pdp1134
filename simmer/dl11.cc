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

// PDP-11 teletype interface

#include <stdio.h>

#include "dl11.h"

#define INTVEC 060
#define BUSADD 0777560

DL11::DL11 ()
{
    unidevtable[(BUSADD&017770)/2+0] = this;
    unidevtable[(BUSADD&017770)/2+1] = this;
    unidevtable[(BUSADD&017770)/2+2] = this;
    unidevtable[(BUSADD&017770)/2+3] = this;
}

/////////////////////
//  axibus access  //
/////////////////////

uint32_t DL11::axirdslv (uint32_t index)
{
    switch (index) {
        case 0: return 0x444C1002;  // "DL"; size; version
        case 1: return ((uint32_t) rbuf << 16) | rcsr;
        case 2: return ((uint32_t) xbuf << 16) | xcsr;
        case 3: return (enable << 31) | (INTVEC << 18) | BUSADD;
    }
    return 0xDEADBEEF;
}

void DL11::axiwrslv (uint32_t index, uint32_t data)
{
    switch (index) {
        case 1: {
            rbuf = data >> 16;
            rcsr = (rcsr & ~ 0200) | (data & 0200);
            break;
        }
        case 2: {
            xcsr = (xcsr & ~ 0200) | (data & 0200);
            break;
        }
        case 3: {
            enable = data >> 31;
            break;
        }
    }
}

/////////////////////
//  unibus access  //
/////////////////////

void DL11::resetslave ()
{
    rcsr = 0;
    xcsr = 0200;
}

uint8_t DL11::getintslave (uint16_t level)
{
    if ((level == 4) && ((rcsr & 0300) == 0300)) return 060;
    if ((level == 4) && ((xcsr & 0300) == 0300)) return 064;
    return 0;
}

bool DL11::rdslave (uint32_t physaddr, uint16_t *data)
{
    if (! enable) return false;
    switch (physaddr & 6) {
        case 0: *data = rcsr & 0300; break;
        case 2: *data = rbuf; rcsr &= ~ 0200; break;
        case 4: *data = xcsr & 0300; break;
        case 6: *data = xbuf; break;
    }
    return true;
}

bool DL11::wrslave (uint32_t physaddr, uint16_t data, bool byte)
{
    if (! enable) return false;
    if (byte) {
        switch (physaddr & 7) {
            case 0: rcsr = (rcsr & ~ 0100) | (data & 0100); break;
            case 4: xcsr = (xcsr & ~ 0100) | (data & 0100); break;
            case 6: xbuf = (xbuf & ~ 0377) | (data & 0377); // fallthrough
            case 7: xcsr &= ~ 0200; break;
        }
    } else {
        switch (physaddr & 6) {
            case 0: rcsr = (rcsr & ~ 0100) | (data & 0100); break;
            case 4: xcsr = (xcsr & ~ 0100) | (data & 0100); break;
            case 6: xbuf = (xcsr & ~ 0377) | (data & 0377); xcsr &= ~ 0200; break;
        }
    }
    return true;
}
