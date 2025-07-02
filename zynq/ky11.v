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
// * unibus read/write functions for examine/deposit
//   can also be used by arm devices for dma
//   note dma functionality works regardless of whether processor is running or halted,
//   ...reverting to examine/deposit when halted (ie, doesn't bother with npr/npg like real KY11)

module ky11 (
    input CLOCK, RESET,

    input armwrite,
    input[2:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input turbo,

    input[17:00] a_in_h,
    input ac_lo_in_h,
    input bbsy_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input dc_lo_in_h,
    input hltgr_in_l,
    input hltld_in_h,
    input hltrq_in_h,
    input init_in_h,
    input npg_in_l,
    input pa_in_h,
    input pb_in_h,
    input sack_in_h,
    input syn_msyn_in_h,    // coming from bus, synchronized to 100MHz clock
    input syn_ssyn_in_h,
    input del_msyn_in_h,    // coming from bus, delayed for all multiplexed signals
    input del_ssyn_in_h,

    output reg[2:0] irqlev,
    output reg[7:2] irqvec,

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

    reg dmaperr, dmatimo, enable, halted, haltins, haltreq, stepreq, snaphlt, snapreq, snapwrt;
    reg[1:0] dmactrl;
    reg[2:0] dmastate, haltstate;
    reg[3:0] snapctr;
    reg[9:0] dmadelay;
    reg[15:00] dmadata, lights, switches;
    reg[15:00] snapreg, snapregs[15:00];
    reg[17:00] dmaaddr;
    reg[31:00] dmalock;
    reg[17:16] sr1716;

    reg[15:00] dma_d_out_h, swr_d_out_h;
    assign d_out_h = dma_d_out_h | swr_d_out_h;

    assign armrdata = (armraddr == 0) ? 32'h4B592014 : // [31:16] = 'KY'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? {
                            lights,         //16 ro 777570 light register
                            switches } :    //00 rw 777570 switch register
                      (armraddr == 2) ? {
                            enable,         //31 rw enable 777570 lights/switches register from unibus
                            haltreq,        //30 rw request processor to halt
                            halted,         //29 ro processor is halted
                            stepreq,        //28 rw step processor one cycle (self clearing, then wait for halted)
                            snapctr,        //24 rw number of registers to snapshot - 1
                            sr1716,         //22 rw GUI switch register 17:16 bits
                            haltstate,      //19 ro ky11.v internal debug (normally 0 when runnung; 3 when halted)
                            hltrq_out_h,    //18 ro ky11.v is asserting the HLTRQ line going to processor
                            haltins,        //17 ro processor has executed an HALT instr, ACLO/DCLO reset required to get it going
                            irqlev,         //14 rw arm can request interrupt at this level 4,5,6,7 (NOT self-clearing)
                            irqvec,         //08 rw vector to be sent when interrupt granted
                            6'b0,           //02
                            snaphlt,        //01 rw halt on completion of snapshot (0=continue; 1=halt)
                            snapreq } :     //00 rw request snapshot of registers while processor running
                      (armraddr == 3) ? {
                            dmastate,       //29 rw 0=idle; arm sets to 1 to start a dma cycle, ky11.v sets back to 0 when done
                            dmatimo,        //28 ro dma cycle timed out
                            dmactrl,        //26 rw type of cycle to be done: 0=DATI; 1=DATIP; 2=DATO; 3=DATOB
                            dmaperr,        //25 ro dma read cycle parity error
                            7'b0,           //18
                            dmaaddr } :     //00 rw 18-bit address
                      (armraddr == 4) ? {
                            snapreg,        //16 ro selected snap register contents
                            dmadata } :     //00 rw 16-bit data
                      (armraddr == 5) ? {
                            dmalock } :     //00 rw 0=dma circuitry not in use; else 32-bit pid of process using dma circuitry
                                            //      (see Z11Util::dmalock() and z11ctrlini.tcl dmalock)
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
                snapreq     <= 0;
                stepreq     <= 0;
            end
            a_out_h     <= 0;
            bbsy_out_h  <= 0;
            c_out_h     <= 0;
            dma_d_out_h <= 0;
            dmastate    <= 0;
            haltins     <= 0;
            irqlev      <= 0;
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
                    snapctr  <= armwdata[27:24];
                    sr1716   <= armwdata[23:22];
                    irqlev   <= armwdata[16:14];
                    irqvec   <= armwdata[13:08];
                    snaphlt  <= armwdata[01];
                    snapreq  <= armwdata[00];
                    snapreg  <= snapregs[armwdata[27:24]];
                end
                3: if (dmastate == 0) begin
                    dmaaddr  <= armwdata[17:00];
                    dmactrl  <= armwdata[27:26];
                    dmatimo  <= armwdata[29];
                    dmastate <= { 2'b0, armwdata[29] & ~ init_in_h };
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
        else if (~ del_msyn_in_h) begin
            swr_d_out_h <= 0;
            ssyn_out_h  <= 0;
        end else if (enable & ({ a_in_h[17:01], 1'b0 } == 18'o777570) & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin
                if (~ c_in_h[0] |   a_in_h[00]) lights[15:08] <= d_in_h[15:08];
                if (~ c_in_h[0] | ~ a_in_h[00]) lights[07:00] <= d_in_h[07:00];
                if (d_in_h == 0) irqlev <= 0;
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

            // wait for someone to push our HALT button
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
            // processor should resume processing instructions
            // ...assuming it isn't jammed up somehow
            3: begin
                if (~ haltreq) begin
                    haltstate   <= 0;
                    sack_out_h  <= 0;
                end else if (snapreq) begin

                    // do register snapshot
                    //  arm processor must:
                    //   1) use dmalock to lock dma so nothing else is using dma circuit
                    //   2) set dmaaddr = 777700, dmactrl = 0, ie, start by reading R0
                    //   3) set haltreq = 1, snapctr = 15, snapreq = 1, ie, do this procedure upon halting
                    //   4) wait for snapreq = 0, ie, this procedure complete
                    //      haltreq self clearing so processor resumes asap
                    //   5) retrieve registers from snapregs[], indexing with snapctr
                    //   6) release dmalock
                    case (dmastate)

                        // dma idle, start reading register from processor
                        // assume processor is halted
                        // assume dmaaddr is set up with 777700..777717
                        0: begin
                            if (~ dmatimo) begin
                                dmastate <= 2;
                                dmatimo  <= 1;
                                snapwrt  <= 1;
                            end else begin
                                haltreq  <= 0;
                                snapreq  <= 0;
                            end
                        end

                        // dma just clocked register contents into dmadata
                        // save it then either set up for next or say we're done
                        6: begin
                            if (snapwrt) begin
                                snapregs[snapctr] <= dmadata;
                                snapwrt           <= 0;
                                if (~ dmaperr & (snapctr != 0)) begin
                                    dmaaddr[4:0]  <= dmaaddr[4:0] + ((dmaaddr[12:04] == 9'o774) ? 1 : 2);
                                    snapctr       <= snapctr - 1;
                                end else begin
                                    haltreq <= snaphlt;
                                    snapreq <= 0;
                                end
                            end
                        end
                    endcase
                end
            end
        endcase

        // determine if processor is halted
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
        // - if processor executes an HALT instruction, it jams the HLTRQ line asserted
        //   (and thus HLTGR asserts) until processor is reset via ACLO/DCLO
        if (~ RESET) begin
            if (~ hltgr_in_l) begin
                halted <= 1;
            end else if (~ hltrq_in_h & ~ sack_in_h) begin
                halted <= 0;
            end
        end

        // single stepper
        // - stop requesting processor to halt
        // - as soon as it starts back up (not halted), request halt
        if (~ RESET & ~ armwrite & stepreq) begin
            if (halted) begin
                haltreq <= 0;
            end else begin
                haltreq <= 1;
                stepreq <= 0;
            end
        end

        // dma transaction initiated by arm processor
        if (~ init_in_h) case (dmastate)

            0: dmadelay <= 0;

            // if processor is running, do a non-processor request
            // if processor halted, just start using bus, presumably we are only one that would
            // take into account that processor may halt after we assert npr but before it asserts npg
            1: begin
                dmaperr <= 0;
                if (halted) begin
                    dmastate   <= 2;    // halted, make like exam/deposit, and assume we have the bus
                    npr_out_h  <= 0;
                end else if (~ npr_out_h) begin
                    dmadelay   <= 0;    // running, need to do a non-processor request and wait for grant
                    npr_out_h  <= 1;
                end else if (npg_in_l) begin
                    dmadelay   <= 0;    // debounce grant in case something else requested at same time we did
                end else if (dmadelay[2:0] != 4) begin
                    dmadelay   <= dmadelay + 1;
                end else begin
                    dmastate   <= 2;    // debounced grant, acknowledge selection
                    sack_out_h <= 1;
                end
            end

            // make sure bus not busy doing something else, then send out address, control, data, say bus is busy
            2: if (~ bbsy_in_h & ~ syn_msyn_in_h & ~ syn_ssyn_in_h) begin
                a_out_h     <= dmaaddr;
                bbsy_out_h  <= 1;
                c_out_h     <= dmactrl;
                dma_d_out_h <= dmactrl[1] ? dmadata : 0;
                dmadelay    <= 0;
                dmastate    <= 3;
                npr_out_h   <= 0;
            end

            // after 150nS, send out msyn
            3: begin
                sack_out_h     <= halted;
                if ((dmadelay[3:0] != 15) & ~ turbo) begin
                    dmadelay   <= dmadelay + 1;
                end else begin
                    msyn_out_h <= 1;
                    dmadelay   <= 0;
                    dmastate   <= 4;
                end
            end

            // wait up to 10uS for ssyn
            // if timeout, release everything and leave dmatimo set
            4: begin
                if (del_ssyn_in_h) begin
                    dmadelay    <= 0;
                    dmastate    <= 5;
                end else if (dmadelay != 1000) begin
                    dmadelay    <= dmadelay + 1;
                end else begin
                    a_out_h     <= 0;
                    bbsy_out_h  <= 0;
                    c_out_h     <= 0;
                    dma_d_out_h <= 0;
                    dmastate    <= 0;
                    msyn_out_h  <= 0;
                end
            end

            // wait 80nS then clock in read data and drop msyn
            5: begin
                if ((dmadelay[3:0] != 8) & ~ turbo) begin
                    dmadelay <= dmadelay + 1;
                end else begin
                    if (~ dmactrl[1]) begin
                        dmadata <= d_in_h;
                        // same formula as KD11-E K2-1 C8
                        dmaperr <= ~ pa_in_h & pb_in_h;
                    end
                    dmadelay   <= 0;
                    dmastate   <= 6;
                    msyn_out_h <= 0;
                end
            end

            // wait 80nS then drop everything else and tell arm it completed successfully
            6: begin
                if ((dmadelay[3:0] != 8) & ~ turbo) begin
                    dmadelay    <= dmadelay + 1;
                end else if (~ del_ssyn_in_h) begin
                    a_out_h     <= 0;
                    bbsy_out_h  <= 0;
                    c_out_h     <= 0;
                    dma_d_out_h <= 0;
                    dmatimo     <= 0;
                    dmastate    <= 0;
                end
            end
        endcase
    end
endmodule
