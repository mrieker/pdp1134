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

module synk (input CLOCK, output reg q, input o);
    reg eo, p;
    always @(posedge CLOCK) begin
        if (eo) p <= o;
           else q <= p;
        eo <= ~ eo;
    end
endmodule

// main program for the zynq implementation

module Zynq (
    input  CLOCK,               // 100MHz clock
    input  RESET_N,             // power-on reset

    output LEDoutR,             // IO_B34_LN6 R14
    output LEDoutG,             // IO_B34_LP7 Y16
    output LEDoutB,             // IO_B34_LN7 Y17

    input muxa,                 // multiplexed inputs
    input muxb,
    input muxc,
    input muxd,
    input muxe,
    input muxf,
    input muxh,
    input muxj,
    input muxk,
    input muxl,
    input muxm,
    input muxn,
    input muxp,
    input muxr,
    input muxs,

    output rsel1_h,             // multiplexor selectors
    output rsel2_h,
    output rsel3_h,

    input ac_lo_in_h,           // control inputs
    input bbsy_in_h,
    input dc_lo_in_h,
    input hltgr_in_l,
    input init_in_h,
    input intr_in_h,
    input msyn_in_h,
    input npg_in_l,
    input sack_in_h,
    input ssyn_in_h,

    input[7:4] bg_in_l,         // bus grant inputs

    output reg ac_lo_out_h,     // control outputs
    output reg bbsy_out_h,
    output reg dc_lo_out_h,
    output reg hltrq_out_h,
    output reg init_out_h,
    output reg intr_out_h,
    output reg msyn_out_h,
    output reg npg_out_l,
    output reg npr_out_h,
    output reg pa_out_h,
    output reg pb_out_h,
    output reg sack_out_h,
    output reg ssyn_out_h,

    output reg[17:00] a_out_h,  // address bus outputs
    output reg[7:4]   bg_out_l, // bus grant outputs
    output reg[7:4]   br_out_h, // bus request outputs
    output reg[1:0]   c_out_h,  // control bus outputs
    output reg[15:00] d_out_h,  // data bus outputs

    output[16:00] extmemaddr,
    output[17:00] extmemdout,
    input[17:00]  extmemdin,
    output        extmemenab,
    output[1:0]   extmemwena,

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
    input         saxi_WVALID,

    output        armintreq
);

    // [31:16] = '11'; [15:12] = (log2 len)-1; [11:00] = version
    localparam VERSION = 32'h31314021;

    // bus values that are constants
    assign saxi_BRESP = 0;  // A3.4.4/A10.3 transfer OK
    assign saxi_RRESP = 0;  // A3.4.4/A10.3 transfer OK

    reg[11:02] readaddr, writeaddr;

    localparam ILAADDRBITS = 13;    // 13 = 8K = 81.92uS
    reg[63:00] ilaarray[(1<<ILAADDRBITS)-1:0], ilardata, ilacurwd;
    reg[14:00] ilaafter, ilaindex;
    reg ilaarmed, ilaoflow;

    // arm writes these to control fpga
    reg[31:00] regctla, regctlb, regctli, regctll;
    wire[31:00] regctlj;
    reg[23:06]  regctlk_2306;
    wire[05:00] regctlk_0500;

    // fpga sends interrupt request to arm
    reg[30:00] regarmintena;    // arm enables each interrupt source separately
    reg regarmintreq_30;        // arm can interrupt itself with bit 30 (driver init probing int vector)
    wire[29:00] regarmintreq;   // all other sources are from fpga devices
    assign armintreq = (regarmintena & { regarmintreq_30, regarmintreq }) != 0;

    // regctla[31:30] determine overall FPGA mode
    wire[1:0] fpgamode = regctla[31:30];
    localparam FM_OFF  = 0;     // FPGA 'off' - acts as a grant jumper card
    localparam FM_SIM  = 1;     // simulating - still acts as grant jumper to outside world
    localparam FM_REAL = 2;     // real - connected to outside signals
    localparam FM_MAN  = 3;     // manual - connected to outside signals with manual manipulation

    wire turbo = regctll[22] & (fpgamode == FM_SIM);  // skip MSYN/SSYN deskewing delays when in simulation mode

    wire[31:00] regctlh;        // debug display in z11dump

    // wired-and/or of signals from all devices
    // includes unibus whenever in FM_REAL mode
    // includes simulator whenever in FM_SIM mode

    reg dev_ac_lo_h;
    reg dev_bbsy_h;
    reg dev_dc_lo_h;
    reg dev_hltgr_l;
    reg dev_hltld_h;
    reg dev_hltrq_h;
    reg dev_init_h;
    reg dev_intr_h;
    reg dev_npg_l;
    reg dev_npr_h;
    reg dev_pa_h;
    reg dev_pb_h;
    reg dev_sack_h;
    reg dev_del_msyn_h;
    reg dev_syn_msyn_h;
    reg dev_del_ssyn_h;
    reg dev_syn_ssyn_h;

    reg[1:0] dev_c_h;
    reg[7:4] dev_bg_l;
    reg[7:4] dev_br_h;
    reg[15:00] dev_d_h;
    reg[17:00] dev_a_h;

    // various levels of reset

    wire powerup = ~ RESET_N;                       // zynq is booting up
    wire fpgaoff = powerup | (fpgamode == FM_OFF);  // above + fpgamode OFF
    wire dcislow = fpgaoff | dev_dc_lo_h;           // above + DCLO (ie, resetting PDP)
    wire businit = dcislow | dev_init_h;            // above + PDP issuing RESET

    /////////////////////////////////////////////////////////////////
    //  synchronize and demultiplex signals coming in from unibus  //
    /////////////////////////////////////////////////////////////////

    // synchronize non-multiplexed signals to fpga clock
    // - worst-case delay of 20nS
    wire syn_ac_lo_in_h, syn_bbsy_in_h, syn_dc_lo_in_h, syn_hltgr_in_l, syn_init_in_h;
    wire syn_intr_in_h, syn_msyn_in_h, syn_npg_in_l, syn_sack_in_h, syn_ssyn_in_h;
    wire[7:4] syn_bg_in_l;
    synk synkac_lo (CLOCK, syn_ac_lo_in_h, ac_lo_in_h);
    synk synkbbsy  (CLOCK, syn_bbsy_in_h,  bbsy_in_h);
    synk synkdc_lo (CLOCK, syn_dc_lo_in_h, dc_lo_in_h);
    synk synkhltgr (CLOCK, syn_hltgr_in_l, hltgr_in_l);
    synk synkinit  (CLOCK, syn_init_in_h,  init_in_h);
    synk synkintr  (CLOCK, syn_intr_in_h,  intr_in_h);
    synk synkmsyn  (CLOCK, syn_msyn_in_h,  msyn_in_h);
    synk synknpg   (CLOCK, syn_npg_in_l,   npg_in_l);
    synk synksack  (CLOCK, syn_sack_in_h,  sack_in_h);
    synk synkssyn  (CLOCK, syn_ssyn_in_h,  ssyn_in_h);
    synk synkbg_4  (CLOCK, syn_bg_in_l[4], bg_in_l[4]);
    synk synkbg_5  (CLOCK, syn_bg_in_l[5], bg_in_l[5]);
    synk synkbg_6  (CLOCK, syn_bg_in_l[6], bg_in_l[6]);
    synk synkbg_7  (CLOCK, syn_bg_in_l[7], bg_in_l[7]);

    // input demux signal latches
    // - loaded from mux pins every muxdelayt3*10nS
    reg dmx_haltloaded, dmx_hltrq_in_h, dmx_npr_in_h, dmx_pa_in_h, dmx_pb_in_h;
    reg[1:0] dmx_c_in_h;
    reg[7:4] dmx_br_in_h;
    reg[17:00] dmx_a_in_h;
    reg[15:00] dmx_d_in_h;

    // del_msyn_in_h - delayed muxdelayt3*10nS so all demuxed signals up-to-date
    // - specifically we care about dmx_a_in_h, dmx_c_in_h, dmx_d_in_h, dmx_pa_in_h, dmx_pb_in_h
    //   the other dmx_ signals are just passed to arm for debugging
    wire[5:0] muxdelaym1 = { 1'b0, regctll[4:0] - 5'b1 };
    wire[7:0] muxdelayt3 = { 2'b0, regctll[4:0], 1'b0 } + { 3'b0, regctll[4:0] };
    reg del_msyn_in_h;

    // del_ssyn_in_h - delayed muxdelayt3*10nS so all data demuxed signals are up-to-date
    // - dmx_a_in_h and dmx_c_in_h should still be ok from msyn
    //   dmx_d_in_h will be up to date for write functions
    //   ...but needs to be updated for read functions
    reg del_ssyn_in_h;

    reg[7:0] muxcount, mmuxdelay, smuxdelay;
    wire[1:0] rseln = muxcount[7:6];
    assign rsel1_h  = rseln == 1;
    assign rsel2_h  = rseln == 2;
    assign rsel3_h  = rseln == 3;

    wire[1:0] man_rsel_h;

    always @(posedge CLOCK) begin
        if (fpgaoff) begin
            dmx_haltloaded <= 0;
            muxcount <= 64;
        end else begin

            // give transistors MUXDELAY*10nS to switch and soak
            if (muxcount[5:0] != muxdelaym1) begin
                dmx_haltloaded <= 0;
                muxcount[5:0]  <= muxcount[5:0] + 1;
            end else begin

                // all soaked in, clock into corresponding flipflops
                if (rsel1_h) begin
                    dmx_pa_in_h    <= muxa;
                    dmx_d_in_h[11] <= muxb;
                    dmx_haltloaded <= 1;
                    dmx_hltrq_in_h <= muxc;
                    dmx_pb_in_h    <= muxd;
                    dmx_d_in_h[15] <= muxe;
                    dmx_d_in_h[14] <= muxf;
                    dmx_d_in_h[13] <= muxh;
                    dmx_d_in_h[12] <= muxj;
                    dmx_d_in_h[10] <= muxk;
                    dmx_d_in_h[09] <= muxl;
                    dmx_d_in_h[08] <= muxm;
                    dmx_d_in_h[07] <= muxn;
                    dmx_d_in_h[04] <= muxp;
                    dmx_d_in_h[05] <= muxr;
                    dmx_d_in_h[01] <= muxs;
                end
                if (rsel2_h) begin
                    dmx_a_in_h[12] <= muxa;
                    dmx_a_in_h[17] <= muxb;
                    dmx_a_in_h[02] <= muxc;
                    dmx_d_in_h[00] <= muxd;
                    dmx_d_in_h[03] <= muxe;
                    dmx_d_in_h[02] <= muxf;
                    dmx_d_in_h[06] <= muxh;
                    dmx_br_in_h[7] <= muxj;
                    dmx_br_in_h[6] <= muxk;
                    dmx_br_in_h[5] <= muxl;
                    dmx_br_in_h[4] <= muxm;
                    dmx_a_in_h[15] <= muxn;
                    dmx_a_in_h[16] <= muxp;
                    dmx_c_in_h[1]  <= muxr;
                end
                if (rsel3_h) begin
                    dmx_a_in_h[01] <= muxa;
                    dmx_a_in_h[14] <= muxb;
                    dmx_a_in_h[11] <= muxc;
                    dmx_a_in_h[10] <= muxd;
                    dmx_a_in_h[09] <= muxe;
                    dmx_a_in_h[06] <= muxf;
                    dmx_a_in_h[05] <= muxh;
                    dmx_npr_in_h   <= muxj;
                    dmx_a_in_h[00] <= muxk;
                    dmx_c_in_h[0]  <= muxl;
                    dmx_a_in_h[13] <= muxm;
                    dmx_a_in_h[08] <= muxn;
                    dmx_a_in_h[07] <= muxp;
                    dmx_a_in_h[04] <= muxr;
                    dmx_a_in_h[03] <= muxs;
                end

                // increment on to next multiplexor selection
                muxcount[5:0] <= 0;

                // - if non-zero man_rsel_h, use that one
                //   otherwise, cycle on through one to the next
                muxcount[7:6] <=
                        (man_rsel_h != 0) ? man_rsel_h :
                                (muxcount[7:6] == 3) ? 1 : (muxcount[7:6] + 1);
            end

            // delay msyn_in_h for a full demux cycle so we know multiplexed signals are all updated
            // master has given it some delay but give it more to be sure
            if (~ syn_msyn_in_h) begin
                del_msyn_in_h <= 0;                 // drop delayed msyn as soon as external drops
                mmuxdelay     <= 0;                 // init delay counter for next time
            end else if (mmuxdelay != muxdelayt3) begin
                mmuxdelay     <= mmuxdelay + 1;
            end else begin                          // see if all 3 clocked in since transition
                del_msyn_in_h <= 1;                 // ok to assert delayed msyn now
            end

            // delay ssyn_in_h for a full demux cycle so we know multiplexed signals are all updated
            // dmx_a_in_h and dmx_c_in_h were updated by delayed msyn_in_h
            // dmx_d_in_h was updated for 'write' cycles by the delayed msyn_in_h
            // we only need to delay for 'read' cycles so dmx_{d,pa,pb}_in_h get updated with what was read
            // master will give it some delay but give it more to be sure
            if (~ syn_ssyn_in_h) begin
                del_ssyn_in_h <= 0;                 // drop delayed ssyn as soon as external drops
                smuxdelay     <= 0;                 // init delay counter for next time
            end else if (~ dmx_c_in_h[1] & (smuxdelay != muxdelayt3)) begin
                smuxdelay     <= smuxdelay + 1;     // read - count through delay; write - don't bother
            end else begin                          // see if all 3 clocked in since transition
                del_ssyn_in_h <= 1;                 // ok to assert delayed(read)/undelayed(write) ssyn now
            end
        end
    end

    /////////////////////////////////////////////////////////////
    //  signals coming out of simulator going to internal bus  //
    /////////////////////////////////////////////////////////////

    // negated when not in simulator mode

    wire[17:00] sim_a_out_l;
    wire[1:0] sim_c_out_l;
    wire[15:00] sim_d_out_l;
    wire sim_bbsy_out_l;
    wire sim_hltrq_out_l;
    wire sim_init_out_l;
    wire sim_msyn_out_l;
    wire sim_ssyn_out_l;
    wire[7:4] sim_bg_out_h;
    wire sim_npg_out_h;
    wire sim_hltgr_out_h;
    wire[15:00] sim_r0out;
    wire sim_waiting;

    wire sim_reset_h = fpgaoff | (fpgamode != FM_SIM);

    sim1134 siminst (
        .CLOCK (CLOCK),
        .RESET (sim_reset_h),

        .turbo (turbo),
        .pcout (regctlj[15:00]),
        .psout (regctlj[31:16]),
        .stout (regctlk_0500),
        .r0out (sim_r0out),
        .waiting (sim_waiting),

        .bus_ac_lo_in_l   (~ dev_ac_lo_h),      //<< power supply telling cpu it is shutting down
        .bus_bbsy_in_l    (~ dev_bbsy_h),       //<< some device telling cpu it is using the bus as master
        .bus_br_in_l      (~ dev_br_h),         //<< some device is requesting an interrupt
        .bus_dc_lo_in_l   (~ dev_dc_lo_h),      //<< power supply telling cpu it is off
        .bus_intr_in_l    (~ dev_intr_h),       //<< some device telling cpu it is passing interrupt vector
        .bus_npr_in_l     (~ dev_npr_h),        //<< some device requesting dma cycle
        .bus_sack_in_l    (~ dev_sack_h),       //<< some device acknowledging bg/npg/hltgr signal
        .bus_hltrq_in_l   (~ dev_hltrq_h),      //<< some device (front panel) is requesting cpu to halt

        .bus_a_in_l       (~ dev_a_h),          //<< some device passing address of cpu internal register to cpu
        .bus_c_in_l       (~ dev_c_h),          //<< some device passing function for cpu internal register to cpu
        .bus_d_in_l       (~ dev_d_h),          //<< some device passing data to be written to cpu internal register
                                                //   or device/memory returning data read
        .bus_init_in_l    (~ dev_init_h),       //<< bus is being initialized
        .bus_msyn_in_l    (~ dev_del_msyn_h),   //<< some device is accessing a cpu internal register
        .bus_pa_in_l      (~ dev_pa_h),         //<< memory parity check
        .bus_pb_in_l      (~ dev_pb_h),         //<< memory parity check
        .bus_ssyn_in_l    (~ dev_del_ssyn_h),   //<< some device or memory has completed a device register read or write

        .bus_a_out_l      (sim_a_out_l),        //>> cpu is passing address to memory and devices
        .bus_c_out_l      (sim_c_out_l),        //>> cpu is passing function to memory and devices
        .bus_d_out_l      (sim_d_out_l),        //>> cpu is passing write data to memory and devices, or passing read data from cpu internal register
        .bus_bbsy_out_l   (sim_bbsy_out_l),     //>> cpu is busy using the bus as a master
        .bus_hltrq_out_l  (sim_hltrq_out_l),    //>> cpu is jamming itself in halt (HALT instruction)
        .bus_init_out_l   (sim_init_out_l),     //>> cpu is resetting the bus (RESET instruction)
        .bus_msyn_out_l   (sim_msyn_out_l),     //>> cpu is accessing memory or device register
        .bus_ssyn_out_l   (sim_ssyn_out_l),     //>> cpu has completed a cpu internal register read or write

        .bus_bg_out_h     (sim_bg_out_h),       //>> cpu is granting an interrupt request
        .bus_npg_out_h    (sim_npg_out_h),      //>> cpu is granting a dma request
        .bus_hltgr_out_h  (sim_hltgr_out_h)     //>> cpu is granting an halt request
    );

    ///////////////////////////////////////////////////////
    //  give arm direct read-only access to unibus pins  //
    ///////////////////////////////////////////////////////

    wire turnedon = fpgamode != FM_OFF;

    wire[31:00] regctlc = {
        muxa,               // multiplexed inputs
        muxb,
        muxc,
        muxd,
        muxe,
        muxf,
        muxh,
        muxj,
        muxk,
        muxl,
        muxm,
        muxn,
        muxp,
        muxr,
        muxs,

        rsel1_h,            // multiplexor selectors
        rsel2_h,
        rsel3_h,

        ac_lo_in_h,   // power supply indicating AC failure
        bbsy_in_h,    // pdp or real rl11 using bus
        dc_lo_in_h,   // power supply indicating DC failure
        hltgr_in_l,   // pdp is halted
        init_in_h,    // pdp doing RESET
        intr_in_h,    // real rl11 is sending int vector to pdp
        msyn_in_h,    // pdp or rl11 is mastering cycle
        npg_in_l,     // pdp is granting dma
        sack_in_h,    // real rl11 is acknowledging grant
        ssyn_in_h,    // real rl11 slave or real mem completed transfer
        bg_in_l       // bus grant inputs
    };

    wire[31:00] regctld = {
        dev_ac_lo_h,        //31 control outputs
        dev_bbsy_h,         //30
        dev_dc_lo_h,        //29
        dev_hltgr_l,        //28
        dev_hltrq_h,        //27
        dev_init_h,         //26
        dev_intr_h,         //25
        dev_del_msyn_h,     //24
        dev_npg_l,          //23
        dev_npr_h,          //22
        dev_pa_h,           //21
        dev_pb_h,           //20
        dev_sack_h,         //19
        dev_del_ssyn_h,     //18

        dev_a_h             //00 address bus outputs
    };

    wire[31:00] regctle = {
        4'b0,               //28
        dmx_pb_in_h,        //27
        dmx_pa_in_h,        //26
        muxcount,           //18
        dmx_npr_in_h,       //17
        dmx_hltrq_in_h,     //16
        dmx_c_in_h,         //14
        dev_c_h,            //12 control bus outputs
        dmx_br_in_h,        //08
        dev_br_h,           //04 bus request outputs
        dev_bg_l            //00 bus grant outputs
    };

    wire[31:00] regctlf = {
        14'b0,
        dmx_a_in_h
    };

    wire[31:00] regctlg = {
        dmx_d_in_h,
        dev_d_h             // data bus outputs
    };

    // count bus cycles
    reg lastdevmsyn;
    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            regctli <= 0;
        end else if (~ lastdevmsyn & dev_syn_msyn_h) begin
            regctli <= regctli + 1;
        end
        lastdevmsyn <= dev_syn_msyn_h;
    end

    // latch address and data for front panel lights
    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            regctlk_2306   <= 0;
            regctll[21:06] <= 0;
        end else if (sim_waiting) begin
            regctll[21:06] <= sim_r0out;
        end else if (dev_syn_ssyn_h) begin
            regctlk_2306   <= dev_a_h;
            regctll[21:06] <= dev_d_h;
        end
    end

    /////////////////////////////////////
    //  arm reading/writing registers  //
    /////////////////////////////////////

    wire[31:00] bmarmrdata, dlarmrdata, dzarmrdata, kwarmrdata, kyarmrdata, pcarmrdata, rlarmrdata, tmarmrdata;

    assign saxi_RDATA =
        (readaddr        == 10'b0000000000) ? VERSION      :
        (readaddr        == 10'b0000000001) ? regctla      :
        (readaddr        == 10'b0000000010) ? regctlb      :
        (readaddr        == 10'b0000000011) ? regctlc      :
        (readaddr        == 10'b0000000100) ? regctld      :
        (readaddr        == 10'b0000000101) ? regctle      :
        (readaddr        == 10'b0000000110) ? regctlf      :
        (readaddr        == 10'b0000000111) ? regctlg      :
        (readaddr        == 10'b0000001000) ? regctlh      :
        (readaddr        == 10'b0000001001) ? regctli      :
        (readaddr        == 10'b0000001010) ? regctlj      :
        (readaddr        == 10'b0000001011) ? { 8'b0, regctlk_2306, regctlk_0500 } :
        (readaddr        == 10'b0000001100) ? regctll      :
        (readaddr        == 10'b0000011010) ? { 1'b0, regarmintena } :                       // ZG_INTENABS in km
        (readaddr        == 10'b0000011011) ? { armintreq, regarmintreq_30, regarmintreq } : // ZG_INTFLAGS in km
        (readaddr        == 10'b0000011100) ? { ilaarmed, ilaafter, ilaoflow, ilaindex } :
        (readaddr        == 10'b0000011101) ? 0 :
        (readaddr        == 10'b0000011110) ? { ilardata[31:00] } :
        (readaddr        == 10'b0000011111) ? { ilardata[63:32] } :
        (readaddr[11:05] ==  7'b0000100)    ? bmarmrdata   :
        (readaddr[11:05] ==  7'b0000101)    ? rlarmrdata   :
        (readaddr[11:05] ==  7'b0000110)    ? kyarmrdata   :
        (readaddr[11:05] ==  7'b0000111)    ? dzarmrdata   :
        (readaddr[11:05] ==  7'b0001000)    ? tmarmrdata   :
        (readaddr[11:04] ==  8'b00010010)   ? pcarmrdata   :
        (readaddr[11:04] ==  8'b00010011)   ? dlarmrdata   :
        (readaddr[11:03] ==  9'b000101000)  ? kwarmrdata   :
        32'hDEADBEEF;

    wire armwrite = saxi_WREADY & saxi_WVALID;              // arm is writing a register (single fpga clock cycle)

    wire bmarmwrite = armwrite & (writeaddr[11:05] == 7'b0000100);
    wire rlarmwrite = armwrite & (writeaddr[11:05] == 7'b0000101);
    wire kyarmwrite = armwrite & (writeaddr[11:05] == 7'b0000110);
    wire dzarmwrite = armwrite & (writeaddr[11:05] == 7'b0000111);
    wire tmarmwrite = armwrite & (writeaddr[11:05] == 7'b0001000);
    wire pcarmwrite = armwrite & (writeaddr[11:04] == 8'b00010010);
    wire dlarmwrite = armwrite & (writeaddr[11:04] == 8'b00010011);
    wire kwarmwrite = armwrite & (writeaddr[11:03] == 9'b000101000);

    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            saxi_ARREADY <= 1;                              // we are ready to accept read address
            saxi_RVALID  <= 0;                              // we are not sending out read data

            saxi_AWREADY <= 1;                              // we are ready to accept write address
            saxi_WREADY  <= 0;                              // we are not ready to accept write data
            saxi_BVALID  <= 0;                              // we are not acknowledging any write

            regctla[31:30]  <= FM_OFF;                      // FM_OFF disconnect from bus
            regctla[29:22]  <= 0;
            regctla[21]     <= 1'b1;                        // man_npg_out_l
            regctla[20:00]  <= 0;
            regctlb[31:28]  <= 0;
            regctlb[27:24]  <= 4'b1111;                     // man_bg_out_l
            regctlb[23:00]  <= 0;
            regctll[31:22]  <= 0;
            regctll[04:00]  <= 21;                          // muxdelay (18=bad; 19=ok)

            regarmintena    <= 0;                           // disable all interrupts to arm
            regarmintreq_30 <= 0;                           // disable the one arm can request to itself

        end else begin

            /////////////////////
            //  register read  //
            /////////////////////

            // check for PS sending us a read address
            if (saxi_ARREADY & saxi_ARVALID) begin
                readaddr <= saxi_ARADDR[11:02];             // save address bits we care about
                saxi_ARREADY <= 0;                          // we are no longer accepting a read address
                saxi_RVALID <= 1;                           // we are sending out the corresponding data

            // check for PS acknowledging receipt of data
            end else if (saxi_RVALID & saxi_RREADY) begin
                saxi_ARREADY <= 1;                          // we are ready to accept an address again
                saxi_RVALID <= 0;                           // we are no longer sending out data
            end

            //////////////////////
            //  register write  //
            //////////////////////

            // check for PS sending us write data
            if (armwrite) begin
                case (writeaddr)                            // write data to register
                     10'b0000000001: begin
                        regctla <= saxi_WDATA;
                    end
                    10'b0000000010: begin
                        regctlb <= saxi_WDATA;
                    end
                    10'b0000001100: begin
                        regctll[05:00] <= saxi_WDATA[05:00];
                        regctll[22]    <= saxi_WDATA[22];
                    end
                    10'b0000011010: begin
                        regarmintena    <= saxi_WDATA[30:00];
                    end
                    10'b0000011011: begin
                        regarmintreq_30 <= saxi_WDATA[30];
                    end
                    default: begin end
                endcase
                saxi_AWREADY <= 1;                          // we are ready to accept an address again
                saxi_WREADY  <= 0;                          // we are no longer accepting write data
                saxi_BVALID  <= 1;                          // we have accepted the data

            end else begin
                // check for PS sending us a write address
                if (saxi_AWREADY & saxi_AWVALID) begin
                    writeaddr <= saxi_AWADDR[11:02];        // save address bits we care about
                    saxi_AWREADY <= 0;                      // we are no longer accepting a write address
                    saxi_WREADY  <= 1;                      // we are ready to accept write data
                end

                // check for PS acknowledging write acceptance
                if (saxi_BVALID & saxi_BREADY) begin
                    saxi_BVALID <= 0;
                end
            end
        end
    end

    ////////////////////////
    //  internal devices  //
    ////////////////////////

    wire irq4_intr_out_h, irq5_intr_out_h, irq6_intr_out_h, irq7_intr_out_h;
    wire[7:0] irq4_d70_out_h, irq5_d70_out_h, irq6_d70_out_h, irq7_d70_out_h;

    assign regarmintreq[29:02] = 0;

    // big memory
    wire bm_pb_out_h, bm_ssyn_out_h;
    wire[17:00] bm_a_out_h;
    wire[15:00] bm_d_out_h;

    bigmem bminst (
        .CLOCK (CLOCK),
        .powerup (powerup),
        .fpgaoff (fpgaoff),
        .businit (businit),

        .armraddr (readaddr[4:2]),
        .armrdata (bmarmrdata),
        .armwaddr (writeaddr[4:2]),
        .armwdata (saxi_WDATA),
        .armwrite (bmarmwrite),

        .a_in_h (dev_a_h),
        .c_in_h (dev_c_h),
        .d_in_h (dev_d_h),
        .msyn_in_h (dev_del_msyn_h),

        .a_out_h    (bm_a_out_h),
        .d_out_h    (bm_d_out_h),
        .pb_out_h   (bm_pb_out_h),
        .ssyn_out_h (bm_ssyn_out_h),

        .extmemaddr (extmemaddr),
        .extmemdout (extmemdout),
        .extmemdin  (extmemdin),
        .extmemenab (extmemenab),
        .extmemwena (extmemwena)
    );

    // paper tape reader/punch
    wire pcintreq, pc_ssyn_out_h;
    wire[7:0] pcintvec;
    wire[15:00] pc_d_out_h;

    pc11 pcinst (
        .CLOCK (CLOCK),
        .RESET (fpgaoff),

        .armraddr (readaddr[3:2]),
        .armrdata (pcarmrdata),
        .armwaddr (writeaddr[3:2]),
        .armwdata (saxi_WDATA),
        .armwrite (pcarmwrite),

        .intreq (pcintreq),
        .irvec  (pcintvec),
        .intgnt (irq4_intr_out_h),
        .igvec  (irq4_d70_out_h),

        .a_in_h (dev_a_h),
        .c_in_h (dev_c_h),
        .d_in_h (dev_d_h),
        .init_in_h (dev_init_h),
        .msyn_in_h (dev_del_msyn_h),

        .d_out_h (pc_d_out_h),
        .ssyn_out_h (pc_ssyn_out_h));

    // rl01/2 disk controller
    wire rlintreq, rl_ssyn_out_h;
    wire[7:0] rlintvec;
    wire[15:00] rl_d_out_h;
    wire rltrigger;
    wire [15:00] rlcs;

    rl11 rlinst (
        .CLOCK (CLOCK),
        .RESET (fpgaoff),

        .armraddr (readaddr[4:2]),
        .armrdata (rlarmrdata),
        .armwaddr (writeaddr[4:2]),
        .armwdata (saxi_WDATA),
        .armwrite (rlarmwrite),
        .armintrq (regarmintreq[00]),

        .intreq (rlintreq),
        .irvec  (rlintvec),
        .intgnt (irq5_intr_out_h),
        .igvec  (irq5_d70_out_h),

        .a_in_h (dev_a_h),
        .c_in_h (dev_c_h),
        .d_in_h (dev_d_h),
        .init_in_h (dev_init_h),
        .msyn_in_h (dev_del_msyn_h),

        .d_out_h (rl_d_out_h),
        .ssyn_out_h (rl_ssyn_out_h)

        ,.rlcs (rlcs)
        ,.trigger (rltrigger));

    // tm11/tu10 tape controller
    wire tmintreq, tm_ssyn_out_h;
    wire[7:0] tmintvec;
    wire[15:00] tm_d_out_h;

    tm11 tminst (
        .CLOCK (CLOCK),
        .RESET (fpgaoff),

        .armraddr (readaddr[4:2]),
        .armrdata (tmarmrdata),
        .armwaddr (writeaddr[4:2]),
        .armwdata (saxi_WDATA),
        .armwrite (tmarmwrite),
        .armintrq (regarmintreq[01]),

        .intreq (tmintreq),
        .irvec  (tmintvec),
        .intgnt (irq5_intr_out_h),
        .igvec  (irq5_d70_out_h),

        .a_in_h (dev_a_h),
        .c_in_h (dev_c_h),
        .d_in_h (dev_d_h),
        .init_in_h (dev_init_h),
        .msyn_in_h (dev_del_msyn_h),

        .d_out_h (tm_d_out_h),
        .ssyn_out_h (tm_ssyn_out_h));

    // switches and lights
    wire ky_bbsy_out_h, ky_hltrq_out_h, ky_msyn_out_h;
    wire ky_npg_out_l, ky_npr_out_h, ky_sack_out_h, ky_ssyn_out_h;
    wire[1:0] ky_c_out_h;
    wire[2:0] ky_irqlev;
    wire[7:2] ky_irqvec;
    wire[15:00] ky_d_out_h;
    wire[17:00] ky_a_out_h;

    ky11 kyinst (
        .CLOCK (CLOCK),
        .powerup (powerup),
        .fpgaoff (fpgaoff),

        .armraddr (readaddr[4:2]),
        .armrdata (kyarmrdata),
        .armwaddr (writeaddr[4:2]),
        .armwdata (saxi_WDATA),
        .armwrite (kyarmwrite),

        .turbo (turbo),

        .a_in_h      (dev_a_h),          //<< address from pdp/sim to read switch register or write light register
        .ac_lo_in_h  (dev_ac_lo_h),      //<< power is going down
        .bbsy_in_h   (dev_bbsy_h),       //<< something is busy using the bus
        .c_in_h      (dev_c_h),          //<< control code from pdp/sim to read switch register or write light register
        .d_in_h      (dev_d_h),          //<< data from pdp/sim to write to light register or data being read from real memory or device
        .dc_lo_in_h  (dev_dc_lo_h),      //<< power is down
        .hltgr_in_l  (dev_hltgr_l),      //<< halt grant from pdp/sim indicating it has halted
        .hltld_in_h  (dev_hltld_h),      //<< dev_hltrq_h is updated this cycle
        .hltrq_in_h  (dev_hltrq_h),      //<< something (such as pdp original front panel or this thing) is requesting halt
        .init_in_h   (dev_init_h),       //<< bus init signal from pdp/sim for RESET instruction
        .npg_in_l    (dev_npg_l),        //<< pdp/sim says it is ok to do a DMA transfer
        .pa_in_h     (dev_pa_h),         //<< memory parity check
        .pb_in_h     (dev_pb_h),         //<< memory parity check
        .sack_in_h   (dev_sack_h),       //<< signal from pdp/sim/device indicating it is acknowledging a grant
        .syn_msyn_in_h (dev_syn_msyn_h), //<< signal from pdp/sim when reading/writing switch/light register
        .syn_ssyn_in_h (dev_syn_ssyn_h), //<< signal from pdp/sim/device indicating data transfer complete
        .del_msyn_in_h (dev_del_msyn_h), //<< signal from pdp/sim when reading/writing switch/light register
        .del_ssyn_in_h (dev_del_ssyn_h), //<< signal from pdp/sim/device indicating data transfer complete

        .irqlev (ky_irqlev),            //>> arm requesting interrupt
        .irqvec (ky_irqvec),

        .a_out_h     (ky_a_out_h),      //>> signal from front panel to read or write memory or device register
        .bbsy_out_h  (ky_bbsy_out_h),   //>> front panel is using the bus
        .c_out_h     (ky_c_out_h),      //>> control from front panel to read or write memory or device register
        .d_out_h     (ky_d_out_h),      //>> data being written to pdp/sim/memory or data being read from switch register
        .hltrq_out_h (ky_hltrq_out_h),  //>> halt switch on requesting pdp/sim to halt
        .msyn_out_h  (ky_msyn_out_h),   //>> memory cycle being performed by front panel
        .npg_out_l   (ky_npg_out_l),    //>> pass dma grant signal along
        .npr_out_h   (ky_npr_out_h),    //>> request use of bus for dma transfer
        .sack_out_h  (ky_sack_out_h),   //>> acknowledge selection to pdp/sim
        .ssyn_out_h  (ky_ssyn_out_h));  //>> switch register or light register transfer complete

    // console tty
    wire dlintreq, dl_ssyn_out_h;
    wire[7:0] dlintvec;
    wire[15:00] dl_d_out_h;

    dl11 dlinst (
        .CLOCK (CLOCK),
        .RESET (fpgaoff),

        .armraddr (readaddr[3:2]),
        .armrdata (dlarmrdata),
        .armwaddr (writeaddr[3:2]),
        .armwdata (saxi_WDATA),
        .armwrite (dlarmwrite),

        .intreq (dlintreq),
        .irvec  (dlintvec),
        .intgnt (irq4_intr_out_h),
        .igvec  (irq4_d70_out_h),

        .a_in_h (dev_a_h),
        .c_in_h (dev_c_h),
        .d_in_h (dev_d_h),
        .init_in_h (dev_init_h),
        .msyn_in_h (dev_del_msyn_h),

        .d_out_h (dl_d_out_h),
        .ssyn_out_h (dl_ssyn_out_h));

    // serial line multiplexor
    wire dzintreq, dz_ssyn_out_h;
    wire[7:0] dzintvec;
    wire[15:00] dz_d_out_h;
    wire dz_pb_out_h;

    dz11 dzinst (
        .CLOCK (CLOCK),
        .RESET (fpgaoff),

        .armraddr (readaddr[4:2]),
        .armrdata (dzarmrdata),
        .armwaddr (writeaddr[4:2]),
        .armwdata (saxi_WDATA),
        .armwrite (dzarmwrite),

        .intreq (dzintreq),
        .irvec  (dzintvec),
        .intgnt (irq5_intr_out_h),
        .igvec  (irq5_d70_out_h),

        .a_in_h (dev_a_h),
        .c_in_h (dev_c_h),
        .d_in_h (dev_d_h),
        .init_in_h (dev_init_h),
        .msyn_in_h (dev_del_msyn_h),

        .d_out_h    (dz_d_out_h),
        .pb_out_h   (dz_pb_out_h),
        .ssyn_out_h (dz_ssyn_out_h));

    // line clock
    wire kwintreq, kw_ssyn_out_h;
    wire[7:0] kwintvec;
    wire[15:00] kw_d_out_h;

    kw11 kwinst (
        .CLOCK (CLOCK),
        .RESET (fpgaoff),

        .armraddr (readaddr[2]),
        .armrdata (kwarmrdata),
        .armwaddr (writeaddr[2]),
        .armwdata (saxi_WDATA),
        .armwrite (kwarmwrite),

        .intreq (kwintreq),
        .irvec  (kwintvec),
        .intgnt (irq6_intr_out_h),
        .igvec  (irq6_d70_out_h),

        .a_in_h (dev_a_h),
        .c_in_h (dev_c_h),
        .d_in_h (dev_d_h),
        .init_in_h (dev_init_h),
        .msyn_in_h (dev_del_msyn_h),

        .d_out_h (kw_d_out_h),
        .ssyn_out_h (kw_ssyn_out_h));

    /////////////////////////////
    //  interrupt controllers  //
    /////////////////////////////

    // generate interrupt request cycles from simple request/vector lines from internal devices

    wire[7:0] intvec4 = (ky_irqlev == 4) ? { ky_irqvec, 2'b0 } : pcintreq ? pcintvec : dlintreq ? dlintvec : 1;
    wire[7:0] intvec5 = (ky_irqlev == 5) ? { ky_irqvec, 2'b0 } : rlintreq ? rlintvec : tmintreq ? tmintvec : dzintreq ? dzintvec : 1;
    wire[7:0] intvec6 = (ky_irqlev == 6) ? { ky_irqvec, 2'b0 } : kwintreq ? kwintvec : 1;
    wire[7:0] intvec7 = (ky_irqlev == 7) ? { ky_irqvec, 2'b0 } : 1;

    wire irq4_bbsy_out_h, irq4_sack_out_h;
    wire irq5_bbsy_out_h, irq5_sack_out_h;
    wire irq6_bbsy_out_h, irq6_sack_out_h;
    wire irq7_bbsy_out_h, irq7_sack_out_h;
    wire[7:4] irq_br_out_h;

    intctl irq4inst (
        .CLOCK (CLOCK),
        .RESET (fpgaoff),

        .intvec (intvec4),

        .bbsy_in_h (dev_bbsy_h),
        .bg_in_l   (dev_bg_l[4]),
        .init_in_h (dev_init_h),
        .sack_in_h (dev_sack_h),
        .syn_msyn_in_h (dev_syn_msyn_h),
        .syn_ssyn_in_h (dev_syn_ssyn_h),

        .bbsy_out_h (irq4_bbsy_out_h),
        .br_out_h   (irq_br_out_h[4]),
        .d70_out_h  (irq4_d70_out_h),
        .intr_out_h (irq4_intr_out_h),
        .sack_out_h (irq4_sack_out_h));

    intctl irq5inst (
        .CLOCK (CLOCK),
        .RESET (fpgaoff),

        .intvec (intvec5),

        .bbsy_in_h (dev_bbsy_h),
        .bg_in_l   (dev_bg_l[5]),
        .init_in_h (dev_init_h),
        .sack_in_h (dev_sack_h),
        .syn_msyn_in_h (dev_syn_msyn_h),
        .syn_ssyn_in_h (dev_syn_ssyn_h),

        .bbsy_out_h (irq5_bbsy_out_h),
        .br_out_h   (irq_br_out_h[5]),
        .d70_out_h  (irq5_d70_out_h),
        .intr_out_h (irq5_intr_out_h),
        .sack_out_h (irq5_sack_out_h));

    intctl irq6inst (
        .CLOCK (CLOCK),
        .RESET (fpgaoff),

        .intvec (intvec6),

        .bbsy_in_h (dev_bbsy_h),
        .bg_in_l   (dev_bg_l[6]),
        .init_in_h (dev_init_h),
        .sack_in_h (dev_sack_h),
        .syn_msyn_in_h (dev_syn_msyn_h),
        .syn_ssyn_in_h (dev_syn_ssyn_h),

        .bbsy_out_h (irq6_bbsy_out_h),
        .br_out_h   (irq_br_out_h[6]),
        .d70_out_h  (irq6_d70_out_h),
        .intr_out_h (irq6_intr_out_h),
        .sack_out_h (irq6_sack_out_h));

    intctl irq7inst (
        .CLOCK (CLOCK),
        .RESET (fpgaoff),

        .intvec (intvec7),

        .bbsy_in_h (dev_bbsy_h),
        .bg_in_l   (dev_bg_l[7]),
        .init_in_h (dev_init_h),
        .sack_in_h (dev_sack_h),
        .syn_msyn_in_h (dev_syn_msyn_h),
        .syn_ssyn_in_h (dev_syn_ssyn_h),

        .bbsy_out_h (irq7_bbsy_out_h),
        .br_out_h   (irq_br_out_h[7]),
        .d70_out_h  (irq7_d70_out_h),
        .intr_out_h (irq7_intr_out_h),
        .sack_out_h (irq7_sack_out_h));

    /////////////////////////////////////
    //  generate internal bus signals  //
    /////////////////////////////////////

    // bus configuration for FM_OFF,FM_SIM:

    //   [our devices]  =>  wor_ (wired-or)  =>  dev_  =>  [our devices]

    //   hi-Z  =>  unibus (unprefixed)  =>  synchronizers/demuxers syn_/dmx_/del_  =>  ignored

    // bus configuration for FM_REAL:

    //   [our devices]  =>  wor_ (wired-or)  =>  unibus (unprefixed)  =>  synchronizers/demuxers syn_/dmx_/del_  =>  dev_  =>  [our devices]

    // bus configuration for FM_MAN:

    //   zeroes  =>  dev_  =>  [our devices]

    //   man_ (arm)  =>  unibus (unprefixed)  =>  synchronizers/demuxers syn_/dmx_/del_  =>  ignored

    // manual overrides from arm processor
    wire[17:00] man_a_out_h     = regctlb[17:00];
    wire        man_ac_lo_out_h = regctla[28];
    wire        man_bbsy_out_h  = regctla[27];
    wire[7:4]   man_bg_out_l    = regctlb[27:24];
    wire[7:4]   man_br_out_h    = regctlb[23:20];
    wire[1:0]   man_c_out_h     = regctlb[19:18];
    wire[15:00] man_d_out_h     = regctla[15:00];
    wire        man_dc_lo_out_h = regctla[26];
    wire        man_hltrq_out_h = regctla[25];
    wire        man_init_out_h  = regctla[24];
    wire        man_intr_out_h  = regctla[23];
    wire        man_msyn_out_h  = regctla[22];
    wire        man_npg_out_l   = regctla[21];
    wire        man_npr_out_h   = regctla[20];
    wire        man_pa_out_h    = regctla[19];
    wire        man_pb_out_h    = regctla[18];
    assign      man_rsel_h      = regctlb[29:28];
    wire        man_sack_out_h  = regctla[17];
    wire        man_ssyn_out_h  = regctla[16];

    // wired-or of internal device outputs

    wire[17:00] wor_a_h     = man_a_out_h     |
                              bm_a_out_h      |
                              ky_a_out_h      |
                            ~ sim_a_out_l;

    wire        wor_ac_lo_h = man_ac_lo_out_h;

    wire        wor_bbsy_h  = man_bbsy_out_h  |
                              irq4_bbsy_out_h |
                              irq5_bbsy_out_h |
                              irq6_bbsy_out_h |
                              irq7_bbsy_out_h |
                              ky_bbsy_out_h   |
                            ~ sim_bbsy_out_l;

    wire[7:4]   wor_br_h    = man_br_out_h    |
                              irq_br_out_h;

    wire[1:0]   wor_c_h     = man_c_out_h     |
                              ky_c_out_h      |
                            ~ sim_c_out_l;

    wire[15:00] wor_d_h     = man_d_out_h     |
                              bm_d_out_h      |
                              dl_d_out_h      |
                              dz_d_out_h      |
                              kw_d_out_h      |
                              ky_d_out_h      |
                              pc_d_out_h      |
                              rl_d_out_h      |
                            ~ sim_d_out_l     |
                              tm_d_out_h      |

                              { 8'b0, irq4_d70_out_h |
                                      irq5_d70_out_h |
                                      irq6_d70_out_h |
                                      irq7_d70_out_h };

    wire        wor_dc_lo_h = man_dc_lo_out_h;

    wire        wor_hltrq_h = man_hltrq_out_h |
                              ky_hltrq_out_h  |
                            ~ sim_hltrq_out_l;

    wire        wor_init_h  = man_init_out_h  |
                            ~ (sim_reset_h    | sim_init_out_l);

    wire        wor_intr_h  = man_intr_out_h  |
                              irq4_intr_out_h |
                              irq5_intr_out_h |
                              irq6_intr_out_h |
                              irq7_intr_out_h;

    wire        wor_msyn_h  = man_msyn_out_h  |
                              ky_msyn_out_h   |
                            ~ sim_msyn_out_l;

    wire        wor_pa_h    = man_pa_out_h;

    wire        wor_pb_h    = man_pb_out_h    |
                              bm_pb_out_h     |
                              dz_pb_out_h;

    wire        wor_npr_h   = man_npr_out_h   |
                              ky_npr_out_h;

    wire        wor_sack_h  = man_sack_out_h  |
                              irq4_sack_out_h |
                              irq5_sack_out_h |
                              irq6_sack_out_h |
                              irq7_sack_out_h |
                              ky_sack_out_h;

    wire        wor_ssyn_h  = man_ssyn_out_h  |
                              bm_ssyn_out_h   |
                              dl_ssyn_out_h   |
                              dz_ssyn_out_h   |
                              kw_ssyn_out_h   |
                              ky_ssyn_out_h   |
                              pc_ssyn_out_h   |
                              rl_ssyn_out_h   |
                            ~ sim_ssyn_out_l  |
                              tm_ssyn_out_h;

    always @(*) begin
        case (fpgamode)

            FM_OFF, FM_SIM: begin

                // hi-Z all the transistors going out to unibus
                // except pass grant signals through
                a_out_h     = 0;
                ac_lo_out_h = 0;
                bbsy_out_h  = 0;
                bg_out_l    = bg_in_l;
                br_out_h    = 0;
                c_out_h     = 0;
                d_out_h     = 0;
                dc_lo_out_h = 0;
                hltrq_out_h = 0;
                init_out_h  = 0;
                intr_out_h  = 0;
                msyn_out_h  = 0;
                npg_out_l   = npg_in_l;
                npr_out_h   = 0;
                pa_out_h    = 0;
                pb_out_h    = 0;
                sack_out_h  = 0;
                ssyn_out_h  = 0;

                // loop signals directly back to device inputs
                dev_a_h     = wor_a_h;
                dev_ac_lo_h = wor_ac_lo_h;
                dev_bbsy_h  = wor_bbsy_h;
                dev_bg_l    = ~ sim_bg_out_h;
                dev_br_h    = wor_br_h;
                dev_c_h     = wor_c_h;
                dev_d_h     = wor_d_h;
                dev_dc_lo_h = wor_dc_lo_h;
                dev_hltgr_l = ~ sim_hltgr_out_h;
                dev_hltld_h = 1;   // wor_hltrq_h is always up-to-date
                dev_hltrq_h = wor_hltrq_h;
                dev_init_h  = wor_init_h | fpgaoff;
                dev_intr_h  = wor_intr_h;
                dev_del_msyn_h  = wor_msyn_h;
                dev_syn_msyn_h  = wor_msyn_h;
                dev_npg_l   = ~ sim_npg_out_h;
                dev_npr_h   = wor_npr_h;
                dev_pa_h    = wor_pa_h;
                dev_pb_h    = wor_pb_h;
                dev_sack_h  = wor_sack_h;
                dev_del_ssyn_h  = wor_ssyn_h;
                dev_syn_ssyn_h  = wor_ssyn_h;
            end

            FM_REAL: begin

                // send internally generated signals out to unibus
                // for bg,npg: block if requesting, else pass input as is
                a_out_h     = wor_a_h;
                ac_lo_out_h = wor_ac_lo_h;
                bbsy_out_h  = wor_bbsy_h;
                bg_out_l    = wor_br_h | syn_bg_in_l;
                br_out_h    = wor_br_h;
                c_out_h     = wor_c_h;
                d_out_h     = wor_d_h;
                dc_lo_out_h = wor_dc_lo_h;
                hltrq_out_h = wor_hltrq_h;
                init_out_h  = wor_init_h;
                intr_out_h  = wor_intr_h;
                msyn_out_h  = wor_msyn_h;
                npg_out_l   = wor_npr_h | syn_npg_in_l;
                npr_out_h   = wor_npr_h;
                pa_out_h    = wor_pa_h;
                pb_out_h    = wor_pb_h;
                sack_out_h  = wor_sack_h;
                ssyn_out_h  = wor_ssyn_h;

                // receive those same signals back, wire-and/ored with unibus signals
                // delayed a bit as they loop through external transistors then back in through synchronizers / demultiplexors
                dev_a_h     = dmx_a_in_h;
                dev_ac_lo_h = syn_ac_lo_in_h;
                dev_bbsy_h  = syn_bbsy_in_h;
                dev_bg_l    = syn_bg_in_l;
                dev_c_h     = dmx_c_in_h;
                dev_d_h     = dmx_d_in_h;
                dev_dc_lo_h = syn_dc_lo_in_h;
                dev_hltgr_l = syn_hltgr_in_l;
                dev_hltld_h = dmx_haltloaded;  // dmx_hltrq_in_h is updated this cycle
                dev_hltrq_h = dmx_hltrq_in_h;
                dev_init_h  = syn_init_in_h;
                dev_intr_h  = syn_intr_in_h;
                dev_npg_l   = syn_npg_in_l;
                dev_npr_h   = dmx_npr_in_h;
                dev_pa_h    = dmx_pa_in_h;
                dev_pb_h    = dmx_pb_in_h;
                dev_sack_h  = syn_sack_in_h;
                dev_del_msyn_h = del_msyn_in_h;
                dev_syn_msyn_h = syn_msyn_in_h;
                dev_del_ssyn_h = del_ssyn_in_h;
                dev_syn_ssyn_h = syn_ssyn_in_h;
            end

            // manual pin testing (edgepintest.tcl)
            FM_MAN: begin
                a_out_h     = man_a_out_h;
                ac_lo_out_h = man_ac_lo_out_h;
                bbsy_out_h  = man_bbsy_out_h;
                bg_out_l    = man_bg_out_l;
                br_out_h    = man_br_out_h;
                c_out_h     = man_c_out_h;
                d_out_h     = man_d_out_h;
                dc_lo_out_h = man_dc_lo_out_h;
                hltrq_out_h = man_hltrq_out_h;
                init_out_h  = man_init_out_h;
                intr_out_h  = man_intr_out_h;
                msyn_out_h  = man_msyn_out_h;
                npg_out_l   = man_npg_out_l;
                npr_out_h   = man_npr_out_h;
                pa_out_h    = man_pa_out_h;
                pb_out_h    = man_pb_out_h;
                sack_out_h  = man_sack_out_h;
                ssyn_out_h  = man_ssyn_out_h;

                dev_a_h     =  0;
                dev_ac_lo_h =  0;
                dev_bbsy_h  =  0;
                dev_bg_l    = 15;
                dev_c_h     =  0;
                dev_d_h     =  0;
                dev_dc_lo_h =  0;
                dev_hltgr_l =  0;
                dev_hltld_h =  1;  // the 0 for dev_hltrq_h is always up-to-date
                dev_hltrq_h =  0;
                dev_init_h  =  0;
                dev_intr_h  =  0;
                dev_npg_l   =  1;
                dev_npr_h   =  0;
                dev_sack_h  =  0;
                dev_del_msyn_h =  0;
                dev_syn_msyn_h =  0;
                dev_del_ssyn_h =  0;
                dev_syn_ssyn_h =  0;
            end
        endcase
    end

    // debug register on z11dump page
    assign regctlh = { 16'hABCD, 5'b0, dev_bbsy_h, dev_intr_h, dev_sack_h, sim_bg_out_h, dev_br_h };

    /////////////////////////////////
    //  integrated logic analyzer  //
    /////////////////////////////////

    //  ilaarmed = 0: trigger condition satisfied
    //             1: waiting for trigger condition
    //  ilaafter = number of cycles to record after trigger condition satisfied
    //  ilaoflow = 0: index did not overflow while recording
    //             1: index overflowed while recording
    //  ilaindex = next entry in ilaarray to write

    reg lastmsyn;
    always @(posedge CLOCK) begin
        lastmsyn <= dev_syn_msyn_h;
    end

    wire ilatrigr = 0; // ((dev_a_h & 18'o777770) == 18'o760100) & dev_syn_msyn_h;
    wire ilaenabl = (lastmsyn & ~ dev_syn_msyn_h); // 1;

    // detect 'TSTB @R5 ; BPL .-2' to edit out lineclock testing loop
    /***
    reg[2:0] looped1, nextlp1;
    always @(*) begin
                 if ((dev_d_h == 16'o105715) & (regctlk_0500 == 3)) begin
            nextlp1[2]   = (looped1[1:0] == 3);
            nextlp1[1:0] = 1;
        end else if ((dev_d_h == 16'o000000) & (looped1[1:0] == 1)) begin
            nextlp1[2]   = looped1[2];
            nextlp1[1:0] = 2;
        end else if ((dev_d_h == 16'o100376) & (looped1[1:0] == 2)) begin
            nextlp1[2]   = looped1[2];
            nextlp1[1:0] = 3;
        end else begin
            nextlp1[2:0] = 0;
        end
    end
    ***/

    // detect 'DEC R4 ; BNE .-2' to edit out delay loop
    /***
    reg[2:0] looped2, nextlp2;
    always @(*) begin
                 if ((dev_d_h == 16'o005304) & (regctlk_0500 == 3)) begin
            nextlp2[2]   = (looped2[1:0] == 2);
            nextlp2[1:0] = 1;
        end else if ((dev_d_h == 16'o001376) & (looped2[1:0] == 1)) begin
            nextlp2[2]   = looped2[2];
            nextlp2[1:0] = 2;
        end else begin
            nextlp2[2:0] = 0;
        end
    end
    ***/

    always @(*) begin
        ilacurwd = {
            10'b0,              //54
            regctlk_0500,       //48 sim state
            dev_a_h,            //30
            dev_bg_l,           //26
            dev_br_h,           //22
            dev_c_h,            //20
            dev_d_h,            //04
            dev_syn_msyn_h,     //03
            dev_npg_l,          //02
            dev_npr_h,          //01
            dev_syn_ssyn_h      //00
        };
    end

    always @(posedge CLOCK) begin
        if (fpgaoff) begin
            ilaafter <= 0;
            ilaarmed <= 0;
            ilaindex <= 0;
         // looped1  <= 0;
         // looped2  <= 0;
        end else begin
            if (armwrite & (writeaddr == 10'b0000011100)) begin

                // arm processor is writing control register
                ilaarmed                    <= saxi_WDATA[31];
                ilaafter[ILAADDRBITS-01:00] <= saxi_WDATA[ILAADDRBITS+15:16];
                ilaoflow                    <= saxi_WDATA[15];
                ilaindex[ILAADDRBITS-01:00] <= saxi_WDATA[ILAADDRBITS-01:00];

                ilardata <= ilaarray[saxi_WDATA[ILAADDRBITS-01:00]];

            // capture signals while before trigger and for ilaafter*(idivid+1) cycles thereafter
            end else if (ilaarmed | (ilaafter != 0)) begin

                // save word
                if (ilaenabl) begin
                 // if (~ nextlp1[2] & ~ nextlp2[2]) begin
                        ilaarray[ilaindex[ILAADDRBITS-01:00]] <= ilacurwd;
                        ilaoflow <= ilaoflow | (ilaindex[ILAADDRBITS-01:00] == (1 << ILAADDRBITS) - 1);
                        ilaindex[ILAADDRBITS-01:00] <= ilaindex[ILAADDRBITS-01:00] + 1;
                        if (~ ilaarmed) ilaafter <= ilaafter - 1;
                 // end
                 // looped1 <= nextlp1;
                 // looped2 <= nextlp2;
                end

                // check trigger condition
                if (ilatrigr) begin
                    ilaarmed <= 0;
                end
            end
        end
    end
endmodule
