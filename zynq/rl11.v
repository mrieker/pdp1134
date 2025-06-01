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

// PDP-11 RL01/2 disk interface

module rl11
    #(parameter[17:00] ADDR=18'o774400,
      parameter[7:0] INTVEC=8'o160) (
    input CLOCK, RESET,

    input armwrite,
    input[2:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    output intreq,
    output[7:0] irvec,
    input intgnt,
    input[7:0] igvec,

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input init_in_h,
    input msyn_in_h,

    output reg[15:00] d_out_h,
    output reg ssyn_out_h

    ,output trigger);

    reg enable;
    reg[15:00] rlba, rlda, rlmp1, rlmp2, rlmp3;
    reg rlcs_15, rlcs_14, rlcs_00;
    reg[13:01] rlcs_1301;
    reg[3:0] driveerrors, drivereadys;
    wire[1:0] driveselect = rlcs_1301[09:08];

    assign armrdata = (armraddr == 0) ? 32'h524C2002 : // [31:16] = 'RL'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { rlba,  rlcs_15, rlcs_14, rlcs_1301, rlcs_00 } :
                      (armraddr == 2) ? { rlmp1, rlda  } :
                      (armraddr == 3) ? { rlmp3, rlmp2 } :
                      (armraddr == 4) ? { 24'b0, driveerrors, drivereadys } :
                      (armraddr == 5) ? { enable, 5'b0, INTVEC, ADDR } :
                      32'hDEADBEEF;

    assign trigger = rlcs_1301[07] & (rlda == 16'o002250);

    intreq rlintreq (
        .CLOCK    (CLOCK),
        .RESET    (init_in_h),
        .INTVEC   (INTVEC),
        .rirqlevl (rlcs_1301[07] & rlcs_1301[06]),
        .xirqlevl (0),
        .intreq   (intreq),
        .irvec    (irvec),
        .intgnt   (intgnt),
        .igvec    (igvec)
    );

    always @(*) begin
        rlcs_00 = drivereadys[driveselect];
        rlcs_14 = driveerrors[driveselect];
        rlcs_15 = rlcs_14 | rlcs_1301[13] | rlcs_1301[12] | rlcs_1301[11] | rlcs_1301[10];
    end

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                enable <= 0;
                driveerrors <= 0;
                drivereadys <= 0;
            end

            rlcs_1301   <= 13'b0000001000000;
            rlba[15:00] <= 0;
            rlda[15:00] <= 0;
            d_out_h     <= 0;
            ssyn_out_h  <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    rlba  <= armwdata[31:16];
                    rlcs_1301 <= armwdata[13:01];
                end
                2: begin
                    rlmp1 <= armwdata[31:16];
                    rlda  <= armwdata[15:00];
                end
                3: begin
                    rlmp3 <= armwdata[31:16];
                    rlmp2 <= armwdata[15:00];
                end
                4: begin
                    driveerrors <= armwdata[07:04];
                    drivereadys <= armwdata[03:00];
                end
                5: begin
                    enable <= armwdata[31];
                end
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

                    // pdp writing control/status register
                    0: begin
                        if (~ c_in_h[0] |   a_in_h[00]) begin
                            rlcs_1301[09:08] <= d_in_h[09:08];
                        end
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            rlcs_1301[07:01] <= d_in_h[07:01];
                        end
                    end

                    // pdp writing bus address
                    1: begin
                        if (~ c_in_h[0] |   a_in_h[00]) begin
                            rlba[15:08] <= d_in_h[15:08];
                        end
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            rlba[07:01] <= d_in_h[07:01];
                        end
                    end

                    // pdp writing disk address
                    2: begin
                        if (~ c_in_h[0] |   a_in_h[00]) begin
                            rlda[15:08] <= d_in_h[15:08];
                        end
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            rlda[07:00] <= d_in_h[07:00];
                        end
                    end

                    // pdp writing multi-purpose
                    3: begin
                        if (~ c_in_h[0] |   a_in_h[00]) begin
                            rlmp1[15:08] <= d_in_h[15:08];
                            rlmp2[15:08] <= d_in_h[15:08];
                            rlmp3[15:08] <= d_in_h[15:08];
                        end
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            rlmp1[07:00] <= d_in_h[07:00];
                            rlmp2[07:00] <= d_in_h[07:00];
                            rlmp3[07:00] <= d_in_h[07:00];
                        end
                    end
                endcase
            end else begin

                // pdp reading a register
                case (a_in_h[02:01])
                    0: begin d_out_h <= { rlcs_15, rlcs_14, rlcs_1301, rlcs_00 }; end
                    1: begin d_out_h <= rlba; end
                    2: begin d_out_h <= rlda; end
                    3: begin d_out_h <= rlmp1; rlmp1 <= rlmp2; rlmp2 <= rlmp3; rlmp3 <= rlmp1; end
                endcase
            end
        end
    end
endmodule
