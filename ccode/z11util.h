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

#ifndef _Z11UTIL_H
#define _Z11UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)
#define ASSERT(cond) do { if (__builtin_constant_p (cond)) { if (!(cond)) asm volatile ("assert failure line %c0" :: "i"(__LINE__)); } else { if (!(cond)) ABORT (); } } while (0)

#ifdef VERISIM
#define ZRD(a) verisim_read(&(a))
#define ZWR(a,d) verisim_write(&(a),d)
#include "../verisim/verisim.h"
#else
#define ZRD(a) (a)
#define ZWR(a,d) do { a = d; } while (0)
#endif

struct Z11Page {
    Z11Page ();
    virtual ~Z11Page ();
    uint32_t volatile *findev (char const *id, bool (*entry) (void *param, uint32_t volatile *dev), void *param, bool lockit, bool killit = false);
    void locksubdev (uint32_t volatile *start, int nwords, bool killit);

    bool dmaread (uint32_t xba, uint16_t *data);
    bool dmawrite (uint32_t xba, uint16_t data);

private:
    int zynqfd;
    uint32_t volatile *slat;
    uint32_t volatile *zynqpage;
    void *zynqptr;

    static uint32_t mypid;

    void dmalock ();
    void dmaunlk ();
};

uint32_t randbits (int nbits);

#endif
