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

#ifndef _SHMTM_H
#define _SHMTM_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SHMTM_NAME "/shm_zturn11_tm"

#define TMSTAT_LOAD   000000001
#define TMSTAT_WRPROT 000000002
#define TMSTAT_READY  000000004
#define TMSTAT_FNSEQ  000007760

#define SHMTMCMD_IDLE 0     // not doing anything
#define SHMTMCMD_LOAD 1     // load drive with filename,readonly
#define SHMTMCMD_UNLD 9     // unload drive
#define SHMTMCMD_DONE 17    // done loading/unloading

#define SHMTM_FNSIZE 500    // make sure it all fits on one page

struct ShmTMDrive {
    uint32_t curpos;        // current byte position within file
    bool readonly;          // write protected
    uint8_t fnseq;          // incremented each change in filename
    char filename[SHMTM_FNSIZE];  // "" for unloaded, else filename loaded
};

struct ShmTM {
    int tmfutex;        // pid of what has it locked
    int svrpid;         // z11tm process id
    int cmdpid;         // what is sending command in some command
    int command;        // command to be processed by z11tm
    int negerr;         // negative errno for load commands (0 if success)
    ShmTMDrive drives[8];
};

int shmtm_load (int drive, bool readonly, char const *filename);
int shmtm_stat (int drive, char *buff, int size, uint32_t *curpos_r);

#endif
