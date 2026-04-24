#!/bin/bash

drivetests="../base/drivetest.pl"

echo
echo "Lab 2 Testing Starting"
echo

# Get absolute path to bpredictor.so
tool="$(realpath ./bpredictor.so)"

tbegin=$(date +%s)

# Invoke test script and forward stdout + stderr
"$drivetests" "$tool" 2>&1

ttotal=$(( $(date +%s) - tbegin ))

echo
echo
echo "Lab 2 Testing Complete in $ttotal seconds"
echo
