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

// PDP-11 teletype interface

// arm registers:
//  [0] = ident='TT',sizecode=1,version
//  [1] = <kbflag> <enable> 000000000000000000 <kbchar>
//  [2] = <prflag> <prfull> 000000000000000000 <prchar>
//  [3] = 00000000000000000000000000 <KBDEV>

module dl11
    #(parameter[8:3] KBDEV=6'o03) (
    input CLOCK, RESET,

    input armwrite,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input bus_ac_lo_l,
    input bus_bbsy_l,
    input bus_br_l[7:4],
    input bus_dc_lo_l,
    input bus_intr_l,
    input bus_npr_l,
    input bus_pa_l,
    input bus_pb_l,
    input bus_sack_l,
    input halt_rqst_l,

    input bus_a_in_l[17:00],
    input bus_c_in_l[1:0],
    input bus_d_in_l[15:00],
    input bus_init_in_l,
    input bus_msyn_in_l,
    input bus_ssyn_in_l,

    output reg bus_a_out_l[17:00],
    output reg bus_c_out_l[1:0],
    output reg bus_d_out_l[15:00],
    output reg bus_init_out_l,
    output reg bus_msyn_out_l,
    output reg bus_ssyn_out_l,

    output reg bus_bg_h[7:4],
    output reg bus_npg_h,
    output reg halt_grant_h
);

    localparam[11:00] kbio = 12'o6000 + (KBDEV << 3);
    localparam[11:00] ttio = 12'o6010 + (KBDEV << 3);

    reg enable, intenab, kbflag, prflag, prfull;
    reg[11:00] kbchar, prchar;

    assign armrdata = (armraddr == 0) ? 32'h444C1001 : // [31:16] = 'DL'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { kbflag, enable, 18'b0, kbchar } :
                      (armraddr == 2) ? { prflag, prfull, 18'b0, prchar } :
                      { 23'b0, intenab, 2'b0, KBDEV };

    assign INT_RQST = intenab & (kbflag | prflag);

    always @(posedge CLOCK) begin
        if (BINIT) begin
            if (RESET) begin
                enable <= 1;
            end
            intenab <= 1;
            kbflag  <= 0;
            prflag  <= 0;
            prfull  <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)

                // arm processor is sending a keyboard char for the PDP-8/L code to read
                // - typically sets kbflag = 1 indicating kbchar has something in it
                1: begin kbflag <= armwdata[31]; enable <= armwdata[30]; kbchar <= armwdata[11:00]; end

                // arm processor is telling PDP-8/L that it has finished printing char
                // - typically sets prflag = 1 meaning it has finished printing char
                //         and sets prfull = 0 indicating prchar is empty
                2: begin prflag <= armwdata[31]; prfull <= armwdata[30]; end
            endcase
        end else if (bus_a_in_l[17:03] == (~ 18'o777560)[17:03]) begin
            if (bus_msyn_l) begin
                bus_d_out_l    <= 16'o177777;
                bus_ssyn_out_l <= 1;
            end else begin
                if (bus_c_in_l[1]) begin
                    case (~ bus_a_in[02:01])
                        0: begin ~ bus_d_out_l <= rcsr; end
                        1: begin ~ bus_d_out_l <= rbuf; rcsr[07] <= 0; end
                        2: begin ~ bus_d_out_l <= xcsr; end
                        3: begin ~ bus_d_out_l <= xbuf; end
                    endcase
                end else begin
                    case (~ bus_a_in[02:01])
                        0: begin
                            if (bus_c_in_l[0] | ~ bus_a_in_l[00]) rcsr[15:08] <= ~ bus_d_in_l[15:08];
                            if (bus_c_in_l[0] |   bus_a_in_l[00]) rcsr[07:00] <= ~ bus_d_in_l[07:00];
                        end
                        1: begin
                            if (bus_c_in_l[0] | ~ bus_a_in_l[00]) rbuf[15:08] <= ~ bus_d_in_l[15:08]; 
                            if (bus_c_in_l[0] |   bus_a_in_l[00]) rbuf[07:00] <= ~ bus_d_in_l[07:00]; 
                        end
                        2: begin
                            if (bus_c_in_l[0] | ~ bus_a_in_l[00]) xcsr[15:08] <= ~ bus_d_in_l[15:08]; 
                            if (bus_c_in_l[0] |   bus_a_in_l[00]) xcsr[07:00] <= ~ bus_d_in_l[07:00]; 
                        end
                        3: begin
                            if (bus_c_in_l[0] | ~ bus_a_in_l[00]) xbuf[15:08] <= ~ bus_d_in_l[15:08]; 
                            if (bus_c_in_l[0] |   bus_a_in_l[00]) xbuf[07:00] <= ~ bus_d_in_l[07:00]; 
                        end
                    endcase
                end
            end
        end


CSTEP) begin

            // process the IOP only on the leading edge
            // ...but leave output signals to PDP-8/L in place until given the all clear
            if (iopstart & enable) begin
                case (ioopcode)
                    kbio+1: begin IO_SKIP <= kbflag; end                                // skip if kb char ready
                    kbio+2: begin AC_CLEAR <= 1; kbflag <= 0; end                       // clear accum, clear flag
                    kbio+4: begin devtocpu <= kbchar; end                               // read kb char
                    kbio+5: begin intenab <= cputodev[00]; end                          // enable/disable interrupts
                    kbio+6: begin devtocpu <= kbchar; AC_CLEAR <= 1; kbflag <= 0; end   // read kb char, clear flag
                    ttio+1: begin IO_SKIP <= prflag; end                                // skip if done printing
                    ttio+2: begin prflag <= 0; end                                      // stop saying done printing
                    ttio+4: begin prchar <= cputodev; prfull <= 1; end                  // start printing char
                    ttio+5: begin IO_SKIP <= INT_RQST; end                              // skip if requesting interrupt
                    ttio+6: begin prchar <= cputodev; prfull <= 1; prflag <= 0; end     // start printing char, clear done flag
                endcase
            end

            // processor no longer sending an IOP, stop sending data over the bus so it doesn't jam other devices
            else if (iopstop) begin
                AC_CLEAR <= 0;
                devtocpu <= 0;
                IO_SKIP  <= 0;
            end
        end
    end
endmodule
