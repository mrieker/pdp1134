
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char **argv)
{
    int dividendneg, divisorneg, quotientneg, remainderneg;
    uint16_t counter, dividend, divisor, quotient, remainder, srcval;
    uint32_t product;

    if (argc == 3) {
        dividend = atoi (argv[1]);
        divisor  = atoi (argv[2]);

        product  = dividend;
        for (counter = 0; counter <= 15; counter ++) {
            product += product;
            if (product >= divisor << 16) {
                product -= divisor << 16;
                product ++;
            }
        }
        quotient = product & 0xFFFF;
        remainder = product >> 16;

        for (dividendneg = 0; dividendneg <= 1; dividendneg ++) {
            for (divisorneg = 0; divisorneg <= 1; divisorneg ++) {
                quotientneg  = dividendneg ^ divisorneg;
                remainderneg = dividendneg;

                int16_t sdividend  = dividendneg  ? - dividend  : dividend;
                int16_t sdivisor   = divisorneg   ? - divisor   : divisor;
                int16_t squotient  = quotientneg  ? - quotient  : quotient;
                int16_t sremainder = remainderneg ? - remainder : remainder;

                int16_t reconstr   = squotient * sdivisor + sremainder;

                printf ("  %4d  /  %4d  =  %4d  r  %4d  =>  %4d\n",
                    sdividend,
                    sdivisor,
                    squotient,
                    sremainder,
                    reconstr);
            }
        }
    }
    return 0;
}
