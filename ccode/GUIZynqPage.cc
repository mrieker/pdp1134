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
    return ((kyat[1] & KY_SWITCHES) / (KY_SWITCHES & - KY_SWITCHES)) |
        (((kyat[2] & KY2_SR1716) / (KY2_SR1716 & - KY2_SR1716)) << 16);
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
    data &= 0777777;
    kyat[1] = (kyat[1] & ~ KY_SWITCHES) | (data & 0177777) * (KY_SWITCHES & - KY_SWITCHES);
    kyat[2] = (kyat[2] & ~ KY2_SR1716)  | (data >> 16) * (KY2_SR1716 & - KY2_SR1716);
}

/*
 * Class:     GUIZynqPage
 * Method:    rdmem
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_rdmem
  (JNIEnv *env, jclass klass, jint addr)
{
    uint32_t fpgamode = (pdpat[Z_RA] & a_fpgamode) / (a_fpgamode & - a_fpgamode);
    if ((fpgamode != FM_SIM) && (fpgamode != FM_REAL)) return -3;

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
    uint32_t fpgamode = (pdpat[Z_RA] & a_fpgamode) / (a_fpgamode & - a_fpgamode);
    if ((fpgamode != FM_SIM) && (fpgamode != FM_REAL)) return -3;

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

static char const *rlfileutfstrings[4];
static jstring rlfilejavstrings[4];

/*
 * Class:     GUIZynqPage
 * Method:    rlload
 * Signature: (IZLjava/lang/String;)Ljava/lang/String;
 *
 *  1) if filename empty, unload any existing file
 *  2) set/clear readonly bit for the drive
 *  3) if filename given, load in drive
 */
JNIEXPORT jstring JNICALL Java_GUIZynqPage_rlload
  (JNIEnv *env, jclass klass, jint drive, jboolean readonly, jstring filename)
{
    char const *fnutf = (filename == NULL) ? NULL : EXCKR (env->GetStringUTFChars (filename, NULL));
    int rc = shmrl_load (drive, readonly, fnutf);
    if (filename != NULL) env->ReleaseStringUTFChars (filename, fnutf);
    return (rc >= 0) ? NULL : env->NewStringUTF (strerror (- rc));
}

/*
 * Class:     GUIZynqPage
 * Method:    rlstat
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_rlstat
  (JNIEnv *env, jclass klass, jint drive)
{
    return shmrl_stat (drive, NULL, 0);
}

/*
 * Class:     GUIZynqPage
 * Method:    rlfile
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_GUIZynqPage_rlfile
  (JNIEnv *env, jclass klass, jint drive)
{
    char fnbuf[SHMRL_FNSIZE];
    int rc = shmrl_stat (drive, fnbuf, sizeof fnbuf);
    if (rc < 0) return NULL;

    // see if we have a string from last call
    if (rlfilejavstrings[drive] != NULL) {

        // see if it matches what is currently loaded in drive
        // if so, just return same string from last call
        if (strcmp (rlfileutfstrings[drive], fnbuf) == 0) {
            return rlfilejavstrings[drive];
        }

        // something different, free off the old string
        env->ReleaseStringUTFChars (rlfilejavstrings[drive], rlfileutfstrings[drive]);
        env->DeleteGlobalRef (rlfilejavstrings[drive]);
        rlfilejavstrings[drive] = NULL;
        rlfileutfstrings[drive] = NULL;
    }

    // make a new java string from filename in shared memory
    jstring fn = env->NewStringUTF (fnbuf);

    // lock that string and save corresponding C string
    fn = (jstring) EXCKR (env->NewGlobalRef (fn));
    rlfileutfstrings[drive] = EXCKR (env->GetStringUTFChars (fn, NULL));
    rlfilejavstrings[drive] = fn;
    return fn;
}
