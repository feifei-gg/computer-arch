#!/bin/bash

drivetests="../base/drivetest.pl"
tool="$(realpath ./caches.so)"

echo "=================================================="
echo "Starting Cache Design Space Exploration"
echo "Base Config: Rows=512 (-r 9), Block=4B (-b 2), Assoc=1 (-a 1)"
echo "=================================================="

run_experiment() {
    local r=$1
    local b=$2
    local a=$3
    local exp_name="Rows_2^${r}_Block_2^${b}_Assoc_${a}"

    echo ""
    echo ">>> Starting: $exp_name <<<"
    echo ">>> Parameters: -r $r -b $b -a $a <<<"

    # The perl script will execute: pin -t /path/caches.so -r X -b Y -a Z -o ...
    "$drivetests" "$tool -r $r -b $b -a $a" 2>&1

    sleep 2
}

tbegin=$(date +%s)

# ---------------------------------------------------------
# EXPERIMENT 1: Vary Associativity (Capacity and Block fixed)
# Base: r=9, b=2. Vary: a = 1, 2, 4, 8
# ---------------------------------------------------------
echo -e "\n\n=== EXPERIMENT 1: VARYING ASSOCIATIVITY ==="
for a in 1 2 4 8; do
    run_experiment 9 2 $a
done

# ---------------------------------------------------------
# EXPERIMENT 2: Vary Block Size (Capacity and Assoc fixed)
# Base: r=9, a=1. Vary: b = 2(4B), 3(8B), 4(16B), 5(32B), 6(64B)
# ---------------------------------------------------------
echo -e "\n\n=== EXPERIMENT 2: VARYING BLOCK SIZE ==="
for b in 2 3 4 5 6; do
    run_experiment 9 $b 1
done

# ---------------------------------------------------------
# EXPERIMENT 3: Vary Capacity / Rows (Block and Assoc fixed)
# Base: b=2, a=1. Vary: r = 7(128), 8(256), 9(512), 10(1024), 11(2048)
# ---------------------------------------------------------
echo -e "\n\n=== EXPERIMENT 3: VARYING ROWS (CAPACITY) ==="
for r in 7 8 9 10 11; do
    run_experiment $r 2 1
done

ttotal=$(( $(date +%s) - tbegin ))

echo ""
echo "=================================================="
echo "All Experiments Complete in $ttotal seconds!"
echo "Check the generated 'results_XXX' folders for your .out files."
echo "=================================================="
