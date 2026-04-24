#! /bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Small simulation
$DIR/binaries/ferret $DIR/inputs/ferret/simsmall/corel lsh $DIR/inputs/ferret/simsmall/queries 10 20 1 $DIR/output.txt

# Large simulation
#$DIR/binaries/ferret $DIR/inputs/ferret/simlarge/corel lsh $DIR/inputs/ferret/simlarge/queries 10 20 1 $DIR/output.txt

# Complete dataset 
#$DIR/binaries/ferret $DIR/inputs/ferret/native/corel lsh $DIR/inputs/ferret/native/queries 10 20 1 $DIR/output.txt
