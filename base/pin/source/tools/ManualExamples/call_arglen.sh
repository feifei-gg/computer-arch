#!/bin/bash

#
# Copyright (C) 2024-2024 Intel Corporation.
# SPDX-License-Identifier: MIT
#

# This script generates a long string using a number of loops
# then it invokes the test executable with this string as first parameter
# in order to have pin instrument the shell script and check
# that pin knows how to handle long parameters or alternatively to exit
# with the right error message
#
# longarg.sh <exe_name> <loop_num>
#

a="0123456789"
i=1
while true; do
    i=$(($i + 1));
    a="0123456789$a"

    if [ "$i" == "$2" ]; then
        break
    fi
done

$1 $a
