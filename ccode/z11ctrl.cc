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



static bool readword (void *dummy, uint16_t addr, uint16_t *data_r)
{
    extern Z11Page *z11page;
    uint32_t paddr = addr;
    if (paddr >= 0160000) paddr |= 0760000;
    return z11page->dmaread (paddr, data_r);
}

// disassemble instruciton
static int cmd_disasop (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int opcode   = -1;
    int operand1 = -1;
    int operand2 = -1;

    switch (objc) {
        case 2: {
            char const *opstr = Tcl_GetString (objv[1]);
            if (strcasecmp (opstr, "help") == 0) {
                puts ("");
                puts ("  disasop <opcode> [<operand1> [<operand2>]]");
                puts ("     opcode = integer opcode");
                puts ("   operand1 = first operand");
                puts ("   operand2 = second operand");
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
    int used = disassem (&str, (uint16_t) opcode, (uint16_t) operand1, (uint16_t) operand2, readword, NULL);
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
