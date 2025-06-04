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

// Memory test program
//  ./memtest.armv7l <lowaddress> <highaddress>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "z11util.h"

uint64_t getnowns ()
{
    struct timespec nowts;
    if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
    return ((uint64_t) nowts.tv_sec * 1000000000U) + nowts.tv_nsec;
}

int main (int argc, char **argv)
{
    if (argc != 3) ABORT ();
    uint32_t loaddr = strtoul (argv[1], NULL, 0) & -2;
    uint32_t hiaddr = strtoul (argv[2], NULL, 0) & -2;
    uint16_t shadow[131072];

    Z11Page *z11page = new Z11Page ();

    printf ("filling %06o..%06o\n", loaddr, hiaddr);
    for (uint32_t addr = loaddr; addr <= hiaddr; addr += 2) {
        uint16_t data = randbits (16);
        if (! z11page->dmawrite (addr, data)) {
            printf ("timeout writing %06o\n", addr);
            ABORT ();
        }
        shadow[addr/2] = data;
    }

    printf ("testing %06o..%06o\n", loaddr, hiaddr);
    z11page->dmalock ();
    uint32_t p = 0;
    uint32_t n = 0;
    uint64_t startns = getnowns ();
    while (true) {
        uint32_t addr = randbits (17) * 2;
        addr %= hiaddr + 2 - loaddr;
        addr += loaddr;
        if (randbits (2) == 0) {
            uint16_t data = randbits (16);
            if (! z11page->dmawritelocked (addr, data)) {
                z11page->dmaunlk ();
                printf ("timeout rewriting %06o\n", addr);
                ABORT ();
            }
            shadow[addr/2] = data;
        } else {
            uint16_t data;
            uint32_t rc = z11page->dmareadlocked (addr, &data);
            if (rc != 0) {
                z11page->dmaunlk ();
                printf ("error %08X reading %06o\n", rc, addr);
                ABORT ();
            }
            if (data != shadow[addr/2]) {
                z11page->dmaunlk ();
                printf ("mismatch %06o has %06o, should be %06o\n", addr, data, shadow[addr/2]);
                ABORT ();
            }
        }
        if (++ n == 1000000) {
            z11page->dmaunlk ();
            uint64_t stopns = getnowns ();
            printf ("%8u: %5llu ns/word\n", ++ p, (unsigned long long) (stopns - startns) / 1000000);
            startns = stopns;
            n = 0;
            z11page->dmalock ();
        }
    }
    return 0;
}
