#! /bin/bash

THREADS=8

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Small simulation
$DIR/binaries/fft -m16 -p$THREADS 

# Large simulation
#$DIR/binaries/fft -m16 -p$THREADS 

# Complete dataset 
#$DIR/binaries/fft -m16 -p$THREADS 
