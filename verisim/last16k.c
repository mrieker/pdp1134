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

// Pass all stdin to stdout
// Pass last 16K chars from stdin to stderr

//  cc -O2 -o last16k last16k.c
//  time ./test | ./last16k 2> x.y | grep '000000 :'

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static char buff[16384];

int main ()
{
    int insert, rc, rc2, remove, wrapped;

    insert  = 0;
    wrapped = 0;
    while ((rc = read (0, buff + insert, sizeof buff - insert)) > 0) {
        for (remove = 0; remove < rc; remove += rc2) {
            rc2 = write (1, buff + insert + remove, rc - remove);
            if (rc2 <= 0) abort ();
        }
        insert += rc;
        if (insert > sizeof buff) abort ();
        if (insert == sizeof buff) {
            insert  = 0;
            wrapped = 1;
        }
    }

    if (wrapped) {
        for (remove = insert; remove < sizeof buff; remove += rc2) {
            rc2 = write (2, buff + remove, sizeof buff - remove);
            if (rc2 <= 0) abort ();
        }
    }

    for (remove = 0; remove < insert; remove += rc2) {
        rc2 = write (2, buff + remove, insert - remove);
        if (rc2 <= 0) abort ();
    }

    return 0;
}
