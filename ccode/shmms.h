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

#ifndef _SHMMS_H
#define _SHMMS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SHMMS_NAME_RH "/shm_zturn11_rh"
#define SHMMS_NAME_RL "/shm_zturn11_rl"
#define SHMMS_NAME_TM "/shm_zturn11_tm"

#define SHMMS_CTLID_RH (('R'<<8)|'H')
#define SHMMS_CTLID_RL (('R'<<8)|'L')
#define SHMMS_CTLID_TM (('T'<<8)|'M')

#define MSSTAT_LOAD   000000001
#define MSSTAT_WRPROT 000000002
#define MSSTAT_READY  000000004
#define MSSTAT_FAULT  000000010
#define MSSTAT_FNSEQ  000007760
#define MSSTAT_RL01   000010000

#define SHMMSCMD_IDLE  0    // not doing anything
#define SHMMSCMD_LOAD  1    // load drive with filename,readonly
#define SHMMSCMD_UNLD  9    // unload drive
#define SHMMSCMD_DONE 17    // done loading/unloading

#define SHMMS_FNSIZE 480    // make sure it all fits on one page
#define SHMMS_NDRIVES 8

struct ShmMSDrive {
    uint64_t rewbganat;     // rewind began at (0 if not rewinding)
    uint64_t rewendsat;     // rewind ends at (0 if not rewinding)
    uint32_t curposn;       // current position
    bool readonly;          // write protected
    bool rl01;              // is an RL01 (or RP04)
    uint8_t fnseq;          // incremented each change in filename
    char filename[SHMMS_FNSIZE];  // "" for unloaded, else filename loaded
};

struct ShmMS {
    int msfutex;        // pid of what has it locked
    int svrpid;         // z11rh/rl/tm process id
    int cmdpid;         // what is sending command in some command
    int command;        // command to be processed by z11rh/rl/tm
    int negerr;         // negative errno for load commands (0 if success)
    int ndrives;        // actual number of drives supported by z11rh/rl/tm
    ShmMSDrive drives[SHMMS_NDRIVES];
};

int shmms_load (int ctlid, int drive, bool readonly, char const *filename);
int shmms_stat (int ctlid, int drive, char *buff, int size, uint32_t *curpos_r);

ShmMS *shmms_svr_initialize (bool resetit, char const *shmmsname, char const *z11name);
void shmms_svr_proccmds (ShmMS *shmms, char const *z11name,
    int (*setdrivetype) (void *param, int drivesel),
    int (*fileloaded) (void *param, int drivesel, int fd),
    void (*unloadfile) (void *param, int drivesel),
    void *param);
void shmms_svr_mutexlock (ShmMS *shmms);
void shmms_svr_mutexunlk (ShmMS *shmms);

#endif
