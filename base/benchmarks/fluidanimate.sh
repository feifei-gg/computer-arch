#! /bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Small simulation
$DIR/binaries/fluidanimate 1 5 $DIR/inputs/fluidanimate/in_35K.fluid

# Large simulation
#$DIR/binaries/fluidanimate 1 5 $DIR/inputs/fluidanimate/in_300K.fluid

# Complete dataset 
#$DIR/binaries/fluidanimate 1 5 $DIR/inputs/fluidanimate/in_500K.fluid

