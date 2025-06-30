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

// Performs TM11/TU10 tape I/O for the PDP-11 Zynq I/O board
// Runs as a background daemon when a file is loaded in a drive
// ...either with z11ctrl tmload command or GUI screen

//  ./z11tm [-reset]

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <tcl.h>
#include <unistd.h>

#include "futex.h"
#include "shmms.h"
#include "z11defs.h"
#include "z11util.h"

#define TAPELEN 20000000                        // number bytes in reel of tape

#define RFLD(n,m) ((ZRD(tmat[n]) & m) / (m & - m))

static bool unloads[8];
static char fns[8][SHMMS_FNSIZE];
static int debug;
static int fds[8];
static uint64_t rewdoneats[8];

#define LOCKIT shmms_svr_mutexlock(shmms)
#define UNLKIT shmms_svr_mutexunlk(shmms)

static int mypid;
static ShmMS *shmms;
static uint32_t volatile *tmat;
static Z11Page *z11p;

static void *tmiothread (void *dummy);
static void updatelowbits ();
static void *timerthread (void *dsptr);
static int setdrivetype (void *param, int drivesel);
static int fileloaded (void *param, int drivesel, int fd);
static void unloadfile (void *param, int drivesel);

int main (int argc, char **argv)
{
    memset (fds, -1, sizeof fds);
    mypid = getpid ();

    bool resetit = (argc > 1) && (strcasecmp (argv[1], "-reset") == 0);

    // access fpga register set for the TM-11 controller
    // lock it so we are only process accessing it
    z11p = new Z11Page ();
    tmat = z11p->findev ("TM", NULL, NULL, true, false);

    // open shared memory, create if not there
    shmms = shmms_svr_initialize (resetit, SHMMS_NAME_TM, "z11tm");
    UNLKIT;

    // enable board to process io instructions
    ZWR(tmat[4], (tmat[4] & TM4_FAST) | TM4_ENAB);

    debug = 0;
    char const *dbgenv = getenv ("z11tm_debug");
    if (dbgenv != NULL) debug = atoi (dbgenv);

    pthread_t tmtid;
    int rc = pthread_create (&tmtid, NULL, tmiothread, NULL);
    if (rc != 0) ABORT ();
    for (int i = 0; i < 8; i ++) {
        rc = pthread_create (&tmtid, NULL, timerthread, (void *)(long)i);
        if (rc != 0) ABORT ();
    }

    shmms_svr_proccmds (shmms, "z11tm", setdrivetype, fileloaded, unloadfile, NULL);

    return 0;
}

// do the tape file I/O
static void *tmiothread (void *dummy)
{
    static uint8_t buf[65536];

    if (debug > 1) fprintf (stderr, "z11tm: thread started\n");

    while (true) {

        // wait for pdp set go mtc[00] or power clear mtc[12]
        z11p->waitint (ZGINT_TM);

        // block tape from being unloaded from under us
        LOCKIT;

        // see if command from pdp waiting to be processed
        uint32_t mtcmtsat = tmat[1];

        uint32_t drivesel = (mtcmtsat & 0x07000000) >> 24;
        ShmMSDrive *dr = &shmms->drives[drivesel];

        uint32_t mtcmts = mtcmtsat & 0x6F7E0000;

        if (mtcmtsat & 0x10000000) {

            // power clear
            memset (rewdoneats, 0, sizeof rewdoneats);
            updatelowbits ();
            mtcmts |= 0x00800000;                   // controller ready
            ZWR(tmat[1], mtcmts);

        } else if (mtcmtsat & 0x00010000) {

            // go - update status at beginning of function, clearing errors and go bit
            ZWR(tmat[1], mtcmts);

            // maybe wait for previously started rewind to complete
            while (rewdoneats[drivesel] != 0) {
                int doneat = (int) rewdoneats[drivesel];
                UNLKIT;
                int rc = futex ((int *)&rewdoneats[drivesel], FUTEX_WAIT, doneat, NULL, NULL, 0);
                if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
                LOCKIT;
            }

            uint16_t mtbrc = RFLD (2, TM2_MTBRC);
            uint32_t mtcma = RFLD (2, TM2_MTCMA) | ((mtcmts & 0x00300000) >> 4);

            if (debug > 0) fprintf (stderr, "z11tm: [%u] start MTC=%06o MTS=%06o MTBRC=%06o MTCMA=%06o\n",
                    drivesel, mtcmts >> 16, mtcmts & 0xFFFF, mtbrc, mtcma);

            bool fastio = (tmat[4] & TM4_FAST) != 0;
            int fd = fds[drivesel];

            int rc = 0;
            uint32_t nbytes = 0;
            uint32_t func = (mtcmts >> 17) & 7;
            switch (func) {

                // read
                case 1: {

                    // read record length before data
                    uint32_t reclen, reclen2;
                    rc = pread (fd, &reclen, sizeof reclen, dr->curposn);
                    if (rc != (int) sizeof reclen) { nbytes = sizeof reclen; goto readerror; }
                    dr->curposn += sizeof reclen;

                    // if zero, hit a tape mark
                    if (reclen == 0) {
                        mtcmts |= 0x4000;
                    } else {

                        // read data
                        nbytes = (reclen > sizeof buf) ? sizeof buf : reclen;
                        rc = pread (fd, buf, nbytes, dr->curposn);
                        if (rc != (int) nbytes) goto readerror;
                        dr->curposn += (reclen + 1) & -2;

                        // read record length after data, should match length before data
                        rc = pread (fd, &reclen2, sizeof reclen2, dr->curposn);
                        if (rc != (int) sizeof reclen2) { nbytes = sizeof reclen2; goto readerror; }
                        if (reclen2 != reclen) {
                            fprintf (stderr, "z11tm: [%u] reclen %u, reclen2 %u at %u\n", drivesel, reclen, reclen2, dr->curposn);
                            goto readerror2;
                        }
                        dr->curposn += sizeof reclen2;

                        // write data to dma buffer
                        uint32_t i = 0;
                        do {
                            if ((mtbrc <= 0xFFFEU) && ! (mtcma & 1)) {
                                uint16_t word = buf[i] | ((uint16_t) buf[i+1] << 8);
                                if (! z11p->dmawrite (mtcma, word)) goto dmaerror;
                                mtbrc += 2;
                                mtcma += 2;
                                i += 2;
                            } else {
                                if (! z11p->dmawbyte (mtcma, buf[i])) goto dmaerror;
                                mtbrc ++;
                                mtcma ++;
                                i ++;
                            }
                            mtcma &= 0777777;
                        } while ((i < nbytes) && (mtbrc != 0));

                        // record length error if record longer than dma buffer
                        if (i < reclen) mtcmts |= 0x200;
                    }
                    break;
                }

                // write
                case 2:
                // write with extended interrecord gap
                case 6: {
                    if (dr->readonly) {
                        mtcmts |= 0x8000;   // illegal command
                    } else {

                        // read data from dma buffer
                        uint32_t reclen = 65536 - mtbrc;
                        ASSERT (reclen <= sizeof buf);
                        uint32_t i = 0;
                        do {
                            uint16_t word;
                            if (z11p->dmaread (mtcma & 0777776, &word) != 0) goto dmaerror;
                            if ((mtbrc <= 0xFFFEU) && ! (mtcma & 1)) {
                                buf[i++] = word;
                                buf[i++] = word >> 8;
                                mtbrc += 2;
                                mtcma += 2;
                            } else {
                                buf[i++] = (mtcma & 1) ? (word >> 8) : word;
                                mtbrc ++;
                                mtcma ++;
                            }
                            mtcma &= 0777777;
                        } while (mtbrc != 0);

                        // write record length before data
                        rc = pwrite (fd, &reclen, sizeof reclen, dr->curposn);
                        if (rc != (int) sizeof reclen) { nbytes = sizeof reclen; goto writerror; }
                        dr->curposn += sizeof reclen;

                        // write data
                        rc = pwrite (fd, buf, reclen, dr->curposn);
                        if (rc != (int) reclen) { nbytes = reclen; goto writerror; }
                        dr->curposn += (reclen + 1) & -2;

                        // write record length after data
                        rc = pwrite (fd, &reclen, sizeof reclen, dr->curposn);
                        if (rc != (int) sizeof reclen) { nbytes = sizeof reclen; goto writerror; }
                        dr->curposn += sizeof reclen;
                    }
                    break;
                }

                // write tape mark
                case 3: {
                    if (dr->readonly) {
                        mtcmts |= 0x8000;   // illegal command
                    } else {

                        // write a zero length for a tape mark
                        uint32_t mark = 0;
                        rc = pwrite (fd, &mark, sizeof mark, dr->curposn);
                        if (rc != (int) sizeof mark) { nbytes = sizeof mark; goto writerror; }
                        dr->curposn += sizeof mark;
                    }
                    break;
                }

                // space forward
                case 4: {
                    do {

                        // get record length being skipped
                        uint32_t reclen;
                        rc = pread (fd, &reclen, sizeof reclen, dr->curposn);
                        if (rc != (int) sizeof reclen) { nbytes = sizeof reclen; goto readerror; }
                        dr->curposn += sizeof reclen;

                        // check for tape mark
                        if (reclen == 0) {
                            mtcmts |= 0x4000;
                            break;
                        }

                        // increment over the data and length after data
                        dr->curposn += ((reclen + 1) & -2) + sizeof reclen;
                    } while (++ mtbrc != 0);
                    break;
                }

                // space reverse
                case 5: {
                    do {

                        // check for beginning of tape
                        if (dr->curposn == 0) break;

                        // read record length in reverse
                        uint32_t reclen;
                        dr->curposn -= sizeof reclen;
                        rc = pread (fd, &reclen, sizeof reclen, dr->curposn);
                        if (rc != (int) sizeof reclen) { nbytes = sizeof reclen; goto readerror; }

                        // check for tape mark
                        if (reclen == 0) {
                            mtcmts |= 0x4000;
                            break;
                        }

                        // decrement over the data and length before data
                        dr->curposn -= ((reclen + 1) & -2) + sizeof reclen;
                    } while (++ mtbrc != 0);
                    break;
                }

                // unload
                case 0:
                    unloads[drivesel] = true;
                // rewind
                case 7: {

                    if ((dr->curposn != 0) && ! fastio) {  // if already at beginning of tape, immediate completion

                        // start a timer which will cause rewind in progress to set and tape unit ready to clear
                        struct timespec nowts;
                        if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
                        uint64_t nowns = (nowts.tv_sec * 1000000000ULL) + nowts.tv_nsec;

                        // rewind:  (150 inch / second) * (800 chars / inch) = 120,000 chars / second = 8.33uS / char
                        uint64_t rewns = dr->curposn * 8333ULL;
                        if (rewns > 5000000000ULL) rewns = 5000000000ULL;

                        int oldsdaint = (int) rewdoneats[drivesel];
                        rewdoneats[drivesel] = nowns + rewns;
                        if ((int) rewdoneats[drivesel] == oldsdaint) rewdoneats[drivesel] ++;
                        if (futex ((int *)&rewdoneats[drivesel], FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
                    } else {
                        dr->curposn = 0;
                        if (unloads[drivesel]) {
                            dr->filename[0] = 0;
                            unloadfile (NULL, drivesel);
                        }
                    }
                    break;
                }

                default: ABORT ();
            }
            goto done;

        readerror:
            if (rc < 0) {
                fprintf (stderr, "z11tm: [%u] error reading tape at %u: %m\n", drivesel, dr->curposn);
            } else {
                fprintf (stderr, "z11tm: [%u] only read %d of %u bytes at %u\n", drivesel, rc, nbytes, dr->curposn);
            }
        readerror2:
            mtcmts |= 0x2000;   // crc error
            goto done;

        writerror:
            if (rc < 0) {
                fprintf (stderr, "z11tm: [%u] error writing tape at %u: %m\n", drivesel, dr->curposn);
            } else {
                fprintf (stderr, "z11tm: [%u] only wrote %d of %u bytes at %u\n", drivesel, rc, nbytes, dr->curposn);
            }
            mtcmts |= 0x2000;   // crc error
            goto done;

        dmaerror:
            fprintf (stderr, "z11tm: [%u] dma error at %06o\n", drivesel, mtcma);
            mtcmts |= 0x80;     // non-existent memory
            goto done;

        done:;
            updatelowbits ();       // update low status bits [06:00]

            mtcmts &= ~02000;       // update end-of-tape
            if (dr->curposn > TAPELEN) mtcmts |= 02000;

            mtcmts = (mtcmts & ~ 0x300000) | 0x800000 | ((mtcma << 4) & 0x300000);

            ZWR(tmat[2], ((uint32_t) mtcma << 16) | mtbrc);

            ZWR(tmat[1], mtcmts);
        }

        UNLKIT;
    }
}

// update low status bits, ie, mts[06:00]
// they are on a per-drive basis so there is an individual bit for each
static void updatelowbits ()
{
    uint32_t turs = 0;                      // figure out which drives are ready
    uint32_t rews = 0;                      // figure out which drives are rewinding
    uint32_t wrls = 0;                      // figure out which drives are write-locked
    uint32_t bots = 0;                      // figure out which drives are at beg of tape
    uint32_t sels = 0;                      // figure out which drives are selected
    for (int i = 0; i < 8; i ++) {
        if (fds[i] >= 0)                    turs |= 1U << i;
        if (rewdoneats[i] != 0)             rews |= 1U << i;
        if (shmms->drives[i].readonly)      wrls |= 1U << i;
        if (shmms->drives[i].curposn == 0) bots |= 1U << i;
        if (fds[i] >= 0)                    sels |= 1U << i;
    }
    turs &= ~ rews;
    ZWR(tmat[5], bots * TM5_BOTS0 | wrls * TM5_WRLS0 | rews * TM5_REWS0 | turs * TM5_TURS0);
    ZWR(tmat[6], sels * TM6_SELS0);
}

// wait for changes in rewdoneats[drivesel]
// when time is up, clear rewind in progress and set tape unit ready
static void *timerthread (void *dsptr)
{
    uint32_t drivesel = (long)dsptr;

    while (true) {

        // see if file open and rewind in progress
        LOCKIT;
        struct timespec *tsptr = NULL;
        struct timespec timeout;
        uint64_t doneat = rewdoneats[drivesel];
        if ((fds[drivesel] >= 0) && (doneat != 0)) {
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
                shmms->drives[drivesel].curposn = 0;
                rewdoneats[drivesel] = doneat = 0;
                if (unloads[drivesel]) {
                    shmms->drives[drivesel].filename[0] = 0;
                    unloadfile (NULL, drivesel);
                }
                if (futex ((int *)&rewdoneats[drivesel], FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
                updatelowbits ();
            }
        }

        // wait for current rewind complete or for another rewind to be started
        UNLKIT;
        int rc = futex ((int *)&rewdoneats[drivesel], FUTEX_WAIT, (int)doneat, tsptr, NULL, 0);
        if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR) && (errno != ETIMEDOUT)) ABORT ();
    }
}

// check that file about to be loaded is ok
static int setdrivetype (void *param, int drivesel)
{
    ShmMSDrive *dr = &shmms->drives[drivesel];
    char const *filenm = dr->filename;
    int fnlen = strlen (filenm);
    if ((fnlen < 5) || (strcasecmp (filenm + fnlen - 4, ".tap") != 0)) {
        fprintf (stderr, "z11tm: [%u] error decoding %s: name does not end with .tap\n", drivesel, filenm);
        return -EBADF;
    }
    return 0;
}

// file successfully loaded, save fd and mark drive online
static int fileloaded (void *param, int drivesel, int fd)
{
    ShmMSDrive *dr = &shmms->drives[drivesel];
    char const *filenm = dr->filename;
    fds[drivesel] = fd;
    if (strcmp (fns[drivesel], filenm) != 0) {
        strcpy (fns[drivesel], filenm);
        dr->curposn = 0;
    }
    updatelowbits ();
    return fd;
}

// close file loaded on a tape drive
static void unloadfile (void *param, int drivesel)
{
    fns[drivesel][0] = 0;
    close (fds[drivesel]);
    fds[drivesel] = -1;
    updatelowbits ();
}
