#!/bin/bash

drivetests="../base/drivetest.pl"
tool="$(realpath ./caches.so)"

echo "=================================================="
echo "Starting PARSEC Cache Interrogation (Q2 Analysis)"
echo "Focus: Working Set, Capacity vs Associativity Area Trade-off"
echo "=================================================="

run_experiment() {
    local r=$1
    local b=$2
    local a=$3
    local exp_name="Rows_2^${r}_Block_2^${b}_Assoc_${a}"

    echo ""
    echo ">>> Running Config: -r $r -b $b -a $a <<<"

    "$drivetests" "$tool -r $r -b $b -a $a" 2>&1
    sleep 2
}

tbegin=$(date +%s)

# ---------------------------------------------------------
# EXPERIMENT A: Finding the Working Set (Varying Capacity)
# Fixed: Block=64B (-b 6), Assoc=1 (-a 1)
# Range: r=6 (4KB) to r=16 (4MB) in steps of 2
# ---------------------------------------------------------
echo -e "\n\n=== EXPERIMENT A: WORKING SET ANALYSIS ==="
for r in 6 8 10 12 14 16; do
    run_experiment $r 6 1
done

# ---------------------------------------------------------
# EXPERIMENT B: Area Trade-off (Capacity vs Associativity)
# Assuming a fixed chip area, higher associativity means 
# we can fit fewer total rows (less overall capacity).
# Config 1: Large Capacity, Direct Mapped (256KB, 1-Way)
# Config 2: Medium Capacity, 2-Way Assoc (128KB, 2-Way)
# Config 3: Small Capacity, 4-Way Assoc (64KB, 4-Way)
# ---------------------------------------------------------
echo -e "\n\n=== EXPERIMENT B: ARCHITECTURAL TRADE-OFF (Fixed Area Proxy) ==="
run_experiment 12 6 1
run_experiment 11 6 2
run_experiment 10 6 4

ttotal=$(( $(date +%s) - tbegin ))

echo ""
echo "=================================================="
echo "Q2 Experiments Complete in $ttotal seconds!"
echo "=================================================="