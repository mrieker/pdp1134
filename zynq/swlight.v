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

// switches and lights
// * 777570 switch and light register
// * halt, continue, step switches, run light
// * init, aclo, dclo signal access
// * memory (bus) read/write functions for exam/deposit
//   can also be used by arm devices for dma

module swlight (
    input CLOCK, RESET,

    input armwrite,
    input[2:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input[17:00] a_in_h,
    input ac_lo_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input dc_lo_in_h,
    input hltgr_in_l,
    input hltld_in_h,
    input hltrq_in_h,
    input init_in_h,
    input msyn_in_h,
    input npg_in_l,
    input sack_in_h,
    input ssyn_in_h,

    output reg[17:00] a_out_h,
    output reg bbsy_out_h,
    output reg[1:0] c_out_h,
    output[15:00] d_out_h,
    output reg hltrq_out_h,
    output reg msyn_out_h,
    output npg_out_l,
    output reg npr_out_h,
    output reg sack_out_h,
    output reg ssyn_out_h);

    reg dmafail, enable, halted, haltins, haltreq, stepreq;
    reg[1:0] dmactrl;
    reg[2:0] dmastate, haltstate;
    reg[9:0] dmadelay;
    reg[15:00] dmadata, lights, switches;
    reg[17:00] dmaaddr;
    reg[31:00] dmalock, dmaflags;

    reg[15:00] dma_d_out_h, swr_d_out_h;
    assign d_out_h = dma_d_out_h | swr_d_out_h;

    assign armrdata = (armraddr == 0) ? 32'h534C200A : // [31:16] = 'SL'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { lights, switches } :
                      (armraddr == 2) ? {
                            enable,         //31
                            haltreq,        //30
                            halted,         //29
                            stepreq,        //28
                            6'b0,           //22
                            haltstate,      //19
                            hltrq_out_h,    //18
                            haltins,        //17
                            17'b0 } :
                      (armraddr == 3) ? { dmastate, dmafail, dmactrl, 8'b0, dmaaddr } :
                      (armraddr == 4) ? { 16'b0, dmadata } :
                      (armraddr == 5) ? { dmalock } :
                      (armraddr == 6) ? dmaflags :
                      32'hDEADBEEF;

    assign npg_out_l = npr_out_h ? 1 : npg_in_l;

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                dmalock     <= 0;
                enable      <= 0;
                halted      <= 0;
                haltstate   <= 0;
                haltreq     <= 0;
                hltrq_out_h <= 0;
                stepreq     <= 0;
            end
            a_out_h     <= 0;
            bbsy_out_h  <= 0;
            c_out_h     <= 0;
            dma_d_out_h <= 0;
            dmaflags    <= 32'h12345678;
            dmastate    <= 0;
            haltins     <= 0;
            msyn_out_h  <= 0;
            npr_out_h   <= 0;
            sack_out_h  <= 0;
            swr_d_out_h <= 0;
            ssyn_out_h  <= 0;
        end

        // arm processor is writing one of the registers
        if (armwrite) begin
            case (armwaddr)
                1: begin
                    switches <= armwdata[15:00];
                end
                2: begin
                    enable   <= armwdata[31];
                    haltreq  <= armwdata[30];
                    stepreq  <= armwdata[28];
                end
                3: if (dmastate == 0) begin
                    dmaaddr  <= armwdata[17:00];
                    dmactrl  <= armwdata[27:26];
                    dmastate <= { 2'b0, armwdata[29] };
                    dmaflags <= { 28'hABCD000, dmaflags[3:0] + 4'h1 };
                end
                4: if (dmastate == 0) begin
                    dmadata  <= armwdata[15:00];
                end
                5: begin
                         if (dmalock == 0) dmalock <= armwdata;
                    else if (dmalock == armwdata) dmalock <= 0;
                end
            endcase
        end

        // something on unibus is accessing a 777570 register
        else if (~ msyn_in_h) begin
            swr_d_out_h <= 0;
            ssyn_out_h  <= 0;
        end else if (enable & ({ a_in_h[17:01], 1'b0 } == 18'o777570) & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin
                if (~ c_in_h[0] |   a_in_h[00]) lights[15:08] <= d_in_h[15:08];
                if (~ c_in_h[0] | ~ a_in_h[00]) lights[07:00] <= d_in_h[07:00];
            end else begin
                swr_d_out_h <= switches;
            end
        end

        // if an HALT instruction makes its way into the instruction register,
        // the processor jams the Unibus HLTRQ_L signal low, and the only recovery
        // is to reset with ACLO/DCLO.  we detect this condition as the processor
        // is the only thing on the bus, other than us, that will assert HLTRQ.
        if (~ hltrq_in_h) haltins <= 0;                     // if Unibus HLTRQ is negated, then HALT instr not in IR
        else if (hltld_in_h & ~ hltrq_out_h) haltins <= 1;  // Unibus HLTRQ is asserted, HALT instr if we aren't requesting halt

        // halt the processor
        // the processor gets confused with HLTRQ and DCLO at same time
        // ...so abandon halt request if doing hard reset with DCLO
        if (dc_lo_in_h) begin
            haltstate   <= 0;
            hltrq_out_h <= 0;
        end else case (haltstate)

            // wait for somone to push our HALT button
            // then request processor to halt (assert HLTRQ)
            0: begin
                if (haltreq) begin
                    haltstate   <= 1;
                    hltrq_out_h <= 1;
                end
            end

            // when processor grants halt (HLTGR asserted), assert SACK
            1: begin
                if (~ hltgr_in_l) begin
                    haltstate   <= 2;
                    sack_out_h  <= 1;
                end
            end

            // when SACK loops back through transistors, negate HLTRQ
            2: begin
                if (sack_in_h) begin
                    haltstate   <= 3;
                    hltrq_out_h <= 0;
                end
            end

            // maintain SACK until HALT button released
            3: begin
                if (~ haltreq) begin
                    haltstate   <= 0;
                    sack_out_h  <= 0;
                end
            end
        endcase

        // determine if processor is halted, even if the external front panel halted it
        // the protocol is:
        //   console asserts HLTRQ
        //   processor asserts HLTGR
        //   console asserts SACK
        //   console negates HLTRQ
        //   processor negates HLTGR
        //   console maintains SACK to hold processor in halt state
        //   console negates SACK to resume processor
        // - assume that if processor is granting halt, it is halted
        // - it may drop the grant but remains halted until request and sack are dropped
        if (~ RESET) begin
            if (~ hltgr_in_l) begin
                halted <= 1;
            end else if (~ hltrq_in_h & ~ sack_in_h) begin
                halted <= 0;
            end
        end

        // dma transaction initiated by arm processor
        case (dmastate)

            0: dmadelay <= 0;

            // if processor is running, do a non-processor request
            // if processor halted, just start using bus, presumably we are only one that would
            // take into account that processor may halt after we assert npg but before it asserts npr
            1: begin
                dmaflags[4] <= 1;
                dmafail <= 0;
                if (halted | npr_out_h & ~ npg_in_l) begin
                    dmaflags[5] <= 1;
                    // deglitch grant signal in case upstream requested at same time we did
                    if (dmadelay[2:0] != 4) begin
                        dmadelay   <= dmadelay + 1;
                    end else begin
                        bbsy_out_h <= 1;
                        dmastate   <= 2;
                        npr_out_h  <= 0;
                        sack_out_h <= 1;
                    end
                end else begin
                    dmaflags[6] <= 1;
                    dmadelay <= 0;
                    // make sure not granted to downstream before we make request
                    // ...so we don't steal its grant after it may have seen it
                    if (npg_in_l) begin
                        npr_out_h  <= 1;
                    end
                end
            end

            // send address, control and maybe data out
            // if reading, d_out_h must be 0 so it doesn't stomp on incoming data
            2: begin
                dmaflags[7] <= 1;
                a_out_h     <= dmaaddr;
                c_out_h     <= dmactrl;
                dma_d_out_h <= dmactrl[1] ? dmadata : 0;
                dmadelay    <= 0;
                dmastate    <= 3;
            end

            // wait 150nS deskew/decode before sending msyn out
            3: begin
                dmaflags[8] <= 1;
                if (dmadelay[3:0] != 15) begin
                    dmadelay   <= dmadelay + 1;
                end else begin
                    dmastate   <= 4;
                    msyn_out_h <= 1;
                end
            end

            // wait up to 10uS for ssyn reply
            // if not received, set dmafail flag and finish up
            4: begin
                dmaflags[9] <= 1;
                if (ssyn_in_h) begin
                    dmadelay   <= 0;
                    dmastate   <= 5;
                end else if (dmadelay != 1023) begin
                    dmadelay   <= dmadelay + 1;
                end else begin
                    dmadelay   <= 0;
                    dmafail    <= 1;
                    dmastate   <= 6;
                    msyn_out_h <= 0;
                end
            end

            // wait 150nS for deskewing then clock in read data and drop msyn
            5: begin
                dmaflags[10] <= 1;
                if (dmadelay[3:0] != 15) begin
                    dmadelay   <= dmadelay + 1;
                end else begin
                    if (~ dmactrl[1]) begin
                        dmadata <= d_in_h;
                    end
                    dmadelay   <= 0;
                    dmastate   <= 6;
                    msyn_out_h <= 0;
                end
            end

            // wait 150nS then drop everything, we're done
            6: begin
                dmaflags[11] <= 1;
                if (dmadelay[3:0] != 15) begin
                    dmadelay   <= dmadelay + 1;
                end else begin
                    a_out_h     <= 0;
                    bbsy_out_h  <= 0;
                    c_out_h     <= 0;
                    dma_d_out_h <= 0;
                    dmastate    <= 0;
                end
            end
        endcase

        // single stepper
        // - stop requesting processor to halt
        // - as soon as it starts back up, request halt
        if (stepreq) begin
            if (~ halted) begin
                hltrq_out_h <= 1;
                stepreq <= 0;
            end else begin
                hltrq_out_h <= 0;
            end
        end
    end
endmodule
