/*
 * Copyright (C) 2024-2024 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <cstring>
#include <cstdio>

int main(int argc, char** argv)
{
    size_t len = 0;
    if (argv[1])
    {
        len = strlen(argv[1]);
    }
    printf("len = %ld\n", len);
    return 0;
}
