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

// Performs RP04/RP06 disk I/O for the PDP-11 Zynq I/O board
// Runs as a background daemon when a file is loaded in a drive
// ...either with z11ctrl rhload command or GUI screen

//  ./z11rh [-reset]

// page references rjp04 disk subsystem maint, feb 75

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "futex.h"
#include "shmms.h"
#include "z11defs.h"
#include "z11util.h"

#define SECPERTRKM1  (SECPERTRK - 1)
#define QTRSECTIMEM1 (AVGROTUS*100/4/SECPERTRK)

#define NCYLS_RP04 411
#define NCYLS_RP06 815
#define NCYLS (dr->rl01?NCYLS_RP04:NCYLS_RP06)
#define SECPERTRK 22
#define TRKPERCYL 19
#define NSECS (NCYLS*TRKPERCYL*SECPERTRK)
#define WRDPERSEC 256       // words per sector

#define USPERCYL ((100000-15000)/(NCYLS-1))     // usec per cyl = (full seek - one seek) / (num cyls - 1)
#define SEEKONEUS 7333                          // seek one cylinder
#define SEEKEXTUS 170                           // seek extra cylinder
#define AVGROTUS 16667                          // average rotation usec
#define USPERWRD 4                              // usec per word
#define USPERSEC (AVGROTUS*2/SECPERTRK)         // usec per sector

#define NSPERDMA 2350
#define USLEEPOV 72

#define RH1_RPCS1  0x0000FFFFU
#define RH1_RPWC   0xFFFF0000U
#define RH2_RPBA   0x0000FFFFU
#define RH2_RPDA   0xFFFF0000U
#define RH3_RPCS2  0x0000FFFFU
#define RH3_RPDS   0xFFFF0000U
#define RH4_RPER1  0x0000FFFFU
#define RH4_RPAS   0xFFFF0000U
#define RH5_RPLA   0x0000FFFFU
#define RH5_RPDB   0xFFFF0000U
#define RH6_QTRSECTIMEM1 0x000FFFFFU
#define RH6_SECPERTRKM1  0x0FF00000U
#define RH6_ARMDS  0xE0000000U
#define RH7_RPDT   0x0000FFFFU
#define RH7_RPSN   0xFFFF0000U
#define RH8_RPDC   0x0000FFFFU
#define RH8_RPCC   0xFFFF0000U
#define RH9_FASTIO 0x40000000U
#define RH9_ENABLE 0x80000000U

#define RH1_RPCS10  (RH1_RPCS1 & - RH1_RPCS1)
#define RH1_RPWC0   (RH1_RPWC & - RH1_RPWC)
#define RH2_RPBA0   (RH2_RPBA & - RH2_RPBA)
#define RH2_RPDA0   (RH2_RPDA & - RH2_RPDA)
#define RH3_RPCS20  (RH3_RPCS2 & - RH3_RPCS2)
#define RH3_RPDS0   (RH3_RPDS & - RH3_RPDS)
#define RH4_RPER10  (RH4_RPER1 & - RH4_RPER1)
#define RH4_RPAS0   (RH4_RPAS & - RH4_RPAS)
#define RH5_RPLA0   (RH5_RPLA & - RH5_RPLA)
#define RH5_RPDB0   (RH5_RPDB & - RH5_RPDB)
#define RH6_QTRSECTIMEM10 (RH6_QTRSECTIMEM1 & - RH6_QTRSECTIMEM1)
#define RH6_SECPERTRKM10  (RH6_SECPERTRKM1 & - RH6_SECPERTRKM1)
#define RH6_ARMDS0  (RH6_ARMDS & - RH6_ARMDS)
#define RH7_RPDT0   (RH7_RPDT & - RH7_RPDT)
#define RH7_RPSN0   (RH7_RPSN & - RH7_RPSN)
#define RH8_RPDC0   (RH8_RPDC & - RH8_RPDC)
#define RH8_RPCC0   (RH8_RPCC & - RH8_RPCC)

#define ARMDS(n) ((n) * RH6_ARMDS0 | SECPERTRKM1 * RH6_SECPERTRKM10 | QTRSECTIMEM1 * RH6_QTRSECTIMEM10)

static bool vvs[8];
static char fns[8][SHMMS_FNSIZE];
static int debug;
static int fds[8];
static uint64_t seekdoneats[8];

#define LOCKIT shmms_svr_mutexlock(shmms)
#define UNLKIT shmms_svr_mutexunlk(shmms)

static ShmMS *shmms;
static uint16_t wrdbuf[65536];
static uint32_t volatile *rhat;

static void *rhiothread (void *dummy);
static void docommand (int drsel, uint16_t rpcs1);
static int setdrivetype (void *param, int drsel);
static int fileloaded (void *param, int drsel, int fd);
static int writebadblocks (ShmMSDrive *dr, int fd);
static uint16_t getdrivestat (int drsel);
static uint16_t getdrivesern (int drsel);
static uint16_t getdrivetype (int drsel);
static void startseek (int drsel, uint16_t cylno, uint32_t seekus);
static uint32_t blocknumber ();
static void updatecounts (uint16_t rpwc, uint32_t rpba, uint32_t wrdcnt, uint32_t blknum);
static void *timerthread (void *dsptr);
static void unloadfile (void *param, int drsel);
static void unloadfileata (int drsel, uint16_t ata);

int main (int argc, char **argv)
{
    memset (fds, -1, sizeof fds);

    bool resetit = (argc > 1) && (strcasecmp (argv[1], "-reset") == 0);

    // access fpga register set for the RH-11 controller
    // lock it so we are only process accessing it
    z11page = new Z11Page ();
    rhat = z11page->findev ("RH", NULL, NULL, true, false);

    // initialize shared memory - contains filenames and load/unload info
    shmms = shmms_svr_initialize (resetit, SHMMS_NAME_RH, "z11rh");
    shmms->ndrives = 8;

    // ...and no drives are ready or faulted
    uint32_t fastio = ZRD(rhat[9]) & RH9_FASTIO;
    ZWR(rhat[2], 0 * RH2_RPDA0 | 0 * RH2_RPBA0);
    ZWR(rhat[4], 0 * RH4_RPAS0 | 0 * RH4_RPER10);
    ZWR(rhat[5], 0 * RH5_RPDB0);
    for (int i = 0; i < 8; i ++) {
        ZWR(rhat[6], ARMDS (i));
        ZWR(rhat[1], 0 * RH1_RPWC0 | 0 * RH1_RPCS10);
        ZWR(rhat[3], 0 * RH3_RPDS0 | 0 * RH3_RPCS2);
        ZWR(rhat[7], 0 * RH7_RPSN0 | 0 * RH7_RPDT0);
        ZWR(rhat[8], 0 * RH8_RPCC0 | 0 * RH8_RPDC0);
    }
    UNLKIT;

    // enable board to process io instructions
    ZWR(rhat[9], RH9_ENABLE | fastio);

    debug = 0;
    char const *dbgenv = getenv ("z11rh_debug");
    if (dbgenv != NULL) debug = atoi (dbgenv);

    pthread_t rhtid;
    int rc = pthread_create (&rhtid, NULL, rhiothread, NULL);
    if (rc != 0) ABORT ();
    for (int i = 0; i < 8; i ++) {
        rc = pthread_create (&rhtid, NULL, timerthread, (void *)(long)i);
        if (rc != 0) ABORT ();
    }

    shmms_svr_proccmds (shmms, "z11rh", setdrivetype, fileloaded, unloadfile, NULL);

    return 0;
}

// get what sector is currently under the head
// - based on current time and rotation speed
#define SECUNDERHEAD ((ZRD(rhat[5]) & RH5_RPLA) / RH5_RPLA0 / 64)

// do the disk file I/O
static void *rhiothread (void *dummy)
{
    while (true) {

        // wait for pdp to clear rpcsr1[07] (RDY) or set rpcs2[05] (CLR)
        z11page->waitint (ZGINT_RH);

        // block disk from being unloaded from under us
        LOCKIT;

        // check for 'clear controller' command
        uint16_t rpcs2 = (ZRD(rhat[3]) & RH3_RPCS2) / RH3_RPCS20;
        if (debug > 0) fprintf (stderr, "z11rh: rpcs2=%06o\n", rpcs2);
        if (rpcs2 & 040) {

            // clear RPDA, RPBA
            ZWR(rhat[2], 0 * RH2_RPDA | 0 * RH2_RPBA);

            // go through per-drive clears
            for (int drsel = 0; drsel < 8; drsel ++) {

                // select per-drive registers
                ZWR(rhat[6], ARMDS (drsel));

                // cancel any seek in progress
                seekdoneats[drsel] = 0;

                // preserve RPWC
                // preserve RPCS1<IE>, set RPCS1<DVA> (per drive), clear rest of RPCS1
                ZWR(rhat[1], (ZRD(rhat[1]) & (~ RH1_RPCS1 | 0100 * RH1_RPCS10)) | (04000 * RH1_RPCS10));

                // update RPDS, RPCS2 (also clears RPCS2<CLR> 'clear controller' command bit)
                uint16_t rpds = getdrivestat (drsel);
                ZWR(rhat[3], rpds * RH3_RPDS0 | 0 * RH3_RPCS20);

                // clear RPER1
                ZWR(rhat[4], 0 * RH4_RPER1);

                // update RPSN, RPDT
                uint16_t rpsn = getdrivesern (drsel);
                uint16_t rpdt = getdrivetype (drsel);
                ZWR(rhat[7], rpsn * RH7_RPSN0 | rpdt * RH7_RPDT0);

                // clear RPDC
                ZWR(rhat[8], ZRD(rhat[8]) & ~ RH8_RPDC);
            }

            // set RPCS1<RDY> indicating clear is complete, ready to accept command
            ZWR(rhat[1], ZRD(rhat[1]) | 0200 * RH1_RPCS10);
        } else {

            // see if command from pdp waiting to be processed (RPCS1<RDY> is clear)
            uint16_t rpcs1 = (ZRD(rhat[1]) & RH1_RPCS1) / RH1_RPCS10;
            if (debug > 0) fprintf (stderr, "z11rh: rpcs1=%06o\n", rpcs1);
            if (! (rpcs1 & 0200)) {

                // do i/o for any drive with a GO bit set
                for (int drsel = 0; drsel < 8; drsel ++) {

                    // select drive-specific registers to access
                    ZWR(rhat[6], ARMDS (drsel));

                    // check GO bit for drive, if set, do command
                    rpcs1 = (ZRD(rhat[1]) & RH1_RPCS1) / RH1_RPCS10;
                    if (rpcs1 & 1) {
                        docommand (drsel, rpcs1);
                    }
                }

                // set RPCS1<RDY> indicating controller ready to accept new command
                ZWR(rhat[1], ZRD(rhat[1]) | 0200 * RH1_RPCS10);
            }
        }

        UNLKIT;
    }
}

// do command for the given drive
//  input:
//   rpcs1 = RPCS1 contents specific to selected drive (GO bit known to be set)
//   RH6_ARMDS = same as drsel
//   GO bit set
//   DRY (drive ready) clear
//   RDY (ctrlr ready) clear
//   data transfer command: MCPE,DLT,TRE,PE,NED,NXM,PGE,MXF,MDPE clear
//  output:
//   GO bit clear
//   DRY usually set, but still clear for seek
//   RDY still clear
static void docommand (int drsel, uint16_t rpcs1)
{
    ShmMSDrive *dr = &shmms->drives[drsel];

    uint32_t rh8  = ZRD(rhat[8]);
    uint32_t rpdc = (rh8 & RH8_RPDC) / RH8_RPDC0;
    uint32_t rpcc = (rh8 & RH8_RPCC) / RH8_RPCC0;
    uint32_t ncyl = (rpcc > rpdc) ? (rpcc - rpdc) : (rpdc - rpcc);
    uint32_t seekus = (ncyl == 0) ? 0 : SEEKONEUS + SEEKEXTUS * (ncyl - 1);

    uint32_t rpba = ((rpcs1 << 8) & 0600000) + (ZRD(rhat[2]) & RH2_RPBA) / RH2_RPBA0;
    uint32_t rpbi = (ZRD(rhat[3]) & 8 * RH3_RPCS20) ? 0 : 2;

    if (debug > 0) fprintf (stderr, "docommand: [%d] rpcs1=%06o rpdc=%06o rpba=%06o\n", drsel, rpcs1, rpdc, rpba);

    switch ((rpcs1 >> 1) & 31) {

        // nop
        case  0: {
            break;
        }

        // unload
        case  1: {
            // close file, update drive status, don't set ATA
            unloadfileata (drsel, 0);
            break;
        }

        // seek
        case  2: {
            startseek (drsel, rpdc, seekus);
            goto clrgo;
        }

        // recalibrate - seek to cyl 0, take 500mS doing so (p44/v3-8)
        case  3: {
            startseek (drsel, 0, 500000);
            goto clrgo;
        }

        // drive clear
        case  4: {
            // clear ATA,ERR
            uint32_t rpds = getdrivestat (drsel);
            ZWR(rhat[3], (ZRD(rhat[3]) & ~ RH3_RPDS) | rpds * RH3_RPDS0);
            // clear RPER1
            ZWR(rhat[4], ZRD(rhat[4]) & ~ RH4_RPER1);
            break;
        }

        // release - treat as nop (we don't do dual controller)
        case  5: {
            break;
        }

        // offset command
        // return to centerline
        // - seek to current cylinder
        //   take 10mS doing so (p44/v3-8)
        case  6:
        case  7: {
            startseek (drsel, rpcc, 10000);
            goto clrgo;
        }

        // read-in-preset (p3-9)
        case  8: {

            // set volume valid
            vvs[drsel] = true;
            ZWR(rhat[3], ZRD(rhat[3]) | 0100 * RH3_RPDS0);

            // clear sector, track
            ZWR(rhat[2], ZRD(rhat[2]) & ~ RH2_RPDA);

            // clear desired cylinder
            ZWR(rhat[8], ZRD(rhat[8]) & ~ RH8_RPDC);
            break;
        }

        // pack acknowledge
        // - set volume valid
        case  9: {
            vvs[drsel] = true;
            ZWR(rhat[3], ZRD(rhat[3]) | 0100 * RH3_RPDS0);
            break;
        }

        // search command - synchronous seek and verify the sector exists
        case 12: {
            uint32_t blknum = blocknumber ();
            if (blknum == 0xFFFFFFFFU) goto invadderr;

            if (seekus > 0) usleep (seekus);
            ZWR(rhat[8], (rh8 & ~ RH8_RPCC) | (rpdc * RH8_RPCC0));
            dr->curposn = rpdc;

            // set ATA,DRY - attention, drive ready
            ZWR(rhat[3], ZRD(rhat[3]) | 0100200 * RH3_RPDS0);
            goto clrgo;
        }

        // write check data
        case 20: {
            uint32_t blknum = blocknumber ();
            if (blknum == 0xFFFFFFFFU) goto invadderr;

            if (seekus > 0) usleep (seekus);
            ZWR(rhat[8], (rh8 & ~ RH8_RPCC) | (rpdc * RH8_RPCC0));
            dr->curposn = rpdc;

            uint16_t rpwc   = (ZRD(rhat[1]) & RH1_RPWC) / RH1_RPWC0;
            uint32_t wrdcnt = 65536 - rpwc;

            int rc = pread (fds[drsel], wrdbuf, wrdcnt * 2, blknum * WRDPERSEC * 2);
            if (rc != (int) wrdcnt * 2) {
                if (rc < 0) {
                    fprintf (stderr, "z11rh: [%d] error reading at %u: %m\n", drsel, blknum);
                } else {
                    fprintf (stderr, "z11rh: [%d] only read %d bytes of %u at %u\n", drsel, rc, wrdcnt * 2, blknum);
                }
                goto filerror;
            }
            uint16_t *wrdpnt = wrdbuf;
            do {
                uint16_t word;
                if (z11page->dmaread (rpba, &word) != 0) {
                    updatecounts (rpwc, rpba, wrdpnt - wrdbuf, blknum);
                    goto dmaerror;
                }
                if (word != *(wrdpnt ++)) {
                    updatecounts (rpwc, rpba, wrdpnt - wrdbuf, blknum);
                    // set ATA,ERR,WCE - attention, error, write check error
                    ZWR(rhat[3], ZRD(rhat[3]) | 0140000 * RH3_RPDS0 | 0040000 * RH3_RPCS20);
                    goto alldone;
                }
                rpba += rpbi;
            } while (++ rpwc != 0);
            updatecounts (rpwc, rpba, wrdcnt, blknum);
            break;
        }

#if 000
        // write check header and data
        case 21: {
            break;
        }
#endif

        // write data
        case 24: {
            if (dr->readonly) {
                ZWR(rhat[4], ZRD(rhat[4]) | 04000 * RH4_RPER10);    // set WLE - write lock error
                goto miscerror;
            }

            uint32_t blknum = blocknumber ();
            if (blknum == 0xFFFFFFFFU) goto invadderr;

            if (seekus > 0) usleep (seekus);
            ZWR(rhat[8], (rh8 & ~ RH8_RPCC) | (rpdc * RH8_RPCC0));
            dr->curposn = rpdc;

            uint16_t rpwc   = (ZRD(rhat[1]) & RH1_RPWC) / RH1_RPWC0;
            uint32_t wrdcnt = 65536 - rpwc;

            uint16_t *wrdpnt = wrdbuf;
            do {
                if (z11page->dmaread (rpba, (wrdpnt ++)) != 0) {
                    updatecounts (rpwc, rpba, wrdpnt - wrdbuf, blknum);
                    goto dmaerror;
                }
                rpba += rpbi;
            } while (++ rpwc != 0);

            int rc = pwrite (fds[drsel], wrdbuf, wrdcnt * 2, blknum * WRDPERSEC * 2);
            if (rc != (int) wrdcnt * 2) {
                if (rc < 0) {
                    fprintf (stderr, "z11rh: [%d] error writing at %u: %m\n", drsel, blknum);
                } else {
                    fprintf (stderr, "z11rh: [%d] only wrote %d bytes of %u at %u\n", drsel, rc, wrdcnt * 2, blknum);
                }
                goto filerror;
            }
            updatecounts (rpwc, rpba, wrdcnt, blknum);
            break;
        }

#if 000
        // write header and data
        case 25: {
            break;
        }
#endif

        // read data
        case 28: {
            uint32_t blknum = blocknumber ();
            if (blknum == 0xFFFFFFFFU) goto invadderr;

            if (seekus > 0) usleep (seekus);
            ZWR(rhat[8], (rh8 & ~ RH8_RPCC) | (rpdc * RH8_RPCC0));
            dr->curposn = rpdc;

            uint16_t rpwc   = (ZRD(rhat[1]) & RH1_RPWC) / RH1_RPWC0;
            uint32_t wrdcnt = 65536 - rpwc;

            int rc = pread (fds[drsel], wrdbuf, wrdcnt * 2, blknum * WRDPERSEC * 2);
            if (rc != (int) wrdcnt * 2) {
                if (rc < 0) {
                    fprintf (stderr, "z11rh: [%d] error reading at %u: %m\n", drsel, blknum);
                } else {
                    fprintf (stderr, "z11rh: [%d] only read %d bytes of %u at %u\n", drsel, rc, wrdcnt * 2, blknum);
                }
                goto filerror;
            }
            uint16_t *wrdpnt = wrdbuf;
            do {
                if (! z11page->dmawrite (rpba, *(wrdpnt ++))) {
                    updatecounts (rpwc, rpba, wrdpnt - wrdbuf, blknum);
                    goto dmaerror;
                }
                rpba += rpbi;
            } while (++ rpwc != 0);
            updatecounts (rpwc, rpba, wrdcnt, blknum);
            break;
        }

#if 000
        // read header and data
        case 29: {
            break;
        }
#endif

        // unknown
        default: {

            // ILF - illegal function
            ZWR(rhat[4], ZRD(rhat[4]) | 1 * RH4_RPER10);
            goto miscerror;
        }
    }
    goto alldone;

invadderr:;
    // set IAE - invalid address error
    ZWR(rhat[4], ZRD(rhat[4]) | 0004000 * RH4_RPER10);
    goto miscerror;

filerror:;
    // set ECH - ecc hard error
    ZWR(rhat[4], ZRD(rhat[4]) | 0000100 * RH4_RPER10);

miscerror:;
    // set ATA,ERR - attention, error
    ZWR(rhat[3], ZRD(rhat[3]) | 0140000 * RH3_RPDS0);
    goto alldone;

dmaerror:;
    // set ATA,ERR,NXM - attention, error, non-existant memory
    ZWR(rhat[3], ZRD(rhat[3]) | 0140000 * RH3_RPDS0 | 0004000 * RH3_RPCS20);

alldone:;
    // set DRY (drive ready)
    ZWR(rhat[3], ZRD(rhat[3]) | 0200 * RH3_RPDS0);

clrgo:;
    // clear drive-specific GO bit
    ZWR(rhat[1], ZRD(rhat[1]) & ~ (1 * RH1_RPCS10));
}

// compute RPDS contents
static uint16_t getdrivestat (int drsel)
{
    ShmMSDrive *dr = &shmms->drives[drsel];
    uint16_t stat = 000600;                 // DPR,DRY - drive present, drive ready
    if (seekdoneats[drsel] != 0) stat ^= 020200;  // PIP - position in progress
    if (fds[drsel] >= 0) stat |= 010000;    // MOL - medium online
    if (dr->readonly)    stat |= 004000;    // WRL - write locked
    if (vvs[drsel])      stat |= 000100;    // VV  - volume valid
    return stat;
}

// compute RPSN contents
static uint16_t getdrivesern (int drsel)
{
    return 010421 * (drsel + 1);
}

// compute RPDT contents
static uint16_t getdrivetype (int drsel)
{
    ShmMSDrive *dr = &shmms->drives[drsel];
    return dr->rl01 ? 020020   // PR04
                    : 020022;  // RP06
}

// start seek going on the selected drive
//  input:
//   drsel  = drive to do seek on
//   cylno  = cylinder to seek to
//   seekus = how long to take doing the seek
//  output:
//   queues seek to timerthread for this drive
//   any previously queued seek for the drive is cancelled
static void startseek (int drsel, uint16_t cylno, uint32_t seekus)
{
    ShmMSDrive *dr = &shmms->drives[drsel];
    dr->curposn = cylno;    // cyl after seek complete

    // seek time starts from here
    struct timespec nowts;
    if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
    uint64_t nowus = (nowts.tv_sec * 1000000ULL) + (nowts.tv_nsec / 1000);

    // set up when seek completes
    // overwrite any previously queued seek
    int doneat = (int) seekdoneats[drsel];
    seekdoneats[drsel] = nowus + seekus;
    if (doneat == (int) seekdoneats[drsel]) seekdoneats[drsel] ++;

    // wake timerthread()
    int rc = futex ((int *)&seekdoneats[drsel], FUTEX_WAKE, 1000000000, NULL, NULL, 0);
    if (rc < 0) ABORT ();

    // set PIP - position in progress
    ZWR(rhat[3], ZRD(rhat[3]) | 020000 * RH3_RPDS0);
}

// compute block number
//  input:
//   ARMDS = selected drive
//  returns:
//   0xFFFFFFFFU: invalid
//          else: block number
static uint32_t blocknumber ()
{
    uint32_t drsel = (ZRD(rhat[6]) & RH6_ARMDS) / RH6_ARMDS0;
    ShmMSDrive const *dr = &shmms->drives[drsel];
    uint32_t cylndr = (ZRD(rhat[8]) & RH8_RPDC) / RH8_RPDC0;
    if (cylndr >= NCYLS) return 0xFFFFFFFFU;
    uint16_t trksec = (ZRD(rhat[2]) & RH2_RPDA) / RH2_RPDA0;
    uint16_t sector = trksec & 0xFF;
    uint16_t track  = trksec >> 8;
    if (sector >= SECPERTRK) return 0xFFFFFFFFU;
    if (track  >= TRKPERCYL) return 0xFFFFFFFFU;
    return (cylndr * TRKPERCYL + track) * SECPERTRK + sector;
}

// update count registers after (possibly partial) dma transfer
//  input:
//   rpwc   = incremented 2s comp word count
//   rpba   = incremented 18-bit bus address
//   wrdcnt = wordcount transferred
//   blknum = original block number
static void updatecounts (uint16_t rpwc, uint32_t rpba, uint32_t wrdcnt, uint32_t blknum)
{
    // calculate what rpda, rpcc would be
    blknum += (wrdcnt + WRDPERSEC - 1) / WRDPERSEC;
    uint32_t sector = blknum % SECPERTRK;
    blknum /= SECPERTRK;
    uint32_t track  = blknum % TRKPERCYL;
    uint32_t rpcc   = blknum / TRKPERCYL;
    uint16_t rpda   = (track << 8) | sector;
    ZWR(rhat[2], (ZRD(rhat[2]) & ~ RH2_RPDA) | rpda * RH2_RPDA0);
    ZWR(rhat[8], (ZRD(rhat[8]) & ~ RH8_RPCC) | rpcc * RH8_RPCC0);

    // word count RPWC, bus address extension bits RPCS1<09:08>
    ZWR(rhat[1], (ZRD(rhat[1]) & ~ RH1_RPWC & ~ (01400 * RH1_RPCS10)) | rpwc * RH1_RPWC0 | ((rpba >> 8) & 01400) * RH1_RPCS10);

    // disk address RPDA, bus address RPBA
    ZWR(rhat[2], rpda * RH2_RPDA0 | (rpba & 0177777) * RH2_RPBA0);
}

// wait for changes in seekdoneats[drsel]
// when time is up, set DRY,ATA and clear seekdoneats[drsel]
static void *timerthread (void *dsptr)
{
    int drsel = (int)(long)dsptr;
    ShmMSDrive *dr = &shmms->drives[drsel];

    while (true) {

        // see if file open and seek in progress
        LOCKIT;
        struct timespec *tsptr = NULL;
        struct timespec timeout;
        uint64_t doneat = seekdoneats[drsel];
        if ((fds[drsel] >= 0) && (doneat != 0)) {
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

            // seek complete, update drive state
            // then leave tsptr NULL to wait indefinitely for next seek to begin
            else {
                ZWR(rhat[6], ARMDS (drsel));
                // update current cylinder
                uint32_t rh8  = ZRD(rhat[8]);
                uint32_t rpcc = dr->curposn;
                ZWR(rhat[8], (rh8 & ~ RH8_RPCC) | (rpcc * RH8_RPCC0));
                // clear PIP - position in progress; set ATA,DRY - attention, drive ready
                ZWR(rhat[3], (ZRD(rhat[3]) & ~ (0020000 * RH3_RPDS0)) | 0100200 * RH3_RPDS0);
                seekdoneats[drsel] = doneat = 0;
            }
        }

        // wait for current seek complete or for another seek to be started
        UNLKIT;
        int rc = futex ((int *)&seekdoneats[drsel], FUTEX_WAIT, (int)doneat, tsptr, NULL, 0);
        if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR) && (errno != ETIMEDOUT)) ABORT ();
    }
}

// about to load a file, set drive type according to file characteristics
static int setdrivetype (void *param, int drsel)
{
    ShmMSDrive *dr = &shmms->drives[drsel];
    char const *filenm = dr->filename;

    int fnlen = strlen (filenm);
         if ((fnlen >= 5) && (strcasecmp (filenm + fnlen - 5, ".rp04") == 0)) dr->rl01 = true;
    else if ((fnlen >= 5) && (strcasecmp (filenm + fnlen - 5, ".rp06") == 0)) dr->rl01 = false;
    else {
        fprintf (stderr, "z11rh: [%u] error decoding %s: name ends with neither .rp04 nor .rp06\n", drsel, filenm);
        return -EBADF;
    }

    return 0;
}

// file was successfully opened
// save the fd and mark drive online
static int fileloaded (void *param, int drsel, int fd)
{
    ShmMSDrive *dr = &shmms->drives[drsel];
    if (! dr->readonly) {
        struct stat statbuf;
        if (fstat (fd, &statbuf) < 0) return -1;
        if (ftruncate (fd, NSECS * WRDPERSEC * 2) < 0) return -1;
        if (S_ISREG (statbuf.st_mode) && (statbuf.st_size == 0)) {
            if (writebadblocks (dr, fd) < 0) return -1;
        }
    }
    fds[drsel] = fd;
    if (strcmp (fns[drsel], dr->filename) != 0) {
        strcpy (fns[drsel], dr->filename);
        vvs[drsel] = false;
        dr->curposn = 0;
    }

    // update RPDS and set ATA - attention active
    ZWR(rhat[6], ARMDS (drsel));
    ZWR(rhat[3], (rhat[3] & ~ RH3_RPDS) | getdrivestat (drsel) * RH3_RPDS0 | 0100000 * RH3_RPDS0);

    return 0;
}

// newly created file (it was empty)
// write a null badblock list so rsx is happy
// ref p1-9
static int writebadblocks (ShmMSDrive *dr, int fd)
{
    // get random bits for serial number
    uint16_t snbuf[2];
    int randfd = open ("/dev/urandom", O_RDONLY);
    if (randfd < 0) ABORT ();
    if (read (randfd, snbuf, sizeof snbuf) != (int) sizeof snbuf) ABORT ();
    close (randfd);

    // set up bad block file
    // written 4 sectors at a time
    uint16_t sectors0003[512];
    memset (sectors0003, -1, sizeof sectors0003);

    sectors0003[0] = snbuf[0] & 077777;
    sectors0003[1] = snbuf[1] & 077777;
    sectors0003[2] = 0;
    sectors0003[3] = 0;

    // fill last 40 sectors with repetition of those 4 sectors
    for (int i = NSECS - 40; i < NSECS; i += 4) {
        int rc = pwrite (fd, sectors0003, sizeof sectors0003, i * WRDPERSEC * 2);
        if (rc < 0) {
            fprintf (stderr, "z11rh: error writing badblock file at %u: %m\n",
                i * WRDPERSEC * 2);
            return -1;
        }
    }
    return 0;
}

// close file loaded on a disk drive
// mark drive offline
static void unloadfile (void *param, int drsel)
{
    unloadfileata (drsel, 0100000);
}

static void unloadfileata (int drsel, uint16_t ata)
{
    fns[drsel][0] = 0;
    close (fds[drsel]);
    fds[drsel] = -1;

    // update RPDS and maybe set ATA - attention active
    ZWR(rhat[6], ARMDS (drsel));
    ZWR(rhat[3], (rhat[3] & ~ RH3_RPDS) | getdrivestat (drsel) * RH3_RPDS0 | ata * RH3_RPDS0);
}

#if 000
static void dumpbuf (uint16_t drsel, uint16_t const *buf, uint32_t off, uint32_t xba, char const *func)
{
    fprintf (stderr, "  drv=%u off=%08o xba=%06o  %s\n", drsel, off, xba, func);
    for (int i = 0; i < 128; i += 16) {
        fprintf (stderr, "   ");
        for (int j = 16; -- j >= 0;) {
            fprintf (stderr, " %06o", buf[i+j]);
        }
        fprintf (stderr, " : %03o\n", i);
    }
}
#endif
