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

// handle bus request / bus grant for a single level

//  input:
//   RESET  = includes bus init
//   intvec = 1 : no interrupt being requested
//         else : interrupt vector (must be 32-vut aligned)

module intctl (
    input CLOCK, RESET,

    input[7:0] intvec,

    input bbsy_in_h,
    input bg_in_l,
    input sack_in_h,
    input ssyn_in_h,

    output reg bbsy_out_h,
    output bg_out_l,
    output reg br_out_h,
    output reg[15:00] d_out_h,
    output reg intr_out_h,
    output reg sack_out_h);

    assign bg_out_l = br_out_h ? 1 : bg_in_l;

    always @(posedge CLOCK) begin
        if (RESET) begin
            bbsy_out_h <= 0;
            br_out_h   <= 0;
            d_out_h    <= 0;
            intr_out_h <= 0;
            sack_out_h <= 0;
        end else begin
            if (~ intvec[0] & ~ sack_out_h & ~ intr_out_h & ~ br_out_h) begin
                br_out_h   <= 1;
            end else if (br_out_h & ~ bg_in_l & ~ sack_in_h) begin
                br_out_h   <= 0;
                sack_out_h <= 1;
            end else if (sack_out_h & ~ bbsy_in_h & bg_in_l & ~ ssyn_in_h) begin
                if (~ intvec[0]) begin
                    bbsy_out_h <= 1;
                    d_out_h    <= { 8'b0, intvec[7:2], 2'b0 };
                    intr_out_h <= 1;
                end
                sack_out_h <= 0;
            end else if (bbsy_out_h & ssyn_in_h) begin
                bbsy_out_h <= 0;
                d_out_h    <= 0;
                intr_out_h <= 0;
            end
        end
    end
endmodule
