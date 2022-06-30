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
#include "libc/calls/internal.h"
#include "libc/calls/struct/timeval.h"
#include "libc/dce.h"
#include "libc/intrin/kprintf.h"
#include "libc/nt/version.h"
#include "libc/runtime/runtime.h"
#include "libc/sock/sock.h"
#include "libc/sysv/consts/af.h"
#include "libc/sysv/consts/so.h"
#include "libc/sysv/consts/sock.h"
#include "libc/sysv/consts/sol.h"
#include "libc/testlib/testlib.h"
#include "libc/time/time.h"

__attribute__((__constructor__)) static void init(void) {
  pledge("stdio rpath cpath proc unix", 0);
  errno = 0;
}

char testlib_enable_tmp_setup_teardown;

void DatagramServer(void) {
  char buf[256] = {0};
  uint32_t len = sizeof(struct sockaddr_un);
  struct sockaddr_un addr = {AF_UNIX, "foo.sock"};
  alarm(3);
  ASSERT_SYS(0, 3, socket(AF_UNIX, SOCK_DGRAM, 0));
  ASSERT_SYS(0, 0, bind(3, (void *)&addr, len));
  bzero(&addr, sizeof(addr));
  ASSERT_SYS(0, 0, getsockname(3, (void *)&addr, &len));
  ASSERT_EQ(11, len);
  ASSERT_STREQ("foo.sock", addr.sun_path);
  ASSERT_SYS(0, 5, read(3, buf, 256));
  EXPECT_STREQ("hello", buf);
  ASSERT_SYS(0, 0, close(3));
}

TEST(unix, datagram) {
  if (IsWindows()) return;  // no unix datagram on windows :'(
  int ws;
  uint32_t len = sizeof(struct sockaddr_un);
  struct sockaddr_un addr = {AF_UNIX, "foo.sock"};
  if (!fork()) {
    DatagramServer();
    _Exit(0);
  }
  alarm(3);
  while (!fileexists(addr.sun_path)) usleep(10000);
  ASSERT_SYS(0, 3, socket(AF_UNIX, SOCK_DGRAM, 0));
  ASSERT_SYS(0, 5, sendto(3, "hello", 5, 0, (void *)&addr, len));
  ASSERT_SYS(0, 0, close(3));
  ASSERT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(0, WEXITSTATUS(ws));
  alarm(0);
}

void StreamServer(void) {
  char buf[256] = {0};
  uint32_t len = sizeof(struct sockaddr_un);
  struct sockaddr_un addr = {AF_UNIX, "foo.sock"};
  alarm(3);
  ASSERT_SYS(0, 3, socket(AF_UNIX, SOCK_STREAM, 0));
  ASSERT_SYS(0, 0, bind(3, (void *)&addr, len));
  bzero(&addr, sizeof(addr));
  ASSERT_SYS(0, 0, getsockname(3, (void *)&addr, &len));
  ASSERT_EQ(2 + 8 + 1, len);
  ASSERT_EQ(AF_UNIX, addr.sun_family);
  ASSERT_STREQ("foo.sock", addr.sun_path);
  ASSERT_SYS(0, 0, listen(3, 10));
  bzero(&addr, sizeof(addr));
  len = sizeof(addr);
  ASSERT_SYS(0, 4, accept(3, &addr, &len));
  ASSERT_EQ(AF_UNIX, addr.sun_family);
  EXPECT_STREQ("", addr.sun_path);
  ASSERT_SYS(0, 5, read(4, buf, 256));
  EXPECT_STREQ("hello", buf);
  ASSERT_SYS(0, 0, close(3));
}

TEST(unix, stream) {
  if (IsWindows() && !IsAtLeastWindows10()) return;
  int ws;
  uint32_t len = sizeof(struct sockaddr_un);
  struct sockaddr_un addr = {AF_UNIX, "foo.sock"};
  // TODO(jart): move this line down when kFdProcess is gone
  ASSERT_SYS(0, 3, socket(AF_UNIX, SOCK_STREAM, 0));
  if (!fork()) {
    close(3);
    StreamServer();
    _Exit(0);
  }
  alarm(3);
  while (!fileexists(addr.sun_path)) usleep(10000);
  ASSERT_SYS(0, 0, connect(3, (void *)&addr, len));
  ASSERT_SYS(0, 5, write(3, "hello", 5));
  ASSERT_SYS(0, 0, close(3));
  ASSERT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(0, WEXITSTATUS(ws));
  alarm(0);
}
