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

// Library for processing tape-drive style I/O

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "futex.h"
#include "shmms.h"
#include "tapelib.h"
#include "z11util.h"

// rewind:  (150 inch / second) * (800 chars / inch) = 120,000 chars / second = 8.33uS / char
#define REWNSPERCHR  8333ULL
#define REWNSMAXIMUM 5000000000ULL

// skip: (45 inch / second) * (800 chars / inch) = 36,000 chars / second = 27.78uS / char
#define SKPUSPERCHR 28

// skip: (45 inch / second) * (gap / 0.5 inch) = 11.1mS
#define SKPUSPERGAP 11111

// read/write: same as skip except assume approx 4uS per word dma transfer time
#define RDWUSPERCHR (SKPUSPERCHR-2)
#define RDWUSPERGAP SKPUSPERGAP

// controller-level constructor
//  input:
//   shmms = mass-storage shared memory pointer
TapeCtrlr::TapeCtrlr (ShmMS *shmms, char const *progname)
{
    this->shmms = shmms;
    this->progname = progname;
    for (int i = 0; i < SHMMS_NDRIVES; i ++) {
        this->drives[i].ctor (this, i);
    }

    pthread_t tid;
    int rc = pthread_create (&tid, NULL, iothreadwrap, this);
    if (rc != 0) ABORT ();
}

void *TapeCtrlr::iothreadwrap (void *zhis)
{
    ((TapeCtrlr *) zhis)->iothread ();
    return NULL;
}

// process commands coming from shared memory (load, unload)
void TapeCtrlr::proccmds ()
{
    shmms_svr_proccmds (shmms, this->progname, setdrivetype, fileloaded, unloadfile, this);
}

// power-on reset, start all drives rewinding
void TapeCtrlr::resetall ()
{
    for (int i = 0; i < SHMMS_NDRIVES; i ++) {
        TapeDrive *td = &this->drives[i];
        if (td->fd >= 0) td->startrewind (false);
    }
    this->updstbits ();
}

// lock and unlock state mutex for the controller, drives and shared memory
void TapeCtrlr::lockit ()
{
    shmms_svr_mutexlock (shmms);
}

void TapeCtrlr::unlkit ()
{
    shmms_svr_mutexunlk (shmms);
}

// check that file about to be loaded is ok
int TapeCtrlr::setdrivetype (void *zhis, int drivesel)
{
    TapeCtrlr *tc = (TapeCtrlr *) zhis;
    ShmMSDrive *dr = &tc->shmms->drives[drivesel];
    char const *filenm = dr->filename;
    int fnlen = strlen (filenm);
    if ((fnlen < 5) || (strcasecmp (filenm + fnlen - 4, ".tap") != 0)) {
        fprintf (stderr, "%s: [%u] error decoding %s: name does not end with .tap\n", tc->progname, drivesel, filenm);
        return -EBADF;
    }
    return 0;
}

// file successfully loaded, save fd and mark drive online
int TapeCtrlr::fileloaded (void *zhis, int drivesel, int fd)
{
    TapeCtrlr *tc = (TapeCtrlr *) zhis;
    TapeDrive *td = &tc->drives[drivesel];
    ShmMSDrive *dr = td->dr;
    td->fd = fd;
    if (strcmp (td->fn, dr->filename) != 0) {
        strcpy (td->fn, dr->filename);
        dr->curposn = 0;
    }
    tc->updstbits ();
    return fd;
}

// close file loaded on a tape drive
void TapeCtrlr::unloadfile (void *zhis, int drivesel)
{
    TapeCtrlr *tc = (TapeCtrlr *) zhis;
    TapeDrive *td = &tc->drives[drivesel];
    td->fn[0] = 0;
    close (td->fd);
    td->fd = -1;
    tc->updstbits ();
}

//////////////////////////////
//  Drive-specific methods  //
//////////////////////////////

void TapeDrive::ctor (TapeCtrlr *ctrlr, uint32_t drsel)
{
    this->unload = false;
    this->fn[0]  = 0;
    this->fd     = -1;
    this->dr     = &ctrlr->shmms->drives[drsel];
    this->ctrlr  = ctrlr;
    this->drsel  = drsel;
    this->dr->rewendsat = 0;

    pthread_t tmid;
    int rc = pthread_create (&tmid, NULL, timerthread, this);
    if (rc != 0) ABORT ();
}

// start rewinding the given drive
void TapeDrive::startrewind (bool unload)
{
    this->unload = unload;

    if ((dr->curposn != 0) && ! ctrlr->fastio) {  // if already at beginning of tape, immediate completion

        // start a timer which will cause rewind in progress to set and tape unit ready to clear
        struct timespec nowts;
        if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
        uint64_t nowns = (nowts.tv_sec * 1000000000ULL) + nowts.tv_nsec;

        // rewind:  (150 inch / second) * (800 chars / inch) = 120,000 chars / second = 8.33uS / char
        uint64_t rewns = dr->curposn * REWNSPERCHR;
        if (rewns > REWNSMAXIMUM) rewns = REWNSMAXIMUM;

        int oldsdaint = (int) dr->rewendsat;
        dr->rewbganat = nowns;
        dr->rewendsat = nowns + rewns;
        if ((int) dr->rewendsat == oldsdaint) dr->rewendsat ++;
        if (futex ((int *)&dr->rewendsat, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
    } else {
        dr->curposn = 0;
        if (this->unload) {
            dr->filename[0] = 0;
            TapeCtrlr::unloadfile (this->ctrlr, this->drsel);
        }
    }
}

// wait for previously started rewind/unload to complete
void TapeDrive::waitrewind ()
{
    while (dr->rewendsat != 0) {
        int doneat = (int) dr->rewendsat;
        ctrlr->unlkit ();
        int rc = futex ((int *)&dr->rewendsat, FUTEX_WAIT, doneat, NULL, NULL, 0);
        if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
        ctrlr->lockit ();
    }
}

// read data block from tape
//  input:
//   mtbc = 2s comp number of bytes (0 means 65536)
//   mtma = starting memory address (can be odd)
//  output:
//   returns -1: file read error
//           -2: start/end length mismatch
//           -3: dma error
//            0: tape mark
//            1: normal read
//            2: record too long
//   mtbc = incremented
//   mtma = incremented
int TapeDrive::readfwd (uint16_t &mtbc, uint32_t &mtma)
{
    // read record length before data
    uint32_t reclen  = 0;
    uint32_t reclen2 = 0;
    int rc = pread (fd, &reclen, sizeof reclen, dr->curposn);
    if (rc != (int) sizeof reclen) { readerror (rc, sizeof reclen); return -1; }
    dr->curposn += sizeof reclen;

    // delay for read/write
    if (! ctrlr->fastio) usleep (reclen * RDWUSPERCHR + RDWUSPERGAP);

    // if zero, hit a tape mark
    if (reclen == 0) return 0;

    // read data
    if (reclen > 65536) ABORT ();
    uint8_t buf[reclen];
    rc = pread (fd, buf, reclen, dr->curposn);
    if (rc != (int) reclen) { readerror (rc, reclen); return -1; }
    dr->curposn += (reclen + 1) & -2;

    if (rc == 80) {
        fprintf (stderr, "TapeDrive::readfwd*: %.80s\n", buf);
    }

    // read record length after data, should match length before data
    rc = pread (fd, &reclen2, sizeof reclen2, dr->curposn);
    if (rc != (int) sizeof reclen2) { readerror (rc, sizeof reclen2); return -1; }
    if (reclen2 != reclen) {
        readerror2 (reclen, reclen2);
        return -2;
    }
    dr->curposn += sizeof reclen2;

    // write data to dma buffer
    uint32_t i = 0;
    do {
        if ((mtbc <= 0xFFFEU) && ! (mtma & 1) && (reclen - i >= 2)) {
            uint16_t word = buf[i] | ((uint16_t) buf[i+1] << 8);
            if (! z11page->dmawrite (mtma, word)) { dmaerror (mtma); return -3; }
            mtbc += 2;
            mtma += 2;
            i += 2;
        } else {
            if (! z11page->dmawbyte (mtma, buf[i])) { dmaerror (mtma); return -3; }
            mtbc ++;
            mtma ++;
            i ++;
        }
        mtma &= 0777777;
    } while ((mtbc != 0) && (i < reclen));
    return (i < reclen) ? 2 : 1;
}

// write data block to tape
//  input:
//   mtbc = 2s comp number of bytes (0 means 65536)
//   mtma = starting memory address (can be odd)
//  output:
//   returns -3: dma error
//           -4: file write error
//          > 0: length of record written
//   mtbc = incremented
//   mtma = incremented
int TapeDrive::wrdata (uint16_t &mtbc, uint32_t &mtma)
{
    // read data from dma buffer
    uint32_t reclen = 65536 - mtbc;
    uint8_t buf[reclen];
    uint32_t i = 0;
    do {
        uint16_t word;
        if (z11page->dmaread (mtma & 0777776, &word) != 0) { dmaerror (mtma); return -3; }
        if ((mtbc <= 0xFFFEU) && ! (mtma & 1)) {
            buf[i++] = word;
            buf[i++] = word >> 8;
            mtbc += 2;
            mtma += 2;
        } else {
            buf[i++] = (mtma & 1) ? (word >> 8) : word;
            mtbc ++;
            mtma ++;
        }
        mtma &= 0777777;
    } while (mtbc != 0);

    // write record length before data
    int rc = pwrite (fd, &reclen, sizeof reclen, dr->curposn);
    if (rc != (int) sizeof reclen) { writerror (rc, sizeof reclen); return -4; }
    dr->curposn += sizeof reclen;

    // write data
    rc = pwrite (fd, buf, reclen, dr->curposn);
    if (rc != (int) reclen) { writerror (rc, reclen); return -4; }
    dr->curposn += (reclen + 1) & -2;

    // write record length after data
    rc = pwrite (fd, &reclen, sizeof reclen, dr->curposn);
    if (rc != (int) sizeof reclen) { writerror (rc, sizeof reclen); return -4; }
    dr->curposn += sizeof reclen;

    // delay for read/write
    if (! ctrlr->fastio) usleep (reclen * RDWUSPERCHR + RDWUSPERGAP);

    return reclen;
}

// write a zero length for a tape mark
int TapeDrive::wrmark ()
{
    uint32_t mark = 0;
    int rc = pwrite (fd, &mark, sizeof mark, dr->curposn);
    if (rc != (int) sizeof mark) { writerror (rc, sizeof mark); return -4; }
    dr->curposn += sizeof mark;

    // delay for read/write
    if (! ctrlr->fastio) usleep (RDWUSPERGAP);

    return 0;
}

// skip forward the given number of records, stopping on tape mark if found
int TapeDrive::skipfwd ()
{
    // get record length being skipped
    uint32_t reclen;
    int rc = pread (fd, &reclen, sizeof reclen, dr->curposn);
    if (rc != (int) sizeof reclen) { readerror (rc, sizeof reclen); return -1; }
    dr->curposn += sizeof reclen;

    // delay for skip
    // unlock during delay so gui can update during repeated skips
    if (! ctrlr->fastio) {
        ctrlr->unlkit ();
        usleep (reclen * SKPUSPERCHR + SKPUSPERGAP);
        ctrlr->lockit ();
    }

    // check for tape mark
    if (reclen == 0) return 1;

    // increment over the data and length after data
    dr->curposn += ((reclen + 1) & -2) + sizeof reclen;
    return 0;
}

// reverse skip the given number of records, stopping on tape mark if found
int TapeDrive::skiprev ()
{
    // check for beginning of tape
    if (dr->curposn == 0) return 2;

    // read record length in reverse
    uint32_t reclen;
    dr->curposn -= sizeof reclen;
    int rc = pread (fd, &reclen, sizeof reclen, dr->curposn);
    if (rc != (int) sizeof reclen) { readerror (rc, sizeof reclen); return -1; }

    // delay for skip
    // unlock during delay so gui can update during repeated skips
    if (! ctrlr->fastio) {
        ctrlr->unlkit ();
        usleep (reclen * SKPUSPERCHR + SKPUSPERGAP);
        ctrlr->lockit ();
    }

    // check for tape mark
    if (reclen == 0) return 1;

    // decrement over the data and length before data
    dr->curposn -= ((reclen + 1) & -2) + sizeof reclen;
    return 0;
}

// wait for changes in dr->rewendsat
// when time is up, clear rewind in progress and unload tape or set tape unit ready
void *TapeDrive::timerthread (void *zhis)
{
    TapeDrive *td = (TapeDrive *) zhis;
    ShmMSDrive *dr = td->dr;

    while (true) {

        // see if file open and rewind in progress
        td->ctrlr->lockit ();
        struct timespec *tsptr = NULL;
        struct timespec timeout;
        uint64_t doneat = dr->rewendsat;
        if ((td->fd >= 0) && (doneat != 0)) {
            struct timespec nowts;
            if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
            uint64_t nowns = (nowts.tv_sec * 1000000000ULL) + nowts.tv_nsec;

            // see if still waiting for rewind to complete
            // if so, set up timeout for remaining delta
            if (doneat > nowns) {
                memset (&timeout, 0, sizeof timeout);
                timeout.tv_sec  = (doneat - nowns) / 1000000000;
                timeout.tv_nsec = (doneat - nowns) % 1000000000;
                tsptr = &timeout;
            }

            // rewind complete, mark drive rewound and ready
            // then leave tsptr NULL to wait indefinitely for next rewind to begin
            else {
                dr->rewbganat = dr->rewendsat = doneat = 0;
                dr->curposn = 0;
                if (td->unload) {
                    dr->filename[0] = 0;
                    TapeCtrlr::unloadfile (td->ctrlr, td->drsel);
                }
                if (futex ((int *)&dr->rewendsat, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
                td->ctrlr->updstbits ();
            }
        }

        // wait for current rewind complete or for another rewind to be started
        td->ctrlr->unlkit ();
        int rc = futex ((int *)&dr->rewendsat, FUTEX_WAIT, (int)doneat, tsptr, NULL, 0);
        if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR) && (errno != ETIMEDOUT)) ABORT ();
    }
}

void TapeDrive::readerror (int rc, int nbytes)
{
    if (rc < 0) {
        fprintf (stderr, "%s: [%u] error reading tape at %u: %m\n", ctrlr->progname, drsel, dr->curposn);
    } else {
        fprintf (stderr, "%s: [%u] only read %d of %u bytes at %u\n", ctrlr->progname, drsel, rc, nbytes, dr->curposn);
    }
}

void TapeDrive::readerror2 (uint32_t reclen, uint32_t reclen2)
{
    fprintf (stderr, "%s: [%u] reclen %u, reclen2 %u at %u\n", ctrlr->progname, drsel, reclen, reclen2, dr->curposn);
}

void TapeDrive::writerror (int rc, int nbytes)
{
    if (rc < 0) {
        fprintf (stderr, "%s: [%u] error writing tape at %u: %m\n", ctrlr->progname, drsel, dr->curposn);
    } else {
        fprintf (stderr, "%s: [%u] only wrote %d of %u bytes at %u\n", ctrlr->progname, drsel, rc, nbytes, dr->curposn);
    }
}

void TapeDrive::dmaerror (uint32_t mtma)
{
    fprintf (stderr, "%s: [%u] dma error at %06o\n", ctrlr->progname, drsel, mtma);
}
