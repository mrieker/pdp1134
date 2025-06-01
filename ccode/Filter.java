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

// Filter out redundant pairs of lines from stdin to stdout
//   A
//   B
//   C
//   D
//   C
//   D
//   C
//   D
//   C
//   D
//   C
//   D
//   E
//   F
// filters to:
//   A
//   B
//   C
//   D
//     ....
//   C
//   D
//   E
//   F

import java.io.BufferedReader;
import java.io.InputStreamReader;

public class Filter {
    public static void main (String[] args)
            throws Exception
    {
        String[] lines = new String[6];

        BufferedReader br = new BufferedReader (new InputStreamReader (System.in));
        for (int i = 0; i < 6; i ++) {
            lines[i] = br.readLine ();
        }

        System.out.println (lines[0]);
        System.out.println (lines[1]);

        while (true) {

            // invariant:
            //   lines 0,1 : printed
            //     2,3,4,5 : unknown

            boolean dotdotdot = false;
            while (lines[0].equals (lines[2]) && lines[1].equals (lines[3]) && lines[2].equals (lines[4]) && lines[3].equals (lines[5])) {
                if (! dotdotdot) {
                    System.out.println ("   ....");
                    dotdotdot = true;
                }
                lines[2] = lines[4];
                lines[3] = lines[5];
                lines[4] = br.readLine ();
                lines[5] = br.readLine ();
            }

            // invariant:
            //   lines 0,1 : printed
            //     2,3,4,5 : something doesn't match up
            if (lines[2] == null) break;
            System.out.println (lines[2]);
            lines[0] = lines[1];
            lines[1] = lines[2];
            lines[2] = lines[3];
            lines[3] = lines[4];
            lines[4] = lines[5];
            lines[5] = br.readLine ();
        }
    }
}
