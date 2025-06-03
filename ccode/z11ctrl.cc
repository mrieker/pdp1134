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

// pdp-11 controller main program
// see ./z11ctrl -? for options

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <tcl.h>
#include <unistd.h>

#include "cmd_pin.h"
#include "disassem.h"
#include "readprompt.h"
#include "tclmain.h"
#include "z11util.h"

// internal TCL commands
static Tcl_ObjCmdProc cmd_disasop;
static Tcl_ObjCmdProc cmd_gettod;
static Tcl_ObjCmdProc cmd_readchar;

static TclFunDef const fundefs[] = {
    { cmd_disasop,  "disasop",  "disassemble instruction" },
    { cmd_gettod,   "gettod",   "get current time in us precision" },
    { CMD_PIN },
    { cmd_readchar, "readchar", "read character with timeout" },
    { NULL, NULL, NULL }
};


int main (int argc, char **argv)
{
    setlinebuf (stdout);

    char const *logname = NULL;
    int tclargs = argc;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("  ./z11ctrl [-log <logfile>] [-real | -sim] [<scriptfile.tcl>]");
            puts ("");
            puts ("         -log : record output to given log file");
            puts ("     <scriptfile.tcl> : execute script then exit");
            puts ("                 else : read and process commands from stdin");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-log") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing filename after -log\n");
                return 1;
            }
            logname = argv[i];
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        tclargs = i;
        break;
    }

    // process tcl commands
    return tclmain (fundefs, argv[0], "z11ctrl", logname, getenv ("z11ctrlini"), argc - tclargs, argv + tclargs);
}



struct DisRdCtx {
    bool mmr0valid;
    bool pswvalid;
    uint8_t pdrvalid;
    uint8_t parvalid;
    uint16_t mmr0, par, pdr, psw;
};

static bool disread (void *param, uint32_t addr, uint16_t *data_r)
{
    extern Z11Page *z11page;
    DisRdCtx *drctx = (DisRdCtx *) param;

    // maybe we're given a physical address
    if (! (addr & disassem_PA)) {

        // virtual, see if MMU enabled
        if (! drctx->mmr0valid && z11page->dmaread (0777572, &drctx->mmr0)) {
            return false;
        }
        drctx->mmr0valid = true;
        if (! (drctx->mmr0 & 1)) {

            // no MMU, maybe set top 2 bits
            if (addr >= 0160000) addr |= 0760000;
        } else {

            // MMU enabled, see if KNL or USR mode
            if (! drctx->pswvalid && z11page->dmaread (0777776, &drctx->psw)) {
                return false;
            }
            drctx->pswvalid = true;

            // point to relocation register set based on mode
            uint32_t regs = (drctx->psw & 0140000) ? 0777600 : 0772300;

            uint16_t page = (addr >> 13) & 7;
            if (drctx->psw & 0140000) page += 8;

            // read descriptor for addressed page
            if ((drctx->pdrvalid != page) && z11page->dmaread (regs + ((page & 7) * 2), &drctx->pdr)) {
                return false;
            }
            drctx->pdrvalid = page;

            // check read access allowed and page length
            if (! (drctx->pdr & 2)) {
                return false;
            }
            uint16_t blok = (addr >> 6) & 0177;
            uint16_t len  = (drctx->pdr >> 7) & 0177;
            if (drctx->pdr & 8) {
                if (blok < len) {
                    return false;
                }
            } else {
                if (blok > len) {
                    return false;
                }
            }

            // get and apply relocation factor
            if ((drctx->parvalid != page) && z11page->dmaread (regs + 040 + ((page & 7) * 2), &drctx->par)) {
                return false;
            }
            drctx->parvalid = page;
            addr = (addr & 017777) + ((drctx->par & 07777) << 6);
        }
    }

    // don't read misc i/o registers in case of side effects
    // but allow reading processor registers (no side effects)
    addr &= 0777777;
    if (addr >= 0760000) {
        if ((addr >= 0777700) && (addr <= 0777717)) goto good;  // registers
        if ((addr >= 0772300) && (addr <= 0772377)) goto good;  // knl pdrs, pars
        if ((addr >= 0777570) && (addr <= 0777577)) goto good;  // sr/lr, mmr0, mmr2
        if ((addr >= 0777600) && (addr <= 0777677)) goto good;  // usr pdrs, pars
        if (addr == 0777776) goto good;                         // psw
        return false;                                           // other i/o registers
    }
good:;
    return z11page->dmaread (addr, data_r) == 0;
}

// disassemble instruciton
static int cmd_disasop (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int opcode   = -1;
    int operand1 = -1;
    int operand2 = -1;

    DisRdCtx drctx;
    drctx.mmr0valid = false;
    drctx.pswvalid = false;
    drctx.pdrvalid = 99;
    drctx.parvalid = 99;

    switch (objc) {

        // no operands, get instruction pointed to by program counter
        case 1: {
            int step = 1;
            uint16_t pcreg, val;
            if (! disread (&drctx, disassem_PA | 0777707, &pcreg)) goto err;
            step = 2;
            if (! disread (&drctx, pcreg, &val)) goto err;
            opcode   = val;
            step = 3;
            if (! disread (&drctx, pcreg + 2, &val)) goto err;
            operand1 = val;
            step = 4;
            if (! disread (&drctx, pcreg + 4, &val)) goto err;
            operand2 = val;
            break;
        err:;
            Tcl_SetResultF (interp, "unable to read PC or instruction (step %d)", step);
            return TCL_ERROR;
        }

        case 2: {
            char const *opstr = Tcl_GetString (objv[1]);
            if (strcasecmp (opstr, "help") == 0) {
                puts ("");
                puts ("  disasop [<opcode> [<operand1> [<operand2>]]]");
                puts ("     opcode = integer opcode");
                puts ("   operand1 = first operand");
                puts ("   operand2 = second operand");
                puts ("");
                puts ("  no operands means disassemble instruction program counter is pointing to");
                puts ("");
                puts ("   returns string, first char is digit giving total number of words used");
                puts ("     0 : illegal instruction");
                puts ("     1 : just opcode used");
                puts ("     2 : opcode and operand1 used");
                puts ("     3 : opcode, operand1 and operand2 used");
                puts ("");
                return TCL_OK;
            }
            int rc = Tcl_GetIntFromObj (interp, objv[1], &opcode);
            if (rc != TCL_OK) return rc;
            break;
        }
        case 3: {
            int rc = Tcl_GetIntFromObj (interp, objv[1], &opcode);
            if (rc == TCL_OK) rc = Tcl_GetIntFromObj (interp, objv[2], &operand1);
            if (rc != TCL_OK) return rc;
            break;
        }
        case 4: {
            int rc = Tcl_GetIntFromObj (interp, objv[1], &opcode);
            if (rc == TCL_OK) rc = Tcl_GetIntFromObj (interp, objv[2], &operand1);
            if (rc == TCL_OK) rc = Tcl_GetIntFromObj (interp, objv[3], &operand2);
            if (rc != TCL_OK) return rc;
            break;
        }
        default: {
            Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
            return TCL_ERROR;
        }
    }

    std::string str;
    int used = disassem (&str, (uint16_t) opcode, (uint16_t) operand1, (uint16_t) operand2, disread, &drctx);
    str.insert (0, 1, (char) ('0' + used));

    Tcl_SetResult (interp, strdup (str.c_str ()), (void (*) (char *)) free);

    return TCL_OK;
}

// get time of day
static int cmd_gettod (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    struct timeval nowtv;
    if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
    Tcl_SetResultF (interp, "%u.%06u", (uint32_t) nowtv.tv_sec, (uint32_t) nowtv.tv_usec);
    return TCL_OK;
}

// read character with timeout as an integer
// returns null string if timeout, else decimal integer
//  readchar file timeoutms
static int cmd_readchar (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 3) {
        char const *stri = Tcl_GetString (objv[1]);
        int fd = STDIN_FILENO;
        if (strcmp (stri, "stdin") != 0) {
            if (memcmp (stri, "file", 4) != 0) {
                Tcl_SetResultF (interp, "first argument not a file");
                return TCL_ERROR;
            }
            fd = atoi (stri + 4);
        }
        int tmoms;
        int rc = Tcl_GetIntFromObj (interp, objv[2], &tmoms);
        if (rc != TCL_OK) return rc;
        struct pollfd fds = { fd, POLLIN, 0 };
        rc = poll (&fds, 1, tmoms);
        uint8_t buff;
        if (rc > 0) rc = read (fd, &buff, 1);
        if (rc < 0) {
            if (errno == EINTR) return TCL_OK;
            Tcl_SetResultF (interp, "%m");
            return TCL_ERROR;
        }
        if (rc > 0) Tcl_SetResultF (interp, "%u", (uint32_t) buff);
        return TCL_OK;
    }

    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}
