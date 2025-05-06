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

// tcl command to give direct access to z34 fpga signals

#include <stdint.h>
#include <string.h>

#include "cmd_pin.h"
#include "tclmain.h"
#include "z11defs.h"
#include "z11util.h"

// pin definitions
struct PinDef {
    char name[16];
    int dev;
    int reg;
    uint32_t mask;
    int lobit;
    bool writ;
};

#define DEV_11 0
#define DEV_BM 1
#define DEV_SL 2
#define DEV_DL 3

static uint32_t volatile *devs[4];

static PinDef const pindefs[] = {

    { "man_d_out_h",     DEV_11, Z_RA, a_d_out_h,     0, true  },
    { "man_ssyn_out_h",  DEV_11, Z_RA, a_ssyn_out_h,  0, true  },
    { "man_sack_out_h",  DEV_11, Z_RA, a_sack_out_h,  0, true  },
    { "man_pb_out_h",    DEV_11, Z_RA, a_pb_out_h,    0, true  },
    { "man_pa_out_h",    DEV_11, Z_RA, a_pa_out_h,    0, true  },
    { "man_npr_out_h",   DEV_11, Z_RA, a_npr_out_h,   0, true  },
    { "man_npg_out_l",   DEV_11, Z_RA, a_npg_out_l,   0, true  },
    { "man_msyn_out_h",  DEV_11, Z_RA, a_msyn_out_h,  0, true  },
    { "man_intr_out_h",  DEV_11, Z_RA, a_intr_out_h,  0, true  },
    { "man_init_out_h",  DEV_11, Z_RA, a_init_out_h,  0, true  },
    { "man_hltrq_out_h", DEV_11, Z_RA, a_hltrq_out_h, 0, true  },
    { "man_dc_lo_out_h", DEV_11, Z_RA, a_dc_lo_out_h, 0, true  },
    { "man_bbsy_out_h",  DEV_11, Z_RA, a_bbsy_out_h,  0, true  },
    { "man_ac_lo_out_h", DEV_11, Z_RA, a_ac_lo_out_h, 0, true  },
    { "fpgamode",        DEV_11, Z_RA, a_fpgamode,    0, true  },

    { "man_a_out_h",     DEV_11, Z_RB, b_a_out_h,     0, true  },
    { "man_c_out_h",     DEV_11, Z_RB, b_c_out_h,     0, true  },
    { "man_br_out_h",    DEV_11, Z_RB, b_br_out_h,    4, true  },
    { "man_bg_out_l",    DEV_11, Z_RB, b_bg_out_l,    4, true  },
    { "man_rsel_h",      DEV_11, Z_RB, b_rsel_h,      0, true  },

    { "muxa",            DEV_11, Z_RC, c_muxa,        0, false },
    { "muxb",            DEV_11, Z_RC, c_muxb,        0, false },
    { "muxc",            DEV_11, Z_RC, c_muxc,        0, false },
    { "muxd",            DEV_11, Z_RC, c_muxd,        0, false },
    { "muxe",            DEV_11, Z_RC, c_muxe,        0, false },
    { "muxf",            DEV_11, Z_RC, c_muxf,        0, false },
    { "muxh",            DEV_11, Z_RC, c_muxh,        0, false },
    { "muxj",            DEV_11, Z_RC, c_muxj,        0, false },
    { "muxk",            DEV_11, Z_RC, c_muxk,        0, false },
    { "muxl",            DEV_11, Z_RC, c_muxl,        0, false },
    { "muxm",            DEV_11, Z_RC, c_muxm,        0, false },
    { "muxn",            DEV_11, Z_RC, c_muxn,        0, false },
    { "muxp",            DEV_11, Z_RC, c_muxp,        0, false },
    { "muxr",            DEV_11, Z_RC, c_muxr,        0, false },
    { "muxs",            DEV_11, Z_RC, c_muxs,        0, false },
    { "rsel1_h",         DEV_11, Z_RC, c_rsel1_h,     0, false },
    { "rsel2_h",         DEV_11, Z_RC, c_rsel2_h,     0, false },
    { "rsel3_h",         DEV_11, Z_RC, c_rsel3_h,     0, false },
    { "ac_lo_in_h",      DEV_11, Z_RC, c_ac_lo_in_h,  0, false },
    { "bbsy_in_h",       DEV_11, Z_RC, c_bbsy_in_h,   0, false },
    { "dc_lo_in_h",      DEV_11, Z_RC, c_dc_lo_in_h,  0, false },
    { "hltgr_in_l",      DEV_11, Z_RC, c_hltgr_in_l,  0, false },
    { "init_in_h",       DEV_11, Z_RC, c_init_in_h,   0, false },
    { "intr_in_h",       DEV_11, Z_RC, c_intr_in_h,   0, false },
    { "msyn_in_h",       DEV_11, Z_RC, c_msyn_in_h,   0, false },
    { "npg_in_l",        DEV_11, Z_RC, c_npg_in_l,    0, false },
    { "sack_in_h",       DEV_11, Z_RC, c_sack_in_h,   0, false },
    { "ssyn_in_h",       DEV_11, Z_RC, c_ssyn_in_h,   0, false },
    { "bg_in_l",         DEV_11, Z_RC, c_bg_in_l,     4, false },

    { "a_out_h",         DEV_11, Z_RD, d_a_out_h,     0, false },
    { "ac_lo_out_h",     DEV_11, Z_RD, d_ac_lo_out_h, 0, false },
    { "bbsy_out_h",      DEV_11, Z_RD, d_bbsy_out_h,  0, false },
    { "dc_lo_out_h",     DEV_11, Z_RD, d_dc_lo_out_h, 0, false },
    { "hltgr_out_l",     DEV_11, Z_RD, d_hltgr_out_l, 0, false },
    { "hltrq_out_h",     DEV_11, Z_RD, d_hltrq_out_h, 0, false },
    { "init_out_h",      DEV_11, Z_RD, d_init_out_h,  0, false },
    { "intr_out_h",      DEV_11, Z_RD, d_intr_out_h,  0, false },
    { "msyn_out_h",      DEV_11, Z_RD, d_msyn_out_h,  0, false },
    { "npg_out_l",       DEV_11, Z_RD, d_npg_out_l,   0, false },
    { "npr_out_h",       DEV_11, Z_RD, d_npr_out_h,   0, false },
    { "pa_out_h",        DEV_11, Z_RD, d_pa_out_h,    0, false },
    { "pb_out_h",        DEV_11, Z_RD, d_pb_out_h,    0, false },
    { "sack_out_h",      DEV_11, Z_RD, d_sack_out_h,  0, false },
    { "ssyn_out_h",      DEV_11, Z_RD, d_ssyn_out_h,  0, false },

    { "npr_in_h",        DEV_11, Z_RE, e_npr_in_h,    0, false },
    { "pa_in_h",         DEV_11, Z_RE, e_pa_in_h,     0, false },
    { "pb_in_h",         DEV_11, Z_RE, e_pb_in_h,     0, false },
    { "hltrq_in_h",      DEV_11, Z_RE, e_hltrq_in_h,  0, false },
    { "c_in_h",          DEV_11, Z_RE, e_c_in_h,      0, false },
    { "c_out_h",         DEV_11, Z_RE, e_c_out_h,     0, false },
    { "br_in_h",         DEV_11, Z_RE, e_br_in_h,     4, false },
    { "br_out_h",        DEV_11, Z_RE, e_br_out_h,    4, false },
    { "bg_out_l",        DEV_11, Z_RE, e_bg_out_l,    4, false },
    { "muxcount",        DEV_11, Z_RE, e_muxcount,    0, false },

    { "a_in_h",          DEV_11, Z_RF, f_a_in_h,      0, false },

    { "d_in_h",          DEV_11, Z_RG, g_d_in_h,      0, false },
    { "d_out_h",         DEV_11, Z_RG, g_d_out_h,     0, false },

    { "bm_enablo",       DEV_BM, 1,    0xFFFFFFFF,    0, true  },
    { "bm_enabhi",       DEV_BM, 2,    0x3FFFFFFF,    0, true  },
    { "bm_armfunc",      DEV_BM, 3,    0xE0000000,    0, true  },
    { "bm_armaddr",      DEV_BM, 3,    0x0003FFFF,    0, true  },
    { "bm_delay",        DEV_BM, 4,    0xE0000000,    0, false },
    { "bm_armdata",      DEV_BM, 4,    0x0000FFFF,    0, true  },

    { "sl_switches",     DEV_SL, 1,    0x0000FFFF,    0, true  },
    { "sl_lights",       DEV_SL, 1,    0xFFFF0000,    0, false },
    { "sl_enable",       DEV_SL, 2,    0x80000000,    0, true  },
    { "sl_haltreq",      DEV_SL, 2,    0x40000000,    0, true  },
    { "sl_halted",       DEV_SL, 2,    0x20000000,    0, false },
    { "sl_stepreq",      DEV_SL, 2,    0x10000000,    0, true  },
    { "sl_businit",      DEV_SL, 2,    0x08000000,    0, true  },

    { "sl_dmastate",     DEV_SL, 3,    0xE0000000,    0, true  },
    { "sl_dmafail",      DEV_SL, 3,    0x10000000,    0, false },
    { "sl_dmactrl",      DEV_SL, 3,    0x0C000000,    0, true  },
    { "sl_dmaaddr",      DEV_SL, 3,    0x0003FFFF,    0, true  },
    { "sl_dmadata",      DEV_SL, 4,    0x0800FFFF,    0, true  },

    { "dl_rcsr",         DEV_DL, 1,    0x0000FFFF,    0, true  },
    { "dl_rbuf",         DEV_DL, 1,    0xFFFF0000,    0, true  },
    { "dl_xcsr",         DEV_DL, 2,    0x0000FFFF,    0, true  },
    { "dl_xbuf",         DEV_DL, 2,    0xFFFF0000,    0, true  },
    { "dl_enable",       DEV_DL, 3,    0x80000000,    0, true  },

    { "", 0, 0, 0, 0, false }
};

static Z11Page *z11page;
static uint32_t volatile *extmemptr;

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

    if (z11page == NULL) {

        // access the zynq io page and find devices thereon
        z11page = new Z11Page ();
        devs[DEV_11] = z11page->findev ("11", NULL, NULL, false);
        devs[DEV_BM] = z11page->findev ("BM", NULL, NULL, false);
        devs[DEV_SL] = z11page->findev ("SL", NULL, NULL, false);
        devs[DEV_DL] = z11page->findev ("DL", NULL, NULL, false);
#if 000
        // get pointer to the 32K-word ram
        // maps each 12-bit word into low 12 bits of 32-bit word
        // upper 20 bits discarded on write, readback as zeroes
        extmemptr = z11page->extmem ();
#endif
    }

    if ((objc == 2) && (strcasecmp (Tcl_GetString (objv[1]), "list") == 0)) {
        for (PinDef const *pte = pindefs; pte->name[0] != 0; pte ++) {
            printf ("  %-16s", pte->name);
            uint32_t volatile *ptr = devs[pte->dev];
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
                (ptr[0] >> 24) & 0xFFU, (ptr[0] >> 16) & 0xFFU, pte->reg, pte->mask);
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

        bool writeable;
        int width;
        uint32_t mask;
        uint32_t volatile *ptr;

        // em:address
        if (strncasecmp (name, "em:", 3) == 0) {
            char *p;
            mask = strtoul (name + 3, &p, 0);
            if ((*p != 0) || (mask > 077777)) {
                if (! testmode) {
                    Tcl_SetResultF (interp, "extended memory address %s must be integer in range 000000..077777", name + 3);
                    return TCL_ERROR;
                }
                gotvals[ngotvals++] = Tcl_NewIntObj (-1);
                continue;
            }
            ptr = extmemptr + mask;
            mask = 0177777;
            width = 16;
            writeable = true;
        }

        // signalname
        else {
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
            mask  = pte->mask;
            width = __builtin_popcount (pte->mask);
            if (p != NULL) {
                if ((hibit - lobit >= width) || (lobit < pte->lobit)) goto badname;
                mask  = ((mask & - mask) << (lobit - pte->lobit)) * (1U << (hibit - lobit));
                width = hibit - lobit + 1;
            }
            ptr = devs[pte->dev] + pte->reg;
            writeable = pte->writ;
        }
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
            *ptr = (*ptr & ~ mask) | ((uint32_t) val) * (mask & - mask);
        } else {
            uint32_t val = (*ptr & mask) / (mask & - mask);
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
