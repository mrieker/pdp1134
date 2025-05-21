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

// M9312 Boot ROM card

module m9312 (
    input CLOCK,

    input[17:00] a_in_h,
    input msyn_in_h,

    output reg[15:00] d_out_h,
    output reg ssyn_out_h
);

    reg[4:0] delay;
    always @(posedge CLOCK) begin
        if (~ msyn_in_h | (a_in_h[17:09] != 9'o765)) begin
            d_out_h    <= 0;
            delay      <= 0;
            ssyn_out_h <= 0;
        end else if (delay != 19) begin
            delay <= delay + 1;
        end else begin
            case ({a_in_h[08:01],1'b0})
                9'o000: d_out_h <= 16'o165000;
                9'o002: d_out_h <= 16'o165000;
                9'o004: d_out_h <= 16'o100000;
                9'o006: d_out_h <= 16'o177777;
                9'o010: d_out_h <= 16'o165006;
                9'o012: d_out_h <= 16'o165006;
                9'o014: d_out_h <= 16'o000500;
                9'o016: d_out_h <= 16'o000501;
                9'o020: d_out_h <= 16'o005003;
                9'o022: d_out_h <= 16'o005203;
                9'o024: d_out_h <= 16'o005103;
                9'o026: d_out_h <= 16'o006203;
                9'o030: d_out_h <= 16'o006303;
                9'o032: d_out_h <= 16'o006003;
                9'o034: d_out_h <= 16'o005703;
                9'o036: d_out_h <= 16'o005403;
                9'o040: d_out_h <= 16'o005303;
                9'o042: d_out_h <= 16'o005603;
                9'o044: d_out_h <= 16'o006103;
                9'o046: d_out_h <= 16'o005503;
                9'o050: d_out_h <= 16'o000303;
                9'o052: d_out_h <= 16'o001377;
                9'o054: d_out_h <= 16'o012702;
                9'o056: d_out_h <= 16'o165000;
                9'o060: d_out_h <= 16'o011203;
                9'o062: d_out_h <= 16'o022203;
                9'o064: d_out_h <= 16'o001377;
                9'o066: d_out_h <= 16'o063203;
                9'o070: d_out_h <= 16'o165203;
                9'o072: d_out_h <= 16'o044203;
                9'o074: d_out_h <= 16'o056203;
                9'o076: d_out_h <= 16'o000012;
                9'o100: d_out_h <= 16'o037203;
                9'o102: d_out_h <= 16'o000012;
                9'o104: d_out_h <= 16'o001777;
                9'o106: d_out_h <= 16'o010703;
                9'o110: d_out_h <= 16'o000123;
                9'o112: d_out_h <= 16'o012703;
                9'o114: d_out_h <= 16'o165122;
                9'o116: d_out_h <= 16'o000133;
                9'o120: d_out_h <= 16'o000113;
                9'o122: d_out_h <= 16'o165120;
                9'o124: d_out_h <= 16'o105767;
                9'o126: d_out_h <= 16'o177654;
                9'o130: d_out_h <= 16'o001377;
                9'o132: d_out_h <= 16'o022222;
                9'o134: d_out_h <= 16'o105722;
                9'o136: d_out_h <= 16'o001377;
                9'o140: d_out_h <= 16'o105712;
                9'o142: d_out_h <= 16'o100377;
                9'o144: d_out_h <= 16'o010701;
                9'o146: d_out_h <= 16'o000554;
                9'o150: d_out_h <= 16'o010701;
                9'o152: d_out_h <= 16'o000526;
                9'o154: d_out_h <= 16'o010400;
                9'o156: d_out_h <= 16'o000524;
                9'o160: d_out_h <= 16'o010600;
                9'o162: d_out_h <= 16'o010701;
                9'o164: d_out_h <= 16'o000521;
                9'o166: d_out_h <= 16'o010500;
                9'o170: d_out_h <= 16'o000517;
                9'o172: d_out_h <= 16'o010605;
                9'o174: d_out_h <= 16'o010701;
                9'o176: d_out_h <= 16'o000540;
                9'o200: d_out_h <= 16'o112702;
                9'o202: d_out_h <= 16'o000100;
                9'o204: d_out_h <= 16'o010703;
                9'o206: d_out_h <= 16'o000554;
                9'o210: d_out_h <= 16'o010706;
                9'o212: d_out_h <= 16'o000544;
                9'o214: d_out_h <= 16'o000302;
                9'o216: d_out_h <= 16'o000542;
                9'o220: d_out_h <= 16'o020227;
                9'o222: d_out_h <= 16'o046040;
                9'o224: d_out_h <= 16'o001450;
                9'o226: d_out_h <= 16'o020402;
                9'o230: d_out_h <= 16'o001001;
                9'o232: d_out_h <= 16'o005725;
                9'o234: d_out_h <= 16'o010204;
                9'o236: d_out_h <= 16'o020227;
                9'o240: d_out_h <= 16'o042440;
                9'o242: d_out_h <= 16'o001446;
                9'o244: d_out_h <= 16'o020227;
                9'o246: d_out_h <= 16'o042040;
                9'o250: d_out_h <= 16'o001432;
                9'o252: d_out_h <= 16'o020227;
                9'o254: d_out_h <= 16'o051415;
                9'o256: d_out_h <= 16'o001002;
                9'o260: d_out_h <= 16'o000005;
                9'o262: d_out_h <= 16'o000115;
                9'o264: d_out_h <= 16'o012704;
                9'o266: d_out_h <= 16'o173000;
                9'o270: d_out_h <= 16'o031427;
                9'o272: d_out_h <= 16'o000200;
                9'o274: d_out_h <= 16'o001323;
                9'o276: d_out_h <= 16'o022402;
                9'o300: d_out_h <= 16'o001405;
                9'o302: d_out_h <= 16'o061404;
                9'o304: d_out_h <= 16'o020427;
                9'o306: d_out_h <= 16'o174000;
                9'o310: d_out_h <= 16'o001731;
                9'o312: d_out_h <= 16'o000766;
                9'o314: d_out_h <= 16'o010701;
                9'o316: d_out_h <= 16'o000423;
                9'o320: d_out_h <= 16'o000005;
                9'o322: d_out_h <= 16'o113705;
                9'o324: d_out_h <= 16'o173024;
                9'o326: d_out_h <= 16'o106105;
                9'o330: d_out_h <= 16'o106105;
                9'o332: d_out_h <= 16'o000164;
                9'o334: d_out_h <= 16'o000010;
                9'o336: d_out_h <= 16'o010701;
                9'o340: d_out_h <= 16'o000412;
                9'o342: d_out_h <= 16'o010015;
                9'o344: d_out_h <= 16'o000713;
                9'o346: d_out_h <= 16'o010701;
                9'o350: d_out_h <= 16'o000406;
                9'o352: d_out_h <= 16'o010005;
                9'o354: d_out_h <= 16'o005004;
                9'o356: d_out_h <= 16'o000706;
                9'o360: d_out_h <= 16'o010506;
                9'o362: d_out_h <= 16'o011505;
                9'o364: d_out_h <= 16'o000675;
                9'o366: d_out_h <= 16'o005000;
                9'o370: d_out_h <= 16'o005002;
                9'o372: d_out_h <= 16'o010703;
                9'o374: d_out_h <= 16'o000453;
                9'o376: d_out_h <= 16'o120227;
                9'o400: d_out_h <= 16'o000015;
                9'o402: d_out_h <= 16'o001433;
                9'o404: d_out_h <= 16'o162702;
                9'o406: d_out_h <= 16'o000070;
                9'o410: d_out_h <= 16'o062702;
                9'o412: d_out_h <= 16'o000010;
                9'o414: d_out_h <= 16'o103357;
                9'o416: d_out_h <= 16'o006300;
                9'o420: d_out_h <= 16'o006300;
                9'o422: d_out_h <= 16'o006300;
                9'o424: d_out_h <= 16'o050200;
                9'o426: d_out_h <= 16'o000760;
                9'o430: d_out_h <= 16'o012702;
                9'o432: d_out_h <= 16'o000030;
                9'o434: d_out_h <= 16'o000261;
                9'o436: d_out_h <= 16'o006100;
                9'o440: d_out_h <= 16'o106102;
                9'o442: d_out_h <= 16'o010703;
                9'o444: d_out_h <= 16'o000435;
                9'o446: d_out_h <= 16'o012702;
                9'o450: d_out_h <= 16'o020206;
                9'o452: d_out_h <= 16'o006300;
                9'o454: d_out_h <= 16'o001403;
                9'o456: d_out_h <= 16'o106102;
                9'o460: d_out_h <= 16'o103774;
                9'o462: d_out_h <= 16'o000765;
                9'o464: d_out_h <= 16'o000302;
                9'o466: d_out_h <= 16'o010703;
                9'o470: d_out_h <= 16'o000423;
                9'o472: d_out_h <= 16'o022121;
                9'o474: d_out_h <= 16'o000161;
                9'o476: d_out_h <= 16'o177776;
                9'o500: d_out_h <= 16'o012702;
                9'o502: d_out_h <= 16'o014012;
                9'o504: d_out_h <= 16'o010703;
                9'o506: d_out_h <= 16'o000414;
                9'o510: d_out_h <= 16'o061702;
                9'o512: d_out_h <= 16'o003767;
                9'o514: d_out_h <= 16'o105002;
                9'o516: d_out_h <= 16'o152702;
                9'o520: d_out_h <= 16'o000015;
                9'o522: d_out_h <= 16'o000770;
                9'o524: d_out_h <= 16'o105737;
                9'o526: d_out_h <= 16'o177560;
                9'o530: d_out_h <= 16'o100375;
                9'o532: d_out_h <= 16'o105002;
                9'o534: d_out_h <= 16'o153702;
                9'o536: d_out_h <= 16'o177562;
                9'o540: d_out_h <= 16'o105737;
                9'o542: d_out_h <= 16'o177564;
                9'o544: d_out_h <= 16'o100375;
                9'o546: d_out_h <= 16'o110237;
                9'o550: d_out_h <= 16'o177566;
                9'o552: d_out_h <= 16'o142702;
                9'o554: d_out_h <= 16'o100200;
                9'o556: d_out_h <= 16'o022323;
                9'o560: d_out_h <= 16'o000163;
                9'o562: d_out_h <= 16'o177776;
                9'o564: d_out_h <= 16'o012705;
                9'o566: d_out_h <= 16'o165006;
                9'o570: d_out_h <= 16'o012702;
                9'o572: d_out_h <= 16'o000500;
                9'o574: d_out_h <= 16'o011503;
                9'o576: d_out_h <= 16'o005012;
                9'o600: d_out_h <= 16'o112512;
                9'o602: d_out_h <= 16'o005202;
                9'o604: d_out_h <= 16'o112512;
                9'o606: d_out_h <= 16'o005302;
                9'o610: d_out_h <= 16'o023512;
                9'o612: d_out_h <= 16'o001015;
                9'o614: d_out_h <= 16'o005202;
                9'o616: d_out_h <= 16'o143522;
                9'o620: d_out_h <= 16'o024542;
                9'o622: d_out_h <= 16'o143522;
                9'o624: d_out_h <= 16'o001010;
                9'o626: d_out_h <= 16'o010502;
                9'o630: d_out_h <= 16'o016505;
                9'o632: d_out_h <= 16'o177772;
                9'o634: d_out_h <= 16'o110532;
                9'o636: d_out_h <= 16'o150572;
                9'o640: d_out_h <= 16'o000000;
                9'o642: d_out_h <= 16'o020352;
                9'o644: d_out_h <= 16'o001407;
                9'o646: d_out_h <= 16'o000000;
                9'o650: d_out_h <= 16'o005723;
                9'o652: d_out_h <= 16'o001011;
                9'o654: d_out_h <= 16'o021605;
                9'o656: d_out_h <= 16'o001007;
                9'o660: d_out_h <= 16'o000203;
                9'o662: d_out_h <= 16'o000000;
                9'o664: d_out_h <= 16'o011206;
                9'o666: d_out_h <= 16'o012702;
                9'o670: d_out_h <= 16'o165650;
                9'o672: d_out_h <= 16'o005726;
                9'o674: d_out_h <= 16'o004312;
                9'o676: d_out_h <= 16'o000000;
                9'o700: d_out_h <= 16'o004362;
                9'o702: d_out_h <= 16'o000004;
                9'o704: d_out_h <= 16'o012705;
                9'o706: d_out_h <= 16'o160000;
                9'o710: d_out_h <= 16'o005037;
                9'o712: d_out_h <= 16'o000006;
                9'o714: d_out_h <= 16'o012737;
                9'o716: d_out_h <= 16'o165722;
                9'o720: d_out_h <= 16'o000004;
                9'o722: d_out_h <= 16'o012706;
                9'o724: d_out_h <= 16'o000502;
                9'o726: d_out_h <= 16'o005745;
                9'o730: d_out_h <= 16'o005003;
                9'o732: d_out_h <= 16'o010313;
                9'o734: d_out_h <= 16'o005723;
                9'o736: d_out_h <= 16'o020305;
                9'o740: d_out_h <= 16'o101774;
                9'o742: d_out_h <= 16'o005003;
                9'o744: d_out_h <= 16'o005413;
                9'o746: d_out_h <= 16'o060313;
                9'o750: d_out_h <= 16'o005723;
                9'o752: d_out_h <= 16'o001004;
                9'o754: d_out_h <= 16'o020305;
                9'o756: d_out_h <= 16'o101772;
                9'o760: d_out_h <= 16'o000164;
                9'o762: d_out_h <= 16'o000002;
                9'o764: d_out_h <= 16'o014304;
                9'o766: d_out_h <= 16'o010300;
                9'o770: d_out_h <= 16'o005006;
                9'o772: d_out_h <= 16'o000000;
                9'o774: d_out_h <= 16'o040460;
                9'o776: d_out_h <= 16'o123162;
                default: d_out_h <= 0;
            endcase
            ssyn_out_h <= 1;
        end
    end
endmodule
