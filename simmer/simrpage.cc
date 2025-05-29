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

#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include "simrpage.h"

static SimrPage *shm;

static void simrpage_tranw (uint32_t oldfunc, uint32_t newfunc);
static void simrpage_trani (uint32_t oldfunc, uint32_t newfunc);
static void simrpage_wake ();

static bool atomic_compare_exchange (uint32_t *ptr, uint32_t *oldptr, uint32_t newval)
{
    return __atomic_compare_exchange_n (ptr, oldptr, newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static int futex (uint32_t *uaddr, int futex_op, uint32_t val,
             const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3)
{
    return syscall (SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

// open access to shared page maintained by simmer
uint32_t volatile *simrpage_init (int *simrfd_r)
{
    // open shared memory created by simmer
    int shmfd = shm_open (SIMRNAME, O_RDWR, 0);
    if (shmfd < 0) {
        fprintf (stderr, "error opening shared memory %s: %m\n", SIMRNAME);
        abort ();
    }

    // map it to va space
    void *shmptr = mmap (NULL, sizeof *shm, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shmptr == MAP_FAILED) {
        fprintf (stderr, "error accessing shared memory %s: %m\n", SIMRNAME);
        abort ();
    }

    shm = (SimrPage *) shmptr;
    *simrfd_r = shmfd;
    return (uint32_t volatile *) shmptr;
}

// read a word from the axi-like page maintained by simmer
uint32_t simrpage_read (uint32_t volatile *addr)
{
    simrpage_tranw (SIMRFUNC_IDLE, SIMRFUNC_BUSY);
    shm->simrindx = addr - (uint32_t volatile *) shm;
    simrpage_trani (SIMRFUNC_BUSY, SIMRFUNC_READ);
    simrpage_wake ();
    simrpage_tranw (SIMRFUNC_DONE, SIMRFUNC_BUSY);
    uint32_t data = shm->simrdata;
    simrpage_trani (SIMRFUNC_BUSY, SIMRFUNC_IDLE);
    simrpage_wake ();
    return data;
}

// write a word to the axi-like page maintained by simmer
void simrpage_write (uint32_t volatile *addr, uint32_t data)
{
    simrpage_tranw (SIMRFUNC_IDLE, SIMRFUNC_BUSY);
    shm->simrindx = addr - (uint32_t volatile *) shm;
    shm->simrdata = data;
    simrpage_trani (SIMRFUNC_BUSY, SIMRFUNC_WRITE);
    simrpage_wake ();
}

// wait until the given transition is possible then do it
static void simrpage_tranw (uint32_t oldfunc, uint32_t newfunc)
{
    while (true) {
        uint32_t tmpfunc = oldfunc;
        if (atomic_compare_exchange (&shm->simrfunc, &tmpfunc, newfunc)) break;
        struct timespec timeout;
        memset (&timeout, 0, sizeof timeout);
        timeout.tv_sec = 1;
        int rc = futex (&shm->simrfunc, FUTEX_WAIT, tmpfunc, &timeout, NULL, 0);
        if ((rc < 0) && (errno != EAGAIN) && (errno != ETIMEDOUT)) abort ();
        int pid = shm->simmerpid;
        if (kill (pid, 0) < 0) {
            fprintf (stderr, "simrpage_live: simmer pid %d died: %m\n", pid);
            abort ();
        }
    }
}

// transition should happen immediately, abort if not
static void simrpage_trani (uint32_t oldfunc, uint32_t newfunc)
{
    uint32_t tmpfunc = oldfunc;
    if (! atomic_compare_exchange (&shm->simrfunc, &tmpfunc, newfunc)) abort ();
}

// wake everything waiting on the futex
static void simrpage_wake ()
{
    int rc = futex (&shm->simrfunc, FUTEX_WAKE, 1000000000, NULL, NULL, 0);
    if (rc < 0) abort ();
}
