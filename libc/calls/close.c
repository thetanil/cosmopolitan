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
#include "libc/assert.h"
#include "libc/bits/weaken.h"
#include "libc/calls/calls.h"
#include "libc/calls/internal.h"
#include "libc/calls/state.internal.h"
#include "libc/calls/strace.internal.h"
#include "libc/calls/syscall-nt.internal.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/intrin/spinlock.h"
#include "libc/sock/syscall_fd.internal.h"
#include "libc/sysv/errfuns.h"
#include "libc/zipos/zipos.internal.h"

/**
 * Closes file descriptor.
 *
 * This function may be used for file descriptors returned by functions
 * like open, socket, accept, epoll_create, and landlock_create_ruleset.
 *
 * This function should never be called twice for the same file
 * descriptor, regardless of whether or not an error happened. However
 * that doesn't mean the error should be ignored.
 *
 * @return 0 on success, or -1 w/ errno
 * @error EINTR means a signal was received while closing in which case
 *     close() does not need to be called again, since the fd will close
 *     in the background
 * @asyncsignalsafe
 * @vforksafe
 */
int close(int fd) {
  int rc;
  if (fd == -1) {
    rc = 0;
  } else if (fd < 0) {
    rc = einval();
  } else {
    // for performance reasons we want to avoid holding __fds_lock()
    // while sys_close() is happening. this leaves the kernel / libc
    // having a temporarily inconsistent state. routines that obtain
    // file descriptors the way __zipos_open() does need to retry if
    // there's indication this race condition happened.
    if (__isfdkind(fd, kFdZip)) {
      rc = weaken(__zipos_close)(fd);
    } else {
      if (!IsWindows() && !IsMetal()) {
        rc = sys_close(fd);
      } else if (IsMetal()) {
        rc = 0;
      } else {
        if (__isfdkind(fd, kFdEpoll)) {
          rc = weaken(sys_close_epoll_nt)(fd);
        } else if (__isfdkind(fd, kFdSocket)) {
          rc = weaken(sys_closesocket_nt)(g_fds.p + fd);
        } else if (__isfdkind(fd, kFdFile) ||     //
                   __isfdkind(fd, kFdConsole) ||  //
                   __isfdkind(fd, kFdProcess)) {  //
          rc = sys_close_nt(g_fds.p + fd);
        } else {
          rc = eio();
        }
      }
    }
    if (!__vforked) {
      __releasefd(fd);
    }
  }
  STRACE("%s(%d) → %d% m", "close", fd, rc);
  return rc;
}
