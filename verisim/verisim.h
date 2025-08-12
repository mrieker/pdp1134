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

#ifndef _VERISIM_H
#define _VERISIM_H

#include <stdint.h>

#define VERISIM_SHMNM "/shm_verisim"
#define VERISIM_IDENT (('V' << 24) | ('E' << 16) | 0x2001)

#define VERISIM_IDLE  0
#define VERISIM_BUSY  1
#define VERISIM_READ  2
#define VERISIM_WRITE 3
#define VERISIM_DONE  4

struct VeriPage {
    uint32_t ident;
    int state;
    int serverpid;
    int clientpid;
    uint32_t data;
    uint16_t indx;
    uint32_t armintmsk;
};

uint32_t volatile *verisim_init ();
uint32_t verisim_read (uint32_t volatile *addr);
void verisim_write (uint32_t volatile *addr, uint32_t data);
void verisim_wfi (uint32_t mask);

#endif
