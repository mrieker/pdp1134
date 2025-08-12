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
#include "shmms.h"
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
    z11page->stepreq ();
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
    z11page->contreq ();
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
    z11page->haltreq ();
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
    z11page->resetit ();
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
    return FIELD (ZRD(pdpat[Z_RK]), k_lataddr);
}

/*
 * Class:     GUIZynqPage
 * Method:    data
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_data
  (JNIEnv *env, jclass klass)
{
    return FIELD (ZRD(pdpat[Z_RL]), l_latdata);
}

/*
 * Class:     GUIZynqPage
 * Method:    getlr
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_getlr
  (JNIEnv *env, jclass klass)
{
    return (ZRD(kyat[1]) >> 16) & 0xFFFF;
}

/*
 * Class:     GUIZynqPage
 * Method:    getsr
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_getsr
  (JNIEnv *env, jclass klass)
{
    return ((ZRD(kyat[1]) & KY_SWITCHES) / (KY_SWITCHES & - KY_SWITCHES)) |
        (((ZRD(kyat[2]) & KY2_SR1716) / (KY2_SR1716 & - KY2_SR1716)) << 16);
}

/*
 * Class:     GUIZynqPage
 * Method:    running
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_running
  (JNIEnv *env, jclass klass)
{
    z11page->dmalock ();                    // make sure snapregs not running
    uint32_t ky2 = ZRD(kyat[2]);                 // get halted state flags
    z11page->dmaunlk ();                    // allow snapregs to run
    if (! (ky2 & KY2_HALTED)) return 1;     //  1 = running
    return (ky2 & KY2_HALTINS) ? -1 : 0;    // -1 = halt instr; 0 = requested halt
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
    z11page->dmalock ();                    // make sure snapregs not running
    ZWR(kyat[1], (ZRD(kyat[1]) & ~ KY_SWITCHES) | (data & 0177777) * (KY_SWITCHES & - KY_SWITCHES));
    ZWR(kyat[2], (ZRD(kyat[2]) & ~ KY2_SR1716)  | (data >> 16)     * (KY2_SR1716  & - KY2_SR1716));
    z11page->dmaunlk ();                    // allow snapregs to run
}

/*
 * Class:     GUIZynqPage
 * Method:    snapregs
 * Signature: (I[S)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_snapregs
  (JNIEnv *env, jclass klass, jint addr, jshortArray regs)
{
    jsize count = EXCKR (env->GetArrayLength (regs));
    jboolean iscopy;
    jshort *regarray = EXCKR (env->GetShortArrayElements (regs, &iscopy));
    if (regarray == NULL) ABORT ();
    ASSERT (sizeof (jshort) == sizeof (uint16_t));
    int rc = z11page->snapregs (addr, count, (uint16_t *) regarray);
    env->ReleaseShortArrayElements (regs, regarray, 0);
    return rc;
}

/*
 * Class:     GUIZynqPage
 * Method:    rdmem
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_rdmem
  (JNIEnv *env, jclass klass, jint addr)
{
    uint32_t fpgamode = (ZRD(pdpat[Z_RA]) & a_fpgamode) / (a_fpgamode & - a_fpgamode);
    if ((fpgamode != FM_SIM) && (fpgamode != FM_REAL)) return -3;

    try {
        uint16_t data;
        uint32_t rc = z11page->dmaread (addr, &data);
        if (rc & KY3_DMATIMO) return -1;
        if (rc & KY3_DMAPERR) return -2;
        if (rc != 0) abort ();
        return (uint32_t) data;
    } catch (Z11DMAException &de) {
        fprintf (stderr, "Java_GUIZynqPage_rdmem: exception %s\n", de.what ());
        return -3;
    }
}

/*
 * Class:     GUIZynqPage
 * Method:    wrmem
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_wrmem
  (JNIEnv *env, jclass klass, jint addr, jint data)
{
    uint32_t fpgamode = (ZRD(pdpat[Z_RA]) & a_fpgamode) / (a_fpgamode & - a_fpgamode);
    if ((fpgamode != FM_SIM) && (fpgamode != FM_REAL)) return -3;

    try {
        return z11page->dmawrite (addr, data) ? data : -1;
    } catch (Z11DMAException &de) {
        fprintf (stderr, "Java_GUIZynqPage_wrmem: exception %s\n", de.what ());
        return -3;
    }
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
    return (ZRD(*ptr) & pte->mask) / (pte->mask & - pte->mask);
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
    ZWR(*ptr, (ZRD(*ptr) & ~ pte->mask) | val);
    return true;
}

////////////////////////////
//  MASS STORAGE DEVICES  //
////////////////////////////

static char const *rhfileutfstrings[8];
static char const *rlfileutfstrings[4];
static char const *tmfileutfstrings[8];
static jstring rhfilejavstrings[8];
static jstring rlfilejavstrings[4];
static jstring tmfilejavstrings[8];

/*
 * Class:     GUIZynqPage
 * Method:    msload
 * Signature: (IZLjava/lang/String;)Ljava/lang/String;
 *
 *  1) if filename empty, unload any existing file
 *  2) set/clear readonly bit for the drive
 *  3) if filename given, load in drive
 */
JNIEXPORT jstring JNICALL Java_GUIZynqPage_msload
  (JNIEnv *env, jclass klass, jint ctlid, jint drive, jboolean readonly, jstring filename)
{
    char const *fnutf = (filename == NULL) ? NULL : EXCKR (env->GetStringUTFChars (filename, NULL));
    int rc = shmms_load (ctlid, drive, readonly, fnutf);
    if (filename != NULL) env->ReleaseStringUTFChars (filename, fnutf);
    return (rc >= 0) ? NULL : env->NewStringUTF (strerror (- rc));
}

/*
 * Class:     GUIZynqPage
 * Method:    msstat
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_msstat
  (JNIEnv *env, jclass klass, jint ctlid, jint drive)
{
    uint32_t curpos;
    return shmms_stat (ctlid, drive, NULL, 0, &curpos);
}

/*
 * Class:     GUIZynqPage
 * Method:    msposn
 * Signature: (II)J
 */
JNIEXPORT jlong JNICALL Java_GUIZynqPage_msposn
  (JNIEnv *env, jclass klass, jint ctlid, jint drive)
{
    uint32_t curpos;
    int rc = shmms_stat (ctlid, drive, NULL, 0, &curpos);
    if (rc < 0) return rc;
    return (uint64_t) curpos;
}

/*
 * Class:     GUIZynqPage
 * Method:    msfile
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_GUIZynqPage_msfile
  (JNIEnv *env, jclass klass, jint ctlid, jint drive)
{
    char fnbuf[SHMMS_FNSIZE];
    uint32_t curpos;
    int rc = shmms_stat (ctlid, drive, fnbuf, sizeof fnbuf, &curpos);
    if (rc < 0) return NULL;

    char const **msfileutfstrings = NULL;
    jstring *msfilejavstrings = NULL;
    switch (ctlid) {
        case SHMMS_CTLID_RH: {
            ASSERT ((drive >= 0) && (drive <= 7));
            msfileutfstrings = rhfileutfstrings;
            msfilejavstrings = rhfilejavstrings;
        }
        case SHMMS_CTLID_RL: {
            ASSERT ((drive >= 0) && (drive <= 3));
            msfileutfstrings = rlfileutfstrings;
            msfilejavstrings = rlfilejavstrings;
        }
        case SHMMS_CTLID_TM: {
            ASSERT ((drive >= 0) && (drive <= 7));
            msfileutfstrings = tmfileutfstrings;
            msfilejavstrings = tmfilejavstrings;
            break;
        }
        default: ABORT ();
    }

    // see if we have a string from last call
    if (msfilejavstrings[drive] != NULL) {

        // see if it matches what is currently loaded in drive
        // if so, just return same string from last call
        if (strcmp (msfileutfstrings[drive], fnbuf) == 0) {
            return msfilejavstrings[drive];
        }

        // something different, free off the old string
        env->ReleaseStringUTFChars (msfilejavstrings[drive], msfileutfstrings[drive]);
        env->DeleteGlobalRef (msfilejavstrings[drive]);
        msfilejavstrings[drive] = NULL;
        msfileutfstrings[drive] = NULL;
    }

    // make a new java string from filename in shared memory
    jstring fn = env->NewStringUTF (fnbuf);

    // lock that string and save corresponding C string
    fn = (jstring) EXCKR (env->NewGlobalRef (fn));
    msfileutfstrings[drive] = EXCKR (env->GetStringUTFChars (fn, NULL));
    msfilejavstrings[drive] = fn;
    return fn;
}
