#!/usr/bin/env bash

for i in `seq 0 1 ${1}`;
do
    port_number=$((9000+${i}))
    ./cct_proxy -i ${i} -c ${2} -f parties.conf -l 700 -p ${port_number} &
done
