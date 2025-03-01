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
#include "libc/calls/ioctl.h"
#include "libc/calls/pledge.internal.h"
#include "libc/calls/struct/bpf.h"
#include "libc/calls/struct/filter.h"
#include "libc/calls/struct/flock.h"
#include "libc/calls/struct/seccomp.h"
#include "libc/calls/struct/sigaction.h"
#include "libc/calls/struct/stat.h"
#include "libc/calls/syscall_support-sysv.internal.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/kprintf.h"
#include "libc/macros.internal.h"
#include "libc/mem/io.h"
#include "libc/mem/mem.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/runtime.h"
#include "libc/sock/sock.h"
#include "libc/sock/struct/sockaddr.h"
#include "libc/stdio/stdio.h"
#include "libc/sysv/consts/af.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/consts/f.h"
#include "libc/sysv/consts/fio.h"
#include "libc/sysv/consts/ipproto.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/nrlinux.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/pr.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/consts/sock.h"
#include "libc/testlib/ezbench.h"
#include "libc/testlib/testlib.h"
#include "libc/thread/spawn.h"
#include "libc/time/time.h"
#include "libc/x/x.h"

STATIC_YOINK("zip_uri_support");

char testlib_enable_tmp_setup_teardown;

void OnSig(int sig) {
  // do nothing
}

int memfd_secret(unsigned int);  // our ENOSYS threshold

int extract(const char *from, const char *to, int mode) {
  int fdin, fdout;
  if ((fdin = open(from, O_RDONLY)) == -1) return -1;
  if ((fdout = creat(to, mode)) == -1) {
    close(fdin);
    return -1;
  }
  if (_copyfd(fdin, fdout, -1) == -1) {
    close(fdout);
    close(fdin);
    return -1;
  }
  return close(fdout) | close(fdin);
}

void SetUp(void) {
  if (!__is_linux_2_6_23() && !IsOpenbsd()) exit(0);
  ASSERT_SYS(0, 0, extract("/zip/life.elf", "life.elf", 0755));
  ASSERT_SYS(0, 0, extract("/zip/sock.elf", "sock.elf", 0755));
}

TEST(pledge, default_allowsExit) {
  int *job;
  int ws, pid;
  // create small shared memory region
  ASSERT_NE(-1, (job = mmap(0, FRAMESIZE, PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0)));
  job[0] = 2;  // create workload
  job[1] = 2;
  ASSERT_NE(-1, (pid = fork()));  // create enclaved worker
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("", 0));
    job[0] = job[0] + job[1];  // do work
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));  // wait for worker
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(0, WEXITSTATUS(ws));
  EXPECT_EQ(4, job[0]);  // check result
  EXPECT_SYS(0, 0, munmap(job, FRAMESIZE));
}

int Enclave(void *arg, int tid) {
  ASSERT_SYS(0, 0, pledge("", 0));
  int *job = arg;            // get job
  job[0] = job[0] + job[1];  // do work
  return 0;                  // exit
}

TEST(pledge, withThreadMemory) {
  if (IsOpenbsd()) return;  // openbsd doesn't allow it, wisely
  struct spawn worker;
  int job[2] = {2, 2};                              // create workload
  ASSERT_SYS(0, 0, _spawn(Enclave, job, &worker));  // create worker
  ASSERT_SYS(0, 0, _join(&worker));                 // wait for exit
  EXPECT_EQ(4, job[0]);                             // check result
}

TEST(pledge, stdio_forbidsOpeningPasswd1) {
  if (!IsLinux()) return;
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio", 0));
    ASSERT_SYS(EPERM, -1, open("/etc/passwd", O_RDWR));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(0, WEXITSTATUS(ws));
}

TEST(pledge, stdio_forbidsOpeningPasswd2) {
  if (!IsOpenbsd()) return;
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio", 0));
    ASSERT_SYS(EPERM, -1, open("/etc/passwd", O_RDWR));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFSIGNALED(ws));
  EXPECT_EQ(SIGABRT, WTERMSIG(ws));
}

TEST(pledge, multipleCalls_canOnlyBecomeMoreRestrictive1) {
  if (IsOpenbsd()) return;
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio", 0));
    ASSERT_SYS(0, 0, pledge("stdio unix", 0));
    ASSERT_SYS(0, 3, dup(2));
    ASSERT_SYS(EPERM, -1, socket(AF_UNIX, SOCK_STREAM, 0));
    ASSERT_SYS(0, 2, prctl(PR_GET_SECCOMP, 0, 0));
    ASSERT_SYS(0, 0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
    ASSERT_SYS(EINVAL, -1, prctl(PR_SET_NO_NEW_PRIVS, 0, 0, 0, 0));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(0, WEXITSTATUS(ws));
}

TEST(pledge, multipleCalls_canOnlyBecomeMoreRestrictive2) {
  if (!IsOpenbsd()) return;
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio", "stdio"));
    ASSERT_SYS(EPERM, -1, pledge("stdio unix", "stdio unix"));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(0, WEXITSTATUS(ws));
}

TEST(pledge, multipleCalls_canOnlyBecomeMoreRestrictive3) {
  if (!IsOpenbsd()) return;
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio unix", 0));
    ASSERT_SYS(0, 0, pledge("stdio", 0));
    ASSERT_SYS(0, 3, dup(2));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(0, WEXITSTATUS(ws));
}

TEST(pledge, stdio_fcntl_allowsSomeFirstArgs) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  struct flock lk;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio inet", 0));
    ASSERT_SYS(0, 0, pledge("stdio", 0));
    ASSERT_NE(-1, fcntl(0, F_GETFL));
    ASSERT_SYS(0, 0, fcntl(0, F_GETFD));
    ASSERT_SYS(0, 3, fcntl(2, F_DUPFD_CLOEXEC, 3));
    ASSERT_SYS(0, 0, ioctl(0, FIOCLEX, 0));
    ASSERT_SYS(EPERM, 0, isatty(0));
    ASSERT_SYS(EPERM, -1, fcntl(0, -1));
    ASSERT_SYS(EPERM, -1, fcntl(0, F_GETLK, &lk));
    ASSERT_SYS(EPERM, -1, fcntl(0, F_NOTIFY));
    ASSERT_SYS(EPERM, -1, socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
}

TEST(pledge, stdioTty_sendtoRestricted_requiresNullAddr) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid, sv[2];
  ASSERT_SYS(0, 0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio tty", 0));
    ASSERT_SYS(0, 5, send(sv[0], "hello", 5, 0));
    ASSERT_SYS(0, 5, sendto(sv[0], "hello", 5, 0, 0, 0));
    isatty(0);
    ASSERT_NE(EPERM, errno);
    errno = 0;
    // set lower 32-bit word of pointer to zero lool
    struct sockaddr_in *sin =
        mmap((void *)0x300000000000, FRAMESIZE, PROT_READ | PROT_WRITE,
             MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    sin->sin_family = AF_INET;
    ASSERT_SYS(EPERM, -1, sendto(sv[0], "hello", 5, 0, sin, sizeof(*sin)));
    _Exit(0);
  }
  close(sv[0]);
  close(sv[1]);
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
}

TEST(pledge, unix_forbidsInetSockets) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio unix", 0));
    ASSERT_SYS(0, 3, socket(AF_UNIX, SOCK_STREAM, 0));
    ASSERT_SYS(0, 4, socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    ASSERT_SYS(EPERM, -1, socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    ASSERT_SYS(EPERM, -1, socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP));
    ASSERT_SYS(EPERM, -1, socket(AF_UNIX, SOCK_RAW, 0));
    ASSERT_SYS(EPERM, -1, socket(AF_UNIX, SOCK_STREAM, 1));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
}

TEST(pledge, wpath_doesNotImplyRpath) {
  int ws, pid;
  bool *gotsome;
  ASSERT_NE(-1, (gotsome = _mapshared(FRAMESIZE)));
  ASSERT_SYS(0, 0, touch("foo", 0644));
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio wpath", 0));
    ASSERT_SYS(0, 3, open("foo", O_WRONLY));
    *gotsome = true;
    ASSERT_SYS(EPERM, -1, open("foo", O_RDONLY));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  ASSERT_TRUE(*gotsome);
  if (IsOpenbsd()) {
    ASSERT_TRUE(WIFSIGNALED(ws));
    ASSERT_EQ(SIGABRT, WTERMSIG(ws));
  } else {
    ASSERT_TRUE(WIFEXITED(ws));
    ASSERT_EQ(0, WEXITSTATUS(ws));
  }
}

TEST(pledge, inet_forbidsOtherSockets) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio inet", 0));
    ASSERT_SYS(0, 3, socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    ASSERT_SYS(0, 4, socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    ASSERT_SYS(0, 5, socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    ASSERT_SYS(0, 6, socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP));
    ASSERT_SYS(0, 7, socket(AF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP));
    ASSERT_SYS(EPERM, -1, socket(AF_UNIX, SOCK_STREAM, 0));
    ASSERT_SYS(EPERM, -1, socket(AF_BLUETOOTH, SOCK_DGRAM, IPPROTO_UDP));
    ASSERT_SYS(EPERM, -1, socket(AF_INET, SOCK_RAW, IPPROTO_UDP));
    ASSERT_SYS(EPERM, -1, socket(AF_INET, SOCK_DGRAM, IPPROTO_RAW));
    struct sockaddr_in sin = {AF_INET, 0, {htonl(0x7f000001)}};
    ASSERT_SYS(0, 0, bind(4, &sin, sizeof(sin)));
    uint32_t az = sizeof(sin);
    ASSERT_SYS(0, 0, getsockname(4, &sin, &az));
    ASSERT_SYS(0, 5, sendto(3, "hello", 5, 0, &sin, sizeof(sin)));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
}

TEST(pledge, mmap) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  char *p;
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio", 0));
    ASSERT_NE(MAP_FAILED, (p = mmap(0, FRAMESIZE, PROT_READ | PROT_WRITE,
                                    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
    ASSERT_SYS(0, 0, mprotect(p, FRAMESIZE, PROT_READ));
    ASSERT_SYS(EPERM, MAP_FAILED,
               mprotect(p, FRAMESIZE, PROT_READ | PROT_EXEC));
    ASSERT_SYS(EPERM, MAP_FAILED,
               mmap(0, FRAMESIZE, PROT_EXEC | PROT_READ,
                    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
}

TEST(pledge, mmapProtExec) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  char *p;
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio prot_exec", 0));
    ASSERT_NE(MAP_FAILED, (p = mmap(0, FRAMESIZE, PROT_READ | PROT_WRITE,
                                    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
    ASSERT_SYS(0, 0, mprotect(p, FRAMESIZE, PROT_READ));
    ASSERT_SYS(0, 0, mprotect(p, FRAMESIZE, PROT_READ | PROT_EXEC));
    ASSERT_NE(MAP_FAILED, mmap(0, FRAMESIZE, PROT_EXEC | PROT_READ,
                               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
}

TEST(pledge, msyscall) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ax, ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio", 0));
    // now issue authorized syscall where rip isn't privileged
    asm volatile("syscall"
                 : "=a"(ax)
                 : "0"(__NR_linux_dup), "D"(2)
                 : "rcx", "r11", "memory");
    _Exit(1);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFSIGNALED(ws));
  EXPECT_EQ(SIGSYS, WTERMSIG(ws));
}

TEST(pledge, chmod_ignoresDangerBits) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  ASSERT_SYS(0, 3, creat("foo", 0644));
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio rpath wpath", 0));
    ASSERT_SYS(0, 0, fchmod(3, 00700));
    ASSERT_SYS(0, 0, chmod("foo", 00700));
    ASSERT_SYS(0, 0, fchmodat(AT_FDCWD, "foo", 00700, 0));
    ASSERT_SYS(EPERM, -1, fchmod(3, 07700));
    ASSERT_SYS(EPERM, -1, chmod("foo", 04700));
    ASSERT_SYS(EPERM, -1, fchmodat(AT_FDCWD, "foo", 02700, 0));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
  close(3);
}

TEST(pledge, open_rpath) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  ASSERT_SYS(0, 0, touch("foo", 0644));
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio rpath", 0));
    ASSERT_SYS(0, 3, open("foo", O_RDONLY));
    ASSERT_SYS(EPERM, -1, open("foo", O_RDONLY | O_TRUNC));
    ASSERT_SYS(EPERM, -1, open("foo", O_RDONLY | O_TMPFILE));
    ASSERT_SYS(EPERM, -1, open("foo", O_RDWR | O_TRUNC | O_CREAT, 0644));
    ASSERT_SYS(EPERM, -1, open("foo", O_WRONLY | O_TRUNC | O_CREAT, 0644));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
}

TEST(pledge, open_wpath) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  ASSERT_SYS(0, 0, touch("foo", 0644));
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio wpath", 0));
    ASSERT_SYS(EPERM, -1, open(".", O_RDWR | O_TMPFILE, 07644));
    ASSERT_SYS(0, 3, open("foo", O_WRONLY | O_TRUNC));
    ASSERT_SYS(0, 4, open("foo", O_RDWR));
    ASSERT_SYS(EPERM, -1, open("foo", O_WRONLY | O_TRUNC | O_CREAT, 0644));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
}

TEST(pledge, open_cpath) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  struct stat st;
  ASSERT_SYS(0, 0, touch("foo", 0644));
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio cpath", 0));
    ASSERT_SYS(0, 3, open("foo", O_WRONLY | O_TRUNC | O_CREAT, 0644));
    ASSERT_SYS(0, 0, fstat(3, &st));
    ASSERT_EQ(0100644, st.st_mode);
    // make sure open() can't apply the setuid bit
    ASSERT_SYS(EPERM, -1, open("bar", O_WRONLY | O_TRUNC | O_CREAT, 04644));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
}

TEST(pledge, sigaction_isFineButForbidsSigsys) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio", 0));
    struct sigaction sa = {.sa_handler = OnSig};
    ASSERT_SYS(0, 0, sigaction(SIGINT, &sa, 0));
    ASSERT_SYS(EPERM, -1, sigaction(SIGSYS, &sa, 0));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws) && !WEXITSTATUS(ws));
}

TEST(pledge, execpromises_ok) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio exec", "stdio"));
    execl("life.elf", "life.elf", 0);
    _Exit(127);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(42, WEXITSTATUS(ws));
}

TEST(pledge, execpromises_notok) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio exec", "stdio"));
    execl("sock.elf", "sock.elf", 0);
    _Exit(127);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(128 + EPERM, WEXITSTATUS(ws));
}

TEST(pledge, execpromises_reducesAtExecOnLinux) {
  if (IsOpenbsd()) return;  // b/c testing linux bpf
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio inet tty exec", "stdio tty"));
    execl("sock.elf", "sock.elf", 0);
    _Exit(127);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(128 + EPERM, WEXITSTATUS(ws));
}

TEST(pledge_openbsd, execpromisesIsNull_letsItDoAnything) {
  if (!IsOpenbsd()) return;
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio exec", 0));
    execl("sock.elf", "sock.elf", 0);
    _Exit(127);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(3, WEXITSTATUS(ws));
}

TEST(pledge_openbsd, execpromisesIsSuperset_letsItDoAnything) {
  if (!IsOpenbsd()) return;
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio rpath exec", "stdio rpath tty inet"));
    execl("sock.elf", "sock.elf", 0);
    _Exit(127);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(3, WEXITSTATUS(ws));
}

TEST(pledge_linux, execpromisesIsSuperset_notPossible) {
  if (IsOpenbsd()) return;
  ASSERT_SYS(EINVAL, -1, pledge("stdio exec", "stdio inet exec"));
}

TEST(pledge_openbsd, execpromises_notok) {
  if (!IsOpenbsd()) return;
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio exec", "stdio"));
    execl("sock.elf", "sock.elf", 0);
    _Exit(127);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFSIGNALED(ws));
  EXPECT_EQ(SIGABRT, WTERMSIG(ws));
}

TEST(pledge_openbsd, bigSyscalls) {
  if (IsOpenbsd()) return;  // testing lunix
  int ws, pid;
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio", 0));
    ASSERT_SYS(ENOSYS, -1, memfd_secret(0));
    ASSERT_SYS(ENOSYS, -1, sys_bogus());
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(0, WEXITSTATUS(ws));
}

int LockWorker(void *arg, int tid) {
  flockfile(stdout);
  ASSERT_EQ(gettid(), stdout->lock.lock);
  funlockfile(stdout);
  return 0;
}

TEST(pledge, threadWithLocks_canCodeMorph) {
  struct spawn worker;
  int ws, pid;
  // not sure how this works on OpenBSD but it works!
  if (!fork()) {
    ASSERT_SYS(0, 0, pledge("stdio prot_exec", 0));
    ASSERT_SYS(0, 0, _spawn(LockWorker, 0, &worker));
    ASSERT_SYS(0, 0, _join(&worker));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(0, WEXITSTATUS(ws));
}

TEST(pledge, everything) {
  int ws, pid;
  if (!fork()) {
    // contains 548 bpf instructions [2022-07-24]
    ASSERT_SYS(0, 0,
               pledge("stdio rpath wpath cpath dpath "
                      "flock fattr inet unix dns tty "
                      "recvfd sendfd proc exec id "
                      "unveil settime prot_exec "
                      "vminfo tmppath",
                      "stdio rpath wpath cpath dpath "
                      "flock fattr inet unix dns tty "
                      "recvfd sendfd proc exec id "
                      "unveil settime prot_exec "
                      "vminfo tmppath"));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(0, WEXITSTATUS(ws));
}

TEST(pledge, execWithoutRpath) {
  int ws, pid;
  ASSERT_SYS(0, 0, touch("foo", 0644));
  ASSERT_NE(-1, (pid = fork()));
  if (!pid) {
    ASSERT_SYS(0, 0, pledge("stdio prot_exec exec", "stdio prot_exec exec"));
    ASSERT_SYS(EPERM, -1, open("foo", O_RDONLY));
    _Exit(0);
  }
  EXPECT_NE(-1, wait(&ws));
  if (IsOpenbsd()) {
    EXPECT_TRUE(WIFSIGNALED(ws));
    EXPECT_EQ(SIGABRT, WTERMSIG(ws));
  } else {
    EXPECT_TRUE(WIFEXITED(ws));
    EXPECT_EQ(0, WEXITSTATUS(ws));
  }
}

BENCH(pledge, bench) {
  int pid;
  if (!fork()) {
    ASSERT_SYS(0, 0, pledge("stdio", 0));
    EZBENCH2("sched_yield", donothing, sched_yield());
    _Exit(0);
  }
  wait(0);
}
