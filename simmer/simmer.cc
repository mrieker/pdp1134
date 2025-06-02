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

// daemon for simulator
// run on x86_64 as daemon - ./simmer.x86_64
// then run z11ctrl, z11dump, z11ila, etc on same x86_64

#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define ABORT() do { fprintf (stderr, "ABORT %s %d\n", __FILE__, __LINE__); abort (); } while (0)

#include "axidev.h"
#include "bigmem.h"
#include "cpu1134.h"
#include "dl11.h"
#include "kl11.h"
#include "ky11.h"
#include "rl11.h"
#include "simrpage.h"
#include "stepper.h"

static bool atomic_compare_exchange (uint32_t *ptr, uint32_t *oldptr, uint32_t newval)
{
    return __atomic_compare_exchange_n (ptr, oldptr, newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static int futex (uint32_t *uaddr, int futex_op, uint32_t val,
             const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3)
{
    return syscall (SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    char const *cpulogname = NULL;
    for (int i = 0; ++ i < argc;) {
        if (strcasecmp (argv[i], "-cpulog") == 0) {
            if ((++ i >= argc) && (argv[i][0] == '-')) {
                fprintf (stderr, "missing filename for -cpulog\n");
                return 1;
            }
            cpulogname = argv[i];
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    // create shared memory page
    SimrPage *shm;
    int shmfd = shm_open (SIMRNAME, O_RDWR | O_CREAT, 0600);
    if (shmfd < 0) {
        fprintf (stderr, "simmer: error creating shared memory %s: %m\n", SIMRNAME);
        ABORT ();
    }
    uint32_t shmsize = (sizeof *shm + 4095) & ~ 4095;
    if (ftruncate (shmfd, shmsize) < 0) {
        fprintf (stderr, "simmer: error setting shared memory %s size: %m\n", SIMRNAME);
        shm_unlink (SIMRNAME);
        ABORT ();
    }

    // map it to va space and zero it out
    void *shmptr = mmap (NULL, shmsize, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shmptr == MAP_FAILED) {
        fprintf (stderr, "simmer: error accessing shared memory %s: %m\n", SIMRNAME);
        shm_unlink (SIMRNAME);
        ABORT ();
    }

    shm = (SimrPage *) shmptr;
    memset (shm, 0, shmsize);
    for (int i = 0; i < (int) (sizeof shm->baadfood / sizeof shm->baadfood[0]); i ++) {
        shm->baadfood[i] = 0xBAADF00D;
    }

    // plug boards into axi and uni busses
    new BigMem ();
    CPU1134 *cpu1134 = new CPU1134 ();
    new DL11 ();
    new KL11 ();
    new KY11 ();
    new RL11 ();
    AxiDev::axiassign ();

    if (cpulogname != NULL) {
        cpu1134->logit = (strcmp (cpulogname, "-") == 0) ? stdout : fopen (cpulogname, "w");
        if (cpu1134->logit == NULL) {
            fprintf (stderr, "error creating %s: %m\n", cpulogname);
            return 1;
        }
        setlinebuf (cpu1134->logit);
    }

    // process requests from z11xx programs
    shm->simmerpid = getpid ();
    fprintf (stderr, "simmer: pid %d\n", shm->simmerpid);
    shm->simrfunc = SIMRFUNC_IDLE;

    while (true) {

        Stepper::stepemall ();

        uint32_t func = shm->simrfunc;
        switch (func) {

            // do read from axi bus
            case SIMRFUNC_READ: {
                uint32_t index = shm->simrindx;
                if (index > 1023) {
                    fprintf (stderr, "simmer: bad index %u\n", index);
                    ABORT ();
                }
                shm->simrdata = AxiDev::axirdmas (index);
                ////printf ("simmer*:  read %04X => %08X\n", index, shm->simrdata);
                if (! atomic_compare_exchange (&shm->simrfunc, &func, SIMRFUNC_DONE)) ABORT ();
                int rc = futex (&shm->simrfunc, FUTEX_WAKE, 1000000000, NULL, NULL, 0);
                if (rc < 0) ABORT ();
                break;
            }

            // do write to axi bus
            case SIMRFUNC_WRITE: {
                uint32_t index = shm->simrindx;
                if (index > 1023) {
                    fprintf (stderr, "simmer: bad index %u\n", index);
                    ABORT ();
                }
                ////printf ("simmer*: write %04X <= %08X\n", index, shm->simrdata);
                AxiDev::axiwrmas (index, shm->simrdata);
                if (! atomic_compare_exchange (&shm->simrfunc, &func, SIMRFUNC_IDLE)) ABORT ();
                int rc = futex (&shm->simrfunc, FUTEX_WAKE, 1000000000, NULL, NULL, 0);
                if (rc < 0) ABORT ();
                break;
            }
        }
    }
}
