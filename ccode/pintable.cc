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

// table of pins for 'pin' commamd
// also used internally by gui

#include <stdint.h>
#include <string.h>

#include "pintable.h"
#include "z11defs.h"
#include "z11util.h"

static uint32_t volatile *devs[DEV_MAX];
static char const *const devids[DEV_MAX] = { DEVIDS };

PinDef const pindefs[] = {
    { "man_d_out_h",     DEV_11, Z_RA, a_man_d_out_h,     0, true  },
    { "man_ssyn_out_h",  DEV_11, Z_RA, a_man_ssyn_out_h,  0, true  },
    { "man_sack_out_h",  DEV_11, Z_RA, a_man_sack_out_h,  0, true  },
    { "man_pb_out_h",    DEV_11, Z_RA, a_man_pb_out_h,    0, true  },
    { "man_pa_out_h",    DEV_11, Z_RA, a_man_pa_out_h,    0, true  },
    { "man_npr_out_h",   DEV_11, Z_RA, a_man_npr_out_h,   0, true  },
    { "man_npg_out_l",   DEV_11, Z_RA, a_man_npg_out_l,   0, true  },
    { "man_msyn_out_h",  DEV_11, Z_RA, a_man_msyn_out_h,  0, true  },
    { "man_intr_out_h",  DEV_11, Z_RA, a_man_intr_out_h,  0, true  },
    { "man_init_out_h",  DEV_11, Z_RA, a_man_init_out_h,  0, true  },
    { "man_hltrq_out_h", DEV_11, Z_RA, a_man_hltrq_out_h, 0, true  },
    { "man_dc_lo_out_h", DEV_11, Z_RA, a_man_dc_lo_out_h, 0, true  },
    { "man_bbsy_out_h",  DEV_11, Z_RA, a_man_bbsy_out_h,  0, true  },
    { "man_ac_lo_out_h", DEV_11, Z_RA, a_man_ac_lo_out_h, 0, true  },
    { "fpgamode",        DEV_11, Z_RA, a_fpgamode,        0, true  },

    { "man_a_out_h",     DEV_11, Z_RB, b_man_a_out_h,     0, true  },
    { "man_c_out_h",     DEV_11, Z_RB, b_man_c_out_h,     0, true  },
    { "man_br_out_h",    DEV_11, Z_RB, b_man_br_out_h,    4, true  },
    { "man_bg_out_l",    DEV_11, Z_RB, b_man_bg_out_l,    4, true  },
    { "man_rsel_h",      DEV_11, Z_RB, b_man_rsel_h,      0, true  },

    { "muxa",            DEV_11, Z_RC, c_muxa,            0, false },
    { "muxb",            DEV_11, Z_RC, c_muxb,            0, false },
    { "muxc",            DEV_11, Z_RC, c_muxc,            0, false },
    { "muxd",            DEV_11, Z_RC, c_muxd,            0, false },
    { "muxe",            DEV_11, Z_RC, c_muxe,            0, false },
    { "muxf",            DEV_11, Z_RC, c_muxf,            0, false },
    { "muxh",            DEV_11, Z_RC, c_muxh,            0, false },
    { "muxj",            DEV_11, Z_RC, c_muxj,            0, false },
    { "muxk",            DEV_11, Z_RC, c_muxk,            0, false },
    { "muxl",            DEV_11, Z_RC, c_muxl,            0, false },
    { "muxm",            DEV_11, Z_RC, c_muxm,            0, false },
    { "muxn",            DEV_11, Z_RC, c_muxn,            0, false },
    { "muxp",            DEV_11, Z_RC, c_muxp,            0, false },
    { "muxr",            DEV_11, Z_RC, c_muxr,            0, false },
    { "muxs",            DEV_11, Z_RC, c_muxs,            0, false },
    { "rsel1_h",         DEV_11, Z_RC, c_rsel1_h,         0, false },
    { "rsel2_h",         DEV_11, Z_RC, c_rsel2_h,         0, false },
    { "rsel3_h",         DEV_11, Z_RC, c_rsel3_h,         0, false },
    { "ac_lo_in_h",      DEV_11, Z_RC, c_ac_lo_in_h,      0, false },
    { "bbsy_in_h",       DEV_11, Z_RC, c_bbsy_in_h,       0, false },
    { "dc_lo_in_h",      DEV_11, Z_RC, c_dc_lo_in_h,      0, false },
    { "hltgr_in_l",      DEV_11, Z_RC, c_hltgr_in_l,      0, false },
    { "init_in_h",       DEV_11, Z_RC, c_init_in_h,       0, false },
    { "intr_in_h",       DEV_11, Z_RC, c_intr_in_h,       0, false },
    { "msyn_in_h",       DEV_11, Z_RC, c_msyn_in_h,       0, false },
    { "npg_in_l",        DEV_11, Z_RC, c_npg_in_l,        0, false },
    { "sack_in_h",       DEV_11, Z_RC, c_sack_in_h,       0, false },
    { "ssyn_in_h",       DEV_11, Z_RC, c_ssyn_in_h,       0, false },
    { "bg_in_l",         DEV_11, Z_RC, c_bg_in_l,         4, false },

    { "dev_a_h",         DEV_11, Z_RD, d_dev_a_h,         0, false },
    { "dev_ac_lo_h",     DEV_11, Z_RD, d_dev_ac_lo_h,     0, false },
    { "dev_bbsy_h",      DEV_11, Z_RD, d_dev_bbsy_h,      0, false },
    { "dev_dc_lo_h",     DEV_11, Z_RD, d_dev_dc_lo_h,     0, false },
    { "dev_hltgr_l",     DEV_11, Z_RD, d_dev_hltgr_l,     0, false },
    { "dev_hltrq_h",     DEV_11, Z_RD, d_dev_hltrq_h,     0, false },
    { "dev_init_h",      DEV_11, Z_RD, d_dev_init_h,      0, false },
    { "dev_intr_h",      DEV_11, Z_RD, d_dev_intr_h,      0, false },
    { "dev_msyn_h",      DEV_11, Z_RD, d_dev_msyn_h,      0, false },
    { "dev_npg_l",       DEV_11, Z_RD, d_dev_npg_l,       0, false },
    { "dev_npr_h",       DEV_11, Z_RD, d_dev_npr_h,       0, false },
    { "dev_pa_h",        DEV_11, Z_RD, d_dev_pa_h,        0, false },
    { "dev_pb_h",        DEV_11, Z_RD, d_dev_pb_h,        0, false },
    { "dev_sack_h",      DEV_11, Z_RD, d_dev_sack_h,      0, false },
    { "dev_ssyn_h",      DEV_11, Z_RD, d_dev_ssyn_h,      0, false },

    { "dmx_npr_in_h",    DEV_11, Z_RE, e_dmx_npr_in_h,    0, false },
    { "dmx_hltrq_in_h",  DEV_11, Z_RE, e_dmx_hltrq_in_h,  0, false },
    { "dmx_c_in_h",      DEV_11, Z_RE, e_dmx_c_in_h,      0, false },
    { "dev_c_h",         DEV_11, Z_RE, e_dev_c_h,         0, false },
    { "dmx_br_in_h",     DEV_11, Z_RE, e_dmx_br_in_h,     4, false },
    { "dev_br_h",        DEV_11, Z_RE, e_dev_br_h,        4, false },
    { "dev_bg_l",        DEV_11, Z_RE, e_dev_bg_l,        4, false },
    { "muxcount",        DEV_11, Z_RE, e_muxcount,        0, false },

    { "dmx_a_in_h",      DEV_11, Z_RF, f_dmx_a_in_h,      0, false },
    { "dmx_d_in_h",      DEV_11, Z_RG, g_dmx_d_in_h,      0, false },
    { "dev_d_h",         DEV_11, Z_RG, g_dev_d_h,         0, false },
    { "muxdelay",        DEV_11, Z_RL, l_muxdelay,        0, true  },
    { "turbo",           DEV_11, Z_RL, l_turbo,           0, true  },
    { "stepenable",      DEV_11, Z_RL, l_stepenable,      0, true  },
    { "stepsingle",      DEV_11, Z_RL, l_stepsingle,      0, true  },
    { "stephalted",      DEV_11, Z_RL, l_stephalted,      0, false },

    { "ilaafter",        DEV_11, 28,   ILACTL_AFTER,      0, true  },
    { "ilaarmed",        DEV_11, 28,   ILACTL_ARMED,      0, true  },
    { "ilaindex",        DEV_11, 28,   ILACTL_INDEX,      0, true  },
    { "ilaoflow",        DEV_11, 28,   ILACTL_OFLOW,      0, true  },
    { "ilartime",        DEV_11, 29,   0xFFFFFFFF,        0, false },
    { "ilardatalo",      DEV_11, 30,   0xFFFFFFFF,        0, false },
    { "ilardatahi",      DEV_11, 31,   0xFFFFFFFF,        0, false },

    { "bm_enablo",       DEV_BM, 1,    BM_ENABLO,         0, true  },
    { "bm_enabhi",       DEV_BM, 2,    BM2_ENABHI,        0, true  },
    { "bm_armfunc",      DEV_BM, 3,    0xE0000000,        0, true  },
    { "bm_armaddr",      DEV_BM, 3,    0x0003FFFF,        0, true  },
    { "bm_delay",        DEV_BM, 4,    0xE0000000,        0, false },
    { "bm_armdata",      DEV_BM, 4,    0x0000FFFF,        0, true  },
    { "bm_armperr",      DEV_BM, 4,    0x00030000,        0, true  },
    { "bm_ctlreg",       DEV_BM, 5,    BM5_CTLREG,        0, false },
    { "bm_ctlenab",      DEV_BM, 5,    BM5_CTLENAB,       0, true  },
    { "bm_ctladdr",      DEV_BM, 5,    BM5_CTLADDR,       0, true  },
    { "bm_brjame",       DEV_BM, 6,    BM6_BRJAME,        0, true  },
    { "bm_brjama",       DEV_BM, 6,    BM6_BRJAMA,        0, true  },
    { "bm_brenab",       DEV_BM, 6,    BM6_BRENAB,        0, true  },

    { "ky_switches",     DEV_KY, 1,    KY_SWITCHES,       0, true  },
    { "ky_lights",       DEV_KY, 1,    KY_LIGHTS,         0, false },
    { "ky_enable",       DEV_KY, 2,    0x80000000,        0, true  },
    { "ky_haltreq",      DEV_KY, 2,    0x40000000,        0, true  },
    { "ky_halted",       DEV_KY, 2,    0x20000000,        0, false },
    { "ky_stepreq",      DEV_KY, 2,    0x10000000,        0, true  },
    { "ky_snapctr",      DEV_KY, 2,    KY2_SNAPCTR,       0, true  },
    { "ky_sr1716",       DEV_KY, 2,    KY2_SR1716,        0, true  },
    { "ky_haltstate",    DEV_KY, 2,    0x00380000,        0, false },
    { "ky_hltrq_out_h",  DEV_KY, 2,    0x00040000,        0, false },
    { "ky_haltins",      DEV_KY, 2,    0x00020000,        0, false },
    { "ky_irqlev",       DEV_KY, 2,    KY2_IRQLEV,        0, true  },
    { "ky_irqvec",       DEV_KY, 2,    KY2_IRQVEC,        2, true  },
    { "ky_snaphlt",      DEV_KY, 2,    KY2_SNAPHLT,       0, true  },
    { "ky_snapreq",      DEV_KY, 2,    KY2_SNAPREQ,       0, true  },
    { "ky_dmastate",     DEV_KY, 3,    0xE0000000,        0, true  },
    { "ky_dmatimo",      DEV_KY, 3,    KY3_DMATIMO,       0, false },
    { "ky_dmactrl",      DEV_KY, 3,    0x0C000000,        0, true  },
    { "ky_dmaperr",      DEV_KY, 3,    KY3_DMAPERR,       0, false },
    { "ky_dmaaddr",      DEV_KY, 3,    0x0003FFFF,        0, true  },
    { "ky_dmadata",      DEV_KY, 4,    KY4_DMADATA,       0, true  },
    { "ky_snapreg",      DEV_KY, 4,    KY4_SNAPREG,       0, false },
    { "ky_dmalock",      DEV_KY, 5,    0xFFFFFFFF,        0, true  },

    { "dl_rcsr",         DEV_DL, 1,    0x0000FFFF,        0, true  },
    { "dl_rbuf",         DEV_DL, 1,    0xFFFF0000,        0, true  },
    { "dl_xcsr",         DEV_DL, 2,    0x0000FFFF,        0, true  },
    { "dl_xbuf",         DEV_DL, 2,    0xFFFF0000,        0, true  },
    { "dl_enable",       DEV_DL, 3,    0x80000000,        0, true  },

    { "dz_enable",       DEV_DZ, 1,    0x80000000,        0, true  },
    { "dz_intvec",       DEV_DZ, 1,    0x03FC0000,        0, true  },
    { "dz_addres",       DEV_DZ, 1,    0x0003FFFF,        0, true  },
    { "dz_csr",          DEV_DZ, 2,    0x0000FFFF,        1, true  },

    { "kw_enable",       DEV_KW, 1,    0x80000000,        0, true  },
    { "kw_fiftyhz",      DEV_KW, 1,    0x00000004,        0, true  },

    { "rl_rlcs",         DEV_RL, 1,    RL1_RLCS,          0, true  },
    { "rl_rlba",         DEV_RL, 1,    RL1_RLBA,          0, true  },
    { "rl_rlda",         DEV_RL, 2,    RL2_RLDA,          0, true  },
    { "rl_rlmp1",        DEV_RL, 2,    RL2_RLMP1,         0, true  },
    { "rl_rlmp2",        DEV_RL, 3,    RL3_RLMP2,         0, true  },
    { "rl_rlmp3",        DEV_RL, 3,    RL3_RLMP3,         0, true  },
    { "rl_drdy",         DEV_RL, 4,    RL4_DRDY,          0, true  },
    { "rl_derr",         DEV_RL, 4,    RL4_DERR,          0, true  },
    { "rl_enable",       DEV_RL, 5,    RL5_ENAB,          0, true  },
    { "rl_fastio",       DEV_RL, 5,    RL5_FAST,          0, true  },

    { "pc_rcsr",         DEV_PC, 1,    0x0000FFFF,        0, true  },
    { "pc_rbuf",         DEV_PC, 1,    0xFFFF0000,        0, true  },
    { "pc_pcsr",         DEV_PC, 2,    0x0000FFFF,        0, true  },
    { "pc_pbuf",         DEV_PC, 2,    0xFFFF0000,        0, true  },
    { "pc_enable",       DEV_PC, 3,    0x80000000,        0, true  },

    { "tm_enable",       DEV_TM, 4,    TM4_ENAB,          0, true  },
    { "tm_fastio",       DEV_TM, 4,    TM4_FAST,          0, true  },

    { "xe_enable",       DEV_XE, 3,    XE3_ENAB,          0, true  },

    { "", 0, 0, 0, 0, false }
};

// point to section header word for the given device
//  input:
//   dev = dev member from PinDef
//  output:
//   pointer to header word
uint32_t volatile *pindev (int dev)
{
    if ((dev < 0) || (dev >= DEV_MAX)) ABORT ();
    uint32_t volatile *p = devs[dev];
    if (p == NULL) {
        if (z11page == NULL) {
            z11page = new Z11Page ();
        }
        devs[dev] = p = z11page->findev (devids[dev], NULL, NULL, false);
    }
    return p;
}
