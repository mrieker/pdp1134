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

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "futex.h"
#include "shmms.h"
#include "z11defs.h"
#include "z11util.h"

static int mypid;
static ShmMS *shmrl = NULL;
static ShmMS *shmtm = NULL;
static uint32_t volatile *rlat;
static uint32_t volatile *tmat;
static Z11Page *z11page;

static ShmMS *getshmms (int ctlid);
static int msunload (ShmMS *shmms, int drive);
static int mswaitidle (ShmMS *shmms);
static int mswaitdone (ShmMS *shmms);
static int mslock (ShmMS *shmms);
static void lockmutex (ShmMS *shmms);
static int forkserver (ShmMS *shmms);
static void msunlk (ShmMS *shmms);

// load/unload a file
//  1) if filename empty, unload any existing file
//  2) set/clear readonly bit for the drive
//  3) if filename given, load in drive
int shmms_load (int ctlid, int drive, bool readonly, char const *filename)
{
    if ((drive < 0) || (drive >= SHMMS_NDRIVES)) ABORT ();

    // insert cwd in front of filename and squeeze out /../ and /./
    char *fnbuf = NULL;
    int fnlen = 0;
    if (filename != NULL) {
        if (filename[0] == 0) {
            fnbuf = strdup ("");
            if (fnbuf == NULL) abort ();
        } else {
            fnbuf = realpath (filename, NULL);
            if (fnbuf == NULL) {
                int rc = - errno;
                fprintf (stderr, "shmms_load: failed to expand %s: %m\n", filename);
                return rc;
            }
            fnlen = strlen (fnbuf);
            if (fnlen >= SHMMS_FNSIZE) {
                fprintf (stderr, "shmms_load: filename too long %s\n", fnbuf);
                free (fnbuf);
                return -ERANGE;
            }
        }
    }

    // lock shared page and wait for it to be idle
    ShmMS *shmms = getshmms (ctlid);
    int rc = mswaitidle (shmms);

    // fnbuf == NULL means keep the same file loaded (probably just changing readonly status)
    // fnbuf == "" means unload any file that's in there

    // if empty filename, unload any previous file
    if ((rc >= 0) && (fnbuf != NULL) && (fnbuf[0] == 0)) rc = msunload (shmms, drive);

    if (rc >= 0) {

        // mark entry readonly status
        ShmMSDrive *dr = &shmms->drives[drive];
        dr->readonly = readonly;

        // if we aren't going to load anything, we're done
        if ((fnbuf != NULL) ? (fnbuf[0] == 0) : (dr->filename[0] == 0)) {
            msunlk (shmms);
        } else {

            // send a load command for this drive
            // if filename is NULL, keep same file (probably just changing readonly status)
            shmms->cmdpid  = getpid ();
            shmms->command = SHMMSCMD_LOAD + drive;
            if (fnbuf != NULL) {
                memcpy (dr->filename, fnbuf, ++ fnlen);
                if (++ dr->fnseq == 0) ++ dr->fnseq; // 0 reserved for initial condition
            }

            // unlock, wait for done, then re-lock
            rc = mswaitdone (shmms);
            if (rc >= 0) {

                // completed, get load status and say page is idle
                rc = shmms->negerr;
                shmms->command = SHMMSCMD_IDLE;
                msunlk (shmms);
            }
        }
    }
    if (fnbuf != NULL) free (fnbuf);
    return rc;
}

// get drive status
int shmms_stat (int ctlid, int drive, char *buff, int size, uint32_t *curpos_r)
{
    if ((drive < 0) || (drive >= SHMMS_NDRIVES)) ABORT ();

    ShmMS *shmms = getshmms (ctlid);
    int statbits = mslock (shmms);
    if (statbits >= 0) {

        // status bits from shared memory page
        statbits = 0;
        ShmMSDrive *dr = &shmms->drives[drive];
        if (dr->filename[0] != 0) statbits |= MSSTAT_LOAD;  // something loaded
        if (dr->readonly)    statbits |= MSSTAT_WRPROT;     // write protected
        if (dr->rl01)        statbits |= MSSTAT_RL01;       // RL01 drive
        statbits |= dr->fnseq * (MSSTAT_FNSEQ & - MSSTAT_FNSEQ);
        *curpos_r = dr->curposn;                            // current position
        if (size > 0) {
            char const *fn = dr->filename;                  // loaded filename
            strncpy (buff, fn, size);
            buff[size-1] = 0;
        }

        // status bits from fpga register page
        if (z11page == NULL) {
            z11page = new Z11Page ();
        }
        switch (ctlid) {

            case SHMMS_CTLID_RL: {
                if (rlat == NULL) {
                    rlat = z11page->findev ("RL", NULL, NULL, false);
                }
                uint32_t rl4 = ZRD(rlat[4]) >> drive;
                if (rl4 & RL4_DRDY0) statbits |= MSSTAT_READY;      // ready (not seeking etc)
                if (rl4 & RL4_DERR0) statbits |= MSSTAT_FAULT;      // fault (drive error)
                break;
            }

            case SHMMS_CTLID_TM: {
                if (tmat == NULL) {
                    tmat = z11page->findev ("TM", NULL, NULL, false);
                }
                uint32_t tm5 = ZRD(tmat[5]) >> drive;
                if (tm5 & TM5_TURS0) statbits |= MSSTAT_READY;      // ready (not skipping etc)
                break;
            }

            default: ABORT ();
        }

        msunlk (shmms);
    }

    return statbits;
}

// point to shared memory for the given controller
//  input:
//   ctlid = SHMMS_CTLID_RL : use RL controller
//           SHMMS_CTLID_TM : use TM controller
//  output:
//   pointer to shared memory
static ShmMS *getshmms (int ctlid)
{
    char const *shmname;
    ShmMS **ptr;
    switch (ctlid) {
        case SHMMS_CTLID_RL: {
            ptr = &shmrl;
            shmname = SHMMS_NAME_RL;
            break;
        }
        case SHMMS_CTLID_TM: {
            ptr = &shmtm;
            shmname = SHMMS_NAME_TM;
            break;
        }
        default: ABORT ();
    }

    // if shared mem not open yet, open it
    ShmMS *shmms = *ptr;
    if (shmms == NULL) {
        int shmfd = shm_open (shmname, O_RDWR | O_CREAT, 0666);
        if (shmfd < 0) {
            fprintf (stderr, "mslock: error opening %s: %m\n", shmname);
            ABORT ();
        }
        if (ftruncate (shmfd, sizeof *shmms) < 0) {
            fprintf (stderr, "mslock: error setting %s size: %m\n", shmname);
            ABORT ();
        }

        // map the shared memory
        shmms = (ShmMS *) mmap (NULL, sizeof *shmms, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
        if (shmms == MAP_FAILED) ABORT ();

        *ptr = shmms;
    }

    return shmms;
}

// unload given drive
// - call locked in IDLE, returns locked in IDLE, unlocks during
static int msunload (ShmMS *shmms, int drive)
{
    shmms->command = SHMMSCMD_UNLD + drive;
    int rc = mswaitdone (shmms);
    if (rc >= 0) shmms->command = SHMMSCMD_IDLE;
    return rc;
}

// lock RL shared memory and wait for it to be idle
static int mswaitidle (ShmMS *shmms)
{
    for (int i = 1000; i < 2000; i ++) {
        int rc = mslock (shmms);
        if (rc < 0) return rc;

        // if stuck in DONE state back to a process that is gone, mark it idle
        if ((shmms->command == SHMMSCMD_DONE) && (kill (shmms->cmdpid, 0) < 0) && (errno == ESRCH)) {
            shmms->command = SHMMSCMD_IDLE;
        }

        // if idle, return success status
        int cmd = shmms->command;
        if (cmd == SHMMSCMD_IDLE) return 0;

        // busy, unlock, wait then try again
        msunlk (shmms);
        rc = futex (&shmms->command, FUTEX_WAIT, cmd, NULL, NULL, 0);
        if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
    }

    fprintf (stderr, "mswaitidle: waited too long for idle (is %d by %d)\n",
            shmms->command, shmms->cmdpid);

    // jammed up somehow
    return - EDEADLK;
}

// wait for DONE state
// - call locked, returns locked, but may unlock during
static int mswaitdone (ShmMS *shmms)
{
    int svrpid = shmms->svrpid;
    int cmd;
    while ((cmd = shmms->command) != SHMMSCMD_DONE) {
        msunlk (shmms);
        int rc = futex (&shmms->command, FUTEX_WAIT, cmd, NULL, NULL, 0);
        if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
        rc = mslock (shmms);
        if (rc < 0) return rc;
        if (shmms->svrpid != svrpid) {
            fprintf (stderr, "mswaitdone: server restarted while processing function\n");
            msunlk (shmms);
            return - EDEADLK;
        }
    }
    return 0;
}

// lock RL/TM shared memory
// spawn z11rl/tm if it isn't running
static int mslock (ShmMS *shmms)
{
    if (mypid == 0) mypid = getpid ();

    // lock mutex and make sure server process is running
    while (true) {
        lockmutex (shmms);

        // if server is supposedly running, make sure it actually is
        if (shmms->svrpid != 0) {
            if (kill (shmms->svrpid, 0) >= 0) break;
            if (errno != ESRCH) {
                fprintf (stderr, "mslock: failed to probe pid %d: %m\n", shmms->svrpid);
                ABORT ();
            }
            fprintf (stderr, "mslock: old server %d died\n", shmms->svrpid);
            shmms->svrpid = 0;
        }

        // server not running, start it
        int rc = forkserver (shmms);
        if (rc < 0) {
            msunlk (shmms);
            return rc;
        }

        // wait for it to establish its pid whilst still locked so nothing else tries to start it up
        while (shmms->svrpid == 0) {
            int rc = futex (&shmms->svrpid, FUTEX_WAIT, 0, NULL, NULL, 0);
            if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
        }

        // unlock and re-check.  pause a little so we don't go in fast loop trying to create processes.
        msunlk (shmms);
        usleep (100000);
    }

    // all success
    return 0;
}

static void lockmutex (ShmMS *shmms)
{
    int tmpfutex = 0;
    while (! atomic_compare_exchange (&shmms->msfutex, &tmpfutex, mypid)) {
        if ((kill (tmpfutex, 0) < 0) && (errno == ESRCH)) {
            fprintf (stderr, "mslock: locker %d dead\n", tmpfutex);
        } else {
            int rc = futex (&shmms->msfutex, FUTEX_WAIT, tmpfutex, NULL, NULL, 0);
            if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
            tmpfutex = 0;
        }
    }
}

static int forkserver (ShmMS *shmms)
{
    // create process for the server
    int pid = fork ();

    if (pid > 0) return pid;

    if (pid < 0) {
        int rc = - errno;
        fprintf (stderr, "mslock: error forking: %m\n");
        return rc;
    }

    // get directory the Z11 stuff is in
    char *exebuf;
    char const *z11home = getenv ("Z11HOME");
    if (z11home == NULL) {
        exebuf = (char *) alloca (1024 + 10);
        int rc = readlink ("/proc/self/exe", exebuf, 1024);
        if (rc < 0) {
            fprintf (stderr, "mslock: error reading /proc/self/exe link: %m\n");
            ABORT ();
        }
        exebuf[rc] = 0;
        char *p = strrchr (exebuf, '/');
        if (p == NULL) {
            fprintf (stderr, "mslock: no slash in /proc/self/exe %s\n", exebuf);
            ABORT ();
        }
        *p = 0;
    } else {
        exebuf = (char *) alloca (strlen (z11home) + 10);
        strcpy (exebuf, z11home);
    }

    // form server program name
    char const *z11name = NULL;
    if (shmms == shmrl) z11name = "/z11rl";
    if (shmms == shmtm) z11name = "/z11tm";
    strcat (exebuf, z11name);

    // close any fds except stdin,stdout,stderr
    for (int i = 3; i < 1024; i ++) close (i);

    // create log file
    char const *homedir = getenv ("HOME");
    if (homedir == NULL) homedir = "/tmp";
    char *logname = (char *) alloca (strlen (homedir) + 30);
    sprintf (logname, "%s%s.log.%10u", homedir, z11name, (unsigned) time (NULL));
    int logfd = open (logname, O_CREAT | O_WRONLY, 0666);
    if (logfd < 0) {
        fprintf (stderr, "mslock: error creating %s: %m\n", logname);
        ABORT ();
    }

    // detach from parent
    if (daemon (0, 0) < 0) {
        fprintf (stderr, "mslock: error daemonizing: %m\n");
        ABORT ();
    }

    // inform caller server pid is now set up
    shmms->svrpid = getpid ();
    if (futex (&shmms->svrpid, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();

    // shift new log file in for stdout, stderr
    dup2 (logfd, 1);
    dup2 (logfd, 2);
    close (logfd);

    // run the daemon program
    char const *args[] = { exebuf, NULL };
    execve (exebuf, (char *const *) args, NULL);
    fprintf (stderr, "mslock: error spawning %s: %m\n", exebuf);
    ABORT ();
    return 0;
}

static void msunlk (ShmMS *shmms)
{
    int tmpfutex = mypid;
    if (! atomic_compare_exchange (&shmms->msfutex, &tmpfutex, 0)) ABORT ();
    if (futex (&shmms->msfutex, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
    if (futex (&shmms->command, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
}

////////////////////////
//  SERVER FUNCTIONS  //
////////////////////////

static int loadfile (ShmMS *shmms, char const *z11name, int drivesel,
    int (*setdrivetype) (void *param, int drivesel),
    int (*fileloaded) (void *param, int drivesel, int fd),
    void *param);

ShmMS *shmms_svr_initialize (bool resetit, char const *shmmsname, char const *z11name)
{
    mypid = getpid ();

    // open shared memory, create if not there
    int shmfd = shm_open (shmmsname, O_RDWR | O_CREAT, 0666);
    if (shmfd < 0) {
        fprintf (stderr, "%s: error creating %s: %m\n", z11name, shmmsname);
        ABORT ();
    }
    ShmMS *shmms;
    if (ftruncate (shmfd, sizeof *shmms) < 0) {
        fprintf (stderr, "%s: error extending %s: %m\n", z11name, shmmsname);
        ABORT ();
    }
    shmms = (ShmMS *) mmap (NULL, sizeof *shmms, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shmms == MAP_FAILED) {
        fprintf (stderr, "%s: error mmapping %s: %m\n", z11name, shmmsname);
        ABORT ();
    }

    if (resetit) memset (shmms, 0, sizeof *shmms);

    // put our pid in there in case we die when something is waiting for us to do something
    // exit if there is another one of us already running
    shmms_svr_mutexlock (shmms);
    int oldpid = shmms->svrpid;
    if ((oldpid != 0) && (oldpid != mypid)) {
        if (kill (oldpid, 0) >= 0) {
            shmms_svr_mutexunlk (shmms);
            fprintf (stderr, "%s: duplicate %s process %d\n", z11name, z11name, oldpid);
            exit (0);
        }
        if (errno != ESRCH) ABORT ();
    }
    fprintf (stderr, "%s: new %s process %d\n", z11name, z11name, mypid);
    shmms->svrpid = mypid;

    // we don't know about any loaded files so say drives are empty
    // ...unless there is an outstanding load request for a drive
    for (int i = 0; i < SHMMS_NDRIVES; i ++) {
        if (shmms->command != SHMMSCMD_LOAD + i) {
            shmms->drives[i].filename[0] = 0;
        }
    }
    return shmms;
}

// process commands (such as load/unload file) passed by client
void shmms_svr_proccmds (ShmMS *shmms, char const *z11name,
    int (*setdrivetype) (void *param, int drivesel),
    int (*fileloaded) (void *param, int drivesel, int fd),
    void (*unloadfile) (void *param, int drivesel),
    void *param)
{
    while (true) {

        // check for load/unload commands in shared memory waiting to be processed
        shmms_svr_mutexlock (shmms);
        int cmd = shmms->command;
        switch (cmd) {

            // something requesting file be loaded
            case SHMMSCMD_LOAD+0 ... SHMMSCMD_LOAD+SHMMS_NDRIVES-1: {
                int driveno = cmd - SHMMSCMD_LOAD;
                ShmMSDrive *dr = &shmms->drives[driveno];
                shmms_svr_mutexunlk (shmms);
                unloadfile (param, driveno);
                int rc = loadfile (shmms, z11name, driveno, setdrivetype, fileloaded, param);
                shmms_svr_mutexlock (shmms);
                if (rc >= 0) {
                    shmms->negerr = 0;
                } else {
                    dr->filename[0] = 0;
                    shmms->negerr = rc;
                }
                shmms->command = SHMMSCMD_DONE;
                if (futex (&shmms->command, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
                break;
            }

            // something requesting file be unloaded
            case SHMMSCMD_UNLD+0 ... SHMMSCMD_UNLD+SHMMS_NDRIVES-1: {
                int driveno = cmd - SHMMSCMD_UNLD;
                ShmMSDrive *dr = &shmms->drives[driveno];
                dr->filename[0] = 0;
                if (++ dr->fnseq == 0) ++ dr->fnseq; // 0 reserved for initial condition
                unloadfile (param, driveno);
                shmms->command = SHMMSCMD_DONE;
                if (futex (&shmms->command, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
                break;
            }

            // nothing to do, wait
            default: {
                shmms_svr_mutexunlk (shmms);
                int rc = futex (&shmms->command, FUTEX_WAIT, cmd, NULL, NULL, 0);
                if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
                shmms_svr_mutexlock (shmms);
            }
        }
        shmms_svr_mutexunlk (shmms);
    }
}

// load file in a drive
//  input:
//   drivesel = drive select
//  output:
//   returns < 0: errno
//          else: successful
static int loadfile (ShmMS *shmms, char const *z11name, int drivesel,
    int (*setdrivetype) (void *param, int drivesel),
    int (*fileloaded) (void *param, int drivesel, int fd),
    void *param)
{
    ASSERT ((drivesel >= 0) && (drivesel < SHMMS_NDRIVES));

    ShmMSDrive *dr = &shmms->drives[drivesel];
    bool readwrite = ! dr->readonly;
    char const *filenm = dr->filename;

    // maybe there is a drive type that needs decoding
    int rc = setdrivetype (param, drivesel);
    if (rc < 0) return rc;

    // open the file
    int fd = open (filenm, readwrite ? O_RDWR | O_CREAT : O_RDONLY, 0666);
    if (fd < 0) {
        fd = - errno;
        fprintf (stderr, "%s: [%u] error opening %s: %m\n", z11name, drivesel, filenm);
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
            fprintf (stderr, "%s: [%u] error locking %s: locked by pid %d\n", z11name, drivesel, filenm, (int) flockit.l_pid);
        } else {
            fprintf (stderr, "%s: [%u] error locking %s: %m\n", z11name, drivesel, filenm);
        }
        close (fd);
        return rc;
    }

    // tell server what the resulting fd is
    // extend it to full size (read/write disk files only)
    if (fileloaded (param, drivesel, fd) < 0) {
        int rc = - errno;
        ASSERT (rc < 0);
        fprintf (stderr, "%s: [%u] error extending %s: %m\n", z11name, drivesel, filenm);
        close (fd);
        return rc;
    }

    // all is good
    fprintf (stderr, "%s: [%u] loaded read%s file %s\n", z11name, drivesel, (readwrite ? "/write" : "-only"), filenm);
    return 0;
}

void shmms_svr_mutexlock (ShmMS *shmms)
{
    int newfutex = mypid;
    int tmpfutex = 0;
    while (true) {
        if (atomic_compare_exchange (&shmms->msfutex, &tmpfutex, newfutex)) break;
        if ((kill (tmpfutex, 0) < 0) && (errno == ESRCH)) {
            fprintf (stderr, "z11rl: locker %d dead\n", tmpfutex);
        } else {
            int rc = futex (&shmms->msfutex, FUTEX_WAIT, tmpfutex, NULL, NULL, 0);
            if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
            tmpfutex = 0;
        }
    }
}

void shmms_svr_mutexunlk (ShmMS *shmms)
{
    int newfutex = 0;
    int tmpfutex = mypid;
    if (! atomic_compare_exchange (&shmms->msfutex, &tmpfutex, newfutex)) ABORT ();
    if (futex (&shmms->msfutex, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
}
