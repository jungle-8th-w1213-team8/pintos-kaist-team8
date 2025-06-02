#!/bin/bash

filename="output-$(date '+%Y-%m-%d-%H-%M-%S').txt"

make clean
make check | tee "$filename"
./parse-and-print-details.sh "$filename"
#rm tmp_output.txt

