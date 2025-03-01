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
#include "libc/bits/atomic.h"
#include "libc/calls/calls.h"
#include "libc/calls/struct/sigaction.h"
#include "libc/calls/struct/siginfo.h"
#include "libc/calls/ucontext.h"
#include "libc/intrin/kprintf.h"
#include "libc/runtime/memtrack.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/consts/sa.h"
#include "libc/testlib/testlib.h"
#include "third_party/xed/x86.h"

volatile int gotsignal;
char testlib_enable_tmp_setup_teardown;

void ContinueOnError(int sig, siginfo_t *si, ucontext_t *ctx) {
  struct XedDecodedInst xedd;
  xed_decoded_inst_zero_set_mode(&xedd, XED_MACHINE_MODE_LONG_64);
  xed_instruction_length_decode(&xedd, (void *)ctx->uc_mcontext.rip, 15);
  ctx->uc_mcontext.rip += xedd.length;
  gotsignal = sig;
}

noasan bool MemoryExists(char *p) {
  volatile char c;
  struct sigaction old[2];
  struct sigaction sa = {
      .sa_sigaction = ContinueOnError,
      .sa_flags = SA_SIGINFO,
  };
  gotsignal = 0;
  sigaction(SIGSEGV, &sa, old + 0);
  sigaction(SIGBUS, &sa, old + 1);
  c = atomic_load(p);
  sigaction(SIGBUS, old + 1, 0);
  sigaction(SIGSEGV, old + 0, 0);
  return !gotsignal;
}

TEST(munmap, doesntExist_doesntCare) {
  EXPECT_SYS(0, 0, munmap(0, FRAMESIZE * 8));
  if (IsAsan()) {
    // make sure it didn't unmap the null pointer shadow memory
    EXPECT_TRUE(MemoryExists((char *)0x7fff8000));
  }
}

TEST(munmap, invalidParams) {
  EXPECT_SYS(EINVAL, -1, munmap(0, 0));
  EXPECT_SYS(EINVAL, -1, munmap((void *)0x100080000000, 0));
  EXPECT_SYS(EINVAL, -1, munmap((void *)0x100080000001, FRAMESIZE));
}

TEST(munmap, test) {
  char *p;
  ASSERT_NE(MAP_FAILED, (p = mmap(0, FRAMESIZE, PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
  EXPECT_TRUE(MemoryExists(p));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE));
  EXPECT_FALSE(MemoryExists(p));
}

TEST(munmap, punchHoleInMemory) {
  char *p;
  ASSERT_NE(MAP_FAILED, (p = mmap(0, FRAMESIZE * 3, PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 2));
  EXPECT_SYS(0, 0, munmap(p + FRAMESIZE, FRAMESIZE));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 2));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE));
  EXPECT_SYS(0, 0, munmap(p + FRAMESIZE * 2, FRAMESIZE));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 2));
}

TEST(munmap, memoryHasHole) {
  char *p;
  ASSERT_NE(MAP_FAILED, (p = mmap(0, FRAMESIZE * 3, PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
  EXPECT_SYS(0, 0, munmap(p + FRAMESIZE, FRAMESIZE));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 2));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE * 3));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 2));
}

TEST(munmap, blanketFree) {
  char *p;
  ASSERT_NE(MAP_FAILED, (p = mmap(0, FRAMESIZE * 3, PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 2));
  EXPECT_SYS(0, 0, munmap(p + FRAMESIZE * 0, FRAMESIZE));
  EXPECT_SYS(0, 0, munmap(p + FRAMESIZE * 2, FRAMESIZE));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 2));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE * 3));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 2));
}

TEST(munmap, trimLeft) {
  char *p;
  ASSERT_NE(MAP_FAILED, (p = mmap(0, FRAMESIZE * 2, PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE * 2));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 1));
}

TEST(munmap, trimRight) {
  char *p;
  ASSERT_NE(MAP_FAILED, (p = mmap(0, FRAMESIZE * 2, PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_SYS(0, 0, munmap(p + FRAMESIZE, FRAMESIZE));
  EXPECT_TRUE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 1));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE * 2));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 0));
  EXPECT_FALSE(MemoryExists(p + FRAMESIZE * 1));
}

TEST(munmap, memoryGone) {
  char *p;
  ASSERT_NE(MAP_FAILED, (p = mmap(0, FRAMESIZE, PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE));
}

TEST(munmap, testTooSmallToUnmapAsan) {
  if (!IsAsan()) return;
  char *p;
  ASSERT_NE(MAP_FAILED, (p = mmap(0, FRAMESIZE, PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
  EXPECT_TRUE(MemoryExists((char *)(((intptr_t)p >> 3) + 0x7fff8000)));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE));
  EXPECT_TRUE(MemoryExists((char *)(((intptr_t)p >> 3) + 0x7fff8000)));
}

TEST(munmap, testLargeEnoughToUnmapAsan) {
  if (!IsAsan()) return;
  if (IsWindows()) {
    // we're unfortunately never able to unmap asan pages on windows
    // because the memtrack array items always have to be 64kb so we
    // we're able to store a handle for each
    return;
  }
  char *p;
  size_t n;
  n = FRAMESIZE * 8 * 2;
  ASSERT_NE(MAP_FAILED, (p = mmap(0, n, PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
  EXPECT_SYS(0, 0, munmap(p, n));
#if 0
  EXPECT_FALSE(
      MemoryExists((char *)(((intptr_t)(p + n / 2) >> 3) + 0x7fff8000)));
#endif
}

TEST(munmap, tinyFile_roundupUnmapSize) {
  char *p;
  ASSERT_SYS(0, 3, open("doge", O_WRONLY | O_CREAT | O_TRUNC, 0644));
  ASSERT_SYS(0, 5, write(3, "hello", 5));
  ASSERT_SYS(0, 0, close(3));
  ASSERT_SYS(0, 3, open("doge", O_RDONLY));
  ASSERT_NE(MAP_FAILED, (p = mmap(0, 5, PROT_READ, MAP_PRIVATE, 3, 0)));
  ASSERT_SYS(0, 0, close(3));
  EXPECT_TRUE(MemoryExists(p));
  // some kernels/versions support this, some don't
  EXPECT_FALSE(MemoryExists(p + PAGESIZE));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE));
  EXPECT_FALSE(MemoryExists(p));
  EXPECT_FALSE(MemoryExists(p + 5));
}

TEST(munmap, tinyFile_preciseUnmapSize) {
  char *p, *q;
  ASSERT_SYS(0, 3, open("doge", O_WRONLY | O_CREAT | O_TRUNC, 0644));
  ASSERT_SYS(0, 5, write(3, "hello", 5));
  ASSERT_SYS(0, 0, close(3));
  ASSERT_SYS(0, 3, open("doge", O_RDONLY));
  ASSERT_NE(MAP_FAILED, (p = mmap(0, 5, PROT_READ, MAP_PRIVATE, 3, 0)));
  ASSERT_NE(MAP_FAILED, (q = mmap(0, 5, PROT_READ, MAP_PRIVATE, 3, 0)));
  ASSERT_SYS(0, 0, close(3));
  EXPECT_TRUE(MemoryExists(p));
  EXPECT_TRUE(MemoryExists(q));
  EXPECT_SYS(0, 0, munmap(p, 5));
  EXPECT_FALSE(MemoryExists(p));
  EXPECT_TRUE(MemoryExists(q));
  EXPECT_SYS(0, 0, munmap(q, 5));
  EXPECT_FALSE(MemoryExists(q));
}

// clang-format off
TEST(munmap, tinyFile_mapThriceUnmapOnce) {
  char *p = (char *)0x02000000;
  ASSERT_SYS(0, 3, open("doge", O_RDWR | O_CREAT | O_TRUNC, 0644));
  ASSERT_SYS (0, 5, write(3, "hello", 5));
  ASSERT_NE(MAP_FAILED, mmap(p+FRAMESIZE*0, FRAMESIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0));
  ASSERT_NE(MAP_FAILED, mmap(p+FRAMESIZE*1, 5, PROT_READ, MAP_PRIVATE|MAP_FIXED, 3, 0));
  ASSERT_NE(MAP_FAILED, mmap(p+FRAMESIZE*3, 5, PROT_READ, MAP_PRIVATE|MAP_FIXED, 3, 0));
  ASSERT_SYS(0, 0, close(3));
  EXPECT_TRUE(MemoryExists(p+FRAMESIZE*0));
  EXPECT_TRUE(MemoryExists(p+FRAMESIZE*1));
  EXPECT_FALSE(MemoryExists(p+FRAMESIZE*2));
  EXPECT_TRUE(MemoryExists(p+FRAMESIZE*3));
  EXPECT_SYS(0, 0, munmap(p, FRAMESIZE*5));
  EXPECT_FALSE(MemoryExists(p+FRAMESIZE*0));
  EXPECT_FALSE(MemoryExists(p+FRAMESIZE*1));
  EXPECT_FALSE(MemoryExists(p+FRAMESIZE*2));
  EXPECT_FALSE(MemoryExists(p+FRAMESIZE*3));
}
// clang-format on
