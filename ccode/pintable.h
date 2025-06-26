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

#ifndef _PINTABLE_H
#define _PINTABLE_H

#include <stdint.h>

struct PinDef {
    char name[16];
    int dev;
    int reg;
    uint32_t mask;
    int lobit;
    bool writ;
};

#define DEV_11 0
#define DEV_BM 1
#define DEV_DL 2
#define DEV_DZ 3
#define DEV_KW 4
#define DEV_KY 5
#define DEV_PC 6
#define DEV_RL 7
#define DEV_TM 8
#define DEV_MAX 9

#define DEVIDS "11","BM","DL","DZ","KW","KY","PC","RL","TM"

#include "z11util.h"

extern PinDef const pindefs[];

extern Z11Page *z11page;

uint32_t volatile *pindev (int dev);

#endif
