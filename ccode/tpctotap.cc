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

//  Convert a TPC-format tape file to a TAP-format file

//    ./tpctotap < somefile.tpc > somefile.tap

//  TPC format files have:
//    <16-bit-length-A> <data-A> <16-bit-length-B> <data-B> ...

//  TAP format files have:
//    <32-bit-length-A> <data-A> <32-bit-length-A> <32-bit-length-B> <data-B> <32-bit-length-B> ...

//  data in both formats is padded to even length
//  tape mark in TPC is represented by a length of 0
//  tape mark in TAP is represented by a single length of 0

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main ()
{
    uint8_t data[65536];
    uint16_t i, inlen;
    uint32_t inlen2, outlen;

    while (fread (&inlen, sizeof inlen, 1, stdin) > 0) {
        outlen = inlen;
        fprintf (stderr, "len=%u", outlen);
        if (outlen != 0) {
            inlen2 = (inlen + 1) & -2;
            if (fread (data, inlen2, 1, stdin) <= 0) {
                fprintf (stderr, "eof reading block\n");
                break;
            }
            fwrite (&outlen, sizeof outlen, 1, stdout);
            fwrite (data, inlen2, 1, stdout);
        }
        fwrite (&outlen, sizeof outlen, 1, stdout);
        if (outlen == 80) {
            fputc (' ', stderr);
            for (i = 0; i < outlen; i ++) {
                if ((data[i] < ' ') || (data[i] > 126)) {
                    fputc ('.', stderr);
                } else {
                    fputc (data[i], stderr);
                }
            }
        }
        fprintf (stderr, "\n");
    }
    fflush (stdout);
    return 0;
}
