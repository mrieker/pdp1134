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

// tcp daemon for verilator simulator
// run on x86_64 as daemon - ./verimain.x86_64
// then run z11ctrl, z11dump, z11ila on same x86_64

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>

#define ABORT() do { fprintf (stderr, "ABORT %s %d\n", __FILE__, __LINE__); abort (); } while (0)

#include "obj_dir/VMyBoard.h"

#include "../ccode/futex.h"
#include "verisim.h"

static VMyBoard *vmybd;

static void kerchunk ();

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    int shmfd = shm_open (VERISIM_SHMNM, O_RDWR | O_CREAT, 0666);
    if (shmfd < 0) {
        fprintf (stderr, "verimain: error creating %s: %m\n", VERISIM_SHMNM);
        ABORT ();
    }

    VeriPage *pageptr;

    if (ftruncate (shmfd, sizeof *pageptr) < 0) {
        fprintf (stderr, "verimain: error truncating %s: %m\n", VERISIM_SHMNM);
        ABORT ();
    }

    if (flock (shmfd, LOCK_EX | LOCK_NB) < 0) {
        fprintf (stderr, "verimain: error locking %s: %m\n", VERISIM_SHMNM);
        ABORT ();
    }

    void *ptr = mmap (NULL, sizeof *pageptr, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (ptr == MAP_FAILED) {
        fprintf (stderr, "verimain: error mmapping %s: %m\n", VERISIM_SHMNM);
        ABORT ();
    }
    pageptr = (VeriPage *) ptr;
    memset (pageptr, 0, sizeof *pageptr);
    pageptr->ident = VERISIM_IDENT;
    pageptr->serverpid = getpid ();
    fprintf (stderr, "verimain: pid %d\n", pageptr->serverpid);

    VMyBoard vmyboard;
    vmybd = &vmyboard;
    vmybd->RESET_N = 0;
    for (int i = 0; i < 10; i ++) {
        kerchunk ();
    }
    vmybd->RESET_N = 1;
    for (int i = 0; i < 10; i ++) {
        kerchunk ();
    }

    char const *env = getenv ("verimain_debug");
    int debug = (env == NULL) ? 0 : atoi (env);

    while (true) {
        int state = pageptr->state;
        switch (state) {

            // some client wants to read one of the arm-side registers
            case VERISIM_READ: {
                uint32_t index = pageptr->indx;
                if (index > 1023) ABORT ();

                // send read address out over AXI bus and say we are ready to accept read data
                vmybd->saxi_ARADDR  = index * sizeof pageptr->data;
                vmybd->saxi_ARVALID = 1;
                vmybd->saxi_RREADY  = 1;

                // keep kerchunking until both transfers have completed
                // capture read data on cycle where both RREADY and RVALID are set
                for (int i = 0; vmybd->saxi_ARVALID | vmybd->saxi_RREADY; i ++) {
                    if (i > 100) ABORT ();
                    bool arready = vmybd->saxi_ARREADY;
                    kerchunk ();
                    if (arready) vmybd->saxi_ARVALID = 0;
                    bool rvalid  = vmybd->saxi_RVALID;
                    if (rvalid & vmybd->saxi_RREADY) {
                        pageptr->data = vmybd->saxi_RDATA;
                        vmybd->saxi_RREADY = 0;

                        if (debug > 1) printf ("verimain:  read %03X > %08X\n", index, pageptr->data);

                        if (! atomic_compare_exchange (&pageptr->state, &state, VERISIM_DONE)) ABORT ();
                        if (futex (&pageptr->state, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
                    }
                }
                break;
            }

            // some client wants to write one of the arm-side registers
            case VERISIM_WRITE: {
                uint32_t index = pageptr->indx;
                if (index > 1023) ABORT ();

                // put address and write data on AXI bus
                // say they are valid and also we are ready to accept write acknowledge
                vmybd->saxi_AWADDR  = index * sizeof pageptr->data;
                vmybd->saxi_AWVALID = 1;
                vmybd->saxi_WDATA   = pageptr->data;
                vmybd->saxi_WVALID  = 1;
                vmybd->saxi_BREADY  = 1;

                if (debug > 0) printf ("verimain: write %03X < %08X\n", index, pageptr->data);

                if (! atomic_compare_exchange (&pageptr->state, &state, VERISIM_DONE)) ABORT ();
                if (futex (&pageptr->state, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();

                // keep kerchunking until all 3 transfers have completed
                for (int i = 0; vmybd->saxi_AWVALID | vmybd->saxi_WVALID | vmybd->saxi_BREADY; i ++) {
                    if (i > 100) ABORT ();
                    bool awready = vmybd->saxi_AWREADY;
                    bool wready  = vmybd->saxi_WREADY;
                    kerchunk ();
                    if (awready) vmybd->saxi_AWVALID = 0;
                    if (wready)  vmybd->saxi_WVALID  = 0;
                    bool bvalid  = vmybd->saxi_BVALID;
                    if (bvalid)  vmybd->saxi_BREADY  = 0;
                }
                break;
            }

            // no client waiting for something, just keep clocking processor
            default: {
                kerchunk ();
                break;
            }
        }

        // if arm interrupt bit set, wake anything that's waiting for a set bit
        uint32_t oldirqmsk = pageptr->armintmsk;
        uint32_t newirqmsk = vmybd->regarmintreq;
        pageptr->armintmsk = newirqmsk;
        if ((debug > 0) && (oldirqmsk != newirqmsk)) printf ("verimain: armirqmsk %08X\n", newirqmsk);
        if ((newirqmsk & ~ oldirqmsk) &&
                (futex ((int *) &pageptr->armintmsk, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0)) {
            ABORT ();
        }
    }
}

// call with clock still low and input signals just changed
// returns with output signals updated and clock just set low
static void kerchunk ()
{
    vmybd->eval ();    // let input signals soak in
    vmybd->CLOCK = 1;  // clock the state
    vmybd->eval ();    // let new state settle in
    vmybd->CLOCK = 0;  // get ready for more input changes
}
