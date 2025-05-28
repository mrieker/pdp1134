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

#ifndef _UNIDEV_H
#define _UNIDEV_H

#include <stdint.h>

struct UniDev {
    static void resetmaster ();
    static uint8_t getintmaster (uint16_t level);
    static bool rdmaster (uint32_t addr, uint16_t *data);
    static bool wrmaster (uint32_t addr, uint16_t data, bool byte);

protected:
    UniDev ();

    virtual void resetslave () = 0;
    virtual uint8_t getintslave (uint16_t level) = 0;
    virtual bool rdslave (uint32_t addr, uint16_t *data) = 0;
    virtual bool wrslave (uint32_t addr, uint16_t data, bool byte) = 0;

    static UniDev *unidevmemory;
    static UniDev *unidevtable[4096];

private:
    UniDev *nextunidev;
    static UniDev *allunidevs;
};

#endif
