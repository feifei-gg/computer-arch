/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#ifndef TOOL_COMMON_H
#define TOOL_COMMON_H

#include "pin.H"

#if defined(TARGET_LINUX)
#include <sys/syscall.h>
#include <unistd.h>

/*!
 * Read OS release number into the specified buffer.
 * Return True if the release number was written to specified buffer, false in all other cases.
 */
static inline bool UtilsGetKernelRelease(CHAR* buf, USIZE bufsize)
{
    struct utsname uname;
    // t_syscall_arg unameArgs[] = {(t_syscall_arg)&uname};
    // if (0 != syscall(SYS_native_syscall_unsafe, __NR_uname, 0, 1, unameArgs))
    // {
    //     return false;
    // }

    auto ret = OS_Syscall_unsafe(__NR_uname, &uname);
    if (IS_PINOS_SYSCALL_ERROR(ret))
    {
        return false;
    }

    if (bufsize < (strlen(uname.release) + 1))
    {
        return false;
    }

    strcpy(buf, uname.release);

    return true;
}

#endif // if defined(TARGET_LINUX)

#endif // TOOL_COMMON_H
