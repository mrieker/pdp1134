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

// PDP-11 paper tape reader/punch interface

module pc11
    #(parameter[17:00] ADDR=18'o777550,
      parameter[7:0] INTVEC=8'o070) (
    input CLOCK, RESET,

    input armwrite,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    output intreq,      // interrupt being requested
    output[7:0] irvec,  // vector for the interrupt request
    input intgnt,       // interrupt being granted (clear request)
    input[7:0] igvec,   // what interrupt is being granted

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input init_in_h,
    input msyn_in_h,

    output reg[15:00] d_out_h,
    output reg ssyn_out_h);

    reg enable, lastrenab;
    reg[15:00] rcsr, rbuf, xcsr, xbuf;
    wire pulserirq;

    assign armrdata = (armraddr == 0) ? 32'h50431004 : // [31:16] = 'PC'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { rbuf, rcsr } :
                      (armraddr == 2) ? { xbuf, xcsr } :
                      { enable, 5'b0, INTVEC, ADDR };

    intreq pcintreq (
        .CLOCK    (CLOCK),
        .RESET    (init_in_h),
        .INTVEC   (INTVEC),
        .rirqlevl ((rcsr[15] | rcsr[07]) & rcsr[06] & ~ pulserirq),
        .xirqlevl ((xcsr[15] | xcsr[07]) & xcsr[06]),
        .intreq   (intreq),
        .irvec    (irvec),
        .intgnt   (intgnt),
        .igvec    (igvec)
    );

    // pulse reader interrupt request when trying to set GO when ERROR is currently set
    assign pulserirq = msyn_in_h & enable & (a_in_h[17:01] == ADDR[17:01]) & c_in_h[1] &
                (~ c_in_h[0] | ~ a_in_h[00]) & d_in_h[00] & rcsr[15];

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                enable   <= 0;
                rcsr[15] <= 1;
                xcsr[15] <= 1;
            end
            rcsr[14:00] <= 15'o00000;
            xcsr[14:00] <= (RESET | xcsr[15]) ? 15'o0000 : 15'o00200;
            d_out_h     <= 0;
            ssyn_out_h  <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    rbuf[07:00] <= armwdata[23:16];
                    rcsr[15]    <= armwdata[15];
                    rcsr[11]    <= armwdata[11];
                    rcsr[07]    <= armwdata[07];
                    rcsr[00]    <= armwdata[00];
                end
                2: begin
                    xcsr[15] <= armwdata[15];
                    xcsr[07] <= armwdata[07];
                end
                3: enable <= armwdata[31];
            endcase
        end

        // pdp or something else is accessing an i/o register
        else if (~ msyn_in_h) begin
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end else if (enable & (a_in_h[17:03] == ADDR[17:03]) & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin
                case (a_in_h[02:01])

                    // pdp writing reader status
                    0: begin
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            rcsr[06] <= d_in_h[06];         // interrupt enable
                            if (~ rcsr[15]) begin
                                rcsr[00] <= d_in_h[00];     // reader start
                                if (d_in_h[00]) begin       // reader start
                                    rcsr[07] <= 0;          // clear done
                                    rcsr[11] <= 1;          // set busy
                                    rbuf     <= 0;          // clear data buffer
                                end
                            end
                        end
                    end

                    // pdp writing punch status
                    2: begin
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            xcsr[06] <= d_in_h[06]; // interrupt enable
                        end
                    end

                    // pdp writing punch buffer
                    3: begin
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            xbuf[07:00] <= d_in_h[07:00];
                        end
                        xcsr[07] <= 0;              // punch busy
                    end
                endcase
            end else begin

                // pdp reading a register
                case (a_in_h[02:01])
                    0: begin d_out_h <= rcsr & 16'o104300; end
                    1: begin d_out_h <= rbuf; rcsr[07] <= 0; end
                    2: begin d_out_h <= xcsr & 16'o100300; end
                    3: begin d_out_h <= xbuf; end
                endcase
            end
        end

        lastrenab <= rcsr[00];
    end
endmodule
