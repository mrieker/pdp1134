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

// Performs RL01/RL02 disk I/O for the PDP-11 Zynq I/O board
// Runs as a background daemon when a file is loaded in a drive
// ...either with z11ctrl rlload command or GUI screen

//  ./z11rl [-reset]

// page references rl01/rl02 user guide sep 81

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
#include "shmrl.h"
#include "z11defs.h"
#include "z11util.h"

#define BLKNUM(da) ((uint16_t)((da >> 6) * 40 + (da & 63)))
#define SECCNT(wc) ((uint16_t)((65536 - wc + 127) / 128))

                        // (p25/v1-13)
#define NCYLS_RL01 256
#define NCYLS_RL02 512
#define NCYLS (dr->rl01?NCYLS_RL01:NCYLS_RL02)
#define SECPERTRK 40
#define TRKPERCYL 2
#define NSECS (NCYLS*TRKPERCYL*SECPERTRK)

#define WRDPERSEC 128

#define USPERCYL ((100000-15000)/(NCYLS-1))     // usec per cyl = (full seek - one seek) / (num cyls - 1)
#define SETTLEUS (15000-USPERCYL)               // settle time = one seek - usec per cyl
#define AVGROTUS 6500                           // average rotation usec
#define USPERWRD 4                              // usec per word
#define USPERSEC (AVGROTUS*2/SECPERTRK)         // usec per sector

#define NSPERDMA 2350
#define USLEEPOV 72

#define RFLD(n,m) ((ZRD(rlat[n]) & m) / (m & - m))

static bool vcs[4];
static char fns[4][SHMRL_FNSIZE];
static int debug;
static int fds[4];
static uint64_t seekdoneats[4];

#define LOCKIT mutexlock()
#define UNLKIT mutexunlk()

static bool volatile exiting;
static int mypid;
static ShmRL *shmrl;
static uint32_t volatile *rlat;
static Z11Page *z11p;

static void mutexlock ()
{
    int newfutex = mypid;
    int tmpfutex = 0;
    while (true) {
        if (atomic_compare_exchange (&shmrl->rlfutex, &tmpfutex, newfutex)) break;
        if ((kill (tmpfutex, 0) < 0) && (errno == ESRCH)) {
            fprintf (stderr, "z11rl: locker %d dead\n", tmpfutex);
        } else {
            int rc = futex (&shmrl->rlfutex, FUTEX_WAIT, tmpfutex, NULL, NULL, 0);
            if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
            tmpfutex = 0;
        }
    }
}

static void mutexunlk ()
{
    int newfutex = 0;
    int tmpfutex = mypid;
    if (! atomic_compare_exchange (&shmrl->rlfutex, &tmpfutex, newfutex)) ABORT ();
    if (futex (&shmrl->rlfutex, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
}

static void *rliothread (void *dummy);
static void *timerthread (void *dsptr);
static void procommands ();
static int loadfile (uint16_t drivesel);
static void unloadfile (uint16_t drivesel);
static void dumpbuf (uint16_t drivesel, uint16_t const *buf, uint32_t off, uint32_t xba, char const *func);

int main (int argc, char **argv)
{
    memset (fds, -1, sizeof fds);
    mypid = getpid ();

    bool resetit = (argc > 1) && (strcasecmp (argv[1], "-reset") == 0);

    // access fpga register set for the RL-11 controller
    // lock it so we are only process accessing it
    z11p = new Z11Page ();
    rlat = z11p->findev ("RL", NULL, NULL, true, false);

    // open shared memory, create if not there
    int shmfd = shm_open (SHMRL_NAME, O_RDWR | O_CREAT, 0666);
    if (shmfd < 0) {
        fprintf (stderr, "z11rl: error creating %s: %m\n", SHMRL_NAME);
        return 1;
    }
    if (ftruncate (shmfd, sizeof *shmrl) < 0) {
        fprintf (stderr, "z11rl: error extending %s: %m\n", SHMRL_NAME);
        return 1;
    }
    shmrl = (ShmRL *) mmap (NULL, sizeof *shmrl, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shmrl == MAP_FAILED) {
        fprintf (stderr, "z11rl: error mmapping %s: %m\n", SHMRL_NAME);
        return 1;
    }
    if (resetit) memset (shmrl, 0, sizeof *shmrl);

    // put our pid in there in case we die when something is waiting for us to do something
    // exit if there is another one of us already running
    LOCKIT;
    int oldpid = shmrl->svrpid;
    if ((oldpid != 0) && (oldpid != mypid)) {
        if (kill (oldpid, 0) >= 0) {
            UNLKIT;
            fprintf (stderr, "z11rl: duplicate z11rl process %d\n", oldpid);
            return 0;
        }
        if (errno != ESRCH) ABORT ();
    }
    fprintf (stderr, "z11rl: new z11rl process %d\n", mypid);
    shmrl->svrpid = mypid;

    // we don't know about any loaded files so say drives are empty
    // ...unless there is an outstanding load request for a drive
    for (int i = 0; i < 4; i ++) {
        if (shmrl->command != SHMRLCMD_LOAD + i) {
            shmrl->drives[i].filename[0] = 0;
        }
    }
    UNLKIT;

    // enable board to process io instructions
    ZWR(rlat[5], RL5_ENAB);

    debug = 0;
    char const *dbgenv = getenv ("z11rl_debug");
    if (dbgenv != NULL) debug = atoi (dbgenv);

    pthread_t rltid;
    int rc = pthread_create (&rltid, NULL, rliothread, NULL);
    if (rc != 0) ABORT ();
    for (int i = 0; i < 4; i ++) {
        rc = pthread_create (&rltid, NULL, timerthread, (void *)(long)i);
        if (rc != 0) ABORT ();
    }
    procommands ();

    return 0;
}

// get what sector is currently under the head
// - based on current time and rotation speed
#define SECUNDERHEAD (nowus / USPERSEC % SECPERTRK)

// do the disk file I/O
static void *rliothread (void *dummy)
{
    if (debug > 1) fprintf (stderr, "z11rl: thread started\n");

    int logrlfd = -1; // open ("/tmp/logrl.bin", O_RDONLY);
    uint32_t logrlpos = 0;

    while (true) {

        // wait for pdp to clear rlcsr[07]
        z11p->waitint (ZGINT_RL);

        // block disk from being unloaded from under us
        LOCKIT;

        // see if command from pdp waiting to be processed
        uint16_t rlcs = RFLD (1, RL1_RLCS);
        if (! (rlcs & 0x0080)) {
            uint16_t rlba = RFLD (1, RL1_RLBA) & 0xFFFEU;
            uint16_t rlda = RFLD (2, RL2_RLDA);
            uint16_t rlmp = RFLD (2, RL2_RLMP1);
            uint16_t rlmp2, rlmp3;

            if (debug > 0) fprintf (stderr, "z11rl: start RLCS=%06o RLBA=%06o RLDA=%06o RLMP=%06o\n", rlcs, rlba, rlda, rlmp);

            uint32_t rlxba = ((rlcs & 0x30) << 12) + rlba;

            rlcs &= 0xC3FFU;                                            // clear error bits<13:10> in RLCS
                                                                        // rl11.v should have cleared them but do it here too

            bool fastio = (rlat[5] & RL5_FAST) != 0;                    // skip any sleeping

            uint16_t drivesel = (rlcs >> 8) & 3;
            int fd = fds[drivesel];
            ZWR(rlat[4], ZRD(rlat[4]) & ~ (RL4_DRDY0 << drivesel));     // clear drive ready bit for selected drive while I/O in progress
            ShmRLDrive *dr = &shmrl->drives[drivesel];
            dr->ready = false;

            struct timespec nowts;
            if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
            uint64_t nowus = (nowts.tv_sec * 1000000ULL) + (nowts.tv_nsec / 1000);

            uint32_t seekdelay = (seekdoneats[drivesel] > nowus) ? seekdoneats[drivesel] - nowus : 0;
            uint32_t rotndelay = ((rlda & 63) + SECPERTRK - SECUNDERHEAD) % SECPERTRK;
            uint32_t xferdelay = (WRDPERSEC + 65535 - rlmp) / WRDPERSEC * USPERSEC - (NSPERDMA * (65536 - rlmp)) / 1000;
            uint32_t totldelay = seekdelay + rotndelay + xferdelay;
            totldelay = (totldelay > USLEEPOV) ? totldelay - USLEEPOV : 0;

            if (debug > 1) fprintf (stderr, "z11rl:       xba=%06o dsel=%u fd=%d\n", rlxba, drivesel, fd);

            switch ((rlcs >> 1) & 7) {

                // NOP
                case 0: {
                    if (debug > 0) fprintf (stderr, "z11rl:   nop\n");
                    break;
                }

                // WRITE CHECK
                case 1: {
                    if (! fastio) usleep (totldelay);

                    if (debug > 0) fprintf (stderr, "z11rl: [%u]   writecheck wc=%06o da=%06o xba=%06o\n", drivesel, 65536 - rlmp, rlda, rlxba);

                    if (dr->lastposn != (rlda & 0xFFC0U)) {
                        if (debug > 0) fprintf (stderr, "z11rl: [%u]       latestposition=%06o rlda=%06o\n", drivesel, dr->lastposn, rlda);
                        goto hnferr;
                    }
                    uint16_t trk = (rlda >> 6) & 1;
                    uint16_t cyl =  rlda >> 7;

                    do {
                        uint16_t sec = rlda & 63;
                        if (sec >= SECPERTRK) {
                            if (debug > 0) fprintf (stderr, "z11rl: [%u]       sec=%02o\n", drivesel, sec);
                            goto hnferr;
                        }

                        uint16_t buf[WRDPERSEC];
                        uint32_t off = (((uint32_t) cyl * TRKPERCYL + trk) * SECPERTRK + sec) * sizeof buf;
                        int rc = pread (fd, buf, sizeof buf, off);
                        if (rc < 0) {
                            fprintf (stderr, "z11rl: [%u] error reading at %u: %m\n", drivesel, off);
                            goto opierr;
                        }
                        if (rc < (int) sizeof buf) {
                            fprintf (stderr, "z11rl: [%u] only read %d of %d bytes at %u\n", drivesel, rc, (int) sizeof buf, off);
                            goto opierr;
                        }
                        rlda ++;

                        z11p->dmalock ();
                        uint32_t rd = 0;
                        int i;
                        for (i = 0; i < WRDPERSEC; i ++) {
                            uint16_t memword;
                            rd = z11p->dmareadlocked (rlxba, &memword);
                            if (rd != 0) break;
                            if (memword != buf[i]) break;
                            rlxba = (rlxba + 2) & 0x3FFFE;
                            if (++ rlmp == 0) break;
                        }
                        z11p->dmaunlk ();
                        if (rd & KY3_DMATIMO) goto nxmerr;
                        if (rd & KY3_DMAPERR) goto mperr;
                        if (rd != 0) ABORT ();
                        if ((i < WRDPERSEC) && (rlmp != 0)) goto wckerr;
                    } while (rlmp != 0);
                    break;
                }

                // GET STATUS
                case 2: {
                    if (debug > 0) fprintf (stderr, "z11rl: [%u]   getstatus\n", drivesel);
                    if (rlda & 8) {                     // reset
                        vcs[drivesel] = false;
                    }
                    rlmp = 0x000DU;                     // 0 0 wl 0 -- 0 wge vc 0 -- 1 hs dt ho 1 0 0 0
                    if (! dr->rl01) rlmp |= 0x0080U;    // is an RL02
                    if (dr->readonly) rlmp |= 0x2000U;  // write locked
                    if (vcs[drivesel]) rlmp |= 0x0200U; // volume check
                    rlmp |= dr->lastposn & 0x0040U;     // head select
                    if (fd >= 0)       rlmp |= 0x0010U; // heads out over disk
                    if (seekdelay > 0) rlmp |= 0x0004U; // seeking
                    else if (fd >= 0)  rlmp |= 0x0005U; // 'lock on'
                    break;
                }

                // SEEK
                case 3: {
                    if (fd < 0) goto opierr;

                    if (debug > 0) fprintf (stderr, "z11rl: [%u]   seek da=%06o\n", drivesel, rlda);

                    int32_t newcyl = dr->lastposn >> 7;
                    if (rlda & 4) newcyl += rlda >> 7;
                             else newcyl -= rlda >> 7;
                    if (debug > 1) fprintf (stderr, "z11rl: [%u]       newcyl=%d\n", drivesel, newcyl);
                    if (newcyl < 0) newcyl = 0;
                    if (newcyl > NCYLS - 1) newcyl = NCYLS - 1;

                    dr->lastposn = (newcyl << 7) | ((rlda << 2) & 0x40);

                    int oldsdaint = (int) seekdoneats[drivesel];
                    seekdoneats[drivesel] = nowus + (fastio ? 0 : (rlda >> 7) * USPERCYL + SETTLEUS);
                    if ((int) seekdoneats[drivesel] == oldsdaint) seekdoneats[drivesel] ++;
                    if (futex ((int *)&seekdoneats[drivesel], FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
                    break;
                }

                // READ HEADER
                case 4: {
                    if (debug > 0) fprintf (stderr, "z11rl: [%u]   readheader\n", drivesel);
                    if (! fastio) usleep (seekdelay);
                    nowus += seekdelay;
                    rlmp   = dr->lastposn | SECUNDERHEAD;
                    rlmp2  = 0;
                    rlmp3  = 0;
                    goto rhddone;
                }

                // WRITE DATA
                case 5: {
                    if (! fastio) usleep (totldelay);

                    if (debug > 0) fprintf (stderr, "z11rl: [%u]   writedata wc=%06o da=%06o xba=%06o\n", drivesel, 65536 - rlmp, rlda, rlxba);

                    if (dr->lastposn != (rlda & 0xFFC0U)) {
                        if (debug > 0) fprintf (stderr, "z11rl: [%u]       latestposition=%06o rlda=%06o\n", drivesel, dr->lastposn, rlda);
                        goto hnferr;
                    }
                    uint16_t trk = (rlda >> 6) & 1;
                    uint16_t cyl =  rlda >> 7;

                    if (logrlfd >= 0) {
                        uint16_t hdr[3];
                        int rc = pread (logrlfd, hdr, sizeof hdr, logrlpos);
                        if (rc < (int) sizeof hdr) {
                            fprintf (stderr, "z11rl: [%u] only read %d of %d bytes from logrl\n", drivesel, rc, (int) sizeof hdr);
                            ABORT ();
                        }
                        uint16_t exp[3] = { ('W' << 8) | 'R', BLKNUM (rlda), SECCNT (rlmp) };
                        if (memcmp (hdr, exp, 6) != 0) {
                            fprintf (stderr, "z11rl: [%u] %08X got %06o %06o %06o expect %06o %06o %06o\n", drivesel, logrlpos, hdr[0], hdr[1], hdr[2], exp[0], exp[1], exp[2]);
                            ABORT ();
                        }
                        logrlpos += sizeof hdr + (uint32_t) hdr[2] * 256;
                    }

                    do {
                        uint16_t sec = rlda & 63;
                        if (sec >= SECPERTRK) {
                            if (debug > 0) fprintf (stderr, "z11rl: [%u]       sec=%02o\n", drivesel, sec);
                            goto hnferr;
                        }

                        z11p->dmalock ();
                        uint32_t xbasave = rlxba;
                        uint16_t buf[WRDPERSEC];
                        uint32_t rd = 0;
                        for (int i = 0; i < WRDPERSEC; i ++) {
                            rd = z11p->dmareadlocked (rlxba, &buf[i]);
                            if (rd != 0) break;
                            rlxba = (rlxba + 2) & 0x3FFFE;
                            if (++ rlmp == 0) {
                                memset (&buf[i+1], 0, (WRDPERSEC - i - 1) * sizeof buf[0]);
                                break;
                            }
                        }
                        z11p->dmaunlk ();
                        if (rd & KY3_DMATIMO) goto nxmerr;
                        if (rd & KY3_DMAPERR) goto mperr;
                        if (rd != 0) ABORT ();

                        uint32_t off = (((uint32_t) cyl * TRKPERCYL + trk) * SECPERTRK + sec) * sizeof buf;
                        int rc = pwrite (fd, buf, sizeof buf, off);
                        if (rc < 0) {
                            fprintf (stderr, "z11rl: [%u] error writing at %u: %m\n", drivesel, off);
                            goto opierr;
                        }
                        if (rc < (int) sizeof buf) {
                            fprintf (stderr, "z11rl: [%u] only wrote %d of %d bytes at %u\n", drivesel, rc, (int) sizeof buf, off);
                            goto opierr;
                        }
                        if (debug > 2) dumpbuf (drivesel, buf, off, xbasave, "write");
                        rlda ++;
                    } while (rlmp != 0);
                    break;
                }

                // READ DATA
                case 6: {
                    if (! fastio) usleep (totldelay);

                    if (debug > 0) fprintf (stderr, "z11rl: [%u]   readdata wc=%06o da=%06o xba=%06o\n", drivesel, 65536 - rlmp, rlda, rlxba);

                    if (dr->lastposn != (rlda & 0xFFC0U)) {
                        if (debug > 0) fprintf (stderr, "z11rl: [%u]       latestposition=%06o rlda=%06o\n", drivesel, dr->lastposn, rlda);
                        goto hnferr;
                    }
                    uint16_t trk = (rlda >> 6) & 1;
                    uint16_t cyl =  rlda >> 7;

                    if (logrlfd >= 0) {
                        uint16_t hdr[3];
                        int rc = pread (logrlfd, hdr, sizeof hdr, logrlpos);
                        if (rc < (int) sizeof hdr) {
                            fprintf (stderr, "z11rl: [%u] only read %d of %d bytes from logrl\n", drivesel, rc, (int) sizeof hdr);
                            ABORT ();
                        }
                        uint16_t exp[3] = { ('R' << 8) | 'D', BLKNUM (rlda), SECCNT (rlmp) };
                        if (memcmp (hdr, exp, 6) != 0) {
                            fprintf (stderr, "z11rl: [%u] %08X got %06o %06o %06o expect %06o %06o %06o\n", drivesel, logrlpos, hdr[0], hdr[1], hdr[2], exp[0], exp[1], exp[2]);
                            ABORT ();
                        }
                        logrlpos += sizeof hdr + (uint32_t) hdr[2] * 256;
                    }

                    do {
                        uint16_t sec = rlda & 63;
                        if (sec >= SECPERTRK) {
                            if (debug > 0) fprintf (stderr, "z11rl: [%u]       sec=%02o\n", drivesel, sec);
                            goto hnferr;
                        }

                        uint16_t buf[WRDPERSEC];
                        uint32_t off = (((uint32_t) cyl * TRKPERCYL + trk) * SECPERTRK + sec) * sizeof buf;
                        int rc = pread (fd, buf, sizeof buf, off);
                        if (rc < 0) {
                            fprintf (stderr, "z11rl: [%u] error reading at %u: %m\n", drivesel, off);
                            goto opierr;
                        }
                        if (rc < (int) sizeof buf) {
                            fprintf (stderr, "z11rl: [%u] only read %d of %d bytes at %u\n", drivesel, rc, (int) sizeof buf, off);
                            goto opierr;
                        }
                        if (debug > 2) dumpbuf (drivesel, buf, off, rlxba, "read");
                        rlda ++;

                        z11p->dmalock ();
                        bool ok = true;
                        for (int i = 0; i < WRDPERSEC; i ++) {
                            ok = z11p->dmawritelocked (rlxba, buf[i]);
                            if (! ok) break;
                            rlxba = (rlxba + 2) & 0x3FFFE;
                            if (++ rlmp == 0) break;
                        }
                        z11p->dmaunlk ();
                        if (! ok) goto nxmerr;
                    } while (rlmp != 0);
                    break;
                }

                // READ DATA WITHOUT HEADER CHECK
                case 7: {
                    if (debug > 0) fprintf (stderr, "z11rl: [%u]   read data without header check\n", drivesel);
                    goto opierr;
                }
            }
            goto alldone;
        opierr:;
            rlcs |= 1U << 10;                       // operation incomplete
            goto alldone;
        wckerr:;
            rlcs |= 2U << 10;                       // write check error
            goto alldone;
        hnferr:;
            rlcs |= 5U << 10;                       // header not found
            goto alldone;
        nxmerr:;
            rlcs |= 8U << 10;                       // non-existant memory
            goto alldone;
        mperr:;
            rlcs |= 9U << 10;                       // memory parity error
        alldone:;
            rlmp3 = rlmp2 = rlmp;
        rhddone:;

            // merge top bus address bits and set done bit
            rlcs  = (rlcs & ~ 0x30) | 0x80 | ((rlxba >> 12) & 0x30);

            // update drive ready before updating RLCS so rl11.v will fill in RLCS<00> correctly
            if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
            nowus = (nowts.tv_sec * 1000000ULL) + (nowts.tv_nsec / 1000);
            uint16_t drdy = ((fd >= 0) && (seekdoneats[drivesel] <= nowus)) ? RL4_DRDY0 : 0;
            ZWR(rlat[4], (ZRD(rlat[4]) & ~ (RL4_DRDY0 << drivesel)) | (drdy << drivesel));
            dr->ready = drdy != 0;

            // update RLMPs, RLDA, then RLBA and RLCS
            ZWR(rlat[3], ((uint32_t) rlmp3 << 16) | rlmp2);
            ZWR(rlat[2], ((uint32_t) rlmp  << 16) | rlda);
            ZWR(rlat[1], ((uint32_t) rlxba << 16) | rlcs);
            if (debug > 0) fprintf (stderr, "z11rl: [%u]  done RLCS=%06o RLxBA=%06o RLDA=%06o RLMP=%06o %06o %06o\n",
                    drivesel, rlcs, rlxba, rlda, rlmp, rlmp2, rlmp3);
        }
        UNLKIT;
    }
}

// wait for changes in seekdoneats[drivesel]
// when time is up, set RL4_DRDY<drivesel> and clear seekdoneats[drivesel]
static void *timerthread (void *dsptr)
{
    int drivesel = (int)(long)dsptr;

    while (true) {

        // see if file open and seek in progress
        LOCKIT;
        struct timespec *tsptr = NULL;
        struct timespec timeout;
        uint64_t doneat = seekdoneats[drivesel];
        if ((fds[drivesel] >= 0) && (doneat != 0)) {
            struct timespec nowts;
            if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
            uint64_t nowus = (nowts.tv_sec * 1000000ULL) + (nowts.tv_nsec / 1000);

            // see if still waiting for seek to complete
            // if so, set up timeout for remaining delta
            if (doneat > nowus) {
                memset (&timeout, 0, sizeof timeout);
                timeout.tv_sec  = (doneat - nowus) / 1000000;
                timeout.tv_nsec = (doneat - nowus) % 1000000 * 1000;
                tsptr = &timeout;
            }

            // seek complete, mark drive ready
            // then leave tsptr NULL to wait indefinitely for next seek to begin
            else {
                ZWR(rlat[4], ZRD(rlat[4]) | (RL4_DRDY0 << drivesel));
                seekdoneats[drivesel] = doneat = 0;
            }
        }

        // wait for current seek complete or for another seek to be started
        UNLKIT;
        int rc = futex ((int *)&seekdoneats[drivesel], FUTEX_WAIT, (int)doneat, tsptr, NULL, 0);
        if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR) && (errno != ETIMEDOUT)) ABORT ();
    }
}

// process load/unload commands
static void procommands ()
{
    while (true) {

        // check for load/unload commands in shared memory waiting to be processed
        LOCKIT;
        int cmd = shmrl->command;
        switch (cmd) {

            // something requesting file be loaded
            case SHMRLCMD_LOAD+0 ... SHMRLCMD_LOAD+3: {
                int driveno = cmd - SHMRLCMD_LOAD;
                ShmRLDrive *dr = &shmrl->drives[driveno];
                UNLKIT;
                unloadfile (driveno);
                int rc = loadfile (driveno);
                LOCKIT;
                if (rc >= 0) {
                    shmrl->negerr = 0;
                } else {
                    dr->filename[0] = 0;
                    shmrl->negerr = rc;
                }
                shmrl->command = SHMRLCMD_DONE;
                if (futex (&shmrl->command, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
                break;
            }

            // something requesting file be unloaded
            case SHMRLCMD_UNLD+0 ... SHMRLCMD_UNLD+3: {
                int driveno = cmd - SHMRLCMD_UNLD;
                ShmRLDrive *dr = &shmrl->drives[driveno];
                dr->filename[0] = 0;
                if (++ dr->fnseq == 0) ++ dr->fnseq; // 0 reserved for initial condition
                fns[driveno][0] = 0;
                unloadfile (driveno);
                shmrl->command = SHMRLCMD_DONE;
                if (futex (&shmrl->command, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
                break;
            }

            // nothing to do, wait
            default: {
                UNLKIT;
                int rc = futex (&shmrl->command, FUTEX_WAIT, cmd, NULL, NULL, 0);
                if ((rc < 0) && (errno != EINTR)) ABORT ();
                LOCKIT;
            }
        }
        UNLKIT;
    }
}

// load file in a drive
//  input:
//   diskno = disk number 0..3
//  output:
//   returns < 0: errno
//          else: successful
static int loadfile (uint16_t drivesel)
{
    ShmRLDrive *dr = &shmrl->drives[drivesel];
    bool readwrite = ! dr->readonly;
    char const *filenm = dr->filename;

    int fnlen = strlen (filenm);
         if ((fnlen >= 5) && (strcasecmp (filenm + fnlen - 5, ".rl01") == 0)) dr->rl01 = true;
    else if ((fnlen >= 5) && (strcasecmp (filenm + fnlen - 5, ".rl02") == 0)) dr->rl01 = false;
    else {
        fprintf (stderr, "z11rl: [%u] error decoding %s: name ends with neither .rl01 nor .rl02\n", drivesel, filenm);
        return -EBADF;
    }

    int fd = open (filenm, readwrite ? O_RDWR | O_CREAT : O_RDONLY, 0666);
    if (fd < 0) {
        fd = - errno;
        fprintf (stderr, "z11rl: [%u] error opening %s: %m\n", drivesel, filenm);
        return fd;
    }

    // don't let it be put in more than one drive if either is write-enabled
    struct flock flockit;
lockit:;
    memset (&flockit, 0, sizeof flockit);
    flockit.l_type   = readwrite ? F_WRLCK : F_RDLCK;
    flockit.l_whence = SEEK_SET;
    flockit.l_len    = 4096;
    if (fcntl (fd, F_OFD_SETLK, &flockit) < 0) {
        int rc = - errno;
        ASSERT (rc < 0);
        if (((errno == EACCES) || (errno == EAGAIN)) && (fcntl (fd, F_GETLK, &flockit) >= 0)) {
            if (flockit.l_type == F_UNLCK) goto lockit;
            fprintf (stderr, "z11rl: [%u] error locking %s: locked by pid %d\n", drivesel, filenm, (int) flockit.l_pid);
        } else {
            fprintf (stderr, "z11rl: [%u] error locking %s: %m\n", drivesel, filenm);
        }
        close (fd);
        return rc;
    }

    // extend it to full size
    if (readwrite && (ftruncate (fd, NSECS * WRDPERSEC * 2) < 0)) {
        int rc = - errno;
        ASSERT (rc < 0);
        fprintf (stderr, "z11rl: [%u] error extending %s: %m\n", drivesel, filenm);
        close (fd);
        return rc;
    }

    // all is good
    fprintf (stderr, "z11rl: [%u] loaded read%s file %s\n", drivesel, (readwrite ? "/write" : "-only"), filenm);
    fds[drivesel] = fd;
    if (strcmp (fns[drivesel], filenm) != 0) {
        strcpy (fns[drivesel], filenm);
        vcs[drivesel] = true;
        dr->lastposn  = 0;
    }
    return fd;
}

// close file loaded on a disk drive
static void unloadfile (uint16_t drivesel)
{
    close (fds[drivesel]);
    fds[drivesel] = -1;
}

static void dumpbuf (uint16_t drivesel, uint16_t const *buf, uint32_t off, uint32_t xba, char const *func)
{
    fprintf (stderr, "  drv=%u off=%08o xba=%06o  %s\n", drivesel, off, xba, func);
    for (int i = 0; i < 128; i += 16) {
        fprintf (stderr, "   ");
        for (int j = 16; -- j >= 0;) {
            fprintf (stderr, " %06o", buf[i+j]);
        }
        fprintf (stderr, " : %03o\n", i);
    }
}
