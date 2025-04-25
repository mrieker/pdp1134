
// cc -o bussignals bussignals.c
// ./bussignals < ../bussigs.csv > signals.xdc

#include <stdio.h>
#include <string.h>

// BUS_A00_L,CH2,"Q57A,B",3K,J11-17,B13-LP13,Y07,a_out_h[00],address from zynq out to unibus

// set_property PACKAGE_PIN R17 [get_ports bDMABUSB_0]
// set_property IOSTANDARD LVCMOS33 [get_ports bDMABUSB_0]

int main ()
{
    char *cols[16], line[256], *p;
    int n, q;

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
        if ((n > 7) && (cols[6][0] != 0) && (cols[7][0] != 0)) {
            p = strchr (cols[7], '[');
            if (p == NULL) {
                printf ("set_property PACKAGE_PIN %s [get_ports %s_0]\n", cols[6], cols[7]);
                printf ("set_property IOSTANDARD LVCMOS33 [get_ports %s_0]\n", cols[7]);
            } else {
                *(p ++) = 0;
                if ((p[0] == '0') && (p[1] != ']')) p ++;
                printf ("set_property PACKAGE_PIN %s [get_ports {%s_0[%s}]\n", cols[6], cols[7], p);
                printf ("set_property IOSTANDARD LVCMOS33 [get_ports {%s_0[%s}]\n", cols[7], p);
            }
        }
    }
    return 0;
}
