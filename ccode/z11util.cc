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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "futex.h"
#include "z11defs.h"
#include "z11util.h"

Z11Page *z11page;

struct DMALockShm { int futex; };
static DMALockShm *dmalockshm;
static int mypid;

Z11Page::Z11Page ()
{
    zynqpage = NULL;
    zynqptr = NULL;

    kyat = NULL;

#if defined VERISIM

    zynqpage = verisim_init ();
    zynqfd = open ("/tmp/zynqpdp11", O_RDWR | O_CREAT, 0666);
    if (zynqfd < 0) {       // need temp file so flock() will work
        fprintf (stderr, "Z11Page::Z11Page: error creating /tmp/zynqpdp11: %m\n");
        ABORT ();
    }

#elif defined SIMRPAGE

    zynqpage = simrpage_init (&zynqfd);

#else

    zynqfd = open ("/proc/zynqpdp11", O_RDWR);
    if (zynqfd < 0) {
        fprintf (stderr, "Z11Page::Z11Page: error opening /proc/zynqpdp11: %m\n");
        ABORT ();
    }

    zynqptr = mmap (NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, zynqfd, 0);
    if (zynqptr == MAP_FAILED) {
        fprintf (stderr, "Z11Page::Z11Page: error mmapping /proc/zynqpdp11: %m\n");
        ABORT ();
    }

    zynqpage = (uint32_t volatile *) zynqptr;

#endif

    ASSERT (z11page == NULL);
    z11page = this;

    mypid = getpid ();

    int dmalockfd = shm_open ("/shm_zturn11_dma", O_RDWR | O_CREAT, 0666);
    if (dmalockfd < 0) {
        fprintf (stderr, "Z11Page::dmalock: error opening /shm_zturn11_dma: %m\n");
        ABORT ();
    }
    fchmod (dmalockfd, 0666);
    if (ftruncate (dmalockfd, sizeof *dmalockshm) < 0) {
        fprintf (stderr, "Z11Page::dmalock: error setting /shm_zturn11_dma size: %m\n");
        ABORT ();
    }

    dmalockshm = (DMALockShm *) mmap (NULL, sizeof *dmalockshm, PROT_READ | PROT_WRITE, MAP_SHARED, dmalockfd, 0);
    if (dmalockshm == MAP_FAILED) ABORT ();

    kyat = findev ("KY", NULL, NULL, false);
}

Z11Page::~Z11Page ()
{
    if (zynqptr != NULL) munmap (zynqptr, 4096);
    close (zynqfd);
    z11page = NULL;
    zynqpage = NULL;
    zynqptr = NULL;
    zynqfd = -1;
    kyat = NULL;
}

// find a device in the Z11 page
//  input:
//   id = NULL: all entries passed to entry() for checking
//        else: two-char string ident to check for
//   entry = NULL: return first dev that matches id
//           else: call this func to match dev
//   param = passed to entry()
//   lock = true: lock access to dev when found
//         false: don't bother locking
//  output:
//   returns pointer to dev
uint32_t volatile *Z11Page::findev (char const *id, bool (*entry) (void *param, uint32_t volatile *dev), void *param, bool lockit, bool killit)
{
    for (int idx = 0; idx < 1024;) {
        uint32_t volatile *dev = &zynqpage[idx];
        int len = 2 << ((ZRD(*dev) >> 12) & 15);
        if (idx + len > 1024) break;
        if ((id == NULL) || (((ZRD(*dev) >> 24) == (uint8_t) id[0]) && (((ZRD(*dev) >> 16) & 255) == (uint8_t) id[1]))) {
            if ((entry == NULL) || entry (param, dev)) {
                if (lockit) locksubdev (dev, len, killit);
                return dev;
            }
        }
        idx += len;
    }
    if (entry == NULL) {
        fprintf (stderr, "Z11Page::findev: cannot find %s\n", id);
        ABORT ();
    }
    entry (param, NULL);
    return NULL;
}

// lock a sub-device
//  input:
//   start = first register of sub-device to lock
//   nwords = number of registers starting at 'start'
//   killit = true: try to kill other process if locked
//           false: abort if already locked
void Z11Page::locksubdev (uint32_t volatile *start, int nwords, bool killit)
{
    // find device that contains the start word
    int len, ofs;
    uint32_t volatile *dev;
    for (int idx = 0; idx < 1024;) {
        dev = &zynqpage[idx];
        len = 2 << ((ZRD(*dev) >> 12) & 15);
        if (idx + len > 1024) break;
        ofs = start - dev;
        if ((ofs >= 0) && (ofs < len)) goto found;
        idx += len;
    }
    ABORT ();
found:;
    if (len - ofs < nwords) ABORT ();

    // dev = device within zynqpage
    // ofs = offset of start within dev
    // len = total number of words in device

    int kills = killit ? 3 : 0;

    while (true) {

        // try to lock the requested range
        struct flock flockit;
        memset (&flockit, 0, sizeof flockit);
        flockit.l_type   = F_WRLCK;
        flockit.l_whence = SEEK_SET;
        flockit.l_start  = (long)start - (long)zynqpage;
        flockit.l_len    = nwords * sizeof zynqpage[0];
        if (fcntl (zynqfd, F_SETLK, &flockit) >= 0) break;

        // failed, try to find out what pid has it locked
        if (((errno == EACCES) || (errno == EAGAIN)) && (fcntl (zynqfd, F_GETLK, &flockit) >= 0)) {
            if (flockit.l_type == F_UNLCK) continue;

            // print out message saying pid that has it locked
            fprintf (stderr, "Z11Page::findev: %c%c+%d locked by pid %d\n", ZRD(*dev) >> 24, ZRD(*dev) >> 16, ofs, (int) flockit.l_pid);
            if (-- kills >= 0) {

                // try to kill it then try locking again
                fprintf (stderr, "Z11Page::findev: killing pid %d\n", (int) flockit.l_pid);
                kill ((int) flockit.l_pid, SIGTERM);
                usleep (125000);
                continue;
            }
        } else {
            fprintf (stderr, "Z11Page::findev: error locking %c%c: %m\n", ZRD(*dev) >> 24, ZRD(*dev) >> 16);
        }
        ABORT ();
    }
}

// unlock a device
//  input:
//   start = first register of device to unlock
void Z11Page::unlkdev (uint32_t volatile *start)
{
    // find device that contains the start word
    int len, ofs;
    uint32_t volatile *dev;
    for (int idx = 0; idx < 1024;) {
        dev = &zynqpage[idx];
        len = 2 << ((ZRD(*dev) >> 12) & 15);
        if (idx + len > 1024) break;
        ofs = start - dev;
        if ((ofs >= 0) && (ofs < len)) goto found;
        idx += len;
    }
    return;

    // release lock on the device
found:;
    struct flock flockit;
    memset (&flockit, 0, sizeof flockit);
    flockit.l_type   = F_UNLCK;
    flockit.l_whence = SEEK_SET;
    flockit.l_start  = (long)start - (long)zynqpage;
    flockit.l_len    = len * sizeof zynqpage[0];

    fcntl (zynqfd, F_SETLK, &flockit);
}

// read a word from unibus via dma
// uses ky11.v so it works even when 11/34 is halted
uint32_t Z11Page::dmaread (uint32_t xba, uint16_t *data)
{
    dmalock ();
    try {
        uint32_t rc = dmareadlocked (xba, data);
        dmaunlk ();
        return rc;
    } catch (...) {
        dmaunlk ();
        throw;
    }
}

uint32_t Z11Page::dmareadlocked (uint32_t xba, uint16_t *data)
{
    dmachecklocked ();
    ZWR(kyat[3], KY3_DMASTATE0 | KY3_DMAADDR0 * xba);
    uint32_t rc;
    for (int i = 0; ((rc = ZRD(kyat[3])) & KY3_DMASTATE) != 0; i ++) {
        if (i > 100000) {
            throw Z11DMAException ("Z11Page::dmaread: dma stuck");
        }
    }
    *data = (ZRD(kyat[4]) & KY4_DMADATA) / KY4_DMADATA0;
    return rc & (KY3_DMATIMO | KY3_DMAPERR);
}

// write a byte to unibus via dma
// uses ky11.v so it works even when 11/34 is halted
bool Z11Page::dmawbyte (uint32_t xba, uint8_t data)
{
    dmalock ();
    try {
        bool ok = dmawbytelocked (xba, data);
        dmaunlk ();
        return ok;
    } catch (...) {
        dmaunlk ();
        throw;
    }
}

bool Z11Page::dmawbytelocked (uint32_t xba, uint8_t data)
{
    dmachecklocked ();
    ZWR(kyat[4], KY4_DMADATA0 * 0401 * data);
    ZWR(kyat[3], KY3_DMASTATE0 | KY3_DMACTRL0 * 3 | KY3_DMAADDR0 * xba);
    for (int i = 0; (ZRD(kyat[3]) & KY3_DMASTATE) != 0; i ++) {
        if (i > 100000) {
            throw Z11DMAException ("Z11Page::dmawbyte: dma stuck");
        }
    }
    return ! (ZRD(kyat[3]) & KY3_DMATIMO);
}

// write a word to unibus via dma
// uses ky11.v so it works even when 11/34 is halted
bool Z11Page::dmawrite (uint32_t xba, uint16_t data)
{
    dmalock ();
    try {
        bool ok = dmawritelocked (xba, data);
        dmaunlk ();
        return ok;
    } catch (...) {
        dmaunlk ();
        throw;
    }
}

bool Z11Page::dmawritelocked (uint32_t xba, uint16_t data)
{
    dmachecklocked ();
    ZWR(kyat[4], KY4_DMADATA0 * data);
    ZWR(kyat[3], KY3_DMASTATE0 | KY3_DMACTRL0 * 2 | KY3_DMAADDR0 * xba);
    for (int i = 0; (ZRD(kyat[3]) & KY3_DMASTATE) != 0; i ++) {
        if (i > 100000) {
            throw Z11DMAException ("Z11Page::dmawrite: dma stuck");
        }
    }
    return ! (ZRD(kyat[3]) & KY3_DMATIMO);
}

// acquire exclusive access to dma controller
// wait indefinitely in case being used by TCL scripting
void Z11Page::dmalock ()
{
    int tmpfutex = 0;
    while (! atomic_compare_exchange (&dmalockshm->futex, &tmpfutex, mypid)) {
        ASSERT (tmpfutex != mypid);
        if ((kill (tmpfutex, 0) < 0) && (errno == ESRCH)) {
            fprintf (stderr, "Z11Page::dmalock: locker %d dead\n", tmpfutex);
        } else {
            int rc = futex (&dmalockshm->futex, FUTEX_WAIT, tmpfutex, NULL, NULL, 0);
            if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
            tmpfutex = 0;
        }
    }
    dmachecklocked ();
}

// release exclusive access to dma controller
void Z11Page::dmaunlk ()
{
    int tmpfutex = mypid;
    if (! atomic_compare_exchange (&dmalockshm->futex, &tmpfutex, 0)) ABORT ();
    if (futex (&dmalockshm->futex, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
}

void Z11Page::dmachecklocked ()
{
    int lockedby = dmalockshm->futex;
    if (lockedby != mypid) {
        fprintf (stderr, "Z11Page::dmachecklocked: locked by %u, expected %u\n", lockedby, mypid);
        ABORT ();
    }
}

// wait for interrupt(s) given in mask
// checks for bit(s) set in regarmintreq
// if not already set,
//   enables the interrupt in regarmintena
//   waits for interrupt
//   clears the bits from regarmintena
//     (unless something else waiting for them)
// caller should do whatever to device to clear bits in regarmintreq before calling again
void Z11Page::waitint (uint32_t mask)
{
    int rc = ioctl (zynqfd, ZGIOCTL_WFI, mask);
    if ((rc < 0) && (errno != EINTR)) {
        fprintf (stderr, "Z11Page::waitint: error waiting for interrupt: %m\n");
        ABORT ();
    }
}

// read registers when processor is running
// halts processor momentarily to read registers
//  input:
//   addr  = starting address of registers to read
//   count = number of registers to read - 1
//  output:
//   returns [31:16] = 0 : success
//                    -2 : stuck doing snapshot
//                    -3 : timed out reading register
//                    -4 : parity error reading register
//           [15:00] = number of registers read
//   *regs = filled in with contents of registers
int Z11Page::snapregs (uint32_t addr, int count, uint16_t *regs)
{
    if ((addr < 0760000) || (addr > 0777776)) ABORT ();
    if ((count < 0) || (count > 15)) ABORT ();

    // keep out other things from using dma registers
    dmalock ();

    // see if already requesting halt
    // if so, set up to restore when snapshot completes
    uint32_t ky2 = ZRD(kyat[2]);

    ky2 = (ky2 & (KY2_ENABLE | KY2_SR1716)) | ((ky2 & KY2_HALTREQ) ? KY2_SNAPHLT : 0);

    // tell ky11.v to do the snapshot
    // it halts the processor, does dma requests to read registers, resumes processor
    ZWR(kyat[3], KY3_DMAADDR0 * addr);
    ZWR(kyat[2], ky2 | KY2_HALTREQ | KY2_SNAPCTR0 * count | KY2_SNAPREQ);

    // wait for snapshot to complete, should be 15-20uS
    for (int i = 0; ((ky2 = ZRD(kyat[2])) & KY2_SNAPREQ) != 0; i ++) {
        if (i > 100000) {
            dmaunlk ();
            return -2 << 16;
        }
    }

    // get number of registers successfully read
    int rc = count - (ky2 & KY2_SNAPCTR) / KY2_SNAPCTR0;

    // check for success
    uint32_t ky3 = ZRD(kyat[3]);
         if (ky3 & KY3_DMATIMO) rc |= -3 << 16;
    else if (ky3 & KY3_DMAPERR) rc |= -4 << 16;
                           else rc ++;

    ky2 &= KY2_ENABLE | KY2_SR1716 | KY2_HALTREQ;

    // get snapshot values
    for (int i = count; i >= 0; -- i) {
        ZWR(kyat[2], ky2 | i * KY2_SNAPCTR0);
        *(regs ++) = (ZRD(kyat[4]) & KY4_SNAPREG) / KY4_SNAPREG0;
    }

    // allow other dma now
    dmaunlk ();

    return rc;
}

// halt the processor
void Z11Page::haltreq ()
{
    // make sure something else isn't using ky registers
    // ...such as snapregs()
    dmalock ();

    // tell processor to halt at end of instruction
    ZWR(kyat[2], ZRD(kyat[2]) | KY2_HALTREQ);

    // we're done with dma registers
    dmaunlk ();
}

// single-step processor
void Z11Page::stepreq ()
{
    dmalock ();
    ZWR(kyat[2], ZRD(kyat[2]) | KY2_STEPREQ);
    dmaunlk ();
}

// tell processor to resume processing instructions
void Z11Page::contreq ()
{
    dmalock ();
    ZWR(kyat[2], ZRD(kyat[2]) & ~ KY2_HALTREQ);
    dmaunlk ();
}

// reset processor no matter what
// leave it halted on return
// processor will have loaded power-on vector (024 026)
void Z11Page::resetit ()
{
    dmalock ();
    kyat[2] |= KY2_HALTREQ;     // so it halts when started back up
    uint32_t volatile *pdpat = findev ("11", NULL, NULL, false);
    pdpat[Z_RA] |= a_man_ac_lo_out_h | a_man_dc_lo_out_h;
    usleep (200000);
    pdpat[Z_RA] &= ~ a_man_dc_lo_out_h;
    usleep (1000);
    pdpat[Z_RA] &= ~ a_man_ac_lo_out_h;
    dmaunlk ();
}

// generate a random number
uint32_t randbits (int nbits)
{
    static uint64_t seed = 0x123456789ABCDEF0ULL;

    uint32_t randval = 0;

    while (-- nbits >= 0) {

        // https://www.xilinx.com/support/documentation/application_notes/xapp052.pdf
        uint64_t xnor = ~ ((seed >> 63) ^ (seed >> 62) ^ (seed >> 60) ^ (seed >> 59));
        seed = (seed << 1) | (xnor & 1);

        randval += randval + (seed & 1);
    }

    return randval;
}

Z11DMAException::Z11DMAException (char const *msg)
{
    this->msg = msg;
}

char const *Z11DMAException::what () const throw()
{
    return msg;
}
