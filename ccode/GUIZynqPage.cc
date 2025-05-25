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

#include <unistd.h>

#include "z11defs.h"
#include "z11util.h"

#define EXCKR(x) x; if (env->ExceptionCheck ()) return 0

#define FIELD(x,m) ((x & m) / (m & - m))

static Z11Page *z11page;
static uint32_t volatile *pdpat;
static uint32_t volatile *slat;

/*
 * Class:     GUIZynqPage
 * Method:    open
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_open
  (JNIEnv *env, jclass klass)
{
    z11page = new Z11Page ();
    pdpat = z11page->findev ("11", NULL, NULL, false, false);
    slat  = z11page->findev ("SL", NULL, NULL, false, false);
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
    slat[2] |= SL2_STEPREQ;
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
    slat[2] &= ~ SL2_HALTREQ;
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
    slat[2] |= SL2_HALTREQ;
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
    slat[2] |= SL2_HALTREQ;     // so it halts when started back up
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
    return FIELD (pdpat[Z_RD], d_dev_a_h);
}

/*
 * Class:     GUIZynqPage
 * Method:    data
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_data
  (JNIEnv *env, jclass klass)
{
    return FIELD (pdpat[Z_RG], g_dev_d_h);
}

/*
 * Class:     GUIZynqPage
 * Method:    lreg
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_lreg
  (JNIEnv *env, jclass klass)
{
    return (slat[1] >> 16) & 0xFFFF;
}

/*
 * Class:     GUIZynqPage
 * Method:    sreg
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_sreg
  (JNIEnv *env, jclass klass)
{
    return slat[1] & 0xFFFF;
}

/*
 * Class:     GUIZynqPage
 * Method:    running
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_GUIZynqPage_running
  (JNIEnv *env, jclass klass)
{
    return ! (slat[2] & SL2_HALTED);
}

/*
 * Class:     GUIZynqPage
 * Method:    setsr
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_GUIZynqPage_setsr
  (JNIEnv *env, jclass klass, jint data)
{
    slat[1] = (slat[1] & 0xFFFF0000) | (data & 0xFFFF);
}

/*
 * Class:     GUIZynqPage
 * Method:    rdmem
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_rdmem
  (JNIEnv *env, jclass klass, jint addr)
{
    // shortcut to read switch register
    if (addr == 0777570) return slat[1] & 0xFFFF;

    // long way around for everything else
    uint16_t data;
    return z11page->dmaread (addr, &data) ? ((uint32_t) data) : -1;
}

/*
 * Class:     GUIZynqPage
 * Method:    wrmem
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_GUIZynqPage_wrmem
  (JNIEnv *env, jclass klass, jint addr, jint data)
{
    return z11page->dmawrite (addr & 0777777, data & 0177777) ? 0 : -1;
}
