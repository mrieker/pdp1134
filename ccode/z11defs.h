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

// bits provided by zynq.v in the zynq

#ifndef _Z11DEFS
#define _Z11DEFS

#define Z_VER 0
#define Z_RA 1
#define Z_RB 2
#define Z_RC 3
#define Z_RD 4
#define Z_RE 5
#define Z_RF 6
#define Z_RG 7

#define a_man_d_out_h     (0177777U << 0)
#define a_man_ssyn_out_h  (1U << 16)
#define a_man_sack_out_h  (1U << 17)
#define a_man_pb_out_h    (1U << 18)
#define a_man_pa_out_h    (1U << 19)
#define a_man_npr_out_h   (1U << 20)
#define a_man_npg_out_l   (1U << 21)
#define a_man_msyn_out_h  (1U << 22)
#define a_man_intr_out_h  (1U << 23)
#define a_man_init_out_h  (1U << 24)
#define a_man_hltrq_out_h (1U << 25)
#define a_man_dc_lo_out_h (1U << 26)
#define a_man_bbsy_out_h  (1U << 27)
#define a_man_ac_lo_out_h (1U << 28)
#define a_fpgamode    (3U << 30)

#define FM_OFF  0U    // reset FPGA and disconnect from bus
#define FM_SIM  1U    // simulated CPU, ignores PDP-11/34, disconnected from bus
#define FM_REAL 2U    // normal mode, assumes PDP-11/34 present
#define FM_MAN  3U    // manual control of all bus lines

#define FMSTRS "OFF", "SIM", "REAL", "MAN"

#define b_man_a_out_h  (0777777U << 0)
#define b_man_c_out_h  (3U << 18)
#define b_man_br_out_h (017U << 20)
#define b_man_bg_out_l (017U << 24)
#define b_man_rsel_h   (3U << 28)

#define c_muxa        (1U << 31)
#define c_muxb        (1U << 30)
#define c_muxc        (1U << 29)
#define c_muxd        (1U << 28)
#define c_muxe        (1U << 27)
#define c_muxf        (1U << 26)
#define c_muxh        (1U << 25)
#define c_muxj        (1U << 24)
#define c_muxk        (1U << 23)
#define c_muxl        (1U << 22)
#define c_muxm        (1U << 21)
#define c_muxn        (1U << 20)
#define c_muxp        (1U << 19)
#define c_muxr        (1U << 18)
#define c_muxs        (1U << 17)
#define c_rsel1_h     (1U << 16)
#define c_rsel2_h     (1U << 15)
#define c_rsel3_h     (1U << 14)
#define c_ac_lo_in_h  (1U << 13)
#define c_bbsy_in_h   (1U << 12)
#define c_dc_lo_in_h  (1U << 11)
#define c_hltgr_in_l  (1U << 10)
#define c_init_in_h   (1U <<  9)
#define c_intr_in_h   (1U <<  8)
#define c_msyn_in_h   (1U <<  7)
#define c_npg_in_l    (1U <<  6)
#define c_sack_in_h   (1U <<  5)
#define c_ssyn_in_h   (1U <<  4)
#define c_bg_in_l     (15U << 0)

#define d_dev_ac_lo_h (1U << 31)
#define d_dev_bbsy_h  (1U << 30)
#define d_dev_dc_lo_h (1U << 29)
#define d_dev_hltgr_l (1U << 28)
#define d_dev_hltrq_h (1U << 27)
#define d_dev_init_h  (1U << 26)
#define d_dev_intr_h  (1U << 25)
#define d_dev_msyn_h  (1U << 24)
#define d_dev_npg_l   (1U << 23)
#define d_dev_npr_h   (1U << 22)
#define d_dev_pa_h    (1U << 21)
#define d_dev_pb_h    (1U << 20)
#define d_dev_sack_h  (1U << 19)
#define d_dev_ssyn_h  (1U << 18)
#define d_dev_a_h     (0777777U << 0)

#define e_muxcount       (0377U << 18)
#define e_dmx_npr_in_h   (   1U << 17)
#define e_dmx_hltrq_in_h (   1U << 16)
#define e_dmx_c_in_h     (   3U << 14)
#define e_dev_c_h        (   3U << 12)
#define e_dmx_br_in_h    ( 017U <<  8)
#define e_dev_br_h       ( 017U <<  4)
#define e_dev_bg_l       ( 017U <<  0)

#define f_dmx_a_in_h  (0777777U <<  0)

#define g_dmx_d_in_h  (0177777U << 16)
#define g_dev_d_h     (0177777U <<  0)

#define ILACTL 034
#define ILATIM 035
#define ILADAT 036

#define CTL_DEPTH  4096
#define CTL_ARMED  0x80000000U
#define CTL_AFTER0 0x00010000U
#define CTL_OFLOW  0x00008000U
#define CTL_INDEX0 0x00000001U
#define CTL_AFTER  (CTL_AFTER0 * (CTL_DEPTH-1))
#define CTL_INDEX  (CTL_INDEX0 * (CTL_DEPTH-1))

#define SL2_ENABLE    0x80000000U   // enable 777570 switches & lights registers
#define SL2_HALTREQ   0x40000000U   // request processor to halt
#define SL2_HALTED    0x20000000U   // processor has halted
#define SL2_STEPREQ   0x10000000U   // single-step processor
#define SL2_HALTSTATE 0x00380000U
#define SL2_HLTRQOUTH 0x00040000U   // swlight.v is requesting processor to halt
#define SL2_HALTINS   0x00020000U   // processor has HALT instr in its IR

#endif
