#! /bin/bash

THREADS=8
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Small simulation
$DIR/binaries/cholesky -p$THREADS < $DIR/inputs/cholesky/tk14.0

# Large simulation
#$DIR/binaries/cholesky -p$THREADS < $DIR/inputs/cholesky/tk29.0

# Complete dataset 
#$DIR/binaries/cholesky -p$THREADS < $DIR/inputs/cholesky/tk29.0
