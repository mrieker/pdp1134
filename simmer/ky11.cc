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

#include "cpu1134.h"
#include "ky11.h"

KY11 *KY11::singleton;

// check for hard halt requested
// - persists until explicitly released by user
bool KY11::kyhaltreq ()
{
    return singleton->haltreq;
}

// check to see if user wants this cpu step to go through
// - clears itself and sets hard halt request for next cycle
bool KY11::kystepreq ()
{
    if (singleton->stepreq) {       // see if allow one step to go through
        singleton->stepreq = false; // ok, just this one cycle
        singleton->haltreq = true;  // ...then hard halt afterward
        return true;
    }
    return ! singleton->haltreq;    // not stepping, maybe already in hard halt
}

KY11::KY11 ()
{
    singleton = this;
    unidevtable[017570/2] = this;

    lights   = 0;
    switches = 0;
    enable   = false;
    dmafail  = false;
    dmactrl  = 0;
    dmaaddr  = 0;
    dmadata  = 0;
    dmalock  = 0;
    haltreq  = false;
    stepreq  = false;
}

/////////////////////
//  axibus access  //
/////////////////////

uint32_t KY11::axirdslv (uint32_t index)
{
    switch (index) {
        case 0: return 0x4B59200D;
        case 1: return ((uint32_t) lights << 16) | switches;
        case 2: return
                    (enable  << 31) |
                    (haltreq << 30) |
                    ((CPU1134::cpuhaltins () | haltreq) << 29) |
                    (stepreq << 28) |
                    (CPU1134::cpuhaltins () << 17) |
                    (irqlev  << 14) |
                    (irqvec  <<  8);
        case 3: return (dmafail << 28) | (dmactrl << 26) | dmaaddr;
        case 4: return dmadata;
        case 5: return dmalock;
    }
    return 0xDEADBEEF;
}

void KY11::axiwrslv (uint32_t index, uint32_t data)
{
    switch (index) {
        case 1: {
            switches = data;
            break;
        }
        case 2: {
            enable  = (data >> 31) &  1;
            haltreq = (data >> 30) &  1;
            stepreq = (data >> 28) &  1;
            irqlev  = (data >> 14) &  7;
            irqvec  = (data >>  8) & 63;
            break;
        }
        case 3: {
            dmaaddr = (data >>  0) & 0777777;
            dmactrl = (data >> 26) & 3;
            dmafail = (data >> 28) & 1;
            if ((data >> 29) & 1) {
                if (dmactrl & 2) {
                    if (dmactrl & dmaaddr & 1) dmadata >>= 8;
                    dmafail = ! wrmaster (dmaaddr, dmadata, dmactrl & 1);
                } else {
                    dmafail = ! rdmaster (dmaaddr, &dmadata);
                }
            }
            break;
        }
        case 4: {
            dmadata = data;
            break;
        }
        case 5: {
                 if (dmalock == 0) dmalock = data;
            else if (dmalock == data) dmalock = 0;
            break;
        }
    }
}

/////////////////////
//  unibus access  //
/////////////////////

void KY11::resetslave ()
{
    irqlev = 0;
}

uint8_t KY11::getintslave (uint16_t level)
{
    return (level == irqlev) ? irqvec * 4 : 0;
}

// something on unibus is attempting to read switches
bool KY11::rdslave (uint32_t physaddr, uint16_t *data)
{
    if (! enable) return false;
    *data = switches;
    return true;
}

// something on unibus is attempting to write lights
bool KY11::wrslave (uint32_t physaddr, uint16_t data, bool byte)
{
    if (! enable) return false;
    if (! byte) lights = data;
    else if (! (physaddr & 1)) lights = (lights & 0177400) | (data & 0377);
                               else lights = (data << 8) | (lights & 0377);
    if (data == 0) irqlev = 0;
    return true;
}
