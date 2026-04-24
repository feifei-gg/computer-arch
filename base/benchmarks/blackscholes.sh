#! /bin/bash

THREADS=8
echo " running blackscholes" 

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Small simulation
$DIR/binaries/blackscholes $THREADS $DIR/inputs/blackscholes/in_4K.txt $DIR/outputs/blackscholes_simsmall.txt

# Large simulation
#$DIR/binaries/blackscholes $THREADS $DIR/inputs/blackscholes/in_64K.txt $DIR/outputs/blackscholes_simlarge.txt

# Complete dataset 
#$DIR/binaries/blackscholes $THREADS $DIR/inputs/blackscholes/in_10M.txt $DIR/outputs/blackscholes_native.txt
