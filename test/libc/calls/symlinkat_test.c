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
#include "libc/calls/struct/stat.h"
#include "libc/fmt/fmt.h"
#include "libc/fmt/itoa.h"
#include "libc/rand/rand.h"
#include "libc/runtime/runtime.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/consts/s.h"
#include "libc/testlib/testlib.h"

char testlib_enable_tmp_setup_teardown;
char p[2][PATH_MAX];
struct stat st;

void SetUpOnce(void) {
  pledge("stdio rpath wpath cpath fattr", 0);
  errno = 0;
}

TEST(symlink, enoent) {
  ASSERT_SYS(ENOENT, -1, symlink("o/foo", ""));
  ASSERT_SYS(ENOENT, -1, symlink("o/foo", "o/bar"));
}

TEST(symlinkat, enotdir) {
  ASSERT_SYS(0, 0, close(creat("yo", 0644)));
  ASSERT_SYS(ENOTDIR, -1, symlink("hrcue", "yo/there"));
}

TEST(symlinkat, test) {
  sprintf(p[0], "%s.%d", program_invocation_short_name, rand());
  sprintf(p[1], "%s.%d", program_invocation_short_name, rand());

  EXPECT_EQ(0, touch(p[0], 0644));
  EXPECT_EQ(0, symlink(p[0], p[1]));

  // check the normal file
  EXPECT_FALSE(issymlink(p[0]));
  EXPECT_EQ(0, lstat(p[0], &st));
  EXPECT_FALSE(S_ISLNK(st.st_mode));

  // check the symlink file
  EXPECT_TRUE(issymlink(p[1]));
  EXPECT_EQ(0, lstat(p[1], &st));
  EXPECT_TRUE(S_ISLNK(st.st_mode));

  // symlink isn't a symlink if we use it normally
  EXPECT_EQ(0, stat(p[1], &st));
  EXPECT_FALSE(S_ISLNK(st.st_mode));
}
