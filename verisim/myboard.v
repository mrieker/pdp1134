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

module MyBoard (
    input  CLOCK,               // 100MHz clock
    input  RESET_N,             // power-on reset

    output LEDoutR,             // IO_B34_LN6 R14
    output LEDoutG,             // IO_B34_LP7 Y16
    output LEDoutB,             // IO_B34_LN7 Y17

    output[31:00] regarmintreq,

    // arm processor memory bus interface (AXI)
    // we are a slave for accessing the control registers (read and write)
    input[11:00]  saxi_ARADDR,
    output reg    saxi_ARREADY,
    input         saxi_ARVALID,
    input[11:00]  saxi_AWADDR,
    output reg    saxi_AWREADY,
    input         saxi_AWVALID,
    input         saxi_BREADY,
    output[1:0]   saxi_BRESP,
    output reg    saxi_BVALID,
    output[31:00] saxi_RDATA,
    input         saxi_RREADY,
    output[1:0]   saxi_RRESP,
    output reg    saxi_RVALID,
    input[31:00]  saxi_WDATA,
    output reg    saxi_WREADY,
    input         saxi_WVALID
);

    // unibus signals
    wire[17:00] bus_a_l;
    wire        bus_ac_lo_l;
    wire        bus_bbsy_l;
    wire[7:4]   bus_br_l;
    wire[1:0]   bus_c_l;
    wire[15:00] bus_d_l;
    wire        bus_dc_lo_l;
    wire        bus_hltrq_l;
    wire        bus_init_l;
    wire        bus_intr_l;
    wire        bus_msyn_l;
    wire        bus_npr_l;
    wire        bus_pa_l;
    wire        bus_pb_l;
    wire        bus_sack_l;
    wire        bus_ssyn_l;

    // signals output by our fillin for real pdp
    wire[17:00] fake_a_l;
    wire[1:0]   fake_c_l;
    wire[15:00] fake_d_l;
    wire        fake_bbsy_l;
    wire[7:4]   fake_bg_h;
    wire        fake_hltgr_h;
    wire        fake_hltrq_l;
    wire        fake_init_l;
    wire        fake_msyn_l;
    wire        fake_npg_h;
    wire        fake_ssyn_l;

    // signals output by the zynq board
    wire rsel1_h;
    wire rsel2_h;
    wire rsel3_h;

    wire[17:00] zynq_a_h;
    wire        zynq_ac_lo_h;
    wire        zynq_bbsy_h;
    wire[7:4]   zynq_bg_l;
    wire[7:4]   zynq_br_h;
    wire[1:0]   zynq_c_h;
    wire[15:00] zynq_d_h;
    wire        zynq_dc_lo_h;
    wire        zynq_hltrq_h;
    wire        zynq_init_h;
    wire        zynq_intr_h;
    wire        zynq_msyn_h;
    wire        zynq_npg_l;
    wire        zynq_npr_h;
    wire        zynq_pa_h;
    wire        zynq_pb_h;
    wire        zynq_sack_h;
    wire        zynq_ssyn_h;

/***
    always @(posedge CLOCK) begin
        $display ("MyBoard*: bus_bbsy_l=%b fake_bbsy_l=%b zynq_bbsy_h=%b", bus_bbsy_l, fake_bbsy_l, zynq_bbsy_h);
    end
***/

    wire[15:00] fake_r0out, fake_pcout, fake_psout;
    wire[5:0] fake_stout;
    wire fake_waiting, fake_stephalted;

    // fillin for the real pdp
    sim1134 fakeinst (
        .CLOCK (CLOCK),
        .RESET (~ RESET_N),

        .pcout (fake_pcout),                    //>> program counter
        .psout (fake_psout),                    //>> processor status
        .stout (fake_stout),                    //>> processor state
        .waiting (fake_waiting),                //>> doing WAIT instruction
        .r0out (fake_r0out),                    //>> R0 contents
        .stephalted (fake_stephalted),          //>> 0=normal; 1=halted by stepenable
        .stepenable (0),                        //<< 0=normal; 1=stop at end of bus cycle
        .stepsingle (0),                        //<< 0=normal; 1=temporarily override stepenable
        .turbo (1),                             //<< 0=provide msyn/ssyn deskewing delays; 1=don't bother

        .bus_ac_lo_in_l   (bus_ac_lo_l),        //<< power supply telling cpu it is shutting down
        .bus_bbsy_in_l    (bus_bbsy_l),         //<< some device telling cpu it is using the bus as master
        .bus_br_in_l      (bus_br_l),           //<< some device is requesting an interrupt
        .bus_dc_lo_in_l   (bus_dc_lo_l),        //<< power supply telling cpu it is off
        .bus_intr_in_l    (bus_intr_l),         //<< some device telling cpu it is passing interrupt vector
        .bus_npr_in_l     (bus_npr_l),          //<< some device requesting dma cycle
        .bus_pa_in_l      (bus_pa_l),           //<< parity available
        .bus_pb_in_l      (bus_pb_l),           //<< parity bit
        .bus_sack_in_l    (bus_sack_l),         //<< some device acknowledging bg/npg/hltgr signal
        .bus_hltrq_in_l   (bus_hltrq_l),        //<< some device (front panel) is requesting cpu to halt

        .bus_a_in_l       (bus_a_l),            //<< some device passing address of cpu internal register to cpu
        .bus_c_in_l       (bus_c_l),            //<< some device passing function for cpu internal register to cpu
        .bus_d_in_l       (bus_d_l),            //<< some device passing data to be written to cpu internal register
        .bus_init_in_l    (bus_init_l),         //<< bus is being initialized
        .bus_msyn_in_l    (bus_msyn_l),         //<< some device is accessing a cpu internal register
        .bus_ssyn_in_l    (bus_ssyn_l),         //<< some device has completed a device register read or write

        .bus_a_out_l      (fake_a_l),           //>> cpu is passing address to memory and devices
        .bus_c_out_l      (fake_c_l),           //>> cpu is passing function to memory and devices
        .bus_d_out_l      (fake_d_l),           //>> cpu is passing write data to memory and devices, or passing read data from cpu internal register
        .bus_bbsy_out_l   (fake_bbsy_l),        //>> cpu is busy using the bus as a master
        .bus_hltrq_out_l  (fake_hltrq_l),       //>> cpu is jamming itself in halt (HALT instruction)
        .bus_init_out_l   (fake_init_l),        //>> cpu is resetting the bus (RESET instruction)
        .bus_msyn_out_l   (fake_msyn_l),        //>> cpu is accessing memory or device register
        .bus_ssyn_out_l   (fake_ssyn_l),        //>> cpu has completed a cpu internal register read or write

        .bus_bg_out_h     (fake_bg_h),          //>> cpu is granting an interrupt request
        .bus_npg_out_h    (fake_npg_h),         //>> cpu is granting a dma request
        .bus_hltgr_out_h  (fake_hltgr_h)        //>> cpu is granting an halt request
    );

    // zynq board
    wire        armintreq;

    Zynq zynq (
        .CLOCK        (CLOCK),               // 100MHz clock
        .RESET_N      (RESET_N),             // power-on reset

        .LEDoutR      (LEDoutR),             // IO_B34_LN6 R14
        .LEDoutG      (LEDoutG),             // IO_B34_LP7 Y16
        .LEDoutB      (LEDoutB),             // IO_B34_LN7 Y17

        .rsel1_h      (rsel1_h),             // multiplexor selectors
        .rsel2_h      (rsel2_h),
        .rsel3_h      (rsel3_h),
        .muxa (~ (rsel1_h & bus_pa_l    | rsel2_h & bus_a_l[12] | rsel3_h & bus_a_l[01] )),
        .muxb (~ (rsel1_h & bus_d_l[11] | rsel2_h & bus_a_l[17] | rsel3_h & bus_a_l[14] )),
        .muxc (~ (rsel1_h & bus_hltrq_l | rsel2_h & bus_a_l[02] | rsel3_h & bus_a_l[11] )),
        .muxd (~ (rsel1_h & bus_pb_l    | rsel2_h & bus_d_l[00] | rsel3_h & bus_a_l[10] )),
        .muxe (~ (rsel1_h & bus_d_l[15] | rsel2_h & bus_d_l[03] | rsel3_h & bus_a_l[09] )),
        .muxf (~ (rsel1_h & bus_d_l[14] | rsel2_h & bus_d_l[02] | rsel3_h & bus_a_l[06] )),
        .muxh (~ (rsel1_h & bus_d_l[13] | rsel2_h & bus_d_l[06] | rsel3_h & bus_a_l[05] )),
        .muxj (~ (rsel1_h & bus_d_l[12] | rsel2_h & bus_br_l[7] | rsel3_h & bus_npr_l   )),
        .muxk (~ (rsel1_h & bus_d_l[10] | rsel2_h & bus_br_l[6] | rsel3_h & bus_a_l[00] )),
        .muxl (~ (rsel1_h & bus_d_l[09] | rsel2_h & bus_br_l[5] | rsel3_h & bus_c_l[0]  )),
        .muxm (~ (rsel1_h & bus_d_l[08] | rsel2_h & bus_br_l[4] | rsel3_h & bus_a_l[13] )),
        .muxn (~ (rsel1_h & bus_d_l[07] | rsel2_h & bus_a_l[15] | rsel3_h & bus_a_l[08] )),
        .muxp (~ (rsel1_h & bus_d_l[04] | rsel2_h & bus_a_l[16] | rsel3_h & bus_a_l[07] )),
        .muxr (~ (rsel1_h & bus_d_l[05] | rsel2_h & bus_c_l[1]  | rsel3_h & bus_a_l[04] )),
        .muxs (~ (rsel1_h & bus_d_l[01] |                         rsel3_h & bus_a_l[03] )),

        .ac_lo_in_h   (~ bus_ac_lo_l),
        .bbsy_in_h    (~ bus_bbsy_l),
        .dc_lo_in_h   (~ bus_dc_lo_l),
        .init_in_h    (~ bus_init_l),
        .intr_in_h    (~ bus_intr_l),
        .msyn_in_h    (~ bus_msyn_l),
        .sack_in_h    (~ bus_sack_l),
        .ssyn_in_h    (~ bus_ssyn_l),

        .a_out_h      (zynq_a_h),
        .ac_lo_out_h  (zynq_ac_lo_h),
        .bbsy_out_h   (zynq_bbsy_h),
        .br_out_h     (zynq_br_h),
        .c_out_h      (zynq_c_h),
        .d_out_h      (zynq_d_h),
        .dc_lo_out_h  (zynq_dc_lo_h),
        .hltrq_out_h  (zynq_hltrq_h),
        .init_out_h   (zynq_init_h),
        .intr_out_h   (zynq_intr_h),
        .msyn_out_h   (zynq_msyn_h),
        .npr_out_h    (zynq_npr_h),
        .pa_out_h     (zynq_pa_h),
        .pb_out_h     (zynq_pb_h),
        .sack_out_h   (zynq_sack_h),
        .ssyn_out_h   (zynq_ssyn_h),

        .bg_in_l      (~ fake_bg_h),
        .hltgr_in_l   (~ fake_hltgr_h),
        .npg_in_l     (~ fake_npg_h),

        .bg_out_l     (zynq_bg_l),
        .npg_out_l    (zynq_npg_l),

        .saxi_ARADDR  (saxi_ARADDR),
        .saxi_ARREADY (saxi_ARREADY),
        .saxi_ARVALID (saxi_ARVALID),
        .saxi_AWADDR  (saxi_AWADDR),
        .saxi_AWREADY (saxi_AWREADY),
        .saxi_AWVALID (saxi_AWVALID),
        .saxi_BREADY  (saxi_BREADY),
        .saxi_BRESP   (saxi_BRESP),
        .saxi_BVALID  (saxi_BVALID),
        .saxi_RDATA   (saxi_RDATA),
        .saxi_RREADY  (saxi_RREADY),
        .saxi_RRESP   (saxi_RRESP),
        .saxi_RVALID  (saxi_RVALID),
        .saxi_WDATA   (saxi_WDATA),
        .saxi_WREADY  (saxi_WREADY),
        .saxi_WVALID  (saxi_WVALID),

        .armintreq (armintreq),
        .zgintflags (regarmintreq)
    );

    // plug the zynq and real pdp boards into the unibus by wire-anding the active-low outputs
    assign bus_a_l     = fake_a_l     & ~ zynq_a_h;
    assign bus_ac_lo_l =                ~ zynq_ac_lo_h;
    assign bus_bbsy_l  = fake_bbsy_l  & ~ zynq_bbsy_h;
    assign bus_br_l    =                ~ zynq_br_h;
    assign bus_c_l     = fake_c_l     & ~ zynq_c_h;
    assign bus_d_l     = fake_d_l     & ~ zynq_d_h;
    assign bus_dc_lo_l =                ~ zynq_dc_lo_h;
    assign bus_hltrq_l = fake_hltrq_l & ~ zynq_hltrq_h;
    assign bus_init_l  = fake_init_l  & ~ zynq_init_h;
    assign bus_intr_l  =                ~ zynq_intr_h;
    assign bus_msyn_l  = fake_msyn_l  & ~ zynq_msyn_h;
    assign bus_npr_l   =                ~ zynq_npr_h;
    assign bus_pa_l    =                ~ zynq_pa_h;
    assign bus_pb_l    =                ~ zynq_pb_h;
    assign bus_sack_l  =                ~ zynq_sack_h;
    assign bus_ssyn_l  = fake_ssyn_l  & ~ zynq_ssyn_h;
endmodule
