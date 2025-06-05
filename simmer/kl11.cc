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

// PDP-11 line clock interface

#include <stdlib.h>
#include <time.h>

#include "kl11.h"

static uint32_t nowtick ()
{
    struct timespec nowts;
    if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) abort ();
    return ((uint32_t) nowts.tv_sec) * 60 + ((uint32_t) nowts.tv_nsec) * 3 / 50000000;
}

KL11::KL11 ()
{
    unidevtable[017546/2] = this;   // 777546

    enable = false;
}

/////////////////////
//  axibus access  //
/////////////////////

uint32_t KL11::axirdslv (uint32_t index)
{
    switch (index) {
        case 0: return 0x4B4C0002;  // "KL"; size; version
        case 1: return (enable << 31) | (intreq << 30);
    }
    return 0xDEADBEEF;
}

void KL11::axiwrslv (uint32_t index, uint32_t data)
{
    if (index == 1) {
        enable = (data >> 31) & 1;
    }
}

/////////////////////
//  unibus access  //
/////////////////////

void KL11::resetslave ()
{
    intreq  = false;
    lastick = nowtick ();
    lkflag  = false;
    lkiena  = false;
}

uint8_t KL11::getintslave (uint16_t level)
{
    if ((level != 6) || ! enable) return 0;
    update ();
    if (! intreq) return 0;
    intreq = false;
    return 0100;
}

bool KL11::rdslave (uint32_t physaddr, uint16_t *data)
{
    if (! enable) return false;
    update ();
    *data = (lkflag << 7) | (lkiena << 6);
    return true;
}

bool KL11::wrslave (uint32_t physaddr, uint16_t data, bool byte)
{
    if (! enable) return false;
    intreq  = false;
    lastick = nowtick ();
    lkflag  = false;
    lkiena  = (data >> 6) & 1;
    return true;
}

void KL11::update ()
{
    uint32_t thistick = nowtick ();
    if (lastick != thistick) {
        lastick  = thistick;
        lkflag   = true;
        intreq   = lkiena;
    }
}
