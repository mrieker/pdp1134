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

static char fns[8][SHMMS_FNSIZE];
static int debug;
static int fds[8];

#define LOCKIT shmms_svr_mutexlock(shmms)
#define UNLKIT shmms_svr_mutexunlk(shmms)

static ShmMS *shmms;
static uint16_t wrdbuf[65536];
static uint32_t volatile *rhat;

static void *rhiothread (void *dummy);
static void dotransfer (uint32_t rh3);
static int setdrivetype (void *param, int drsel);
static int fileloaded (void *param, int drsel, int fd);
static int writebadblocks (ShmMSDrive *dr, int fd);
static void unloadfile (void *param, int drsel);
static void unloadfileata (int drsel, uint16_t ata);
static void upddrivestats (uint32_t rh1);
static void wrreg (int index, uint32_t value);

int main (int argc, char **argv)
{
    setlinebuf (stderr);
    setlinebuf (stdout);

    memset (fds, -1, sizeof fds);

    bool resetit = (argc > 1) && (strcasecmp (argv[1], "-reset") == 0);

    // access fpga register set for the RH-11 controller
    // lock it so we are only process accessing it
    z11page = new Z11Page ();
    rhat = z11page->findev ("RH", NULL, NULL, true, false);

    // initialize shared memory - contains filenames and load/unload info
    shmms = shmms_svr_initialize (resetit, SHMMS_NAME_RH, "z11rh");
    shmms->ndrives = 8;

    // ...and no drives are ready
    uint32_t fastio = ZRD(rhat[4]) & RH4_FAST;
    wrreg (1, RH1_VVS);

    // enable board to process io instructions
    wrreg (4, RH4_ENAB | fastio);
    UNLKIT;

    debug = 0;
    char const *dbgenv = getenv ("z11rh_debug");
    if (dbgenv != NULL) debug = atoi (dbgenv);

    pthread_t rhtid;
    int rc = pthread_create (&rhtid, NULL, rhiothread, NULL);
    if (rc != 0) ABORT ();

    shmms_svr_proccmds (shmms, "z11rh", setdrivetype, fileloaded, unloadfile, NULL);

    return 0;
}

// do the disk file I/O
static void *rhiothread (void *dummy)
{
    while (true) {

        // wait for pdp to start a transfer or set rpcs2[05] (CLR)
        z11page->waitint (ZGINT_RH);

        // block disk from being unloaded from under us
        LOCKIT;

        // check for 'clear controller' command
        uint32_t rh3 = ZRD(rhat[3]);
        if (debug > 0) fprintf (stderr, "z11rh: rh3=%08X\n", rh3);
        if (rh3 & RH3_CLR) {

            // make sure drive status is up to date
            upddrivestats (0);

            // clear RH3<CLR>
            // fpga has cleared all the registers
            wrreg (3, RH3_CLR);
        }

        // any housekeeping or positioning commands are handled by the fpga

        // check for transfer go bit
        // this gets set when the pdp has started a transfer on some drive
        // there can only be one of these going at a time
        // ...because they are gated by the RDY bit in RPCSR1<07>
        // the fpga has aleeady delayed for the implied seek at the beginning
        else if (rh3 & RH3_XGO) {
            dotransfer (rh3);
        }

        UNLKIT;
    }
}

// do transfer for the given drive
//  input:
//   RH2_WRT = 1: write data
//    else RH3_WCE = 1: write check
//                   0: read data
//   RH2_DRV = which drive
//   RH2_CYL = cylinder
//   RH2_ADR[17:01] = word address
//             [00] = bus address inhibit
//   RH3_TRK = track
//   RH3_SEC = sector
//   RH3_WCT = neg word count
static void dotransfer (uint32_t rh3)
{
    uint32_t rh2 = ZRD(rhat[2]);
    int drsel = (rh2 & RH2_DRV) / RH2_DRV0;

    ShmMSDrive *dr = &shmms->drives[drsel];

    uint32_t rpba = (ZRD(rhat[2]) & RH2_ADR) / RH2_ADR0;
    uint32_t rpbi = (rpba & 1) ? 0 : 2;
    rpba &= -2;

    // fpga already range checked these
    uint32_t cylndr = (ZRD(rhat[2]) & RH2_CYL) / RH2_CYL0;
    uint32_t track  = (ZRD(rhat[3]) & RH3_TRK) / RH3_TRK0;
    uint32_t sector = (ZRD(rhat[3]) & RH3_SEC) / RH3_SEC0;
    uint32_t blknum = (cylndr * TRKPERCYL + track) * SECPERTRK + sector;

    dr->curposn = cylndr;

    uint32_t blocksleft = (NSECS >= blknum) ? NSECS - blknum : 0;
    uint32_t wordsleft  = blocksleft * WRDPERSEC;

    uint16_t  rpwc   = (rh3 & RH3_WCT) / RH3_WCT0;
    uint32_t  wrdcnt = 65536 - rpwc;
    uint16_t *wrdpnt = wrdbuf;

    if (wrdcnt > wordsleft) wrdcnt = wordsleft;

    if (debug > 1) fprintf (stderr, "dotransfer: [%d] cyl=%03u trk=%02u sec=%02u blk=%06u  wc=%06o ba=%06o  %s\n",
        drsel, cylndr, track, sector, blknum, rpwc, rpba,
            ((rh2 & RH2_WRT) ? "WRITE" : ((rh3 & RH3_WCE) ? "WCHECK" : "READ")));

    bool per = false;
    bool nxm = false;
    bool fer = false;
    bool wce = false;

    if (rh2 & RH2_WRT) {

        // WRITE

        // read pdp memory into buffer
        for (uint32_t wc = wrdcnt; wc != 0; -- wc) {
            if (z11page->dmaread (rpba, (wrdpnt ++)) != 0) {
                nxm = true; // or per = true
                goto done;
            }
            rpba += rpbi;
            if (++ rpwc == 0) break;
        }

        // zero fill any partial sector
        if (wrdcnt % WRDPERSEC != 0) {
            memset (&wrdbuf[wrdcnt], 0, (WRDPERSEC - wrdcnt % WRDPERSEC) * 2);
            wrdcnt += WRDPERSEC - wrdcnt % WRDPERSEC;
        }

        // write buffer to disk file
        int rc = pwrite (fds[drsel], wrdbuf, wrdcnt * 2, blknum * WRDPERSEC * 2);
        if (rc != (int) wrdcnt * 2) {
            if (rc < 0) {
                fprintf (stderr, "z11rh: [%d] error writing at %u: %m\n", drsel, blknum);
            } else {
                fprintf (stderr, "z11rh: [%d] only wrote %d bytes of %u at %u\n", drsel, rc, wrdcnt * 2, blknum);
            }
            fer = true;
        }
    } else {

        // read disk file to buffer
        int rc = pread (fds[drsel], wrdbuf, wrdcnt * 2, blknum * WRDPERSEC * 2);
        if (rc != (int) wrdcnt * 2) {
            if (rc < 0) {
                fprintf (stderr, "z11rh: [%d] error reading at %u: %m\n", drsel, blknum);
            } else {
                fprintf (stderr, "z11rh: [%d] only read %d bytes of %u at %u\n", drsel, rc, wrdcnt * 2, blknum);
            }
            fer = true;
        }

        else if (rh3 & RH3_WCE) {

            // WRITE CHECK - compare buffer with pdp memory
            for (; wrdcnt > 0; -- wrdcnt) {
                uint16_t word;
                if (z11page->dmaread (rpba, &word) != 0) {
                    nxm = true;
                    break;
                }
                if (word != *(wrdpnt ++)) {
                    wce = true;
                    break;
                }
                rpba += rpbi;
                if (++ rpwc == 0) break;
            }
        } else {

            // READ - copy buffer to pdp memory
            for (; wrdcnt > 0; -- wrdcnt) {
                if (! z11page->dmawrite (rpba, *(wrdpnt ++))) {
                    nxm = true;
                    break;
                }
                rpba += rpbi;
                if (++ rpwc == 0) break;
            }
        }
    }
done:;
    blknum += (wrdpnt - wrdbuf + WRDPERSEC - 1) / WRDPERSEC;
    sector  = blknum % SECPERTRK;
    track   = blknum / SECPERTRK % TRKPERCYL;
    cylndr  = blknum / SECPERTRK / TRKPERCYL;

    if (debug > 1) fprintf (stderr, "dotransfer: [%d] cyl=%03u trk=%02u sec=%02u blk=%06u  wc=%06o ba=%06o  done\n",
        drsel, cylndr, track, sector, blknum, rpwc, rpba);

    wrreg (2, (cylndr * RH2_CYL0) |     // ending cylinder number
                (rpba * RH2_ADR0));     // ending bus address

    wrreg (3, (per ? RH3_PER : 0) |     // memory parity error
              (nxm ? RH3_NXM : 0) |     // non-existant memory
              (fer ? RH3_FER : 0) |     // file i/o error
                                        // clear transfer-go bit
              (wce ? RH3_WCE : 0) |     // write check error
               (track * RH3_TRK0) |     // ending track number
              (sector * RH3_SEC0) |     // ending sector number
                (rpwc * RH3_WCT0));     // ending word count
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
// save the fd and mark media online
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
    uint32_t clrvv = 0;
    if (strcmp (fns[drsel], dr->filename) != 0) {
        strcpy (fns[drsel], dr->filename);
        dr->curposn = 0;
        clrvv = RH1_VVS0 << drsel;
    }

    // update RPDS then set ATA - attention active
    upddrivestats (clrvv);
    ZWR(rhat[5], RH5_ATAS0 << drsel);

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

    // double sector buffer - filled with all ones
    uint16_t secbuf[WRDPERSEC*2];
    memset (secbuf, -1, sizeof secbuf);

    // first four words
    secbuf[0] = snbuf[0] & 077777;
    secbuf[1] = snbuf[1] & 077777;
    secbuf[2] = 0;
    secbuf[3] = 0;

    // write it to all sectors of last track of last cylinder
    uint32_t bytpos = NCYLS * TRKPERCYL * SECPERTRK * WRDPERSEC * 2;
    for (int i = SECPERTRK / 2; -- i >= 0;) {
        bytpos -= sizeof secbuf;
        int rc  = pwrite (fd, secbuf, sizeof secbuf, bytpos);
        if (rc < 0) {
            fprintf (stderr, "z11rh: error writing badblock file at %u: %m\n", bytpos);
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

    // update RPDS then and set ATA - attention active
    upddrivestats (0);
    ZWR(rhat[5], RH5_ATAS0 << drsel);
}

// update mols,wrls,dts bits for all drives
static void upddrivestats (uint32_t rh1)
{
    for (int i = 0; i < 8; i ++) {
        ShmMSDrive *dr = &shmms->drives[i];
        if (fds[i] >= 0)  rh1 |= RH1_MOLS0 << i;    // MOL - medium online
        if (dr->readonly) rh1 |= RH1_WRLS0 << i;    // WRL - write locked
        if (! dr->rl01)   rh1 |= RH1_DTS0  << i;    // DT  - drive type (0=RP04; 1=RP06)
    }
    wrreg (1, rh1);
}

#define RF(f) (value & f) / (f & - f)
static void wrreg (int index, uint32_t value)
{
    ZWR(rhat[index], value);
    if (debug > 2) {
        fprintf (stderr, "z11rh: rhat[%d] <= %08X\n", index, value);
    }
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
