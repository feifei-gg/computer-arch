#! /bin/bash

THREADS=8
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Small simulation
$DIR/binaries/bodytrack $DIR/inputs/bodytrack/simsmall/sequenceB_1/ 4 1 1000 5 0 $THREADS  

# Large simulation
#$DIR/binaries/bodytrack $DIR/inputs/bodytrack/simlarge/sequenceB_4/ 4 4 4000 5 0 $THREADS  

# Full simulation
#$DIR/binaries/bodytrack $DIR/inputs/bodytrack/native/sequenceB_261/ 4 261 4000 5 0 $THREADS
