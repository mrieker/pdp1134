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

#ifndef _TAPELIB_H
#define _TAPELIB_H

#include <stdint.h>

#include "shmms.h"

struct TapeCtrlr;

struct TapeDrive {
    char fn[SHMMS_FNSIZE];
    int fd;
    ShmMSDrive *dr;
    TapeCtrlr *ctrlr;
    uint32_t drsel;

    void ctor (TapeCtrlr *ctrlr, uint32_t drsel);
    void startrewind (bool unload);
    void waitrewind ();
    int readfwd (uint16_t &mtbc, uint32_t &mtma);
    int wrdata (uint16_t &mtbc, uint32_t &mtma);
    int wrmark ();
    int skipfwd ();
    int skiprev ();

    static void *timerthread (void *zhis);

private:
    bool unload;

    void readerror (int rc, int nbytes);
    void readerror2 (uint32_t reclen, uint32_t reclen2);
    void writerror (int rc, int nbytes);
    void dmaerror (uint32_t mtma);
};

struct TapeCtrlr {
    bool fastio;
    char const *progname;
    ShmMS *shmms;
    TapeDrive drives[SHMMS_NDRIVES];
    uint32_t tapelen;                        // number bytes in reel of tape

    TapeCtrlr (ShmMS *shmms, char const *progname);

    virtual void iothread () = 0;
    virtual void updstbits () = 0;

    void proccmds ();
    void resetall ();
    void lockit ();
    void unlkit ();

    static int setdrivetype (void *zhis, int drivesel);
    static int fileloaded (void *zhis, int drivesel, int fd);
    static void unloadfile (void *zhis, int drivesel);

private:
    static void *iothreadwrap (void *zhis);
};

#endif
