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

// generate edge-triggered interrupt request

//  input:
//   CLOCK    = fpga 100MHz clock
//   RESET    = fpga & bus reset
//   INTVEC   = interrupt vector (usually a constant)
//   rirqlevl = request receive interrupt request level
//   xirqlevl = request transmit interrupt request level
//   intgnt   = interrupt vector is being sent to pdp
//   igvec    = interrupt vector being sent to pdp

//  output:
//   intreq = composite edge interrupt request
//   irvec  = corresponding interrupt vector
//            rirq : send INTVEC (has priority over xirq)
//            xirq : send INTVEC | 4

module intreq (
    input CLOCK, RESET,
    input[7:0] INTVEC,

    input rirqlevl,
    input xirqlevl,

    output intreq,
    output[7:0] irvec,
    input intgnt,
    input[7:0] igvec);

    reg lastrilev, lastxilev, rirqedge, xirqedge;
    assign intreq = rirqedge | xirqedge;
    assign irvec  = { INTVEC[7:3], INTVEC[2] | ~ rirqlevl, 2'b0 };

    always @(posedge CLOCK) begin
        if (RESET) begin
            lastrilev <= 0;
            lastxilev <= 0;
            rirqedge  <= 0;
            xirqedge  <= 0;
        end

        // pdp is granting our interrupt request, reset corresponding edge detector
        else if (intgnt & (igvec[7:3] == INTVEC[7:3])) begin
            if (INTVEC[2]) begin
                rirqedge <= 0;
                xirqedge <= 0;
            end else begin
                if (igvec[2]) xirqedge <= 0;
                         else rirqedge <= 0;
            end
        end

        // otherwise, detect interrupt request edges
        else begin

            if (~ rirqlevl) rirqedge <= 0;
            else if (~ lastrilev) rirqedge <= 1;
            lastrilev <= rirqlevl;

            if (~ xirqlevl) xirqedge <= 0;
            else if (~ lastxilev) xirqedge <= 1;
            lastxilev <= xirqlevl;
        end
    end
endmodule
