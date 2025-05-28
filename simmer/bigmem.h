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

#ifndef _BIGMEM_H
#define _BIGMEM_H

#include "axidev.h"
#include "unidev.h"

struct BigMem : AxiDev, UniDev {
    BigMem ();

protected:
    // AxiDev
    virtual uint32_t axirdslv (uint32_t index);
    virtual void axiwrslv (uint32_t index, uint32_t data);

    // UniDev
    virtual void resetslave (); 
    virtual uint8_t getintslave (uint16_t level);
    virtual bool rdslave (uint32_t addr, uint16_t *data);
    virtual bool wrslave (uint32_t addr, uint16_t data, bool byte);

private:
    uint32_t armaddr;
    uint32_t armdata;
    uint64_t enable;

    union {
        uint16_t words[1<<17];
        uint8_t  bytes[1<<18];
    } mem;
};

#endif
