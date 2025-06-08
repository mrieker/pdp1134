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

// tcl command to give direct access to z11 fpga signals

#include <stdint.h>
#include <string.h>

#include "cmd_pin.h"
#include "pintable.h"
#include "tclmain.h"
#include "z11defs.h"
#include "z11util.h"

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
