#!/bin/bash
if [ sim1134.v -ot ../verisim/pdp1134.v ]
then
    sed 's/[$]dis/\/\/$dis/g' ../verisim/pdp1134.v | sed 's/pdp1134/sim1134/g' > sim1134.v
fi
