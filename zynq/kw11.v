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

// PDP-11 line clock interface

module kw11 (
    input CLOCK, RESET,

    input armwrite,
    input armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    output reg intreq,
    output[7:0] irvec,
    input intgnt,
    input[7:0] igvec,

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input init_in_h,
    input msyn_in_h,

    output reg[15:00] d_out_h,
    output reg ssyn_out_h);

    reg enable, fiftyhz, lkflag, lkiena, trigger, tripped;
    reg[22:00] counter, intcount;

    assign armrdata = (armraddr == 0) ? 32'h4B570004 : // [31:16] = 'KW'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      { enable, intcount, lkflag, lkiena, 3'b0, fiftyhz, trigger, tripped };

    assign irvec = 8'o100;

    always @(posedge CLOCK) begin
        if (RESET) begin
            counter <= 0;
            enable  <= 0;
            fiftyhz <= 0;
            trigger <= 0;
        end else begin

            // arm processor is writing one of the registers
            if (armwrite & armwaddr) begin
                enable  <= armwdata[31];
                fiftyhz <= armwdata[02];
            end

            if (fiftyhz) begin

                // toggle trigger 50 times a second
                // 1E8/50 = 2E6
                if (counter == 0) trigger <= ~ trigger;
                counter <= (counter == 1999999) ? 0 : counter + 1;
            end else begin

                // toggle trigger 60 times a second
                // 1E8/60 = 1E7/6 = 5E6/3
                if ((counter == 0) | (counter == 4999999/3) | (counter == 4999999*2/3)) trigger <= ~ trigger;
                counter <= (counter == 4999999) ? 0 : counter + 1;
            end
        end

        if (init_in_h) begin
            intcount   <= 0;
            intreq     <= 0;
            lkflag     <= 1;
            lkiena     <= 0;
            tripped    <= trigger;
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end

        // maybe pdp is granting interrupt
        else if (intgnt & (igvec == irvec) & intreq) begin
            intcount <= intcount + 1;
            intreq   <= 0;
        end

        // if it hasn't seen this transition yet, flag the pdp
        else if (tripped != trigger) begin
            intreq  <= lkiena;
            lkflag  <= 1;
            tripped <= trigger;
        end

        // pdp or something else is accessing an i/o register
        else if (~ msyn_in_h) begin
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end else if (enable & ((a_in_h[17:00] & 18'o777776) == 18'o777546) & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin
                // pdp writing register
                if (~ c_in_h[0] | ~ a_in_h[00]) begin
                    lkflag <= 0;
                    lkiena <= d_in_h[06];
                end
            end else begin
                // pdp reading register
                d_out_h <= { 8'b0, lkflag, lkiena, 6'b0 };
            end
        end
    end
endmodule
