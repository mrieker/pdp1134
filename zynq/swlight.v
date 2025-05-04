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

module swlight (
    input CLOCK, RESET,

    input armwrite,
    input[2:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input hltgr_in_l,
    input init_in_h,
    input msyn_in_h,
    input npg_in_l,
    input ssyn_in_h,

    output reg[17:00] a_out_h,
    output ac_lo_out_h,
    output reg bbsy_out_h,
    output reg[1:0] c_out_h,
    output[15:00] d_out_h,
    output dc_lo_out_h,
    output hltrq_out_h,
    output init_out_h,
    output reg msyn_out_h,
    output npg_out_l,
    output reg npr_out_h,
    output reg sack_out_h,
    output reg ssyn_out_h);

    reg dmafail, enable, haltreq, stepreq, businit, aclow, dclow;
    reg[1:0] dmactrl, haltstate;
    reg[2:0] dmastate;
    reg[9:0] dmadelay;
    reg[15:00] dmadata, lights, switches;
    reg[17:00] dmaaddr;
    wire halted;

    reg[15:00] dma_d_out_h, swr_d_out_h;
    assign d_out_h = dma_d_out_h | swr_d_out_h;

    assign armrdata = (armraddr == 0) ? 32'h534C2003 : // [31:16] = 'SL'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { lights, switches } :
                      (armraddr == 2) ? { enable, haltreq, halted, stepreq, businit, aclow, dclow, 25'b0 } :
                      (armraddr == 3) ? { dmastate, dmafail, dmactrl, 8'b0, dmaaddr } :
                      (armraddr == 4) ? { 16'b0, dmadata } :
                      32'hDEADBEEF;

    assign halted = ~ hltgr_in_l;
    assign hltrq_out_h = haltreq;

    assign ac_lo_out_h = aclow;
    assign dc_lo_out_h = dclow;
    assign init_out_h  = businit;
    assign npg_out_l   = npr_out_h ? 1 : npg_in_l;

    always @(posedge CLOCK) begin
        if (RESET) begin
            aclow       <= 0;
            businit     <= 0;
            dclow       <= 0;
            dmastate    <= 0;
            enable      <= 0;
            haltreq     <= 0;
            haltstate   <= 0;
            stepreq     <= 0;
        end
        if (init_in_h) begin
            a_out_h     <= 0;
            bbsy_out_h  <= 0;
            c_out_h     <= 0;
            dma_d_out_h <= 0;
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
                    enable  <= armwdata[31];
                    haltreq <= armwdata[30];
                    stepreq <= armwdata[28];
                    businit <= armwdata[27];
                end
                3: if (dmastate == 0) begin
                    dmaaddr  <= armwdata[17:00];
                    dmactrl  <= armwdata[27:26];
                    dmastate <= { 2'b0, armwdata[29] };
                end
                4: if (dmastate == 0) begin
                    dmadata  <= armwdata[15:00];
                end
            endcase
        end

        // something on unibus is accessing a register
        else if (~ msyn_in_h) begin
            swr_d_out_h <= 0;
            ssyn_out_h  <= 0;
        end else if (enable & (a_in_h[17:01] == 18'o777570 >> 1) & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin
                if (~ c_in_h[0] |   a_in_h[00]) lights[15:08] <= d_in_h[15:08];
                if (~ c_in_h[0] | ~ a_in_h[00]) lights[07:00] <= d_in_h[07:00];
            end else begin
                swr_d_out_h <= switches;
            end
        end

        // dma transaction initiated by arm processor
        case (dmastate)

            0: dmadelay <= 0;

            // if processor is running, do a non-processor request
            // if processor halted, just start using bus, presumably we are only one that would
            1: begin
                dmafail <= 0;
                if (~ hltgr_in_l | npr_out_h & ~ npg_in_l) begin
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
                a_out_h     <= dmaaddr;
                c_out_h     <= dmactrl;
                dma_d_out_h <= dmactrl[1] ? dmadata : 0;
                dmadelay    <= 0;
                dmastate    <= 3;
            end

            // wait 150nS deskew/decode before sending msyn out
            3: begin
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
    end
endmodule
