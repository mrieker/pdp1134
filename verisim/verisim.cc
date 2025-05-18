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

// Test pdp1134.v by sending random instruction stream and checking the bus cycles

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define ABORT() do { fprintf (stderr, "ABORT %s %d\n", __FILE__, __LINE__); abort (); } while (0)

#include "obj_dir/VZynq.h"

#include "verisim.h"

static uint32_t extmemarray[1<<17];
static uint32_t volatile *pageptr;
static VZynq *vzynq;

static void kerchunk ();

uint32_t volatile *verisim_init ()
{
    void *ptr = mmap (NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) ABORT ();
    pageptr = (uint32_t volatile *) ptr;

    vzynq = new VZynq ();
    vzynq->RESET_N = 0;
    for (int i = 0; i < 10; i ++) {
        kerchunk ();
    }
    vzynq->RESET_N = 1;
    for (int i = 0; i < 10; i ++) {
        kerchunk ();
    }

    return pageptr;
}

uint32_t verisim_read (uint32_t volatile *addr)
{
    uint32_t index = addr - pageptr;
    if (index > 1023) ABORT ();

    vzynq->saxi_ARADDR  = index * sizeof *addr;
    vzynq->saxi_ARVALID = 1;
    for (int i = 0; ! (vzynq->saxi_ARREADY & vzynq->saxi_ARVALID); i ++) {
        if (i > 100) ABORT ();
        kerchunk ();
    }
    kerchunk ();
    vzynq->saxi_ARVALID = 0;
    vzynq->saxi_RREADY  = 1;
    for (int i = 0; ! (vzynq->saxi_RREADY & vzynq->saxi_RVALID); i ++) {
        if (i > 100) ABORT ();
        kerchunk ();
    }
    kerchunk ();
    uint32_t data = vzynq->saxi_RDATA;
    vzynq->saxi_RREADY = 0;
    return data;
}

void verisim_write (uint32_t volatile *addr, uint32_t data)
{
    uint32_t index = addr - pageptr;
    if (index > 1023) ABORT ();

    vzynq->saxi_AWADDR  = index * sizeof *addr;
    vzynq->saxi_AWVALID = 1;
    vzynq->saxi_WDATA   = data;
    vzynq->saxi_WVALID  = 1;
    vzynq->saxi_BREADY  = 1;

    for (int i = 0; vzynq->saxi_AWVALID | vzynq->saxi_WVALID | vzynq->saxi_BREADY; i ++) {
        if (i > 100) ABORT ();
        bool awready = vzynq->saxi_AWREADY;
        bool wready  = vzynq->saxi_WREADY;
        bool bvalid  = vzynq->saxi_BVALID;
        kerchunk ();
        if (awready) vzynq->saxi_AWVALID = 0;
        if (wready)  vzynq->saxi_WVALID  = 0;
        if (bvalid)  vzynq->saxi_BREADY  = 0;
    }
}

// call with clock still low and input signals just changed
// returns with output signals updated and clock just set low
static void kerchunk ()
{
    vzynq->eval ();     // let input signals soak in
    vzynq->CLOCK = 1;   // clock the state
    vzynq->eval ();     // let new state settle in
    vzynq->CLOCK = 0;   // get ready for more input changes

    if (vzynq->extmemenab) {
        if (vzynq->extmemwena & 2) extmemarray[vzynq->extmemaddr] = (extmemarray[vzynq->extmemaddr] & 0x001FF) | (vzynq->extmemdout & 0x3FE00);
        if (vzynq->extmemwena & 1) extmemarray[vzynq->extmemaddr] = (extmemarray[vzynq->extmemaddr] & 0x3FE00) | (vzynq->extmemdout & 0x001FF);
        vzynq->extmemdin = extmemarray[vzynq->extmemaddr];
    }
}
