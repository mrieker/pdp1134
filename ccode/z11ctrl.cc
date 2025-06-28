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

#include "disassem.h"
#include "pintable.h"
#include "readprompt.h"
#include "shmrl.h"
#include "shmtm.h"
#include "tclmain.h"
#include "z11util.h"

extern Z11Page *z11page;

// internal TCL commands
static Tcl_ObjCmdProc cmd_disasop;
static Tcl_ObjCmdProc cmd_gettod;
static Tcl_ObjCmdProc cmd_pin;
static Tcl_ObjCmdProc cmd_readchar;
static Tcl_ObjCmdProc cmd_rlload;
static Tcl_ObjCmdProc cmd_rlstat;
static Tcl_ObjCmdProc cmd_rlunload;
static Tcl_ObjCmdProc cmd_tmload;
static Tcl_ObjCmdProc cmd_tmstat;
static Tcl_ObjCmdProc cmd_tmunload;
static Tcl_ObjCmdProc cmd_waitint;

static TclFunDef const fundefs[] = {
    { cmd_disasop,  "disasop",  "disassemble instruction" },
    { cmd_gettod,   "gettod",   "get current time in us precision" },
    { cmd_pin,      "pin",      "direct access to signals on zynq page" },
    { cmd_readchar, "readchar", "read character with timeout" },
    { cmd_rlload,   "rlload",   "load file in RL drive" },
    { cmd_rlstat,   "rlstat",   "get RL drive status" },
    { cmd_rlunload, "rlunload", "unload file from RL drive" },
    { cmd_tmload,   "tmload",   "load file in TM drive" },
    { cmd_tmstat,   "tmstat",   "get TM drive status" },
    { cmd_tmunload, "tmunload", "unload file from TM drive" },
    { cmd_waitint,  "waitint",  "wait for interrupt" },
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

    z11page = new Z11Page ();

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
            uint16_t len  = (drctx->pdr >> 8) & 0177;
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

// direct access to signals on the zynq page
int cmd_pin (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if ((objc == 2) && (strcasecmp (Tcl_GetString (objv[1]), "help") == 0)) {
        puts ("");
        puts ("  pin list - list all the pins");
        puts ("");
        puts ("  pin {get pin ...} | {set pin val ...} | {test pin ...} ...");
        puts ("    defaults to get");
        puts ("    get returns integer value");
        puts ("    test returns -1: undefined; 0: read-only; 1: read/write");
        puts ("");
        return TCL_OK;
    }

    if ((objc == 2) && (strcasecmp (Tcl_GetString (objv[1]), "list") == 0)) {
        for (PinDef const *pte = pindefs; pte->name[0] != 0; pte ++) {
            printf ("  %-16s", pte->name);
            uint32_t volatile *ptr = pindev (pte->dev);
            int width = __builtin_popcount (pte->mask);
            int lobit = pte->lobit;
            int hibit = lobit + width - 1;
            if (hibit != lobit) {
                printf ("[%02d:%02d]", hibit, lobit);
            } else if (lobit != 0) {
                printf ("[%02d]   ", lobit);
            } else {
                printf ("       ");
            }
            printf ("  %c%c[%2u]  %08X\n",
                (ZRD(ptr[0]) >> 24) & 0xFFU, (ZRD(ptr[0]) >> 16) & 0xFFU, pte->reg, pte->mask);
        }
        return TCL_OK;
    }

    bool setmode = false;
    bool testmode = false;
    int ngotvals = 0;
    Tcl_Obj *gotvals[objc];

    for (int i = 0; ++ i < objc;) {
        char const *name = Tcl_GetString (objv[i]);

        if (strcasecmp (name, "get") == 0) {
            setmode = false;
            testmode = false;
            continue;
        }
        if (strcasecmp (name, "set") == 0) {
            setmode = true;
            testmode = false;
            continue;
        }
        if (strcasecmp (name, "test") == 0) {
            setmode = false;
            testmode = true;
            continue;
        }

        // signalname
        PinDef const *pte;
        int hibit = 0;
        int lobit = 0;
        int namelen = strlen (name);
        char const *p = strrchr (name, '[');
        if (p != NULL) {
            char *q;
            hibit = lobit = strtol (p + 1, &q, 10);
            if (*q == ':') {
                lobit = strtol (q + 1, &q, 10);
            }
            if ((q[0] != ']') || (q[1] != 0) || (hibit < lobit)) goto badname;
            namelen = p - name;
        }
        for (pte = pindefs; pte->name[0] != 0; pte ++) {
            if ((strncasecmp (pte->name, name, namelen) == 0) && (pte->name[namelen] == 0)) goto gotit;
        }
    badname:;
        if (! testmode) {
            Tcl_SetResultF (interp, "bad pin name %s", name);
            return TCL_ERROR;
        }
        gotvals[ngotvals++] = Tcl_NewIntObj (-1);
        continue;
    gotit:;
        uint32_t mask  = pte->mask;
        int width = __builtin_popcount (pte->mask);
        if (p != NULL) {
            if ((hibit - lobit >= width) || (lobit < pte->lobit)) goto badname;
            mask  = ((mask & - mask) << (lobit - pte->lobit)) * (1U << (hibit - lobit));
            width = hibit - lobit + 1;
        }
        uint32_t volatile *ptr = pindev (pte->dev) + pte->reg;
        bool writeable = pte->writ;

        if (testmode) {
            gotvals[ngotvals++] = Tcl_NewIntObj (writeable);
            continue;
        }

        if (setmode) {
            if (! writeable) {
                Tcl_SetResultF (interp, "pin %s not settable", name);
                return TCL_ERROR;
            }
            if (++ i >= objc) {
                Tcl_SetResultF (interp, "missing pin value for set %s", name);
                return TCL_ERROR;
            }
            int val;
            int rc  = Tcl_GetIntFromObj (interp, objv[i], &val);
            if (rc != TCL_OK) return rc;
            if ((uint32_t) val >= 1ULL << width) {
                Tcl_SetResultF (interp, "value 0%o too big for %s", val, name);
                return TCL_ERROR;
            }
            ZWR(*ptr, (ZRD(*ptr) & ~ mask) | ((uint32_t) val) * (mask & - mask));
        } else {
            uint32_t val = (ZRD(*ptr) & mask) / (mask & - mask);
            gotvals[ngotvals++] = Tcl_NewIntObj (val);
        }
    }

    if (ngotvals > 0) {
        if (ngotvals < 2) {
            Tcl_SetObjResult (interp, gotvals[0]);
        } else {
            Tcl_SetObjResult (interp, Tcl_NewListObj (ngotvals, gotvals));
        }
    }
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

static int cmd_rlload (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 2) {
        char const *stri = Tcl_GetString (objv[1]);
        if (strcasecmp (stri, "help") == 0) {
            puts ("");
            puts ("  rlload [-readonly] <drive> <filename>");
            puts ("");
            return TCL_OK;
        }
    }
    bool readonly = false;
    char const *filename = NULL;
    int drive = -1;
    for (int i = 0; ++ i < objc;) {
        char const *stri = Tcl_GetString (objv[i]);
        if (strcasecmp (stri, "-readonly") == 0) {
            readonly = true;
            continue;
        }
        if (stri[0] == '-') {
            Tcl_SetResultF (interp, "unknown option %s", stri);
            return TCL_ERROR;
        }
        if (drive < 0) {
            int rc = Tcl_GetIntFromObj (interp, objv[i], &drive);
            if (rc != TCL_OK) return rc;
            if ((drive < 0) || (drive > 3)) {
                Tcl_SetResultF (interp, "drive number %d out of range 0..3", drive);
                return TCL_ERROR;
            }
            continue;
        }
        if (filename != NULL) {
            Tcl_SetResultF (interp, "unknown argument %s", stri);
            return TCL_ERROR;
        }
        filename = stri;
    }
    if (filename == NULL) {
        Tcl_SetResultF (interp, "missing drive and/or filename");
        return TCL_ERROR;
    }
    int rc = shmrl_load (drive, readonly, filename);
    if (rc < 0) {
        Tcl_SetResultF (interp, "%s", strerror (- rc));
        return TCL_ERROR;
    }
    return TCL_OK;
}

static int cmd_rlstat (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 2) {
        char const *stri = Tcl_GetString (objv[1]);
        if (strcasecmp (stri, "help") == 0) {
            puts ("");
            puts ("  rlstat <drive> [cylinder] [fault] [file] [readonly] [ready] [type]");
            puts ("");
            return TCL_OK;
        }
    }
    if (objc > 1) {
        int drive = -1;
        int rc = Tcl_GetIntFromObj (interp, objv[1], &drive);
        if (rc != TCL_OK) return rc;
        if ((drive < 0) || (drive > 3)) {
            Tcl_SetResultF (interp, "drive number %d out of range 0..3", drive);
            return TCL_ERROR;
        }

        char fnbuf[SHMRL_FNSIZE];
        rc = shmrl_stat (drive, fnbuf, sizeof fnbuf);
        if (rc < 0) {
            Tcl_SetResultF (interp, "%s", strerror (- rc));
            return TCL_ERROR;
        }

        int nvals = 0;
        Tcl_Obj *vals[objc];
        for (int i = 1; ++ i < objc;) {
            char const *stri = Tcl_GetString (objv[i]);
            if (strcasecmp (stri, "cylinder") == 0) {
                int val = (rc & RLSTAT_CYLNO) / (RLSTAT_CYLNO & - RLSTAT_CYLNO);
                vals[nvals++] = Tcl_NewIntObj (val);
                continue;
            }
            if (strcasecmp (stri, "fault") == 0) {
                int val = (rc & RLSTAT_FAULT) / RLSTAT_FAULT;
                vals[nvals++] = Tcl_NewIntObj (val);
                continue;
            }
            if (strcasecmp (stri, "file") == 0) {
                vals[nvals++] = Tcl_NewStringObj (fnbuf, -1);
                continue;
            }
            if (strcasecmp (stri, "readonly") == 0) {
                int val = (rc & RLSTAT_WRPROT) / RLSTAT_WRPROT;
                vals[nvals++] = Tcl_NewIntObj (val);
                continue;
            }
            if (strcasecmp (stri, "ready") == 0) {
                int val = (rc & RLSTAT_READY) / RLSTAT_READY;
                vals[nvals++] = Tcl_NewIntObj (val);
                continue;
            }
            if (strcasecmp (stri, "type") == 0) {
                vals[nvals++] = Tcl_NewStringObj ((rc & RLSTAT_RL01) ? "RL01" : "RL02", -1);
                continue;
            }
            Tcl_SetResultF (interp, "unknown keyword %s", stri);
            return TCL_ERROR;
        }
        if (nvals > 0) {
            if (nvals < 2) {
                Tcl_SetObjResult (interp, vals[0]);
            } else {
                Tcl_SetObjResult (interp, Tcl_NewListObj (nvals, vals));
            }
        }
        return TCL_OK;
    }
    Tcl_SetResultF (interp, "bad number args");
    return TCL_ERROR;
}

static int cmd_rlunload (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 2) {
        char const *stri = Tcl_GetString (objv[1]);
        if (strcasecmp (stri, "help") == 0) {
            puts ("");
            puts ("  rlunload <drive>");
            puts ("");
            return TCL_OK;
        }

        int drive = -1;
        int rc = Tcl_GetIntFromObj (interp, objv[1], &drive);
        if (rc != TCL_OK) return rc;
        if ((drive < 0) || (drive > 3)) {
            Tcl_SetResultF (interp, "drive number %d out of range 0..3", drive);
            return TCL_ERROR;
        }

        rc = shmrl_load (drive, false, "");
        if (rc < 0) {
            Tcl_SetResultF (interp, "%s", strerror (- rc));
            return TCL_ERROR;
        }
        return TCL_OK;
    }
    Tcl_SetResultF (interp, "bad number args");
    return TCL_ERROR;
}

static int cmd_tmload (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 2) {
        char const *stri = Tcl_GetString (objv[1]);
        if (strcasecmp (stri, "help") == 0) {
            puts ("");
            puts ("  tmload [-readonly] <drive> <filename>");
            puts ("");
            return TCL_OK;
        }
    }
    bool readonly = false;
    char const *filename = NULL;
    int drive = -1;
    for (int i = 0; ++ i < objc;) {
        char const *stri = Tcl_GetString (objv[i]);
        if (strcasecmp (stri, "-readonly") == 0) {
            readonly = true;
            continue;
        }
        if (stri[0] == '-') {
            Tcl_SetResultF (interp, "unknown option %s", stri);
            return TCL_ERROR;
        }
        if (drive < 0) {
            int rc = Tcl_GetIntFromObj (interp, objv[i], &drive);
            if (rc != TCL_OK) return rc;
            if ((drive < 0) || (drive > 7)) {
                Tcl_SetResultF (interp, "drive number %d out of range 0..7", drive);
                return TCL_ERROR;
            }
            continue;
        }
        if (filename != NULL) {
            Tcl_SetResultF (interp, "unknown argument %s", stri);
            return TCL_ERROR;
        }
        filename = stri;
    }
    if (filename == NULL) {
        Tcl_SetResultF (interp, "missing drive and/or filename");
        return TCL_ERROR;
    }
    int rc = shmtm_load (drive, readonly, filename);
    if (rc < 0) {
        Tcl_SetResultF (interp, "%s", strerror (- rc));
        return TCL_ERROR;
    }
    return TCL_OK;
}

static int cmd_tmstat (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 2) {
        char const *stri = Tcl_GetString (objv[1]);
        if (strcasecmp (stri, "help") == 0) {
            puts ("");
            puts ("  tmstat <drive> [kbytes] [file] [readonly] [ready]");
            puts ("");
            return TCL_OK;
        }
    }
    if (objc > 1) {
        int drive = -1;
        int rc = Tcl_GetIntFromObj (interp, objv[1], &drive);
        if (rc != TCL_OK) return rc;
        if ((drive < 0) || (drive > 7)) {
            Tcl_SetResultF (interp, "drive number %d out of range 0..7", drive);
            return TCL_ERROR;
        }

        char fnbuf[SHMTM_FNSIZE];
        uint32_t curpos;
        rc = shmtm_stat (drive, fnbuf, sizeof fnbuf, &curpos);
        if (rc < 0) {
            Tcl_SetResultF (interp, "%s", strerror (- rc));
            return TCL_ERROR;
        }

        int nvals = 0;
        Tcl_Obj *vals[objc];
        for (int i = 1; ++ i < objc;) {
            char const *stri = Tcl_GetString (objv[i]);
            if (strcasecmp (stri, "kbytes") == 0) {
                int val = (curpos + 1023) / 1024;
                vals[nvals++] = Tcl_NewIntObj (val);
                continue;
            }
            if (strcasecmp (stri, "file") == 0) {
                vals[nvals++] = Tcl_NewStringObj (fnbuf, -1);
                continue;
            }
            if (strcasecmp (stri, "readonly") == 0) {
                int val = (rc & TMSTAT_WRPROT) / RLSTAT_WRPROT;
                vals[nvals++] = Tcl_NewIntObj (val);
                continue;
            }
            if (strcasecmp (stri, "ready") == 0) {
                int val = (rc & TMSTAT_READY) / RLSTAT_READY;
                vals[nvals++] = Tcl_NewIntObj (val);
                continue;
            }
            Tcl_SetResultF (interp, "unknown keyword %s", stri);
            return TCL_ERROR;
        }
        if (nvals > 0) {
            if (nvals < 2) {
                Tcl_SetObjResult (interp, vals[0]);
            } else {
                Tcl_SetObjResult (interp, Tcl_NewListObj (nvals, vals));
            }
        }
        return TCL_OK;
    }
    Tcl_SetResultF (interp, "bad number args");
    return TCL_ERROR;
}

static int cmd_tmunload (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 2) {
        char const *stri = Tcl_GetString (objv[1]);
        if (strcasecmp (stri, "help") == 0) {
            puts ("");
            puts ("  tmunload <drive>");
            puts ("");
            return TCL_OK;
        }

        int drive = -1;
        int rc = Tcl_GetIntFromObj (interp, objv[1], &drive);
        if (rc != TCL_OK) return rc;
        if ((drive < 0) || (drive > 7)) {
            Tcl_SetResultF (interp, "drive number %d out of range 0..7", drive);
            return TCL_ERROR;
        }

        rc = shmtm_load (drive, false, "");
        if (rc < 0) {
            Tcl_SetResultF (interp, "%s", strerror (- rc));
            return TCL_ERROR;
        }
        return TCL_OK;
    }
    Tcl_SetResultF (interp, "bad number args");
    return TCL_ERROR;
}

static int cmd_waitint (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 2) {
        char const *stri = Tcl_GetString (objv[1]);
        if (strcasecmp (stri, "help") == 0) {
            puts ("");
            puts ("  waitint <mask>");
            puts ("");
            return TCL_OK;
        }

        int mask = -1;
        int rc = Tcl_GetIntFromObj (interp, objv[1], &mask);
        if (rc != TCL_OK) return rc;
        z11page->waitint ((unsigned) mask);
        return TCL_OK;
    }
    Tcl_SetResultF (interp, "bad number args");
    return TCL_ERROR;
}
