/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
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
#include "libc/errno.h"
#include "libc/mem/alloca.h"
#include "libc/mem/mem.h"
#include "libc/runtime/gc.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/sysv/consts/auxv.h"
#include "libc/sysv/consts/ok.h"
#include "libc/testlib/testlib.h"
#include "libc/x/x.h"

char testlib_enable_tmp_setup_teardown;

__attribute__((__constructor__)) static void init(void) {
  pledge("stdio rpath wpath cpath fattr", 0);
  errno = 0;
}

TEST(access, efault) {
  ASSERT_SYS(EFAULT, -1, access(0, F_OK));
  if (IsWindows() && !IsAsan()) return;  // not possible
  ASSERT_SYS(EFAULT, -1, access((void *)77, F_OK));
}

TEST(access, enoent) {
  ASSERT_SYS(ENOENT, -1, access("", F_OK));
  ASSERT_SYS(ENOENT, -1, access("doesnotexist", F_OK));
  ASSERT_SYS(ENOENT, -1, access("o/doesnotexist", F_OK));
}

TEST(access, enotdir) {
  ASSERT_SYS(0, 0, touch("o", 0644));
  ASSERT_SYS(ENOTDIR, -1, access("o/doesnotexist", F_OK));
}

TEST(access, test) {
  ASSERT_SYS(0, 0, close(creat("file", 0644)));
  ASSERT_SYS(0, 0, access("file", F_OK));
  ASSERT_SYS(ENOENT, -1, access("dir", F_OK));
  ASSERT_SYS(ENOTDIR, -1, access("file/dir", F_OK));
  ASSERT_SYS(0, 0, mkdir("dir", 0755));
  ASSERT_SYS(0, 0, access("dir", F_OK));
  ASSERT_SYS(0, 0, access("dir", W_OK));
  ASSERT_SYS(0, 0, access("file", W_OK));
}

TEST(access, testRequestWriteOnReadOnly_returnsEaccess) {
  return; /* TODO(jart): maybe we need root to help? */
  int fd;
  ASSERT_SYS(ENOENT, -1, access("file", F_OK));
  ASSERT_SYS(0, 0, close(creat("file", 0444)));
  ASSERT_SYS(0, 0, access("file", F_OK));
  ASSERT_SYS(0, 0, access("file", F_OK));
  ASSERT_SYS(EACCES, -1, access("file", W_OK));
  ASSERT_SYS(EACCES, -1, access("file", W_OK | R_OK));
  ASSERT_SYS(0, 0, mkdir("dir", 0555));
  ASSERT_SYS(0, 0, access("dir", F_OK));
  ASSERT_SYS(EACCES, -1, access("dir", W_OK));
}

TEST(access, runThisExecutable) {
  ASSERT_SYS(0, 0, access(GetProgramExecutableName(), R_OK | X_OK));
}
