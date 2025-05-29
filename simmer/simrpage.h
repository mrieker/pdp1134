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

#ifndef _SIMRPAGE_H
#define _SIMRPAGE_H

#include <pthread.h>
#include <stdint.h>

#define SIMRFUNC_IDLE 1
#define SIMRFUNC_READ 2
#define SIMRFUNC_WRITE 3
#define SIMRFUNC_DONE 4

#define SIMRNAME "/simrpage"

struct SimrPage {
    uint32_t baadfood[800];
    int simmerpid;
    pthread_cond_t simrcondid;  // waiting for idle
    pthread_cond_t simrcondrw;  // waiting for read/write
    pthread_cond_t simrconddn;  // waiting for done
    pthread_mutex_t simrmutex;
    uint32_t simrdata;
    uint32_t simrfunc;
    uint32_t simrindx;
};

uint32_t volatile *simrpage_init (int *simrfd_r);
uint32_t simrpage_read (uint32_t volatile *addr);
void simrpage_write (uint32_t volatile *addr, uint32_t data);

#endif
