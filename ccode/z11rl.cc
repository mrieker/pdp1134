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

// page references rl01/rl02 user guide sep 81

//  ./z11rl [-killit] [-loadro/-loadrw <driveno> <file>]... [<tclscriptfile>]

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
#include "tclmain.h"
#include "z11defs.h"
#include "z11util.h"

                        // (p25/v1-13)
#define NCYLS 512       // RL02  (256 for RL01)
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

#define RL1_RLCS  0x0000FFFFU
#define RL1_RLBA  0xFFFF0000U
#define RL2_RLDA  0x0000FFFFU
#define RL2_RLMP1 0xFFFF0000U
#define RL3_RLMP2 0x0000FFFFU
#define RL3_RLMP3 0xFFFF0000U
#define RL4_DRDY  0x0000000FU
#define RL4_DERR  0x000000F0U
#define RL5_ENAB  0x80000000U
#define RL4_DRDY0 (RL4_DRDY & - RL4_DRDY)

#define RFLD(n,m) ((ZRD(rlat[n]) & m) / (m & - m))

static bool vcs[4];
static int debug;
static int fds[4];
static uint64_t seekdoneats[4];

// internal TCL commands
static Tcl_ObjCmdProc cmd_rkloadro;
static Tcl_ObjCmdProc cmd_rkloadrw;
static Tcl_ObjCmdProc cmd_rkunload;

static TclFunDef const fundefs[] = {
    { cmd_rkloadro, "rkloadro", "<disknumber> <filename> - load file read-only" },
    { cmd_rkloadrw, "rkloadrw", "<disknumber> <filename> - load file read/write" },
    { cmd_rkunload, "rkunload", "<disknumber> - unload disk" },
    { NULL, NULL, NULL }
};

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

static int loaddisk (bool readwrite, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static bool loadfile (Tcl_Interp *interp, bool readwrite, int diskno, char const *filenm);
static void *rlthread (void *dummy);
static char *lockfile (int fd, int how);
static void dumpbuf (uint16_t drivesel, uint16_t const *buf, uint32_t off, uint32_t xba, char const *func);

int main (int argc, char **argv)
{
    memset (fds, -1, sizeof fds);
    mypid = getpid ();

    // create shared memory page
    shm_unlink (SHMRL_NAME);
    int shmfd = shm_open (SHMRL_NAME, O_RDWR | O_CREAT, 0666);
    if (shmfd < 0) {
        fprintf (stderr, "error creating %s: %m\n", SHMRL_NAME);
        return 1;
    }
    if (ftruncate (shmfd, sizeof *shmrl) < 0) ABORT ();
    shmrl = (ShmRL *) mmap (NULL, sizeof *shmrl, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shmrl == MAP_FAILED) {
        fprintf (stderr, "error mmapping %s: %m\n", SHMRL_NAME);
        return 1;
    }

    // initialize shared memory
    memset (shmrl, 0, sizeof *shmrl);

    // put our pid in there in case we die when something is waiting for us to do something
    shmrl->svrpid = getpid ();

    // parse command line
    bool killit = false;
    bool loadit = false;
    bool tclena = false;
    int tclargs = argc;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Access RL11 controller and drives");
            puts ("");
            puts ("  ./z11rl [-killit] [-loadro/-loadrw <driveno> <file>]... | [-tcl [<tclscriptfile> [<scriptargs>...]]]");
            puts ("     -killit : kill other process accessing RL11 controller");
            puts ("     -loadro/rw : load the given file in the given drive");
            puts ("     -tcl : run tcl script after doing any -loadro/rws");
            puts ("     <tclscriptfile> : execute script then exit");
            puts ("                else : read and process commands from stdin");
            puts ("");
            puts ("     Use -loadro/-loadrw to statically load files in drives.");
            puts ("");
            puts ("     If -tcl option given, will use TCL commands to dynamically load and");
            puts ("     unload drives.  If no <tclscriptfile> given, will read from stdin.");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-killit") == 0) {
            killit = true;
            continue;
        }
        if ((strcasecmp (argv[i], "-loadro") == 0) || (strcasecmp (argv[i], "-loadrw") == 0)) {
            if ((i + 2 >= argc) || (argv[i+1][0] == '-') || (argv[i+2][0] == '-')) {
                fprintf (stderr, "missing disknumber and/or filename for -loadro/rw\n");
                UNLKIT;
                return 1;
            }
            char *p;
            int diskno = strtol (argv[i+1], &p, 0);
            if ((*p != 0) || (diskno < 0) || (diskno > 3)) {
                fprintf (stderr, "disknumber %s must be integer in range 0..3\n", argv[i+1]);
                UNLKIT;
                return 1;
            }
            shmrl->drives[diskno].readonly = strcasecmp (argv[i], "-loadro") == 0;
            strncpy (shmrl->drives[diskno].filename, argv[i+2], sizeof shmrl->drives[diskno].filename);
            shmrl->drives[diskno].filename[sizeof shmrl->drives[diskno].filename-1] = 0;
            loadit = true;
            i += 2;
            continue;
        }
        if (strcasecmp (argv[i], "-tcl") == 0) {
            tclena = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            UNLKIT;
            return 1;
        }
        tclargs = i;
        break;
    }

    z11p = new Z11Page ();
    rlat = z11p->findev ("RL", NULL, NULL, true, killit);
    ZWR(rlat[5], RL5_ENAB);     // enable board to process io instructions

    debug = 0;
    char const *dbgenv = getenv ("z11rl_debug");
    if (dbgenv != NULL) debug = atoi (dbgenv);

    // do any initial loads
    if (loadit) {
        for (int diskno = 0; diskno <= 3; diskno ++) {
            if (shmrl->drives[diskno].filename[0] != 0) {
                fds[diskno] = -1;
                if (! loadfile (NULL, ! shmrl->drives[diskno].readonly, diskno, shmrl->drives[diskno].filename)) {
                    return 1;
                }
            }
        }
    }

    // if no -tcl, run io directly until killed
    if (! tclena) {
        rlthread (NULL);
        return 0;
    }

    // -tcl, spawn thread to do io
    pthread_t threadid;
    int rc = pthread_create (&threadid, NULL, rlthread, NULL);
    if (rc != 0) ABORT ();

    // run scripting
    rc = tclmain (fundefs, argv[0], "z11rl", NULL, NULL, argc - tclargs, argv + tclargs, true);

    exiting = true;
    pthread_join (threadid, NULL);

    return rc;
}

// rkloadro <disknumber> <filename>
static int cmd_rkloadro (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    return loaddisk (false, interp, objc, objv);
}

// rkloadrw <disknumber> <filename>
static int cmd_rkloadrw (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    return loaddisk (true, interp, objc, objv);
}

static int loaddisk (bool readwrite, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if ((objc == 2) && (strcasecmp (Tcl_GetString (objv[1]), "help") == 0)) {
        puts ("");
        puts ("  rkloadro <disknumber> <filenane>");
        puts ("  rkloadrw <disknumber> <filenane>");
        return TCL_OK;
    }

    if (objc == 3) {
        int diskno;
        int rc = Tcl_GetIntFromObj (interp, objv[1], &diskno);
        if (rc != TCL_OK) return rc;
        if ((diskno < 0) || (diskno > 3)) {
            Tcl_SetResultF (interp, "disknumber %d not in range 0..3", diskno);
            return TCL_ERROR;
        }
        char const *filenm = Tcl_GetString (objv[2]);

        return loadfile (interp, readwrite, diskno, filenm) ? TCL_OK : TCL_ERROR;
    }

    Tcl_SetResultF (interp, "rkloadro/rkloadrw <disknumber> <filename>");
    return TCL_ERROR;
}

static bool loadfile (Tcl_Interp *interp, bool readwrite, int diskno, char const *filenm)
{
    int fd = open (filenm, readwrite ? O_RDWR | O_CREAT : O_RDONLY, 0666);
    if (fd < 0) {
        if (interp == NULL) fprintf (stderr, "error opening %s: %m\n", filenm);
        else Tcl_SetResultF (interp, "%m");
        return false;
    }
    char *lockerr = lockfile (fd, readwrite ? F_WRLCK : F_RDLCK);
    if (lockerr != NULL) {
        if (interp == NULL) fprintf (stderr, "error locking %s: %m\n", filenm);
        else Tcl_SetResultF (interp, "%s", lockerr);
        close (fd);
        free (lockerr);
        return false;
    }
    if (readwrite && (ftruncate (fd, NSECS * WRDPERSEC * 2) < 0)) {
        if (interp == NULL) fprintf (stderr, "error extending %s: %m\n", filenm);
        else Tcl_SetResultF (interp, "%m");
        close (fd);
        return false;
    }
    fprintf (stderr, "z11rl: drive %d loaded with read%s file %s\n", diskno, (readwrite ? "/write" : "-only"), filenm);
    LOCKIT;
    close (fds[diskno]);
    fds[diskno] = fd;
    vcs[diskno] = true;
    shmrl->drives[diskno].lastposn = 0;
    shmrl->drives[diskno].readonly = ! readwrite;
    UNLKIT;
    return true;
}

// rkunload <disknumber>
static int cmd_rkunload (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 2) {
        int diskno;
        int rc = Tcl_GetIntFromObj (interp, objv[1], &diskno);
        if (rc != TCL_OK) return rc;
        if ((diskno < 0) || (diskno > 3)) {
            Tcl_SetResultF (interp, "disknumber %d not in range 0..3", diskno);
            return TCL_ERROR;
        }
        fprintf (stderr, "z11rl: drive %d unloaded\n", diskno);
        LOCKIT;
        close (fds[diskno]);
        fds[diskno] = -1;
        UNLKIT;
        return TCL_OK;
    }

    Tcl_SetResultF (interp, "rkunload <disknumber>");
    return TCL_ERROR;
}

#define SECUNDERHEAD (nowus / USPERSEC % SECPERTRK)

// thread what does the disk file I/O
void *rlthread (void *dummy)
{
    if (debug > 1) fprintf (stderr, "z11rl: thread started\r\n");

    while (! exiting) {
        usleep (1000);

        LOCKIT;

        // check for load/unload commands in shared memory waiting to be processed
        switch (shmrl->command) {

            // something requesting file be loaded
            case SHMRLCMD_LOAD+0 ... SHMRLCMD_LOAD+3: {
                int driveno = shmrl->command - SHMRLCMD_LOAD;
                ShmRLDrive *dr = &shmrl->drives[driveno];
                UNLKIT;
                if (loadfile (NULL, ! dr->readonly, driveno, dr->filename)) {
                    shmrl->lderrno = 0;
                } else {
                    shmrl->lderrno = errno;
                    if (shmrl->lderrno == 0) shmrl->lderrno = -1;
                }
                LOCKIT;
                shmrl->command = SHMRLCMD_DONE;
                break;
            }

            // something requesting file be unloaded
            case SHMRLCMD_UNLD+0 ... SHMRLCMD_UNLD+3: {
                int driveno = shmrl->command - SHMRLCMD_UNLD;
                ShmRLDrive *dr = &shmrl->drives[driveno];
                dr->filename[0] = 0;
                if (++ dr->fnseq == 0) ++ dr->fnseq; // 0 reserved for initial condition
                close (fds[driveno]);
                fds[driveno] = -1;
                shmrl->command = SHMRLCMD_DONE;
                break;
            }
        }

        // see if command from pdp waiting to be processed
        struct timespec nowts;
        if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
        uint64_t nowus = (nowts.tv_sec * 1000000ULL) + (nowts.tv_nsec / 1000);

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

            uint16_t drivesel = (rlcs >> 8) & 3;
            int fd = fds[drivesel];
            ZWR(rlat[4], ZRD(rlat[4]) & ~ (RL4_DRDY0 << drivesel));     // clear drive ready bit for selected drive while I/O in progress
            ShmRLDrive *shmdr = &shmrl->drives[drivesel];

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
                    usleep (totldelay);

                    if (debug > 0) fprintf (stderr, "z11rl:   writecheck wc=%06o da=%06o xba=%06o\n", 65536 - rlmp, rlda, rlxba);

                    if (shmdr->lastposn != (rlda & 0xFFC0U)) {
                        if (debug > 0) fprintf (stderr, "z11rl:       latestposition=%06o rlda=%06o\n", shmdr->lastposn, rlda);
                        goto hnferr;
                    }
                    uint16_t trk = (rlda >> 6) & 1;
                    uint16_t cyl =  rlda >> 7;

                    do {
                        uint16_t sec = rlda & 63;
                        if (sec >= SECPERTRK) {
                            if (debug > 0) fprintf (stderr, "z11rl:       sec=%02o\n", sec);
                            goto hnferr;
                        }

                        uint16_t buf[WRDPERSEC];
                        uint32_t off = (((uint32_t) cyl * TRKPERCYL + trk) * SECPERTRK + sec) * sizeof buf;
                        int rc = pread (fd, buf, sizeof buf, off);
                        if (rc < 0) {
                            fprintf (stderr, "z11rl: error reading at %u: %m\n", off);
                            goto opierr;
                        }
                        if (rc < (int) sizeof buf) {
                            fprintf (stderr, "z11rl: only read %d of %d bytes at %u\n", rc, (int) sizeof buf, off);
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
                    if (debug > 0) fprintf (stderr, "z11rl:   getstatus\n");
                    if (rlda & 8) {                     // reset
                        vcs[drivesel] = false;
                    }
                    rlmp = 0x008DU;                     // 0 0 wl 0 -- 0 wge vc 0 -- 1 hs 0 ho -- 1 0 0 0
                    if (shmdr->readonly) rlmp |= 0x2000U; // write locked
                    if (vcs[drivesel]) rlmp |= 0x0200U; // volume check
                    rlmp |= shmdr->lastposn & 0x0040U;  // head select
                    if (fd >= 0)       rlmp |= 0x0010U; // heads out over disk
                    if (seekdelay > 0) rlmp |= 0x0004U; // seeking
                    else if (fd >= 0)  rlmp |= 0x0005U; // 'lock on'
                    break;
                }

                // SEEK
                case 3: {
                    if (fd < 0) goto opierr;

                    if (debug > 0) fprintf (stderr, "z11rl:   seek da=%06o\n", rlda);

                    int32_t newcyl = shmdr->lastposn >> 7;
                    if (rlda & 4) newcyl += rlda >> 7;
                             else newcyl -= rlda >> 7;
                    if (debug > 1) fprintf (stderr, "z11rl:       newcyl=%d\n", newcyl);
                    if (newcyl < 0) newcyl = 0;
                    if (newcyl > NCYLS - 1) newcyl = NCYLS - 1;

                    shmdr->lastposn = (newcyl << 7) | ((rlda << 2) & 0x40);

                    seekdoneats[drivesel] = nowus + (rlda >> 7) * USPERCYL + SETTLEUS;
                    break;
                }

                // READ HEADER
                case 4: {
                    if (debug > 0) fprintf (stderr, "z11rl:   readheader\n");
                    usleep (seekdelay);
                    nowus += seekdelay;
                    rlmp   = shmdr->lastposn | SECUNDERHEAD;
                    rlmp2  = 0;
                    rlmp3  = 0;
                    goto rhddone;
                }

                // WRITE DATA
                case 5: {
                    usleep (totldelay);

                    if (debug > 0) fprintf (stderr, "z11rl:   writedata wc=%06o da=%06o xba=%06o\n", 65536 - rlmp, rlda, rlxba);

                    if (shmdr->lastposn != (rlda & 0xFFC0U)) {
                        if (debug > 0) fprintf (stderr, "z11rl:       latestposition=%06o rlda=%06o\n", shmdr->lastposn, rlda);
                        goto hnferr;
                    }
                    uint16_t trk = (rlda >> 6) & 1;
                    uint16_t cyl =  rlda >> 7;

                    do {
                        uint16_t sec = rlda & 63;
                        if (sec >= SECPERTRK) {
                            if (debug > 0) fprintf (stderr, "z11rl:       sec=%02o\n", sec);
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
                            fprintf (stderr, "z11rl: error writing at %u: %m\n", off);
                            goto opierr;
                        }
                        if (rc < (int) sizeof buf) {
                            fprintf (stderr, "z11rl: only wrote %d of %d bytes at %u\n", rc, (int) sizeof buf, off);
                            goto opierr;
                        }
                        if (debug > 2) dumpbuf (drivesel, buf, off, xbasave, "write");
                        rlda ++;
                    } while (rlmp != 0);
                    break;
                }

                // READ DATA
                case 6: {
                    usleep (totldelay);

                    if (debug > 0) fprintf (stderr, "z11rl:   readdata wc=%06o da=%06o xba=%06o\n", 65536 - rlmp, rlda, rlxba);

                    if (shmdr->lastposn != (rlda & 0xFFC0U)) {
                        if (debug > 0) fprintf (stderr, "z11rl:       latestposition=%06o rlda=%06o\n", shmdr->lastposn, rlda);
                        goto hnferr;
                    }
                    uint16_t trk = (rlda >> 6) & 1;
                    uint16_t cyl =  rlda >> 7;

                    do {
                        uint16_t sec = rlda & 63;
                        if (sec >= SECPERTRK) {
                            if (debug > 0) fprintf (stderr, "z11rl:       sec=%02o\n", sec);
                            goto hnferr;
                        }

                        uint16_t buf[WRDPERSEC];
                        uint32_t off = (((uint32_t) cyl * TRKPERCYL + trk) * SECPERTRK + sec) * sizeof buf;
                        int rc = pread (fd, buf, sizeof buf, off);
                        if (rc < 0) {
                            fprintf (stderr, "z11rl: error reading at %u: %m\n", off);
                            goto opierr;
                        }
                        if (rc < (int) sizeof buf) {
                            fprintf (stderr, "z11rl: only read %d of %d bytes at %u\n", rc, (int) sizeof buf, off);
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
                    if (debug > 0) fprintf (stderr, "z11rl:   read data without header check\n");
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
            rlcs |= 9u << 10;                       // memory parity error
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

            // update RLMPs, RLDA, then RLBA and RLCS
            ZWR(rlat[3], ((uint32_t) rlmp3 << 16) | rlmp2);
            ZWR(rlat[2], ((uint32_t) rlmp  << 16) | rlda);
            ZWR(rlat[1], ((uint32_t) rlxba << 16) | rlcs);
            if (debug > 0) fprintf (stderr, "z11rl:  done RLCS=%06o RLxBA=%06o RLDA=%06o RLMP=%06o %06o %06o\n",
                    rlcs, rlxba, rlda, rlmp, rlmp2, rlmp3);
        }

        // always update drive readies in case a seek just completed
        uint32_t drdy = 0;
        for (int i = 4; -- i >= 0;) {
            drdy += drdy + ((fds[i] >= 0) && (seekdoneats[i] <= nowus));
        }
        ZWR(rlat[4], (ZRD(rlat[4]) & ~ RL4_DRDY) | drdy * RL4_DRDY0);

        UNLKIT;
    }

    return NULL;
}

// try to lock the given file
//  input:
//   fd = file to lock
//   how = F_WRLCK: exclusive access
//         F_RDLCK: shared access
//  output:
//   returns NULL: successful
//           else: error message
static char *lockfile (int fd, int how)
{
    struct flock flockit;

trylk:;
    memset (&flockit, 0, sizeof flockit);
    flockit.l_type   = how;
    flockit.l_whence = SEEK_SET;
    flockit.l_len    = 4096;
    if (fcntl (fd, F_SETLK, &flockit) >= 0) return NULL;

    char *errmsg = NULL;
    if (((errno == EACCES) || (errno == EAGAIN)) && (fcntl (fd, F_GETLK, &flockit) >= 0)) {
        if (flockit.l_type == F_UNLCK) goto trylk;
        if (asprintf (&errmsg, "locked by pid %d", (int) flockit.l_pid) < 0) ABORT ();
    } else {
        if (asprintf (&errmsg, "%m") < 0) ABORT ();
    }
    return errmsg;
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
