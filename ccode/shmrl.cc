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
#include "shmrl.h"
#include "z11defs.h"
#include "z11util.h"

static int mypid;
static int rlshmfd = -1;
static ShmRL *rlshm = NULL;

static int rlunload (int drive);
static int rlwaitidle ();
static int rlwaitdone ();
static int rllock ();
static void lockmutex ();
static int forkserver ();
static void rlunlk ();

// load/unload a disk
//  1) unload any existing file
//  2) set/clear readonly bit for the drive
//  3) if filename given, load in drive
int shmrl_load (int drive, bool readonly, char const *filename)
{
    if ((drive < 0) || (drive > 3)) ABORT ();

    // insert cwd in front of filename and squeeze out /../ and /./
    char *fnbuf = NULL;
    int fnlen = 0;
    if (filename != NULL) {
        fnbuf = realpath (filename, NULL);
        if (fnbuf == NULL) {
            int rc = - errno;
            fprintf (stderr, "shmrl_load: failed to expand %s: %m\n", filename);
            return rc;
        }
        fnlen = strlen (fnbuf);
        if (fnlen >= SHMRL_FNSIZE) {
            free (fnbuf);
            fprintf (stderr, "shmrl_load: filename too long %s\n", fnbuf);
            return -ERANGE;
        }
    }

    // lock shared page and wait for it to be idle
    int rc = rlwaitidle ();

    // make sure any previous file is unloaded
    if (rc >= 0) rc = rlunload (drive);
    if (rc >= 0) {

        // mark entry readonly status
        ShmRLDrive *dr = &rlshm->drives[drive];
        dr->readonly = readonly;
        if (fnbuf == NULL) {

            // if no filename, we're done
            rlunlk ();
        } else {

            // send a load command for this drive
            rlshm->cmdpid  = getpid ();
            rlshm->command = SHMRLCMD_LOAD + drive;
            memcpy (dr->filename, fnbuf, ++ fnlen);
            if (++ dr->fnseq == 0) ++ dr->fnseq; // 0 reserved for initial condition

            // unlock, wait for done, then re-lock
            rc = rlwaitdone ();
            if (rc >= 0) {

                // completed, get load status and say page is idle
                rc = rlshm->lderrno;
                rlshm->command = SHMRLCMD_IDLE;
                rlunlk ();
            }
        }
    }
    if (fnbuf != NULL) free (fnbuf);
    return rc;
}

// get drive status
int shmrl_stat (int drive, char *buff, int size)
{
    if ((drive < 0) || (drive > 3)) ABORT ();
    int statbits = rllock ();
    if (statbits >= 0) {
        statbits = 0;
        ShmRLDrive *dr = &rlshm->drives[drive];
        if (dr->filename[0] != 0) statbits |= RLSTAT_LOAD;  // something loaded
        if (dr->readonly) statbits |= RLSTAT_WRPROT;        // write protected
        if (dr->ready)    statbits |= RLSTAT_READY;         // ready (not seeking etc)
        if (dr->fault)    statbits |= RLSTAT_FAULT;         // fault (drive error)
        statbits |= dr->fnseq * (RLSTAT_FNSEQ & - RLSTAT_FNSEQ);
        statbits |= (dr->lastposn / 128) * (RLSTAT_CYLNO & - RLSTAT_CYLNO);
        if (size > 0) {
            char const *fn = dr->filename;                  // loaded filename
            strncpy (buff, fn, size);
            buff[size-1] = 0;
        }
        rlunlk ();
    }

    return statbits;
}

// unload given drive
// - call locked in IDLE, returns locked in IDLE, unlocks during
static int rlunload (int drive)
{
    rlshm->command = SHMRLCMD_UNLD + drive;
    int rc = rlwaitdone ();
    if (rc >= 0) rlshm->command = SHMRLCMD_IDLE;
    return rc;
}

// lock RL shared memory and wait for it to be idle
static int rlwaitidle ()
{
    for (int i = 1000; i < 2000; i ++) {
        int rc = rllock ();
        if (rc < 0) return rc;

        // if stuck in DONE state back to a process that is gone, mark it idle
        if ((rlshm->command == SHMRLCMD_DONE) && (kill (rlshm->cmdpid, 0) < 0) && (errno == ESRCH)) {
            rlshm->command = SHMRLCMD_IDLE;
        }

        // if idle, return success status
        if (rlshm->command == SHMRLCMD_IDLE) return 0;

        // busy, unlock, wait then try again
        rlunlk ();
        usleep (i);
    }

    fprintf (stderr, "rlwaitidle: waited too long for idle (is %d by %d)\n",
            rlshm->command, rlshm->cmdpid);

    // jammed up somehow
    return - EDEADLK;
}

// wait for DONE state
// - call locked, returns locked, but may unlock during
static int rlwaitdone ()
{
    int svrpid = rlshm->svrpid;
    while (rlshm->command != SHMRLCMD_DONE) {
        rlunlk ();
        usleep (2000);
        int rc = rllock ();
        if (rc < 0) return rc;
        if (rlshm->svrpid != svrpid) {
            fprintf (stderr, "rlwaitdone: server restarted while processing function\n");
            rlunlk ();
            return - EDEADLK;
        }
    }
    return 0;
}

// lock RL shared memory
// spawn z11rl if it isn't running
static int rllock ()
{
    if (mypid == 0) mypid = getpid ();

    // if shared mem not open yet, open it
    if (rlshm == NULL) {
        rlshmfd = shm_open (SHMRL_NAME, O_RDWR | O_CREAT, 0666);
        if (rlshmfd < 0) {
            int rc = - errno;
            fprintf (stderr, "rllock: error opening %s: %m\n", SHMRL_NAME);
            return rc;
        }
        if (ftruncate (rlshmfd, sizeof *rlshm) < 0) {
            int rc = - errno;
            fprintf (stderr, "rllock: error setting %s size: %m\n", SHMRL_NAME);
            close (rlshmfd);
            rlshmfd = -1;
            return rc;
        }

        // map the shared memory
        rlshm = (ShmRL *) mmap (NULL, sizeof *rlshm, PROT_READ | PROT_WRITE, MAP_SHARED, rlshmfd, 0);
        if (rlshm == MAP_FAILED) ABORT ();
    }

    // lock mutex and make sure server process is running
    while (true) {
        lockmutex ();
        if (rlshm->svrpid != 0) {
            if (kill (rlshm->svrpid, 0) >= 0) break;
            if (errno != ESRCH) {
                fprintf (stderr, "rllock: failed to probe pid %d: %m\n", rlshm->svrpid);
                ABORT ();
            }
            fprintf (stderr, "rllock: old server %d died\n", rlshm->svrpid);
            rlshm->svrpid = 0;
        }
        int rc = forkserver ();
        if (rc < 0) {
            rlunlk ();
            return rc;
        }
        rlshm->svrpid = rc;
        rlunlk ();
        usleep (100000);
    }

    // all success
    return 0;
}

static void lockmutex ()
{
    int tmpfutex = 0;
    while (! atomic_compare_exchange (&rlshm->rlfutex, &tmpfutex, mypid)) {
        if ((kill (tmpfutex, 0) < 0) && (errno == ESRCH)) {
            fprintf (stderr, "rllock: locker %d dead\n", tmpfutex);
        } else {
            int rc = futex (&rlshm->rlfutex, FUTEX_WAIT, tmpfutex, NULL, NULL, 0);
            if ((rc < 0) && (errno != EAGAIN) && (errno != EINTR)) ABORT ();
            tmpfutex = 0;
        }
    }
}

static int forkserver ()
{
    int pid = fork ();

    if (pid > 0) return pid;

    if (pid < 0) {
        int rc = - errno;
        fprintf (stderr, "rllock: error forking for z11rl: %m\n");
        return rc;
    }

    char exebuf[1024];
    char const *z11rl = getenv ("Z11RLEXE");
    if (z11rl == NULL) {
        int rc = readlink ("/proc/self/exe", exebuf, sizeof exebuf - 8);
        if (rc < 0) {
            fprintf (stderr, "rllock: error reading /proc/self/exe link: %m\n");
            exit (255);
        }
        exebuf[rc] = 0;
        char *p = strrchr (exebuf, '/');
        if (p == NULL) {
            fprintf (stderr, "rllock: no slash in /proc/self/exe %s\n", exebuf);
            exit (255);
        }
        strcpy (p, "/z11rl");
        z11rl = exebuf;
    }
    char const *args[] = { z11rl, NULL };
    execve (z11rl, (char *const *) args, NULL);
    fprintf (stderr, "rllock: error spawning %s: %m\n", z11rl);
    exit (255);
    return 0;
}

static void rlunlk ()
{
    int tmpfutex = mypid;
    if (! atomic_compare_exchange (&rlshm->rlfutex, &tmpfutex, 0)) ABORT ();
    if (futex (&rlshm->rlfutex, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
}
