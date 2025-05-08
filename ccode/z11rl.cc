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
#include <sys/time.h>
#include <tcl.h>
#include <unistd.h>

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

#define RL1_RLCS  0x0000FFFFU
#define RL1_RLBA  0xFFFF0000U
#define RL2_RLDA  0x0000FFFFU
#define RL2_RLMP  0xFFFF0000U
#define RL3_RHDA  0x0000FFFFU
#define RL3_RHCRC 0xFFFF0000U
#define RL4_DRDY  0x0000000FU
#define RL4_DERR  0x000000F0U
#define RL4_MPMUX 0x00000100U
#define RL5_ENAB  0x80000000U

#define RFLD(n,m) ((rlat[n] & m) / (m & - m))

static bool ros[4], vcs[4];
static int debug;
static int fds[4];
static uint16_t latestpositions[4];
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

#define LOCKIT if (pthread_mutex_lock (&lock) != 0) ABORT ()
#define UNLKIT if (pthread_mutex_unlock (&lock) != 0) ABORT ()

static bool volatile exiting;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t volatile *rlat;
static Z11Page *z11p;

static int loaddisk (bool readwrite, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static bool loadfile (Tcl_Interp *interp, bool readwrite, int diskno, char const *filenm);
static void *rlthread (void *dummy);
static char *lockfile (int fd, int how);
static void dumpbuf (uint16_t drivesel, uint16_t const *buf, uint32_t off, uint32_t xba, char const *func);

int main (int argc, char **argv)
{
    memset (fds, -1, sizeof fds);

    bool killit = false;
    bool loadit = false;
    int tclargs = argc;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Access RL11 controller and drives");
            puts ("");
            puts ("  ./z11rl [-killit] [-loadro/-loadrw <driveno> <file>]... | [<tclscriptfile> [<scriptargs>...]]");
            puts ("     -killit : kill other process accessing RL11 controller");
            puts ("     -loadro/rw : load the given file in the given drive");
            puts ("     <tclscriptfile> : execute script then exit");
            puts ("                else : read and process commands from stdin");
            puts ("");
            puts ("     Use -loadro/-loadrw to statically load files in drives.");
            puts ("     Any <tclscriptfile> given is ignored.");
            puts ("");
            puts ("     If no -loadro/rw options given, will use TCL commands to dynamically load");
            puts ("     and unload drives.  If no <tclscriptfile> given, will read from stdin.");
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
                return 1;
            }
            char *p;
            int diskno = strtol (argv[i+1], &p, 0);
            if ((*p != 0) || (diskno < 0) || (diskno > 3)) {
                fprintf (stderr, "disknumber %s must be integer in range 0..3\n", argv[i+1]);
                return 1;
            }
            fds[diskno] = i;
            ros[diskno] = strcasecmp (argv[i], "-loadro") == 0;
            loadit = true;
            i += 2;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        tclargs = i;
        break;
    }

    z11p  = new Z11Page ();
    rlat = z11p->findev ("RL", NULL, NULL, true, killit);
    rlat[5] = RL5_ENAB;     // enable board to process io instructions

    debug = 0;
    char const *dbgenv = getenv ("z11rl_debug");
    if (dbgenv != NULL) debug = atoi (dbgenv);

    // if -load option, load files then just run io calls
    if (loadit) {
        for (int diskno = 0; diskno <= 3; diskno ++) {
            int i = fds[diskno];
            if (i >= 0) {
                fds[diskno] = -1;
                if (! loadfile (NULL, ! ros[diskno], diskno, argv[i+2])) return 1;
            }
        }
        rlthread (NULL);
        return 0;
    }

    // spawn thread to do io
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
    fprintf (stderr, "IODevRL11::loadfile: drive %d loaded with read%s file %s\n", diskno, (readwrite ? "/write" : "-only"), filenm);
    LOCKIT;
    close (fds[diskno]);
    fds[diskno] = fd;
    ros[diskno] = ! readwrite;
    vcs[diskno] = true;
    latestpositions[diskno] = 0;
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
        fprintf (stderr, "IODevRL11::scriptcmd: drive %d unloaded\n", diskno);
        LOCKIT;
        close (fds[diskno]);
        fds[diskno] = -1;
        UNLKIT;
        return TCL_OK;
    }

    Tcl_SetResultF (interp, "rkunload <disknumber>");
    return TCL_ERROR;
}

#define SETST(n) status = (status & ST_CBSY) | (n)

// thread what does the disk file I/O
void *rlthread (void *dummy)
{
    if (debug > 1) fprintf (stderr, "IODevRL11::rlthread: thread started\r\n");

    while (! exiting) {
        usleep (1000);

        LOCKIT;

        uint16_t rlcs = RFLD (1, RL1_RLCS);
        if (! (rlcs & 0x0080)) {
            uint16_t rlba = RFLD (1, RL1_RLBA) & 0xFFFEU;
            uint16_t rlda = RFLD (2, RL2_RLDA);
            uint16_t rlmp = RFLD (2, RL2_RLMP);

            if (debug > 0) fprintf (stderr, "IODevRL11::rlthread: start RLCS=%04X RLBA=%04X RLDA=%04X RLMP=%04X\n", rlcs, rlba, rlda, rlmp);

            uint32_t rlxba = ((rlcs & 0x30) << 12) + rlba;

            rlcs &= 0xC3FFU;                        // clear error bits

            uint16_t drivesel = (rlcs >> 8) & 3;
            int fd = fds[drivesel];

            struct timespec nowts;
            if (clock_gettime (CLOCK_MONOTONIC, &nowts) < 0) ABORT ();
            uint64_t nowus = (nowts.tv_sec * 1000000ULL) + (nowts.tv_nsec / 1000);

            uint32_t seekdelay = 0;
            if (seekdoneats[drivesel] > nowus) {
                seekdelay = seekdoneats[drivesel] - nowus;
            }

            if (debug > 1) fprintf (stderr, "IODevRL11::rlthread:       xba=%06o dsel=%u fd=%3d\n", rlxba, drivesel, fd);

            switch ((rlcs >> 1) & 7) {

                // NOP
                case 0: {
                    break;
                }

                // WRITE CHECK
                case 1: {
                    usleep (AVGROTUS + (65536 - rlmp) * USPERWRD + seekdelay);

                    if (latestpositions[drivesel] != (rlda & 0xFFC0U)) goto hnferr;
                    uint16_t trk = (rlda >> 6) & 1;
                    uint16_t cyl =  rlda >> 7;

                    do {
                        uint16_t sec = rlda & 63;
                        if (sec >= SECPERTRK) goto hnferr;

                        uint16_t buf[WRDPERSEC];
                        uint32_t off = (((uint32_t) cyl * TRKPERCYL + trk) * SECPERTRK + sec) * sizeof buf;
                        int rc = pread (fd, buf, sizeof buf, off);
                        if (rc < 0) {
                            fprintf (stderr, "IODevRL11::rlthread: error reading at %u: %m\n", off);
                            goto opierr;
                        }
                        if (rc < (int) sizeof buf) {
                            fprintf (stderr, "IODevRL11::rlthread: only read %d of %d bytes at %u\n", rc, (int) sizeof buf, off);
                            goto opierr;
                        }
                        rlda ++;

                        for (int i = 0; i < WRDPERSEC; i ++) {
                            uint16_t memword;
                            if (! z11p->dmaread (rlxba, &memword)) goto nxmerr;
                            if (memword != buf[i]) goto wckerr;
                            rlxba = (rlxba + 2) & 0x3FFFE;
                            if (++ rlmp == 0) break;
                        }
                    } while (rlmp != 0);
                    break;
                }

                // GET STATUS
                case 2: {
                    if (rlda & 8) {                     // reset
                        vcs[drivesel] = false;
                    }
                    rlmp = 0x008DU;                     // 0 0 wl 0 -- 0 wge vc 0 -- 1 hs 0 ho -- 1 0 0 0
                    if (ros[drivesel]) rlmp |= 0x2000U; // write locked
                    if (vcs[drivesel]) rlmp |= 0x0200U; // volume check
                    rlmp |= latestpositions[drivesel] & 0x0040U; // head select
                    if (seekdelay > 0) rlmp |= 0x0004U; // seeking
                    else if (fd >= 0)  rlmp |= 0x0005U; // 'lock on'
                    break;
                }

                // SEEK
                case 3: {
                    if (fd < 0) goto opierr;

                    int32_t newcyl = latestpositions[drivesel] >> 7;
                    if (rlda & 4) newcyl += rlda >> 7;
                             else newcyl -= rlda >> 7;
                    if (newcyl < 0) newcyl = 0;
                    if (newcyl > NCYLS - 1) newcyl = NCYLS - 1;

                    latestpositions[drivesel] = (newcyl << 7) | ((rlda << 2) & 0x40);

                    seekdoneats[drivesel] = nowus + (rlda >> 7) * USPERCYL + SETTLEUS;
                    break;
                }

                // READ HEADER
                case 4: {
                    usleep (AVGROTUS + seekdelay);
                    rlmp = latestpositions[drivesel] | nowus % SECPERTRK;
                    break;
                }

                // WRITE DATA
                case 5: {
                    usleep (AVGROTUS + (65536 - rlmp) * USPERWRD + seekdelay);

                    if (latestpositions[drivesel] != (rlda & 0xFFC0U)) goto hnferr;
                    uint16_t trk = (rlda >> 6) & 1;
                    uint16_t cyl =  rlda >> 7;

                    do {
                        uint16_t sec = rlda & 63;
                        if (sec >= SECPERTRK) goto hnferr;

                        uint32_t xbasave = rlxba;
                        uint16_t buf[WRDPERSEC];
                        for (int i = 0; i < WRDPERSEC; i ++) {
                            if (! z11p->dmaread (rlxba, &buf[i])) goto nxmerr;
                            rlxba = (rlxba + 2) & 0x3FFFE;
                            if (++ rlmp == 0) {
                                memset (&buf[i+1], 0, (WRDPERSEC - i - 1) * sizeof buf[0]);
                                break;
                            }
                        }

                        uint32_t off = (((uint32_t) cyl * TRKPERCYL + trk) * SECPERTRK + sec) * sizeof buf;
                        int rc = pwrite (fd, buf, sizeof buf, off);
                        if (rc < 0) {
                            fprintf (stderr, "IODevRL11::rlthread: error writing at %u: %m\n", off);
                            goto opierr;
                        }
                        if (rc < (int) sizeof buf) {
                            fprintf (stderr, "IODevRL11::rlthread: only wrote %d of %d bytes at %u\n", rc, (int) sizeof buf, off);
                            goto opierr;
                        }
                        if (debug > 2) dumpbuf (drivesel, buf, off, xbasave, "write");
                        rlda ++;
                    } while (rlmp != 0);
                    break;
                }

                // READ DATA
                case 6: {
                    usleep (AVGROTUS + (65536 - rlmp) * USPERWRD + seekdelay);

                    if (latestpositions[drivesel] != (rlda & 0xFFC0U)) goto hnferr;
                    uint16_t trk = (rlda >> 6) & 1;
                    uint16_t cyl =  rlda >> 7;

                    do {
                        uint16_t sec = rlda & 63;
                        if (sec >= SECPERTRK) goto hnferr;

                        uint16_t buf[WRDPERSEC];
                        uint32_t off = (((uint32_t) cyl * TRKPERCYL + trk) * SECPERTRK + sec) * sizeof buf;
                        int rc = pread (fd, buf, sizeof buf, off);
                        if (rc < 0) {
                            fprintf (stderr, "IODevRL11::rlthread: error reading at %u: %m\n", off);
                            goto opierr;
                        }
                        if (rc < (int) sizeof buf) {
                            fprintf (stderr, "IODevRL11::rlthread: only read %d of %d bytes at %u\n", rc, (int) sizeof buf, off);
                            goto opierr;
                        }
                        if (debug > 2) dumpbuf (drivesel, buf, off, rlxba, "read");
                        rlda ++;

                        for (int i = 0; i < WRDPERSEC; i ++) {
                            if (! z11p->dmawrite (rlxba, buf[i])) goto nxmerr;
                            rlxba = (rlxba + 2) & 0x3FFFE;
                            if (++ rlmp == 0) break;
                        }
                    } while (rlmp != 0);
                    break;
                }

                // READ DATA WITHOUT HEADER CHECK
                case 7: {
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
        alldone:;
            rlcs    = (rlcs & ~ 0x30) | 0x80 | ((rlxba >> 12) & 0x30);
            if (debug > 0) fprintf (stderr, "IODevRL11::rlthread:  done RLCS=%04X RLBA=%04X RLDA=%04X RLMP=%04X\n", rlcs, rlba, rlda, rlmp);
            rlat[2] = ((uint32_t) rlmp  << 16) | rlda;
            rlat[1] = ((uint32_t) rlxba << 16) | rlcs;
        }
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
