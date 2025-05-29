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
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "simrpage.h"

static SimrPage *shm;

static void simrpage_lock ();
static void simrpage_wait (uint32_t func, pthread_cond_t *cond);
static void simrpage_live (int rc);

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
    simrpage_lock ();
    shm->simrfunc = SIMRFUNC_READ;
    shm->simrindx = addr - (uint32_t volatile *) shm;
    if (pthread_cond_broadcast (&shm->simrcondrw) != 0) abort ();
    simrpage_wait (SIMRFUNC_DONE, &shm->simrconddn);
    uint32_t data = shm->simrdata;
    shm->simrfunc = SIMRFUNC_IDLE;
    if (pthread_cond_broadcast (&shm->simrcondid) != 0) abort ();
    if (pthread_mutex_unlock (&shm->simrmutex) != 0) abort ();
    return data;
}

// write a word to the axi-like page maintained by simmer
void simrpage_write (uint32_t volatile *addr, uint32_t data)
{
    simrpage_lock ();
    shm->simrfunc = SIMRFUNC_WRITE;
    shm->simrindx = addr - (uint32_t volatile *) shm;
    shm->simrdata = data;
    if (pthread_cond_broadcast (&shm->simrcondrw) != 0) abort ();
    if (pthread_mutex_unlock (&shm->simrmutex) != 0) abort ();
}

// lock access to shared page then wait for it to be idle
static void simrpage_lock ()
{
    while (true) {
        struct timespec nowts;
        if (clock_gettime (CLOCK_REALTIME, &nowts) < 0) abort ();
        nowts.tv_sec += 1;
        int rc = pthread_mutex_timedlock (&shm->simrmutex, &nowts);
        if (rc == 0) break;
        simrpage_live (rc);
    }
    simrpage_wait (SIMRFUNC_IDLE, &shm->simrcondid);
}

// wait for shared page to reach the given state
static void simrpage_wait (uint32_t func, pthread_cond_t *cond)
{
    while (shm->simrfunc != func) {
        struct timespec nowts;
        if (clock_gettime (CLOCK_REALTIME, &nowts) < 0) abort ();
        nowts.tv_sec += 1;
        int rc = pthread_cond_timedwait (cond, &shm->simrmutex, &nowts);
        if (rc != 0) simrpage_live (rc);
    }
}

// check that the simmer process is still alive
static void simrpage_live (int rc)
{
    if (rc != ETIMEDOUT) abort ();
    int pid = shm->simmerpid;
    if (kill (pid, 0) < 0) {
        fprintf (stderr, "simrpage_live: simmer pid %d died: %m\n", pid);
        abort ();
    }
}
