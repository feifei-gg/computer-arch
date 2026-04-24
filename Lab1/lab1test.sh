#!/bin/bash

drivetests="../base/drivetest.pl"

echo
echo "Lab 1 Testing Starting"
echo

# Get absolute path to regDeps.so
tool="$(realpath ./regDeps.so)"

tbegin=$(date +%s)

# Invoke test script and forward stdout + stderr
"$drivetests" "$tool" 2>&1

ttotal=$(( $(date +%s) - tbegin ))

echo
echo
echo "Lab 1 Testing Complete in $ttotal seconds"
echo
