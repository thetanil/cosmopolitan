/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2021 Justine Alexandra Roberts Tunney                              │
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
#include "libc/calls/struct/iovec.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/macros.internal.h"
#include "libc/mem/mem.h"
#include "libc/runtime/gc.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/sock/sock.h"
#include "libc/sysv/consts/auxv.h"
#include "libc/sysv/consts/o.h"
#include "libc/testlib/testlib.h"

char testlib_enable_tmp_setup_teardown;

__attribute__((__constructor__)) static void init(void) {
  pledge("stdio rpath wpath cpath fattr", 0);
  errno = 0;
}

TEST(writev, test) {
  int fd;
  char ba[1] = "a";
  char bb[1] = "b";
  char bc[2] = "cd";
  struct iovec iov[] = {{"", 0}, {ba, 1}, {NULL, 0}, {bb, 1}, {bc, 2}};
  ASSERT_NE(-1, (fd = open("file", O_RDWR | O_CREAT | O_TRUNC, 0644)));
  EXPECT_EQ(4, writev(fd, iov, ARRAYLEN(iov)));
  EXPECT_EQ(1, lseek(fd, 1, SEEK_SET));
  EXPECT_EQ(3, readv(fd, iov, ARRAYLEN(iov)));
  EXPECT_EQ('b', ba[0]);
  EXPECT_EQ('c', bb[0]);
  EXPECT_EQ('d', bc[0]);
  EXPECT_NE(-1, close(fd));
}

TEST(writev, big_fullCompletion) {
  int fd;
  char *ba = gc(malloc(2 * 1024 * 1024));
  char *bb = gc(malloc(2 * 1024 * 1024));
  char *bc = gc(malloc(2 * 1024 * 1024));
  struct iovec iov[] = {
      {"", 0},                //
      {ba, 2 * 1024 * 1024},  //
      {NULL, 0},              //
      {bb, 2 * 1024 * 1024},  //
      {bc, 2 * 1024 * 1024},  //
  };
  ASSERT_NE(-1, (fd = open("file", O_RDWR | O_CREAT | O_TRUNC, 0644)));
  EXPECT_EQ(6 * 1024 * 1024, writev(fd, iov, ARRAYLEN(iov)));
  EXPECT_NE(-1, close(fd));
}

TEST(writev, asanError_efaults) {
  if (!IsAsan()) return;
  void *malloc_(size_t) asm("malloc");
  void free_(void *) asm("free");
  void *p;
  int fd;
  p = malloc_(32);
  EXPECT_NE(-1, (fd = open("asan", O_RDWR | O_CREAT | O_TRUNC, 0644)));
  EXPECT_EQ(32, write(fd, p, 32));
  EXPECT_NE(-1, lseek(fd, 0, SEEK_SET));
  EXPECT_EQ(32, read(fd, p, 32));
  EXPECT_EQ(-1, write(fd, p, 33));
  EXPECT_EQ(EFAULT, errno);
  EXPECT_EQ(-1, write(fd, p, -1));
  EXPECT_EQ(EFAULT, errno);
  free_(p);
  EXPECT_EQ(-1, write(fd, p, 32));
  EXPECT_EQ(EFAULT, errno);
  EXPECT_EQ(-1, read(fd, p, 32));
  EXPECT_EQ(EFAULT, errno);
  close(fd);
}

TEST(writev, empty_stillPerformsIoOperation) {
  int fd;
  struct iovec iov[] = {{"", 0}, {NULL, 0}};
  ASSERT_NE(-1, touch("file", 0644));
  ASSERT_NE(-1, (fd = open("file", O_RDONLY)));
  EXPECT_EQ(-1, writev(fd, iov, ARRAYLEN(iov)));
  EXPECT_EQ(-1, writev(fd, NULL, 0));
  EXPECT_NE(-1, close(fd));
}
