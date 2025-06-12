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

// Provide up to 248KB memory using external block RAM module
// 62 enable bits provide independent enables for each 4KB

module bigmem (
    input CLOCK,
    input powerup,  // fpga is powering up
    input fpgaoff,  // powerup | (fpgamode == FM_OFF)
    input businit,  // fpgaoff | (unibus init)

    input armwrite,
    input[2:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input msyn_in_h,

    output[17:00] a_out_h,
    output reg[15:00] d_out_h,
    output reg pb_out_h,
    output reg ssyn_out_h,

    output reg[16:00] extmemaddr,
    output reg[17:00] extmemdout,
    input[17:00]      extmemdin,
    output reg        extmemenab,
    output reg[1:0]   extmemwena
);

    reg[63:00] enable;
    reg[2:0] armfunc;
    reg[3:0] armcount, brjama, ctladdr, delayline;
    reg[17:00] armaddr;
    reg[15:00] armdata, ctlreg, brenab;
    reg armpehi, armpelo, brjamepc, brjameps, ctlenab;

    assign armrdata = (armraddr == 0) ? 32'h424D2008 :          // [31:16] = 'BM'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { enable[31:00] } :     //00 rw enable unibus access for addresses 000000..377777
                      (armraddr == 2) ? { 2'b0,                 //30
                                          enable[61:32] } :     //00 rw enable unibus access for addresses 400000..757777
                      (armraddr == 3) ? { armfunc,              //29 rw 4=read; 3=write word; 2=write high byte; 1=write low byte (self clearing)
                                          1'b0,                 //28
                                          armcount,             //24 ro number of arm cycles done (debug)
                                          6'b0,                 //18
                                          armaddr } :           //00 rw word being accessed (bit 00 ignored)
                      (armraddr == 4) ? { delayline,            //28 ro internal state (debug)
                                          10'b0,                //18
                                          armpehi,              //17 rw high byte parity bit
                                                                //      reading: 0=ok; 1=error
                                                                //      writing: 0=normal; 1=force error
                                          armpelo,              //16 rw low byte parity bit
                                                                //      reading: 0=ok; 1=error
                                                                //      writing: 0=normal; 1=force error
                                          armdata } :           //00 rw 16-bit data word
                      (armraddr == 5) ? { ctlreg,               //16
                                          11'b0,                //05
                                          ctlenab,              //04 enable M7850-like controller
                                          ctladdr } :           //00 M7850-like control register address 772100..772136
                      (armraddr == 6) ? { 11'b0,                //21
                                          brjamepc | brjameps,  //20 boot rom jam enable
                                          brjama,               //16 boot rom jam addr bits
                                          brenab } :            //00 boot rom addr enables
                      32'hDEADBEEF;

    // detect error on read data coming from block ram
    wire perdinhi = ~ extmemdin[17] ^ extmemdin[16] ^ extmemdin[15] ^ extmemdin[14] ^ extmemdin[13] ^ extmemdin[12] ^ extmemdin[11] ^ extmemdin[10] ^ extmemdin[09];
    wire perdinlo = ~ extmemdin[08] ^ extmemdin[07] ^ extmemdin[06] ^ extmemdin[05] ^ extmemdin[04] ^ extmemdin[03] ^ extmemdin[02] ^ extmemdin[01] ^ extmemdin[00];

    // generate odd parity for write data coming from unibus
    // allow ctlreg[02] to force bad parity bit
    wire pdpparhi = ~ ctlreg[02] ^ d_in_h[15] ^ d_in_h[14] ^ d_in_h[13] ^ d_in_h[12] ^ d_in_h[11] ^ d_in_h[10] ^ d_in_h[09] ^ d_in_h[08];
    wire pdpparlo = ~ ctlreg[02] ^ d_in_h[07] ^ d_in_h[06] ^ d_in_h[05] ^ d_in_h[04] ^ d_in_h[03] ^ d_in_h[02] ^ d_in_h[01] ^ d_in_h[00];

    // generate odd parity for write data coming from arm
    // allow arm to force bad parity bit
    wire armparhi = ~ armpehi ^ armdata[15] ^ armdata[14] ^ armdata[13] ^ armdata[12] ^ armdata[11] ^ armdata[10] ^ armdata[09] ^ armdata[08];
    wire armparlo = ~ armpelo ^ armdata[07] ^ armdata[06] ^ armdata[05] ^ armdata[04] ^ armdata[03] ^ armdata[02] ^ armdata[01] ^ armdata[00];

    // jam address bus
    // forces top address bits to the boot rom whilst processor reads power-up vector
    // ...redirecting the processor to the boot rom xxx024/xxx026
    assign a_out_h = (brjamepc | brjameps) ? { 5'b11111, brjama, 9'b000000000 } : 0;

    wire addressingmainmem = enable[a_in_h[17:12]];                                 // 000000..757777   4KB per enable bit (enable[63:62] always zeroes)
    wire addressingbootrom = (a_in_h[17:13] == 5'b11111) & brenab[a_in_h[12:09]];   // 760000..777777  512B per enable bit

    always @(posedge CLOCK) begin
        if (powerup) begin
            armcount   <= 0;
            armfunc    <= 0;
            ctlenab    <= 0;
            enable     <= 0;
            brjamepc   <= 0;
            brjameps   <= 0;
            brenab     <= 0;
        end
        if (fpgaoff | ~ msyn_in_h & (delayline < 5)) begin
            delayline  <= 0;
            extmemenab <= 0;
            extmemwena <= 0;
        end
        if (businit) begin
            ctlreg     <= 0;
        end
        if (~ msyn_in_h) begin
            d_out_h    <= 0;
            pb_out_h   <= 0;
            ssyn_out_h <= 0;
        end

        // arm processor is writing one of the registers
        if (~ powerup & armwrite) begin
            case (armwaddr)
                1: begin
                    enable[31:00] <= armwdata[31:00];
                end
                2: begin
                    enable[61:32] <= armwdata[29:00];
                end
                3: begin
                    armfunc <= armwdata[31:29];
                    armaddr <= armwdata[17:00];
                end
                4: begin
                    armdata <= armwdata[15:00];
                    armpelo <= armwdata[16];
                    armpehi <= armwdata[17];
                end
                5: begin
                    ctlenab <= armwdata[04];    // enable M7850-like control register
                    ctladdr <= armwdata[03:00]; // register address, 772100..772136
                end
                6: begin
                    brjamepc <= armwdata[20];    // jam address bus bits
                    brjameps <= armwdata[20];
                    brjama   <= armwdata[19:16]; // what a<12:09> get jammed to (a<17:13> get jammed to 11111)
                    brenab   <= armwdata[15:00]; // each 512-byte segment from 760000..777777 to enable
                end
            endcase
        end

        if (~ powerup & ~ armwrite) case (delayline)

            // wait for something to do
            0: begin

                // arm is wanting to access the memory
                //  armfunc = 4: read word
                //            3: write word
                //            2: write upper byte
                //            1: write lower byte
                if (armfunc != 0) begin
                    delayline  <= 5;
                    extmemaddr <= armaddr[17:01];
                    extmemdout[17]    <= armparhi;
                    extmemdout[16:09] <= armdata[15:08];
                    extmemdout[08]    <= armparlo;
                    extmemdout[07:00] <= armdata[07:00];
                    extmemenab <= 1;
                    extmemwena <= armfunc[1:0];
                end

                // something on unibus is accessing a memory location
                else if ((addressingmainmem | addressingbootrom) & msyn_in_h) begin
                    delayline  <= 1;
                    extmemaddr <= a_in_h[17:01];
                    extmemenab <= 1;
                    if (c_in_h[1]) begin
                        extmemdout[17]    <= pdpparhi;
                        extmemdout[16:09] <= d_in_h[15:08];
                        extmemdout[08]    <= pdpparlo;
                        extmemdout[07:00] <= d_in_h[07:00];
                        extmemwena[1] <= ~ c_in_h[00] |   a_in_h[00];
                        extmemwena[0] <= ~ c_in_h[00] | ~ a_in_h[00];
                    end
                end

                // maybe pdp is accessing the M7850-like control register
                else if (ctlenab & (a_in_h[17:01] == { 12'o7721, 1'b0, ctladdr }) & msyn_in_h & ~ ssyn_out_h) begin
                    if (c_in_h[1]) begin
                        if (~ c_in_h[0] |   a_in_h[00]) begin
                            ctlreg[15]    <= d_in_h[15];
                            ctlreg[11:08] <= d_in_h[11:08];
                        end
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            ctlreg[07:03] <= d_in_h[07:03];
                            ctlreg[02]    <= d_in_h[02];
                            ctlreg[00]    <= d_in_h[00];
                        end
                    end else begin
                        d_out_h <= ctlreg;
                    end
                    ssyn_out_h <= 1;
                end
            end

            // 1, 2, 3: delay for pdp access

            // finishing up pdp access
            4: begin
                if (~ msyn_in_h) begin
                    d_out_h   <= 0;
                    delayline <= 0;
                    pb_out_h  <= 0;

                    // just completed reading PC or PS of power up vector
                    // ...clear the boot rom address jam enables one by one
                    // ...so address bus will function normally
                    if (addressingbootrom & ~ a_in_h[01]) brjamepc <= 0;
                    if (addressingbootrom &   a_in_h[01]) brjameps <= 0;
                end else if (~ c_in_h[1] & extmemenab) begin
                    // output read data to unibus
                    d_out_h[15:08] <= extmemdin[16:09];
                    d_out_h[07:00] <= extmemdin[07:00];
                    // 11/34 (K2-1 C8):
                    //  parityerror_h = ~ bus_pb_l & ~ ~ bus_pa_l
                    //                = ~ bus_pb_l & bus_pa_l
                    //                = pb_out_h & ~ pa_out_h
                    // so leave pa_out_h always low (bus_pa_l float high with pullup)
                    // and indicate parity error by asserting pb_out_h (drives bus_pb_l low)
                    if (perdinhi | perdinlo) begin
                        ctlreg[15]    <= 1;
                        ctlreg[11:03] <= a_in_h[17:09];
                        pb_out_h      <= ctlreg[00];
                    end
                end
                extmemenab <= 0;
                extmemwena <= 0;
                ssyn_out_h <= msyn_in_h;
            end

            // 5, 6, 7: delay for arm access

            // finishing up arm access
            8: begin
                if (armfunc[2]) begin
                    armdata[15:08] <= extmemdin[16:09];
                    armdata[07:00] <= extmemdin[07:00];
                    armpehi <= perdinhi;
                    armpelo <= perdinlo;
                end
                armcount   <= armcount + 1;
                armfunc    <= 0;
                delayline  <= 0;
                extmemenab <= 0;
                extmemwena <= 0;
            end

            default: delayline <= delayline + 1;
        endcase
    end
endmodule
