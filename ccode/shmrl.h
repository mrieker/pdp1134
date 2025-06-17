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

#ifndef _SHMRL_H
#define _SHMRL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SHMRL_NAME "/shm_zturn11_rl"

#define RLSTAT_LOAD   000000001
#define RLSTAT_WRPROT 000000002
#define RLSTAT_READY  000000004
#define RLSTAT_FAULT  000000010
#define RLSTAT_FNSEQ  000007760
#define RLSTAT_CYLNO  007770000
#define RLSTAT_RL01   010000000

#define SHMRLCMD_IDLE 0     // not doing anything
#define SHMRLCMD_LOAD 1     // load drive with filename,readonly
#define SHMRLCMD_UNLD 5     // unload drive
#define SHMRLCMD_DONE 9     // done loading/unloading

#define SHMRL_FNSIZE 1000   // make sure it all fits on one page

struct ShmRLDrive {
    uint16_t lastposn;      // last cyl/head/sec
    bool readonly;          // write protected
    bool ready;             // drive ready
    bool fault;             // drive fault
    bool rl01;              // is an RL01
    uint8_t fnseq;          // incremented each change in filename
    char filename[SHMRL_FNSIZE];  // "" for unloaded, else filename loaded
};

struct ShmRL {
    int rlfutex;        // pid of what has it locked
    int svrpid;         // z11rl process id
    int cmdpid;         // what is sending command in some command
    int command;        // command to be processed by z11rl
    int negerr;         // negative errno for load commands (0 if success)
    ShmRLDrive drives[4];
};

int shmrl_load (int drive, bool readonly, char const *filename);
int shmrl_stat (int drive, char *buff, int size);

#endif
