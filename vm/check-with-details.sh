#!/bin/bash

filename="vm-output-$(date '+%Y-%m-%d-%H-%M-%S').txt"

make clean
make check | tee "$filename"

echo "--------------- COUNT DETAILS ----------------"

tac "$filename" | awk 'flag{print} /\(Process exit codes are excluded for matching purposes\.)/ {flag=1; print}' | tac \
  | grep '^FAIL ' \
  | awk -F'/' '{ print $NF }' \
  | awk -F'-' '{ print $1 }' \
  | sort \
  | uniq -c \
  | while read cnt cat;do
      echo "${cat}: ${cnt}"
    done \
  | awk -F': ' '{sum+=$2} {print} END{print "total failed: " sum}'
