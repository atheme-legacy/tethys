#!/bin/sh

function run_test {
  echo "run $1"
  ./map < $1 2>/dev/null | diff -rupN - $1.out
}

for i in test*.txt; do
  run_test $i; done
