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

// Interface z11ctrl to verilated zynq code
// Call verisim_read() and verisim_write() to access the Zynq-like register page

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "../ccode/futex.h"
#include "verisim.h"

#define ABORT() do { fprintf (stderr, "ABORT %s %d\n", __FILE__, __LINE__); abort (); } while (0)

static int mypid, serverpid;
static pthread_mutex_t shmutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t volatile *nullpage;
static VeriPage *pageptr;

static void waitforidle ();
static uint32_t waitfordone (int func);
static void futexwait (int *cell, int valu);

// equivalent of opening /proc/zynqpdp11
// set up access to shared page
uint32_t volatile *verisim_init ()
{
    int shmfd = shm_open (VERISIM_SHMNM, O_RDWR, 0);
    if (shmfd < 0) {
        fprintf (stderr, "verisim_init: error opening %s: %m\n", VERISIM_SHMNM);
        ABORT ();
    }

    mypid = getpid ();

    void *ptr = mmap (NULL, sizeof *pageptr, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (ptr == MAP_FAILED) ABORT ();
    pageptr = (VeriPage *) ptr;

    serverpid = pageptr->serverpid;
    if ((serverpid <= 0) || (kill (serverpid, 0) < 0)) {
        fprintf (stderr, "verisim_init: server %d dead\n", serverpid);
        ABORT ();
    }

    ptr = mmap (NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) ABORT ();
    nullpage = (uint32_t volatile *) ptr;
    return nullpage;
}

// wrapper for all read accesses to fpga register page
uint32_t verisim_read (uint32_t volatile *addr)
{
    waitforidle ();

    pageptr->indx = addr - nullpage;

    return waitfordone (VERISIM_READ);
}

// wrapper for all write accesses to fpga register page
void verisim_write (uint32_t volatile *addr, uint32_t data)
{
    waitforidle ();

    pageptr->indx = addr - nullpage;
    pageptr->data = data;

    waitfordone (VERISIM_WRITE);
}

// simulates the ioctl (ZGIOCTL_WFI) call
// - waits for (reg[ZG_INTFLAGS] & mask) != 0
void verisim_wfi (uint32_t mask)
{
    for (uint32_t armintmsk; ((armintmsk = pageptr->armintmsk) & mask) == 0;) {
        futexwait ((int *) &pageptr->armintmsk, armintmsk);
    }
}

// lock mutex so we're thread safe within this process
// wait for the state to be IDLE
// mark the struct in use by this client
static void waitforidle ()
{
    pthread_mutex_lock (&shmutex);
    while (true) {
        int state = pageptr->state;
        if (state != VERISIM_IDLE) {
            futexwait (&pageptr->state, state);
        } else {
            if (atomic_compare_exchange (&pageptr->state, &state, VERISIM_BUSY)) break;
        }
    }
    pageptr->clientpid = mypid;
}

// transition from BUSY to the given state
// then wait for state to be DONE
// grab the data returned therein
// finally set state to IDLE then release mutex
static uint32_t waitfordone (int func)
{
    int busy = VERISIM_BUSY;
    if (! atomic_compare_exchange (&pageptr->state, &busy, func)) ABORT ();
    if (futex (&pageptr->state, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();

    int state;
    while (true) {
        state = pageptr->state;
        if (state == VERISIM_DONE) break;
        futexwait (&pageptr->state, state);
    }

    uint32_t data = pageptr->data;

    if (! atomic_compare_exchange (&pageptr->state, &state, VERISIM_IDLE)) ABORT ();
    if (futex (&pageptr->state, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();

    pthread_mutex_unlock (&shmutex);

    return data;
}

// wait for the cell to be something other than valu
// abort if server (verimain) has exited
static void futexwait (int *cell, int valu)
{
    struct timespec when;
    memset (&when, 0, sizeof when);
    when.tv_sec = 1;
    while (true) {
        if ((pageptr->serverpid != serverpid) || (kill (serverpid, 0) < 0)) {
            fprintf (stderr, "verisim futexwait: server %d died\n", serverpid);
            ABORT ();
        }
        if (futex (cell, FUTEX_WAIT, valu, &when, NULL, 0) >= 0) break;
        int er = errno;
        if ((er == EAGAIN) || (er == EINTR)) break;
        if (er != ETIMEDOUT) ABORT ();
    }
}
