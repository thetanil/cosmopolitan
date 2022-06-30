/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/calls.h"
#include "libc/calls/strace.internal.h"
#include "libc/calls/struct/seccomp.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/describeflags.internal.h"
#include "libc/sysv/consts/pr.h"
#include "libc/sysv/errfuns.h"

/**
 * Tunes Linux security policy.
 *
 * This system call was first introduced in Linux 3.17. We polyfill
 * automatically features like SECCOMP_SET_MODE_STRICT, for kernels
 * dating back to 2.6.23, whenever possible.
 *
 * @raise ENOSYS on non-Linux.
 */
privileged int seccomp(unsigned operation, unsigned flags, void *args) {
  int rc;
  if (IsLinux()) {
    asm volatile("syscall"
                 : "=a"(rc)
                 : "0"(317), "D"(operation), "S"(flags), "d"(args)
                 : "rcx", "r11", "memory");
    if (rc == -ENOSYS) {
      if (operation == SECCOMP_SET_MODE_STRICT) {
        asm volatile("syscall"
                     : "=a"(rc)
                     : "0"(157), "D"(PR_SET_SECCOMP), "S"(SECCOMP_MODE_STRICT)
                     : "rcx", "r11", "memory");
      } else if (operation == SECCOMP_SET_MODE_FILTER && !flags) {
        asm volatile("syscall"
                     : "=a"(rc)
                     : "0"(157), "D"(PR_SET_SECCOMP), "S"(SECCOMP_MODE_FILTER),
                       "d"(args)
                     : "rcx", "r11", "memory");
      }
    }
    if (rc > -4096u) {
      errno = -rc;
      rc = -1;
    }
  } else {
    rc = enosys();
  }
  STRACE("seccomp(%s, %#x, %p) → %d% m", DescribeSeccompOperation(operation),
         flags, args, rc);
  return rc;
}
