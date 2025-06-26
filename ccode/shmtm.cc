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
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "futex.h"
#include "shmtm.h"
#include "z11defs.h"
#include "z11util.h"

static int mypid;
static int tmshmfd = -1;
static ShmTM *tmshm = NULL;
static uint32_t volatile *tmat;

static int tmunload (int drive);
static int tmwaitidle ();
static int tmwaitdone ();
static int tmlock ();
static void lockmutex ();
static int forkserver ();
static void tmunlk ();

// load/unload a disk
//  1) if filename empty, unload any existing file
//  2) set/clear readonly bit for the drive
//  3) if filename given, load in drive
int shmtm_load (int drive, bool readonly, char const *filename)
{
    if ((drive < 0) || (drive > 3)) ABORT ();

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
                fprintf (stderr, "shmtm_load: failed to expand %s: %m\n", filename);
                return rc;
            }
            fnlen = strlen (fnbuf);
            if (fnlen >= SHMTM_FNSIZE) {
                fprintf (stderr, "shmtm_load: filename too long %s\n", fnbuf);
                free (fnbuf);
                return -ERANGE;
            }
        }
    }

    // lock shared page and wait for it to be idle
    int rc = tmwaitidle ();

    // fnbuf == NULL means keep the same file loaded (probably just changing readonly status)
    // fnbuf == "" means unload any file that's in there

    // if empty filename, unload any previous file
    if ((rc >= 0) && (fnbuf != NULL) && (fnbuf[0] == 0)) rc = tmunload (drive);

    if (rc >= 0) {

        // mark entry readonly status
        ShmTMDrive *dr = &tmshm->drives[drive];
        dr->readonly = readonly;

        // if we aren't going to load anything, we're done
        if ((fnbuf != NULL) ? (fnbuf[0] == 0) : (dr->filename[0] == 0)) {
            tmunlk ();
        } else {

            // send a load command for this drive
            // if filename is NULL, keep same file (probably just changing readonly status)
            tmshm->cmdpid  = getpid ();
            tmshm->command = SHMTMCMD_LOAD + drive;
            if (fnbuf != NULL) {
                memcpy (dr->filename, fnbuf, ++ fnlen);
                if (++ dr->fnseq == 0) ++ dr->fnseq; // 0 reserved for initial condition
            }

            // unlock, wait for done, then re-lock
            rc = tmwaitdone ();
            if (rc >= 0) {

                // completed, get load status and say page is idle
                rc = tmshm->negerr;
                tmshm->command = SHMTMCMD_IDLE;
                tmunlk ();
            }
        }
    }
    if (fnbuf != NULL) free (fnbuf);
    return rc;
}

// get drive status
int shmtm_stat (int drive, char *buff, int size, uint32_t *curpos_r)
{
    if ((drive < 0) || (drive > 7)) ABORT ();
    if (tmat == NULL) {
        Z11Page *z11page = new Z11Page ();
        tmat = z11page->findev ("TM", NULL, NULL, false);
    }
    int statbits = tmlock ();
    if (statbits >= 0) {
        statbits = 0;
        ShmTMDrive *dr = &tmshm->drives[drive];
        uint32_t tm5 = ZRD(tmat[5]) >> drive;
        if (dr->filename[0] != 0) statbits |= TMSTAT_LOAD;  // something loaded
        if (dr->readonly)    statbits |= TMSTAT_WRPROT;     // write protected
        if (tm5 & TM5_TURS0) statbits |= TMSTAT_READY;      // ready (not skipping etc)
        statbits |= dr->fnseq * (TMSTAT_FNSEQ & - TMSTAT_FNSEQ);
        *curpos_r = dr->curpos;                             // current position (byte)
        if (size > 0) {
            char const *fn = dr->filename;                  // loaded filename
            strncpy (buff, fn, size);
            buff[size-1] = 0;
        }
        tmunlk ();
    }

    return statbits;
}

// unload given drive
// - call locked in IDLE, returns locked in IDLE, unlocks during
static int tmunload (int drive)
{
    tmshm->command = SHMTMCMD_UNLD + drive;
    int rc = tmwaitdone ();
    if (rc >= 0) tmshm->command = SHMTMCMD_IDLE;
    return rc;
}

// lock TM shared memory and wait for it to be idle
static int tmwaitidle ()
{
    for (int i = 1000; i < 2000; i ++) {
        int rc = tmlock ();
        if (rc < 0) return rc;

        // if stuck in DONE state back to a process that is gone, mark it idle
        if ((tmshm->command == SHMTMCMD_DONE) && (kill (tmshm->cmdpid, 0) < 0) && (errno == ESRCH)) {
            tmshm->command = SHMTMCMD_IDLE;
        }

        // if idle, return success status
        int cmd = tmshm->command;
        if (cmd == SHMTMCMD_IDLE) return 0;

        // busy, unlock, wait then try again
        tmunlk ();
        rc = futex (&tmshm->command, FUTEX_WAIT, cmd, NULL, NULL, 0);
        if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
    }

    fprintf (stderr, "tmwaitidle: waited too long for idle (is %d by %d)\n",
            tmshm->command, tmshm->cmdpid);

    // jammed up somehow
    return - EDEADLK;
}

// wait for DONE state
// - call locked, returns locked, but may unlock during
static int tmwaitdone ()
{
    int svrpid = tmshm->svrpid;
    int cmd;
    while ((cmd = tmshm->command) != SHMTMCMD_DONE) {
        tmunlk ();
        int rc = futex (&tmshm->command, FUTEX_WAIT, cmd, NULL, NULL, 0);
        if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
        rc = tmlock ();
        if (rc < 0) return rc;
        if (tmshm->svrpid != svrpid) {
            fprintf (stderr, "tmwaitdone: server restarted while processing function\n");
            tmunlk ();
            return - EDEADLK;
        }
    }
    return 0;
}

// lock TM shared memory
// spawn z11tm if it isn't running
static int tmlock ()
{
    if (mypid == 0) mypid = getpid ();

    // if shared mem not open yet, open it
    if (tmshm == NULL) {
        tmshmfd = shm_open (SHMTM_NAME, O_RDWR | O_CREAT, 0666);
        if (tmshmfd < 0) {
            int rc = - errno;
            fprintf (stderr, "tmlock: error opening %s: %m\n", SHMTM_NAME);
            return rc;
        }
        if (ftruncate (tmshmfd, sizeof *tmshm) < 0) {
            int rc = - errno;
            fprintf (stderr, "tmlock: error setting %s size: %m\n", SHMTM_NAME);
            close (tmshmfd);
            tmshmfd = -1;
            return rc;
        }

        // map the shared memory
        tmshm = (ShmTM *) mmap (NULL, sizeof *tmshm, PROT_READ | PROT_WRITE, MAP_SHARED, tmshmfd, 0);
        if (tmshm == MAP_FAILED) ABORT ();
    }

    // lock mutex and make sure server process is running
    while (true) {
        lockmutex ();
        if (tmshm->svrpid != 0) {
            if (kill (tmshm->svrpid, 0) >= 0) break;
            if (errno != ESRCH) {
                fprintf (stderr, "tmlock: failed to probe pid %d: %m\n", tmshm->svrpid);
                ABORT ();
            }
            fprintf (stderr, "tmlock: old server %d died\n", tmshm->svrpid);
            tmshm->svrpid = 0;
        }
        int rc = forkserver ();
        if (rc < 0) {
            tmunlk ();
            return rc;
        }
        tmshm->svrpid = rc;
        tmunlk ();
        usleep (100000);
    }

    // all success
    return 0;
}

static void lockmutex ()
{
    int tmpfutex = 0;
    while (! atomic_compare_exchange (&tmshm->tmfutex, &tmpfutex, mypid)) {
        if ((kill (tmpfutex, 0) < 0) && (errno == ESRCH)) {
            fprintf (stderr, "tmlock: locker %d dead\n", tmpfutex);
        } else {
            int rc = futex (&tmshm->tmfutex, FUTEX_WAIT, tmpfutex, NULL, NULL, 0);
            if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
            tmpfutex = 0;
        }
    }
}

static int forkserver ()
{
    // create process for the server
    int pid = fork ();

    if (pid > 0) return pid;

    if (pid < 0) {
        int rc = - errno;
        fprintf (stderr, "tmlock: error forking for z11tm: %m\n");
        return rc;
    }

    char exebuf[1024];
    char const *z11tm = getenv ("Z11TMEXE");
    if (z11tm == NULL) {
        int rc = readlink ("/proc/self/exe", exebuf, sizeof exebuf - 8);
        if (rc < 0) {
            fprintf (stderr, "tmlock: error reading /proc/self/exe link: %m\n");
            exit (255);
        }
        exebuf[rc] = 0;
        char *p = strrchr (exebuf, '/');
        if (p == NULL) {
            fprintf (stderr, "tmlock: no slash in /proc/self/exe %s\n", exebuf);
            exit (255);
        }
        strcpy (p, "/z11tm");
        z11tm = exebuf;
    }

    // close any fds except stdin,stdout,stderr
    for (int i = 3; i < 1024; i ++) close (i);

    // detach from parent
    setsid ();

    // run the z11tm daemon program
    char const *args[] = { z11tm, NULL };
    execve (z11tm, (char *const *) args, NULL);
    fprintf (stderr, "tmlock: error spawning %s: %m\n", z11tm);
    exit (255);
    return 0;
}

static void tmunlk ()
{
    int tmpfutex = mypid;
    if (! atomic_compare_exchange (&tmshm->tmfutex, &tmpfutex, 0)) ABORT ();
    if (futex (&tmshm->tmfutex, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
    if (futex (&tmshm->command, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
}
