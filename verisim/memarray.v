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

// 128KW memory array
// takes the place of block memory IP block in Vivado
// 2 cycle latency for reading
// byte writeable
// 1 parity bit per byte

module memarray (
  clka,
  ena,
  wea,
  addra,
  dina,
  douta
);

    input wire clka;
    input wire ena;
    input wire [1:0] wea;
    input wire [16:0] addra;
    input wire [17:0] dina;
    output reg [17:0] douta;

    reg[8:0] arrayhi[131071:0];
    reg[8:0] arraylo[131071:0];
    reg[17:0] delay;

    always @(posedge clka) begin
        if (ena) begin
            douta <= delay;
            delay <= { arrayhi[addra], arraylo[addra] };
            if (wea[1]) arrayhi[addra] <= dina[17:09];
            if (wea[0]) arraylo[addra] <= dina[08:00];
        end else begin
            douta <= 18'o615243;
            delay <= 18'o162534;
        end
    end
endmodule
