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

// inteface GUI java program to zynq fpga page

#include "GUIZynqPage.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "futex.h"
#include "pintable.h"
#include "shmrl.h"
#include "z11defs.h"
#include "z11util.h"

#define EXCKR(x) x; if (env->ExceptionCheck ()) return 0

#define FIELD(x,m) ((x & m) / (m & - m))

static int maxpinidx;
static int mypid;
static uint32_t volatile *pdpat;
static uint32_t volatile *kyat;
static uint32_t volatile *rlat;

/*
 * Class:     GUIZynqPage
 * Method:    open
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_open
  (JNIEnv *env, jclass klass)
{
    mypid = getpid ();
    z11page = new Z11Page ();
    pdpat = z11page->findev ("11", NULL, NULL, false, false);
    kyat  = z11page->findev ("KY", NULL, NULL, false, false);
    return 0;
}

/*
 * Class:     GUIZynqPage
 * Method:    step
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_step
  (JNIEnv *env, jclass klass)
{
    kyat[2] |= KY2_STEPREQ;
    return 0;
}

/*
 * Class:     GUIZynqPage
 * Method:    cont
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_cont
  (JNIEnv *env, jclass klass)
{
    kyat[2] &= ~ KY2_HALTREQ;
    return 0;
}

/*
 * Class:     GUIZynqPage
 * Method:    halt
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_halt
  (JNIEnv *env, jclass klass)
{
    kyat[2] |= KY2_HALTREQ;
    return 0;
}

/*
 * Class:     GUIZynqPage
 * Method:    reset
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_reset
  (JNIEnv *env, jclass klass)
{
    kyat[2] |= KY2_HALTREQ;     // so it halts when started back up
    pdpat[Z_RA] |= a_man_ac_lo_out_h | a_man_dc_lo_out_h;
    usleep (200000);
    pdpat[Z_RA] &= ~ a_man_dc_lo_out_h;
    usleep (1000);
    pdpat[Z_RA] &= ~ a_man_ac_lo_out_h;
    return 0;
}

/*
 * Class:     GUIZynqPage
 * Method:    addr
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_addr
  (JNIEnv *env, jclass klass)
{
    return FIELD (pdpat[Z_RK], k_lataddr);
}

/*
 * Class:     GUIZynqPage
 * Method:    data
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_data
  (JNIEnv *env, jclass klass)
{
    return FIELD (pdpat[Z_RL], l_latdata);
}

/*
 * Class:     GUIZynqPage
 * Method:    getlr
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_getlr
  (JNIEnv *env, jclass klass)
{
    return (kyat[1] >> 16) & 0xFFFF;
}

/*
 * Class:     GUIZynqPage
 * Method:    getsr
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_getsr
  (JNIEnv *env, jclass klass)
{
    return kyat[1] & 0xFFFF;
}

/*
 * Class:     GUIZynqPage
 * Method:    running
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_running
  (JNIEnv *env, jclass klass)
{
    if (! (kyat[2] & KY2_HALTED)) return 1;     //  1 = running
    return (kyat[2] & KY2_HALTINS) ? -1 : 0;    // -1 = halt instr; 0 = requested halt
}

/*
 * Class:     GUIZynqPage
 * Method:    setsr
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_GUIZynqPage_setsr
  (JNIEnv *env, jclass klass, jint data)
{
    kyat[1] = (kyat[1] & 0xFFFF0000) | (data & 0xFFFF);
}

/*
 * Class:     GUIZynqPage
 * Method:    rdmem
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_rdmem
  (JNIEnv *env, jclass klass, jint addr)
{
    uint16_t data;
    uint32_t rc = z11page->dmaread (addr, &data);
    if (rc & KY3_DMATIMO) return -1;
    if (rc & KY3_DMAPERR) return -2;
    if (rc != 0) abort ();
    return (uint32_t) data;
}

/*
 * Class:     GUIZynqPage
 * Method:    wrmem
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_wrmem
  (JNIEnv *env, jclass klass, jint addr, jint data)
{
    return z11page->dmawrite (addr, data) ? data : -1;
}

/*
 * Class:     GUIZynqPage
 * Method:    pinfind
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_pinfind
  (JNIEnv *env, jclass klass, jstring namestr)
{
    char const *nameutf = EXCKR (env->GetStringUTFChars (namestr, NULL));
    int index;
    for (index = 0; pindefs[index].name[0] != 0; index ++) {
        if (strcasecmp (pindefs[index].name, nameutf) == 0) goto gotit;
    }
    index = -1;
gotit:;
    if (maxpinidx < index) maxpinidx = index;
    env->ReleaseStringUTFChars (namestr, nameutf);
    return index;
}

/*
 * Class:     GUIZynqPage
 * Method:    pinget
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_pinget
  (JNIEnv *env, jclass klass, jint index)
{
    if ((index < 0) || (index > maxpinidx)) ABORT ();
    PinDef const *pte = pindefs + index;
    uint32_t volatile *ptr = pindev (pte->dev) + pte->reg;
    return (*ptr & pte->mask) / (pte->mask & - pte->mask);
}

/*
 * Class:     GUIZynqPage
 * Method:    pinset
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL Java_GUIZynqPage_pinset
  (JNIEnv *env, jclass klass, jint index, jint value)
{
    if ((index < 0) || (index > maxpinidx)) ABORT ();
    PinDef const *pte = pindefs + index;
    if (! pte->writ) return false;
    uint32_t volatile *ptr = pindev (pte->dev) + pte->reg;
    uint32_t lobit = pte->mask & - pte->mask;
    uint32_t val = ((uint32_t) value * lobit) & pte->mask;
    *ptr = (*ptr & ~ pte->mask) | val;
    return true;
}

/////////////////
//  RL ACCESS  //
/////////////////

#define RLSTAT_LOAD 1
#define RLSTAT_WRPROT 2
#define RLSTAT_READY 4
#define RLSTAT_FAULT 8

static char const *rlfileutfstrings[4];
static int rlshmfd = -1;
static jstring rlfilejavstrings[4];
static ShmRL *rlshm = NULL;

static int rlunload (int drive);
static int rlwaitidle ();
static int rlwaitdone ();
static int rllock ();
static void rlunlk ();

/*
 * Class:     GUIZynqPage
 * Method:    rlload
 * Signature: (IZLjava/lang/String;)Ljava/lang/String;
 *
 *  1) unload any existing file
 *  2) set readonly bit for the drive
 *  3) if filename given, load in drive
 */
JNIEXPORT jstring JNICALL Java_GUIZynqPage_rlload
  (JNIEnv *env, jclass klass, jint drive, jboolean readonly, jstring filename)
{
    if ((drive < 0) || (drive > 3)) ABORT ();
    int rc = rlwaitidle ();
    if (rc >= 0) rc = rlunload (drive);
    if (rc >= 0) {
        rlshm->drives[drive].readonly = readonly;
        if (filename == NULL) {
            rlunlk ();
            return NULL;
        }
        rlshm->cmdpid  = getpid ();
        rlshm->command = SHMRLCMD_LOAD + drive;
        char const *filenameutf = EXCKR (env->GetStringUTFChars (filename, NULL));
        strncpy (rlshm->drives[drive].filename, filenameutf, sizeof rlshm->drives[drive].filename);
        env->ReleaseStringUTFChars (filename, filenameutf);
        rc = rlwaitdone ();
        if (rc >= 0) {
            rc = rlshm->lderrno;
            rlshm->command = SHMRLCMD_IDLE;
            rlunlk ();
            if (rc == 0) return NULL;
        }
    }
    return env->NewStringUTF (strerror (rc < 0 ? - rc : rc));
}

/*
 * Class:     GUIZynqPage
 * Method:    rlstat
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_rlstat
  (JNIEnv *env, jclass klass, jint drive)
{
    if ((drive < 0) || (drive > 3)) ABORT ();

    if (rlat == NULL) rlat = z11page->findev ("RL", NULL, NULL, false);
    int statbits = rllock ();
    if (statbits >= 0) {
        statbits = 0;
        if (rlshm->drives[drive].filename[0] != 0) statbits |= RLSTAT_LOAD; // something loaded
        if (rlshm->drives[drive].readonly) statbits |= RLSTAT_WRPROT;       // write protected
        uint32_t bits = rlat[4] >> drive;
        if (bits &  1) statbits |= RLSTAT_READY;                            // ready (not seeking etc)
        if (bits & 16) statbits |= RLSTAT_FAULT;                            // fault (drive error)
        rlunlk ();
    }

    return statbits;
}

/*
 * Class:     GUIZynqPage
 * Method:    rlfile
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_GUIZynqPage_rlfile
  (JNIEnv *env, jclass klass, jint drive)
{
    // make sure drive number is in range
    if ((drive < 0) || (drive > 3)) ABORT ();

    // lock the shared memory
    if (rllock () < 0) {
        return NULL;
    }

    // see if we have a string from last call
    if (rlfilejavstrings[drive] != NULL) {

        // see if it matches what is currently loaded in drive
        if (strcmp (rlfileutfstrings[drive], rlshm->drives[drive].filename) == 0) {

            // it matches, just return same string from last call
            rlunlk ();
            jstring fn = rlfilejavstrings[drive];
            return fn;
        }

        // something different, free off the old string
        env->ReleaseStringUTFChars (rlfilejavstrings[drive], rlfileutfstrings[drive]);
        env->DeleteGlobalRef (rlfilejavstrings[drive]);
        rlfilejavstrings[drive] = NULL;
        rlfileutfstrings[drive] = NULL;
    }

    // make a new java string from filename in shared memory
    jstring fn = env->NewStringUTF (rlshm->drives[drive].filename);
    rlunlk ();

    // lock that string and save corresponding C string
    fn = (jstring) EXCKR (env->NewGlobalRef (fn));
    rlfileutfstrings[drive] = EXCKR (env->GetStringUTFChars (fn, NULL));
    rlfilejavstrings[drive] = fn;
    return fn;
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

    fprintf (stderr, "rlwaitidle: waited too long for idle (is %d)\n", rlshm->command);

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
    // if shared mem not open yet, open it
tryit:;
    if (rlshm == NULL) {
        rlshmfd = shm_open (SHMRL_NAME, O_RDWR, 0);
        if (rlshmfd < 0) {
            if (errno != ENOENT) {
                int rc = - errno;
                fprintf (stderr, "rllock: error opening %s: %m\n", SHMRL_NAME);
                return rc;
            }

            // does not exist, fork 'z11rl -notcl' command to create it
            int pid = fork ();
            if (pid == 0) {
                char const *z11rl = getenv ("GUI_Z11RL");
                char const *args[] = { z11rl, "-notcl", NULL };
                execve (z11rl, (char *const *) args, NULL);
                fprintf (stderr, "error spawning %s: %m\n", z11rl);
                exit (1);
            }

            if (pid < 0) {
                int rc = - errno;
                fprintf (stderr, "rllock: error forking: %m\n");
                return rc;
            }

            // wait for it to create the shared memory
            for (int i = 0; i < 100; i ++) {
                usleep (100000);
                rlshmfd = shm_open (SHMRL_NAME, O_RDWR, 0);
                if (rlshmfd >= 0) break;
                if (errno != ENOENT) {
                    int rc = - errno;
                    fprintf (stderr, "rllock: error re-opening %s: %m\n", SHMRL_NAME);
                    return rc;
                }
            }
            if (rlshmfd < 0) {
                fprintf (stderr, "rllock: starting z11rl failed to create %s\n", SHMRL_NAME);
                return - ENOENT;
            }
        }

        // map the memory and wait for it to be initialized
        rlshm = (ShmRL *) mmap (NULL, sizeof *rlshm, PROT_READ | PROT_WRITE, MAP_SHARED, rlshmfd, 0);
        if (rlshm == MAP_FAILED) ABORT ();

        for (int i = 0; rlshm->svrpid == 0; i ++) {
            usleep (1000);
            if (i > 1000) {
                munmap (rlshm, sizeof *rlshm);
                close (rlshmfd);
                rlshmfd = -1;
                fprintf (stderr, "rllock: z11rl failed to initialize\n");
                return - ETIMEDOUT;
            }
        }
    }

    // lock mutex
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

    // make sure server (z11rl) still running
    // if not, restart it
    int pid = rlshm->svrpid;
    if ((pid != 0) && (kill (pid, 0) < 0) && (errno == ESRCH)) {
        fprintf (stderr, "rllock: z11rl process %d died\n", pid);
        rlshm->svrpid = pid = 0;
    }
    if (pid == 0) {
        shm_unlink (SHMRL_NAME);
        rlunlk ();
        munmap (rlshm, sizeof *rlshm);
        rlshm = NULL;
        close (rlshmfd);
        rlshmfd = -1;
        goto tryit;
    }

    // all success
    return 0;
}

static void rlunlk ()
{
    int tmpfutex = mypid;
    if (! atomic_compare_exchange (&rlshm->rlfutex, &tmpfutex, 0)) ABORT ();
    if (futex (&rlshm->rlfutex, FUTEX_WAKE, 1000000000, NULL, NULL, 0) < 0) ABORT ();
}
