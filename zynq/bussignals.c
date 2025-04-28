
// create signals.xdc file from bus signals spreadsheet file
// also reads zturn pin assignments file ../zturnpins.csv

// cc -g -o bussignals bussignals.c
// ./bussignals < ../bussigs.csv > signals.xdc

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ZTurnPin {
    char con[8];
    char zynq[4];
} ZTurnPin;

int main ()
{
    char *cols[16], line[256], *p, *s;
    int n, q;
    int nzturnpins;
    ZTurnPin zturnpins[100];

    // J11-42,B34-LN07,Y17,LED-BLU
    FILE *ztpfile = fopen ("../zturnpins.csv", "r");
    if (ztpfile == NULL) abort ();
    nzturnpins = 0;
    while (fgets (line, sizeof line, ztpfile) != NULL) {
        if (nzturnpins >= 100) abort ();
        if (line[6] != ',') abort ();
        memcpy (zturnpins[nzturnpins].con, line, 6);
        zturnpins[nzturnpins].con[6] = 0;
        p = strchr (line + 7, ',');
        if (p == NULL) abort ();
        s = strchr (++ p, ',');
        if (s == NULL) abort ();
        *s = 0;
        if (p - s > 3) abort ();
        strcpy (zturnpins[nzturnpins].zynq, p);
        nzturnpins ++;
    }
    fclose (ztpfile);

    // BUS_A00_L,CH2,"Q57A,B",3K,J11-17,a_out_h[00],address from zynq out to unibus

    // set_property PACKAGE_PIN R17 [get_ports bDMABUSB_0]
    // set_property IOSTANDARD LVCMOS33 [get_ports bDMABUSB_0]
    fgets (line, sizeof line, stdin);
    while (fgets (line, sizeof line, stdin) != NULL) {
        cols[0] = line;
        n = 0;
        q = 0;
        for (p = line; *p != 0; p ++) {
            if (! q && (*p == ',')) {
                *p = 0;
                cols[++n] = p + 1;
            }
            if (*p == '"') q = 1 - q;
        }
        if ((n > 5) && (cols[4][0] != 0) && (cols[5][0] != 0)) {
            for (q = nzturnpins; -- q >= 0;) {
                if (strcmp (cols[4], zturnpins[q].con) == 0) break;
            }
            if (q < 0) abort ();
            p = strchr (cols[5], '[');
            if (p == NULL) {
                printf ("set_property PACKAGE_PIN %s [get_ports %s_0]\n", zturnpins[q].zynq, cols[5]);
                printf ("set_property IOSTANDARD LVCMOS33 [get_ports %s_0]\n", cols[5]);
            } else {
                *(p ++) = 0;
                if ((p[0] == '0') && (p[1] != ']')) p ++;
                printf ("set_property PACKAGE_PIN %s [get_ports {%s_0[%s}]\n", zturnpins[q].zynq, cols[5], p);
                printf ("set_property IOSTANDARD LVCMOS33 [get_ports {%s_0[%s}]\n", cols[5], p);
            }
        }
    }
    return 0;
}
