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

// Performs TM11/TU10 tape I/O for the PDP-11 Zynq I/O board
// Runs as a background daemon when a file is loaded in a drive
// ...either with z11ctrl tmload command or GUI screen

//  ./z11tm [-reset]

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shmms.h"
#include "tapelib.h"
#include "z11defs.h"
#include "z11util.h"

#define TAPELEN 20000000

struct MSTapeCtrlr : TapeCtrlr {
    int debug;
    uint32_t volatile *tmat;

    MSTapeCtrlr (ShmMS *shmms);
    virtual void iothread ();
    virtual void updstbits ();
};

#define RFLD(n,m) ((ZRD(tmat[n]) & m) / (m & - m))

int main (int argc, char **argv)
{
    bool resetit = (argc > 1) && (strcasecmp (argv[1], "-reset") == 0);

    // access fpga register set for the TM-11 controller
    // lock it so we are only process accessing it
    z11page = new Z11Page ();
    uint32_t volatile *tmat = z11page->findev ("TM", NULL, NULL, true, false);

    // open shared memory, create if not there
    ShmMS *shmms = shmms_svr_initialize (resetit, SHMMS_NAME_TM, "z11tm");
    shmms_svr_mutexunlk (shmms);

    // enable tm11.v to process io instructions from pdp
    ZWR(tmat[4], (tmat[4] & TM4_FAST) | TM4_ENAB);

    char const *dbgenv = getenv ("z11tm_debug");

    // initialize tape library
    MSTapeCtrlr *tapectrlr = new MSTapeCtrlr (shmms);
    tapectrlr->debug = (dbgenv == NULL) ? 0 : atoi (dbgenv);
    tapectrlr->tmat  = tmat;

    fprintf (stderr, "z11tm*: debug=%d\n", tapectrlr->debug);

    // process commands from shared memory (load, unload)
    tapectrlr->proccmds ();

    return 0;
}

MSTapeCtrlr::MSTapeCtrlr (ShmMS *shmms)
    : TapeCtrlr (shmms, "z11tm")
{ }

// do the tape file I/O
void MSTapeCtrlr::iothread ()
{
    while (true) {

        // wait for pdp set go mtc[00] or power clear mtc[12]
        if (debug > 2) fprintf (stderr, "z11tm: waiting\n");
        z11page->waitint (ZGINT_TM);
        if (debug > 2) fprintf (stderr, "z11tm: woken\n");

        // block tape from being unloaded from under us
        this->lockit ();
        if (debug > 2) fprintf (stderr, "z11tm: locked\n");

        // see if command from pdp waiting to be processed
        uint32_t mtcmtsat = ZRD(tmat[1]);

        uint32_t mtcmts = mtcmtsat & 0x6F7E0000;

        this->fastio = (ZRD(tmat[4]) & TM4_FAST) != 0;

        if (mtcmtsat & 0x10000000) {
            if (debug > 0) fprintf (stderr, "z11tm: power clear\n");

            // power clear
            this->resetall ();                      // reset tape drives

            mtcmts |= 0x00800000;                   // controller ready
            ZWR(tmat[1], mtcmts);

        } else if (mtcmtsat & 0x00010000) {

            // go - update status at beginning of function, clearing errors and go bit
            ZWR(tmat[1], mtcmts);

            uint32_t drivesel = (mtcmtsat & 0x07000000) >> 24;
            TapeDrive *td = &this->drives[drivesel];
            ShmMSDrive *dr = td->dr;

            // maybe wait for previously started rewind/unload to complete
            if (debug > 2) fprintf (stderr, "z11tm: [%u] wait rewind\n", drivesel);
            td->waitrewind ();

            uint16_t mtbrc = RFLD (2, TM2_MTBRC);
            uint32_t mtcma = RFLD (2, TM2_MTCMA) | ((mtcmts & 0x00300000) >> 4);

            if (debug > 0) fprintf (stderr, "z11tm: [%u] start MTC=%06o MTS=%06o MTBRC=%06o MTCMA=%06o\n",
                    drivesel, mtcmts >> 16, mtcmts & 0xFFFF, mtbrc, mtcma);

            uint32_t func = (mtcmts >> 17) & 7;
            switch (func) {

                // read
                case 1: {
                    if (debug > 1) fprintf (stderr, "z11tm: [%u] read bc=%u pos=%u\n", drivesel, 65536 - mtbrc, dr->curposn);
                    int32_t rc = td->readfwd (mtbrc, mtcma);
                    switch (rc) {
                        case -1:
                        case -2: goto crcerror;
                        case -3: goto nxmerror;
                        case  0: { mtcmts |= 0x4000; break; }   // tape mark
                        case  1: { break; }                     // normal read
                        case  2: { mtcmts |= 0x2000; break; }   // record too long
                        default: ABORT ();
                    }
                    break;
                }

                // write
                case 2:
                // write with extended interrecord gap
                case 6: {
                    if (debug > 1) fprintf (stderr, "z11tm: [%u] write bc=%u pos=%u\n", drivesel, 65536 - mtbrc, dr->curposn);
                    if (dr->readonly) {
                        mtcmts |= 0x8000;   // illegal command
                    } else {
                        int rc = td->wrdata (mtbrc, mtcma);
                        if (rc < 0) {
                            switch (rc) {
                                case -3: goto nxmerror;
                                case -4: goto crcerror;
                                default: ABORT ();
                            }
                        }
                    }
                    break;
                }

                // write tape mark
                case 3: {
                    if (debug > 1) fprintf (stderr, "z11tm: [%u] write mark pos=%u\n", drivesel, dr->curposn);
                    if (dr->readonly) {
                        mtcmts |= 0x8000;   // illegal command
                    } else {
                        int rc = td->wrmark ();
                        if (rc < 0) {
                            switch (rc) {
                                case -3: goto nxmerror;
                                case -4: goto crcerror;
                                default: ABORT ();
                            }
                        }
                    }
                    break;
                }

                // space forward
                case 4: {
                    if (debug > 1) fprintf (stderr, "z11tm: [%u] space fwd beg rc=%u pos=%u\n", drivesel, 65536 - mtbrc, dr->curposn);
                    do {
                        int rc = td->skipfwd ();
                        if (debug > 1) fprintf (stderr, "z11tm: [%u] space fwd end rc=%d pos=%u\n", drivesel, rc, dr->curposn);
                        switch (rc) {
                            case -1: goto crcerror;     // read error
                            case  0: break;             // record skipped
                            case  1: {
                                mtcmts |= 0x4000;       // tape mark
                                mtbrc ++;               // count includes tape mark
                                goto done;
                            }
                            default: ABORT ();
                        }
                        mtbrc ++;
                        ZWR(tmat[2], ((uint32_t) mtcma << 16) | mtbrc);
                    } while (mtbrc != 0);
                    break;
                }

                // space reverse
                case 5: {
                    if (debug > 1) fprintf (stderr, "z11tm: [%u] space rev beg rc=%u pos=%u\n", drivesel, 65536 - mtbrc, dr->curposn);
                    do {
                        int rc = td->skiprev ();
                        if (debug > 1) fprintf (stderr, "z11tm: [%u] space rev end rc=%d pos=%u\n", drivesel, rc, dr->curposn);
                        switch (rc) {
                            case -1: goto crcerror;     // read error
                            case  0: break;             // record skipped
                            case  1: {
                                mtcmts |= 0x4000;       // tape mark
                                mtbrc ++;               // count includes tape mark
                                goto done;
                            }
                            case  2: goto done;         // beginning of tape
                            default: ABORT ();
                        }
                        mtbrc ++;
                        ZWR(tmat[2], ((uint32_t) mtcma << 16) | mtbrc);
                    } while (mtbrc != 0);
                    break;
                }

                // unload
                case 0:
                // rewind
                case 7: {
                    if (debug > 1) fprintf (stderr, "z11tm: [%u] rewind unld=%d pos=%u\n", drivesel, func == 0, dr->curposn);
                    td->startrewind (func == 0);
                    break;
                }

                default: ABORT ();
            }
            goto done;

        crcerror:;
            mtcmts |= 0x2000;               // crc error
            goto done;

        nxmerror:;
            mtcmts |= 0x80;                 // non-existent memory

        done:;
            this->updstbits ();             // update low status bits [06:00]

            mtcmts &= ~02000;               // update end-of-tape
            if (dr->curposn > TAPELEN) mtcmts |= 02000;

            mtcmts = (mtcmts & ~ 0x300000) | 0x800000 | ((mtcma << 4) & 0x300000);

            if (debug > 0) fprintf (stderr, "z11tm: [%u]  done MTC=%06o MTS=%06o MTBRC=%06o MTCMA=%06o\n",
                    drivesel, mtcmts >> 16, mtcmts & 0xFFFF, mtbrc, mtcma);

            ZWR(tmat[2], ((uint32_t) mtcma << 16) | mtbrc);

            ZWR(tmat[1], mtcmts);
        }

        this->unlkit ();
    }
}

// update low status bits, ie, mts[06:00]
// they are on a per-drive basis so there is an individual bit for each
void MSTapeCtrlr::updstbits ()
{
    uint32_t turs = 0;                      // figure out which drives are ready
    uint32_t rews = 0;                      // figure out which drives are rewinding
    uint32_t wrls = 0;                      // figure out which drives are write-locked
    uint32_t bots = 0;                      // figure out which drives are at beg of tape
    uint32_t sels = 0;                      // figure out which drives are selected
    for (int i = 0; i < 8; i ++) {
        TapeDrive *td = &this->drives[i];
        ShmMSDrive *dr = td->dr;
        if (td->fd >= 0)        turs |= 1U << i;
        if (dr->rewendsat != 0) rews |= 1U << i;
        if (dr->readonly)       wrls |= 1U << i;
        if (dr->curposn == 0)   bots |= 1U << i;
        if (td->fd >= 0)        sels |= 1U << i;
    }
    turs &= ~ rews;
    ZWR(tmat[5], bots * TM5_BOTS0 | wrls * TM5_WRLS0 | rews * TM5_REWS0 | turs * TM5_TURS0);
    ZWR(tmat[6], sels * TM6_SELS0);
}
