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
#include "libc/assert.h"
#include "libc/bits/bits.h"
#include "libc/calls/calls.h"
#include "libc/calls/ioctl.h"
#include "libc/calls/makedev.h"
#include "libc/calls/strace.internal.h"
#include "libc/calls/struct/bpf.h"
#include "libc/calls/struct/dirent.h"
#include "libc/calls/struct/itimerval.h"
#include "libc/calls/struct/rusage.h"
#include "libc/calls/struct/siginfo.h"
#include "libc/calls/struct/sigset.h"
#include "libc/calls/struct/stat.h"
#include "libc/calls/struct/timespec.h"
#include "libc/calls/struct/timeval.h"
#include "libc/calls/struct/winsize.h"
#include "libc/calls/ucontext.h"
#include "libc/dns/dns.h"
#include "libc/errno.h"
#include "libc/fmt/fmt.h"
#include "libc/fmt/magnumstrs.internal.h"
#include "libc/log/log.h"
#include "libc/macros.internal.h"
#include "libc/mem/fmt.h"
#include "libc/mem/mem.h"
#include "libc/nt/runtime.h"
#include "libc/runtime/clktck.h"
#include "libc/runtime/memtrack.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/sock/sock.h"
#include "libc/sock/syslog.h"
#include "libc/stdio/append.internal.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/af.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/consts/clock.h"
#include "libc/sysv/consts/dt.h"
#include "libc/sysv/consts/f.h"
#include "libc/sysv/consts/fd.h"
#include "libc/sysv/consts/ip.h"
#include "libc/sysv/consts/ipproto.h"
#include "libc/sysv/consts/itimer.h"
#include "libc/sysv/consts/limits.h"
#include "libc/sysv/consts/log.h"
#include "libc/sysv/consts/msg.h"
#include "libc/sysv/consts/nr.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/ok.h"
#include "libc/sysv/consts/poll.h"
#include "libc/sysv/consts/rlim.h"
#include "libc/sysv/consts/rlimit.h"
#include "libc/sysv/consts/rusage.h"
#include "libc/sysv/consts/s.h"
#include "libc/sysv/consts/sa.h"
#include "libc/sysv/consts/shut.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/consts/sio.h"
#include "libc/sysv/consts/so.h"
#include "libc/sysv/consts/sock.h"
#include "libc/sysv/consts/sol.h"
#include "libc/sysv/consts/tcp.h"
#include "libc/sysv/consts/utime.h"
#include "libc/sysv/consts/w.h"
#include "libc/sysv/errfuns.h"
#include "libc/time/struct/tm.h"
#include "libc/time/time.h"
#include "libc/x/x.h"
#include "third_party/lua/cosmo.h"
#include "third_party/lua/lauxlib.h"
#include "third_party/lua/lgc.h"
#include "third_party/lua/lua.h"
#include "third_party/lua/luaconf.h"
#include "tool/net/luacheck.h"

/**
 * @fileoverview UNIX system calls thinly wrapped for Lua
 * @support Linux, Mac, Windows, FreeBSD, NetBSD, OpenBSD
 */

struct UnixErrno {
  int errno_;
  int winerr;
  const char *call;
};

static lua_State *GL;

static void *LuaRealloc(lua_State *L, void *p, size_t n) {
  void *p2;
  if ((p2 = realloc(p, n))) {
    return p2;
  }
  if (IsLegalSize(n)) {
    WARNF("reacting to malloc() failure by running lua garbage collector...");
    luaC_fullgc(L, 1);
    p2 = realloc(p, n);
  }
  return p2;
}

static void *LuaAlloc(lua_State *L, size_t n) {
  return LuaRealloc(L, 0, n);
}

static void *LuaAllocOrDie(lua_State *L, size_t n) {
  void *p;
  if ((p = LuaAlloc(L, n))) {
    return p;
  } else {
    luaL_error(L, "out of memory");
    unreachable;
  }
}

static lua_Integer FixLimit(long x) {
  if (0 <= x && x < RLIM_INFINITY) {
    return x;
  } else {
    return -1;
  }
}

static void LuaPushSigset(lua_State *L, struct sigset set) {
  struct sigset *sp = lua_newuserdatauv(L, sizeof(*sp), 1);
  luaL_setmetatable(L, "unix.Sigset");
  *sp = set;
}

static void LuaPushStat(lua_State *L, struct stat *st) {
  struct stat *stp = lua_newuserdatauv(L, sizeof(*stp), 1);
  luaL_setmetatable(L, "unix.Stat");
  *stp = *st;
}

static void LuaPushRusage(lua_State *L, struct rusage *set) {
  struct rusage *sp = lua_newuserdatauv(L, sizeof(*sp), 1);
  luaL_setmetatable(L, "unix.Rusage");
  *sp = *set;
}

static void LuaSetIntField(lua_State *L, const char *k, lua_Integer v) {
  lua_pushinteger(L, v);
  lua_setfield(L, -2, k);
}

static dontinline int ReturnInteger(lua_State *L, lua_Integer x) {
  lua_pushinteger(L, x);
  return 1;
}

static dontinline int ReturnBoolean(lua_State *L, int x) {
  lua_pushboolean(L, !!x);
  return 1;
}

static dontinline int ReturnString(lua_State *L, const char *x) {
  lua_pushstring(L, x);
  return 1;
}

static int SysretErrno(lua_State *L, const char *call, int olderr) {
  struct UnixErrno *ep;
  int i, unixerr, winerr;
  unixerr = errno;
  winerr = GetLastError();
  if (!IsTiny() && !(0 < unixerr && unixerr < (!IsWindows() ? 4096 : 65536))) {
    WARNF("errno should not be %d", unixerr);
  }
  lua_pushnil(L);
  ep = lua_newuserdatauv(L, sizeof(*ep), 1);
  luaL_setmetatable(L, "unix.Errno");
  ep->errno_ = unixerr;
  ep->winerr = winerr;
  ep->call = call;
  errno = olderr;
  return 2;
}

int SysretBool(lua_State *L, const char *call, int olderr, int rc) {
  if (!IsTiny() && (rc != 0 && rc != -1)) {
    WARNF("syscall supposed to return 0 / -1 but got %d", rc);
  }
  if (rc != -1) {
    lua_pushboolean(L, true);
    return 1;
  } else {
    return SysretErrno(L, call, olderr);
  }
}

static int SysretInteger(lua_State *L, const char *call, int olderr,
                         int64_t rc) {
  if (rc != -1) {
    if (!IsTiny() && olderr != errno) {
      WARNF("errno unexpectedly changed %d → %d", olderr, errno);
    }
    lua_pushinteger(L, rc);
    return 1;
  } else {
    return SysretErrno(L, call, olderr);
  }
}

static int MakeSockaddr(lua_State *L, int i, struct sockaddr_storage *ss,
                        uint32_t *salen) {
  bzero(ss, sizeof(*ss));
  if (!lua_isinteger(L, i)) {
    ((struct sockaddr_un *)ss)->sun_family = AF_UNIX;
    if (!memccpy(((struct sockaddr_un *)ss)->sun_path, luaL_checkstring(L, i),
                 0, sizeof(((struct sockaddr_un *)ss)->sun_path))) {
      luaL_error(L, "unix path too long");
      unreachable;
    }
    *salen = sizeof(struct sockaddr_un);
    return i + 1;
  } else {
    ((struct sockaddr_in *)ss)->sin_family = AF_INET;
    ((struct sockaddr_in *)ss)->sin_addr.s_addr =
        htonl(luaL_optinteger(L, i, 0));
    ((struct sockaddr_in *)ss)->sin_port = htons(luaL_optinteger(L, i + 1, 0));
    *salen = sizeof(struct sockaddr_in);
    return i + 2;
  }
}

static int PushSockaddr(lua_State *L, const struct sockaddr_storage *ss) {
  if (ss->ss_family == AF_INET) {
    lua_pushinteger(L,
                    ntohl(((const struct sockaddr_in *)ss)->sin_addr.s_addr));
    lua_pushinteger(L, ntohs(((const struct sockaddr_in *)ss)->sin_port));
    return 2;
  } else if (ss->ss_family == AF_UNIX) {
    lua_pushstring(L, ((const struct sockaddr_un *)ss)->sun_path);
    return 1;
  } else {
    luaL_error(L, "bad family");
    unreachable;
  }
}

static void CheckOptvalsize(lua_State *L, uint32_t want, uint32_t got) {
  if (!IsTiny()) {
    if (want == got) return;
    WARNF("getsockopt optvalsize should be %d but was %d", want, got);
  }
}

static void FreeStringList(char **p) {
  int i;
  if (p) {
    for (i = 0; p[i]; ++i) {
      free(p[i]);
    }
    free(p);
  }
}

static char **ConvertLuaArrayToStringList(lua_State *L, int i) {
  int j, n;
  char **p, *s;
  luaL_checktype(L, i, LUA_TTABLE);
  lua_len(L, i);
  n = lua_tointeger(L, -1);
  lua_pop(L, 1);
  if ((p = LuaAlloc(L, (n + 1) * sizeof(*p)))) {
    for (j = 1; j <= n; ++j) {
      lua_geti(L, i, j);
      s = strdup(lua_tostring(L, -1));
      lua_pop(L, 1);
      if (s) {
        p[j - 1] = s;
      } else {
        FreeStringList(p);
        p = 0;
        break;
      }
    }
    p[j - 1] = 0;
  }
  return p;
}

////////////////////////////////////////////////////////////////////////////////
// System Calls

// unix.exit([exitcode:int])
//     └─→ ⊥
static wontreturn int LuaUnixExit(lua_State *L) {
  _Exit(luaL_optinteger(L, 1, 0));
}

static dontinline int LuaUnixGetid(lua_State *L, int f(void)) {
  return ReturnInteger(L, f());
}

// unix.getpid()
//     └─→ pid:int
static int LuaUnixGetpid(lua_State *L) {
  return LuaUnixGetid(L, getpid);
}

// unix.getppid()
//     └─→ pid:int
static int LuaUnixGetppid(lua_State *L) {
  return LuaUnixGetid(L, getppid);
}

// unix.getuid()
//     └─→ uid:int
static int LuaUnixGetuid(lua_State *L) {
  return LuaUnixGetid(L, getuid);
}

// unix.getgid()
//     └─→ gid:int
static int LuaUnixGetgid(lua_State *L) {
  return LuaUnixGetid(L, getgid);
}

// unix.geteuid()
//     └─→ uid:int
static int LuaUnixGeteuid(lua_State *L) {
  return LuaUnixGetid(L, geteuid);
}

// unix.getegid()
//     └─→ gid:int
static int LuaUnixGetegid(lua_State *L) {
  return LuaUnixGetid(L, getegid);
}

// unix.umask(newmask:int)
//     └─→ oldmask:int
static int LuaUnixUmask(lua_State *L) {
  return ReturnInteger(L, umask(luaL_checkinteger(L, 1)));
}

// unix.access(path:str, how:int[, flags:int[, dirfd:int]])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixAccess(lua_State *L) {
  int olderr = errno;
  return SysretBool(
      L, "access", olderr,
      faccessat(luaL_optinteger(L, 3, AT_FDCWD), luaL_checkstring(L, 1),
                luaL_checkinteger(L, 2), luaL_optinteger(L, 4, 0)));
}

// unix.mkdir(path:str[, mode:int[, dirfd:int]])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixMkdir(lua_State *L) {
  int olderr = errno;
  return SysretBool(
      L, "mkdir", olderr,
      mkdirat(luaL_optinteger(L, 3, AT_FDCWD), luaL_checkstring(L, 1),
              luaL_optinteger(L, 2, 0755)));
}

// unix.makedirs(path:str[, mode:int])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixMakedirs(lua_State *L) {
  int olderr = errno;
  return SysretBool(
      L, "makedirs", olderr,
      makedirs(luaL_checkstring(L, 1), luaL_optinteger(L, 2, 0755)));
}

// unix.chdir(path:str)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixChdir(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "chdir", olderr, chdir(luaL_checkstring(L, 1)));
}

// unix.unlink(path:str[, dirfd:int])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixUnlink(lua_State *L) {
  int olderr = errno;
  return SysretBool(
      L, "unlink", olderr,
      unlinkat(luaL_optinteger(L, 2, AT_FDCWD), luaL_checkstring(L, 1), 0));
}

// unix.rmdir(path:str[, dirfd:int])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixRmdir(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "rmdir", olderr,
                    unlinkat(luaL_optinteger(L, 2, AT_FDCWD),
                             luaL_checkstring(L, 1), AT_REMOVEDIR));
}

// unix.rename(oldpath:str, newpath:str[, olddirfd:int, newdirfd:int])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixRename(lua_State *L) {
  int olderr = errno;
  return SysretBool(
      L, "rename", olderr,
      renameat(luaL_optinteger(L, 3, AT_FDCWD), luaL_checkstring(L, 1),
               luaL_optinteger(L, 4, AT_FDCWD), luaL_checkstring(L, 2)));
}

// unix.link(existingpath:str, newpath:str[, flags:int[, olddirfd, newdirfd]])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixLink(lua_State *L) {
  int olderr = errno;
  return SysretBool(
      L, "link", olderr,
      linkat(luaL_optinteger(L, 4, AT_FDCWD), luaL_checkstring(L, 1),
             luaL_optinteger(L, 5, AT_FDCWD), luaL_checkstring(L, 2),
             luaL_optinteger(L, 3, 0)));
}

// unix.symlink(target:str, linkpath:str[, newdirfd:int])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixSymlink(lua_State *L) {
  int olderr = errno;
  return SysretBool(
      L, "symlink", olderr,
      symlinkat(luaL_checkstring(L, 1), luaL_optinteger(L, 3, AT_FDCWD),
                luaL_checkstring(L, 2)));
}

// unix.chown(path:str, uid:int, gid:int[, flags:int[, dirfd:int]])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixChown(lua_State *L) {
  int olderr = errno;
  return SysretBool(
      L, "chown", olderr,
      fchownat(luaL_optinteger(L, 5, AT_FDCWD), luaL_checkstring(L, 1),
               luaL_checkinteger(L, 2), luaL_checkinteger(L, 3),
               luaL_optinteger(L, 4, 0)));
}

// unix.chmod(path:str, mode:int[, flags:int[, dirfd:int]])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixChmod(lua_State *L) {
  int olderr = errno;
  return SysretBool(
      L, "chmod", olderr,
      fchmodat(luaL_optinteger(L, 4, AT_FDCWD), luaL_checkstring(L, 1),
               luaL_checkinteger(L, 2), luaL_optinteger(L, 3, 0)));
}

// unix.readlink(path:str[, dirfd:int])
//     ├─→ content:str
//     └─→ nil, unix.Errno
static int LuaUnixReadlink(lua_State *L) {
  char *buf;
  ssize_t rc;
  const char *path;
  int dirfd, olderr = errno;
  size_t got, bufsiz = 8192;
  path = luaL_checkstring(L, 1);
  dirfd = luaL_optinteger(L, 2, AT_FDCWD);
  buf = LuaAllocOrDie(L, bufsiz);
  if ((rc = readlinkat(dirfd, path, buf, bufsiz)) != -1) {
    got = rc;
    if (got < bufsiz) {
      lua_pushlstring(L, buf, got);
      free(buf);
      return 1;
    } else {
      enametoolong();
    }
  }
  free(buf);
  return SysretErrno(L, "readlink", olderr);
}

// unix.getcwd()
//     ├─→ path:str
//     └─→ nil, unix.Errno
static int LuaUnixGetcwd(lua_State *L) {
  char *path;
  int olderr = errno;
  if ((path = getcwd(0, 0))) {
    lua_pushstring(L, path);
    free(path);
    return 1;
  } else {
    return SysretErrno(L, "getcwd", olderr);
  }
}

// unix.fork()
//     ├─┬─→ 0
//     │ └─→ childpid:int
//     └─→ nil, unix.Errno
static int LuaUnixFork(lua_State *L) {
  int olderr = errno;
  return SysretInteger(L, "fork", olderr, fork());
}

// unix.environ()
//     └─→ {str,...}
static int LuaUnixEnviron(lua_State *L) {
  int i;
  char **e;
  lua_newtable(L);
  for (i = 0, e = environ; *e; ++e) {
    lua_pushstring(L, *e);
    lua_rawseti(L, -2, ++i);
  }
  return 1;
}

// unix.execve(prog:str[, args:List<*>, env:List<*>])
//     └─→ nil, unix.Errno
static int LuaUnixExecve(lua_State *L) {
  int olderr;
  const char *prog;
  char **argv, **envp, **freeme1, **freeme2, *ezargs[2];
  olderr = errno;
  prog = luaL_checkstring(L, 1);
  if (!lua_isnoneornil(L, 2)) {
    if ((argv = ConvertLuaArrayToStringList(L, 2))) {
      freeme1 = argv;
      if (!lua_isnoneornil(L, 3)) {
        if ((envp = ConvertLuaArrayToStringList(L, 3))) {
          freeme2 = envp;
        } else {
          FreeStringList(argv);
          return SysretErrno(L, "execve", olderr);
        }
      } else {
        envp = environ;
        freeme2 = 0;
      }
    } else {
      return SysretErrno(L, "execve", olderr);
    }
  } else {
    ezargs[0] = prog;
    ezargs[1] = 0;
    argv = ezargs;
    envp = environ;
    freeme1 = 0;
    freeme2 = 0;
  }
  execve(prog, argv, envp);
  FreeStringList(freeme1);
  FreeStringList(freeme2);
  return SysretErrno(L, "execve", olderr);
}

// unix.commandv(prog:str)
//     ├─→ path:str
//     └─→ nil, unix.Errno
static int LuaUnixCommandv(lua_State *L) {
  int olderr;
  const char *prog;
  char *pathbuf, *resolved;
  olderr = errno;
  prog = luaL_checkstring(L, 1);
  pathbuf = LuaAllocOrDie(L, PATH_MAX);
  if ((resolved = commandv(prog, pathbuf, PATH_MAX))) {
    lua_pushstring(L, resolved);
    free(pathbuf);
    return 1;
  } else {
    free(pathbuf);
    return SysretErrno(L, "commandv", olderr);
  }
}

// unix.realpath(path:str)
//     ├─→ path:str
//     └─→ nil, unix.Errno
static int LuaUnixRealpath(lua_State *L) {
  char *resolved;
  int olderr;
  const char *path;
  olderr = errno;
  path = luaL_checkstring(L, 1);
  if ((resolved = realpath(path, 0))) {
    lua_pushstring(L, resolved);
    free(resolved);
    return 1;
  } else {
    return SysretErrno(L, "realpath", olderr);
  }
}

// unix.syslog(priority:str, msg:str)
static int LuaUnixSyslog(lua_State *L) {
  syslog(luaL_checkinteger(L, 1), "%s", luaL_checkstring(L, 2));
  return 0;
}

// unix.chroot(path:str)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixChroot(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "chroot", olderr, chroot(luaL_checkstring(L, 1)));
}

// unix.setrlimit(resource:int, soft:int[, hard:int])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixSetrlimit(lua_State *L) {
  int olderr = errno;
  int64_t soft = luaL_checkinteger(L, 2);
  return SysretBool(
      L, "setrlimit", olderr,
      setrlimit(luaL_checkinteger(L, 1),
                &(struct rlimit){soft, luaL_optinteger(L, 3, soft)}));
}

// unix.getrlimit(resource:int)
//     ├─→ soft:int, hard:int
//     └─→ nil, unix.Errno
static int LuaUnixGetrlimit(lua_State *L) {
  int olderr = errno;
  struct rlimit rlim;
  if (!getrlimit(luaL_checkinteger(L, 1), &rlim)) {
    lua_pushinteger(L, FixLimit(rlim.rlim_cur));
    lua_pushinteger(L, FixLimit(rlim.rlim_max));
    return 2;
  } else {
    return SysretErrno(L, "getrlimit", olderr);
  }
}

// unix.getrusage([who:int])
//     ├─→ unix.Rusage
//     └─→ nil, unix.Errno
static int LuaUnixGetrusage(lua_State *L) {
  struct rusage ru;
  int olderr = errno;
  if (!getrusage(luaL_optinteger(L, 1, RUSAGE_SELF), &ru)) {
    LuaPushRusage(L, &ru);
    return 1;
  } else {
    return SysretErrno(L, "getrusage", olderr);
  }
}

// unix.kill(pid:int, sig:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixKill(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "kill", olderr,
                    kill(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2)));
}

// unix.raise(sig:int)
//     ├─→ rc:int
//     └─→ nil, unix.Errno
static int LuaUnixRaise(lua_State *L) {
  int olderr = errno;
  return SysretInteger(L, "raise", olderr, raise(luaL_checkinteger(L, 1)));
}

// unix.wait([pid:int, options:int])
//     ├─→ pid:int, wstatus:int, unix.Rusage
//     └─→ nil, unix.Errno
static int LuaUnixWait(lua_State *L) {
  struct rusage ru;
  int pid, wstatus, olderr = errno;
  if ((pid = wait4(luaL_optinteger(L, 1, -1), &wstatus,
                   luaL_optinteger(L, 2, 0), &ru)) != -1) {
    lua_pushinteger(L, pid);
    lua_pushinteger(L, wstatus);
    LuaPushRusage(L, &ru);
    return 3;
  } else {
    return SysretErrno(L, "wait", olderr);
  }
}

// unix.fcntl(fd:int, cmd:int[, arg:int])
//     ├─→ true, ...
//     └─→ nil, unix.Errno
static int LuaUnixFcntl(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "fcntl", olderr,
                    fcntl(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2),
                          luaL_optinteger(L, 3, 0)));
}

// unix.dup(oldfd:int[, newfd:int[, flags:int]])
//     ├─→ newfd:int
//     └─→ nil, unix.Errno
static int LuaUnixDup(lua_State *L) {
  int rc, oldfd, newfd, flags, olderr;
  olderr = errno;
  oldfd = luaL_checkinteger(L, 1);
  newfd = luaL_optinteger(L, 2, -1);
  flags = luaL_optinteger(L, 3, 0);
  if (newfd == -1) {
    rc = dup(oldfd);
  } else {
    rc = dup3(oldfd, newfd, flags);
  }
  return SysretInteger(L, "dup", olderr, rc);
}

// unix.pipe([flags:int])
//     ├─→ reader:int, writer:int
//     └─→ nil, unix.Errno
static int LuaUnixPipe(lua_State *L) {
  int pipefd[2], olderr = errno;
  if (!pipe2(pipefd, luaL_optinteger(L, 1, 0))) {
    lua_pushinteger(L, pipefd[0]);
    lua_pushinteger(L, pipefd[1]);
    return 2;
  } else {
    return SysretErrno(L, "pipe", olderr);
  }
}

// unix.getsid(pid:int)
//     ├─→ sid:int
//     └─→ nil, unix.Errno
static int LuaUnixGetsid(lua_State *L) {
  int olderr = errno;
  return SysretInteger(L, "getsid", olderr, getsid(luaL_checkinteger(L, 1)));
}

static dontinline int LuaUnixRc0(lua_State *L, const char *call, int f(void)) {
  int olderr = errno;
  return SysretInteger(L, call, olderr, f());
}

// unix.getpgrp()
//     ├─→ pgid:int
//     └─→ nil, unix.Errno
static int LuaUnixGetpgrp(lua_State *L) {
  return LuaUnixRc0(L, "getpgrp", getpgrp);
}

// unix.setpgrp()
//     ├─→ pgid:int
//     └─→ nil, unix.Errno
static int LuaUnixSetpgrp(lua_State *L) {
  return LuaUnixRc0(L, "setpgrp", setpgrp);
}

// unix.setsid()
//     ├─→ sid:int
//     └─→ nil, unix.Errno
static int LuaUnixSetsid(lua_State *L) {
  return LuaUnixRc0(L, "setsid", setsid);
}

// unix.getpgid(pid:int)
//     ├─→ pgid:int
//     └─→ nil, unix.Errno
static int LuaUnixGetpgid(lua_State *L) {
  int olderr = errno;
  return SysretInteger(L, "getpgid", olderr, getpgid(luaL_checkinteger(L, 1)));
}

// unix.setpgid(pid:int, pgid:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixSetpgid(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "setpgid", olderr,
                    setpgid(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2)));
}

static dontinline int LuaUnixSetid(lua_State *L, const char *call, int f(int)) {
  int olderr = errno;
  return SysretBool(L, call, olderr, f(luaL_checkinteger(L, 1)));
}

// unix.setuid(uid:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixSetuid(lua_State *L) {
  return LuaUnixSetid(L, "setuid", setuid);
}

// unix.setgid(gid:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixSetgid(lua_State *L) {
  return LuaUnixSetid(L, "setgid", setgid);
}

static dontinline int LuaUnixSetresid(lua_State *L, const char *call,
                                      int f(uint32_t, uint32_t, uint32_t)) {
  int olderr = errno;
  return SysretBool(L, call, olderr,
                    f(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2),
                      luaL_checkinteger(L, 3)));
}

// unix.setresuid(real:int, effective:int, saved:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixSetresuid(lua_State *L) {
  return LuaUnixSetresid(L, "setresuid", setresuid);
}

// unix.setresgid(real:int, effective:int, saved:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixSetresgid(lua_State *L) {
  return LuaUnixSetresid(L, "setresgid", setresgid);
}

// unix.utimensat(path[, asecs, ananos, msecs, mnanos[, dirfd[, flags]]])
//     ├─→ 0
//     └─→ nil, unix.Errno
static int LuaUnixUtimensat(lua_State *L) {
  struct timespec ts;
  int olderr = errno;
  return SysretInteger(
      L, "utimensat", olderr,
      utimensat(
          luaL_optinteger(L, 6, AT_FDCWD), luaL_checkstring(L, 1),
          (struct timespec[2]){
              {luaL_optinteger(L, 2, 0), luaL_optinteger(L, 3, UTIME_NOW)},
              {luaL_optinteger(L, 4, 0), luaL_optinteger(L, 5, UTIME_NOW)},
          },
          luaL_optinteger(L, 7, 0)));
}

// unix.futimens(fd:int[, asecs, ananos, msecs, mnanos])
//     ├─→ 0
//     └─→ nil, unix.Errno
static int LuaUnixFutimens(lua_State *L) {
  struct timespec ts;
  int olderr = errno;
  return SysretInteger(
      L, "futimens", olderr,
      futimens(luaL_checkinteger(L, 1),
               (struct timespec[2]){
                   {luaL_optinteger(L, 2, 0), luaL_optinteger(L, 3, UTIME_NOW)},
                   {luaL_optinteger(L, 4, 0), luaL_optinteger(L, 5, UTIME_NOW)},
               }));
}

// unix.clock_gettime([clock:int])
//     ├─→ seconds:int, nanos:int
//     └─→ nil, unix.Errno
static int LuaUnixGettime(lua_State *L) {
  struct timespec ts;
  int rc, olderr = errno;
  if (!clock_gettime(luaL_optinteger(L, 1, CLOCK_REALTIME), &ts)) {
    lua_pushinteger(L, ts.tv_sec);
    lua_pushinteger(L, ts.tv_nsec);
    return 2;
  } else {
    return SysretErrno(L, "clock_gettime", olderr);
  }
}

// unix.nanosleep(seconds:int, nanos:int)
//     ├─→ remseconds:int, remnanos:int
//     └─→ nil, unix.Errno
static int LuaUnixNanosleep(lua_State *L) {
  int olderr = errno;
  struct timespec req, rem;
  req.tv_sec = luaL_checkinteger(L, 1);
  req.tv_nsec = luaL_optinteger(L, 2, 0);
  if (!nanosleep(&req, &rem)) {
    lua_pushinteger(L, rem.tv_sec);
    lua_pushinteger(L, rem.tv_nsec);
    return 2;
  } else {
    return SysretErrno(L, "nanosleep", olderr);
  }
}

// unix.sync()
static int LuaUnixSync(lua_State *L) {
  sync();
  return 0;
}

// unix.fsync(fd:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixFsync(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "fsync", olderr, fsync(luaL_checkinteger(L, 1)));
}

// unix.fdatasync(fd:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixFdatasync(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "fdatasync", olderr, fdatasync(luaL_checkinteger(L, 1)));
}

// unix.open(path:str[, flags:int[, mode:int[, dirfd:int]]])
//     ├─→ fd:int
//     └─→ nil, unix.Errno
static int LuaUnixOpen(lua_State *L) {
  int olderr = errno;
  return SysretInteger(
      L, "open", olderr,
      openat(luaL_optinteger(L, 4, AT_FDCWD), luaL_checkstring(L, 1),
             luaL_optinteger(L, 2, O_RDONLY), luaL_optinteger(L, 3, 0)));
}

// unix.close(fd:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixClose(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "close", olderr, close(luaL_checkinteger(L, 1)));
}

// unix.lseek(fd:int, offset:int[, whence:int])
//     ├─→ newposbytes:int
//     └─→ nil, unix.Errno
static int LuaUnixLseek(lua_State *L) {
  int olderr = errno;
  return SysretInteger(L, "lseek", olderr,
                       lseek(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2),
                             luaL_optinteger(L, 3, SEEK_SET)));
}

// unix.truncate(path:str[, length:int])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixTruncate(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "truncate", olderr,
                    truncate(luaL_checkstring(L, 1), luaL_optinteger(L, 2, 0)));
}

// unix.ftruncate(fd:int[, length:int])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixFtruncate(lua_State *L) {
  int olderr = errno;
  return SysretBool(
      L, "ftruncate", olderr,
      ftruncate(luaL_checkinteger(L, 1), luaL_optinteger(L, 2, 0)));
}

// unix.read(fd:int[, bufsiz:str[, offset:int]])
//     ├─→ data:str
//     └─→ nil, unix.Errno
static int LuaUnixRead(lua_State *L) {
  char *buf;
  size_t got;
  ssize_t rc;
  int fd, olderr;
  lua_Integer bufsiz, offset;
  olderr = errno;
  fd = luaL_checkinteger(L, 1);
  bufsiz = luaL_optinteger(L, 2, BUFSIZ);
  offset = luaL_optinteger(L, 3, -1);
  bufsiz = MIN(bufsiz, 0x7ffff000);
  buf = LuaAllocOrDie(L, bufsiz);
  if (offset == -1) {
    rc = read(fd, buf, bufsiz);
  } else {
    rc = pread(fd, buf, bufsiz, offset);
  }
  if (rc != -1) {
    got = rc;
    lua_pushlstring(L, buf, got);
    free(buf);
    return 1;
  } else {
    free(buf);
    return SysretErrno(L, "read", olderr);
  }
}

// unix.write(fd:int, data:str[, offset:int])
//     ├─→ wrotebytes:int
//     └─→ nil, unix.Errno
static int LuaUnixWrite(lua_State *L) {
  ssize_t rc;
  size_t size;
  int fd, olderr;
  const char *data;
  lua_Integer offset;
  olderr = errno;
  fd = luaL_checkinteger(L, 1);
  data = luaL_checklstring(L, 2, &size);
  offset = luaL_optinteger(L, 3, -1);
  if (offset == -1) {
    rc = write(fd, data, size);
  } else {
    rc = pwrite(fd, data, size, offset);
  }
  return SysretInteger(L, "write", olderr, rc);
}

// unix.stat(path:str[, flags:int[, dirfd:int]])
//     ├─→ unix.Stat
//     └─→ nil, unix.Errno
static int LuaUnixStat(lua_State *L) {
  struct stat st;
  int olderr = errno;
  if (!fstatat(luaL_optinteger(L, 3, AT_FDCWD), luaL_checkstring(L, 1), &st,
               luaL_optinteger(L, 2, 0))) {
    LuaPushStat(L, &st);
    return 1;
  } else {
    return SysretErrno(L, "stat", olderr);
  }
}

// unix.fstat(fd:int)
//     ├─→ unix.Stat
//     └─→ nil, unix.Errno
static int LuaUnixFstat(lua_State *L) {
  struct stat st;
  int olderr = errno;
  if (!fstat(luaL_checkinteger(L, 1), &st)) {
    LuaPushStat(L, &st);
    return 1;
  } else {
    return SysretErrno(L, "fstat", olderr);
  }
}

static bool IsSockoptBool(int l, int x) {
  if (l == SOL_SOCKET) {
    return x == SO_TYPE ||        //
           x == SO_DEBUG ||       //
           x == SO_ERROR ||       //
           x == SO_BROADCAST ||   //
           x == SO_REUSEADDR ||   //
           x == SO_REUSEPORT ||   //
           x == SO_KEEPALIVE ||   //
           x == SO_ACCEPTCONN ||  //
           x == SO_DONTROUTE;     //
  } else if (l = SOL_TCP) {
    return x == TCP_NODELAY ||           //
           x == TCP_CORK ||              //
           x == TCP_QUICKACK ||          //
           x == TCP_FASTOPEN_CONNECT ||  //
           x == TCP_DEFER_ACCEPT;        //
  } else if (l = SOL_IP) {
    return x == IP_HDRINCL;  //
  } else {
    return false;
  }
}

static bool IsSockoptInt(int l, int x) {
  if (l == SOL_SOCKET) {
    return x == SO_SNDBUF ||    //
           x == SO_RCVBUF ||    //
           x == SO_RCVLOWAT ||  //
           x == SO_SNDLOWAT;    //
  } else if (l = SOL_TCP) {
    return x == TCP_FASTOPEN ||       //
           x == TCP_KEEPCNT ||        //
           x == TCP_MAXSEG ||         //
           x == TCP_SYNCNT ||         //
           x == TCP_NOTSENT_LOWAT ||  //
           x == TCP_WINDOW_CLAMP ||   //
           x == TCP_KEEPIDLE ||       //
           x == TCP_KEEPINTVL;        //
  } else if (l = SOL_IP) {
    return x == IP_TOS ||  //
           x == IP_MTU ||  //
           x == IP_TTL;    //
  } else {
    return false;
  }
}

static bool IsSockoptTimeval(int l, int x) {
  if (l == SOL_SOCKET) {
    return x == SO_RCVTIMEO ||  //
           x == SO_SNDTIMEO;    //
  } else {
    return false;
  }
}

static int LuaUnixSetsockopt(lua_State *L) {
  void *optval;
  struct linger l;
  uint32_t optsize;
  struct timeval tv;
  int rc, fd, level, optname, optint, olderr = errno;
  fd = luaL_checkinteger(L, 1);
  level = luaL_checkinteger(L, 2);
  optname = luaL_checkinteger(L, 3);
  if (IsSockoptBool(level, optname)) {
    // unix.setsockopt(fd:int, level:int, optname:int, value:bool)
    //     ├─→ true
    //     └─→ nil, unix.Errno
    optint = lua_toboolean(L, 4);
    optval = &optint;
    optsize = sizeof(optint);
  } else if (IsSockoptInt(level, optname)) {
    // unix.setsockopt(fd:int, level:int, optname:int, value:int)
    //     ├─→ true
    //     └─→ nil, unix.Errno
    optint = luaL_checkinteger(L, 4);
    optval = &optint;
    optsize = sizeof(optint);
  } else if (IsSockoptTimeval(level, optname)) {
    // unix.setsockopt(fd:int, level:int, optname:int, secs:int[, nanos:int])
    //     ├─→ true
    //     └─→ nil, unix.Errno
    tv.tv_sec = luaL_checkinteger(L, 4);
    tv.tv_usec = luaL_optinteger(L, 5, 0) / 1000;
    optval = &tv;
    optsize = sizeof(tv);
  } else if (level == SOL_SOCKET && optname == SO_LINGER) {
    // unix.setsockopt(fd:int, level:int, optname:int, secs:int, enabled:bool)
    //     ├─→ true
    //     └─→ nil, unix.Errno
    l.l_linger = luaL_checkinteger(L, 4);
    l.l_onoff = lua_toboolean(L, 5);
    optval = &l;
    optsize = sizeof(l);
  } else {
    einval();
    return SysretErrno(L, "setsockopt", olderr);
  }
  return SysretBool(L, "setsockopt", olderr,
                    setsockopt(fd, level, optname, optval, optsize));
}

static int LuaUnixGetsockopt(lua_State *L) {
  uint32_t size;
  struct linger l;
  struct timeval tv;
  int rc, fd, level, optname, optval, olderr = errno;
  fd = luaL_checkinteger(L, 1);
  level = luaL_checkinteger(L, 2);
  optname = luaL_checkinteger(L, 3);
  if (IsSockoptBool(level, optname) || IsSockoptInt(level, optname)) {
    // unix.getsockopt(fd:int, level:int, optname:int)
    //     ├─→ value:int
    //     └─→ nil, unix.Errno
    size = sizeof(optval);
    if (getsockopt(fd, level, optname, &optval, &size) != -1) {
      CheckOptvalsize(L, sizeof(optval), size);
      lua_pushinteger(L, optval);
      return 1;
    }
  } else if (IsSockoptTimeval(level, optname)) {
    // unix.getsockopt(fd:int, level:int, optname:int)
    //     ├─→ secs:int, nsecs:int
    //     └─→ nil, unix.Errno
    size = sizeof(tv);
    if (getsockopt(fd, level, optname, &tv, &size) != -1) {
      CheckOptvalsize(L, sizeof(tv), size);
      lua_pushinteger(L, tv.tv_sec);
      lua_pushinteger(L, tv.tv_usec * 1000);
      return 2;
    }
  } else if (level == SOL_SOCKET && optname == SO_LINGER) {
    // unix.getsockopt(fd:int, unix.SOL_SOCKET, unix.SO_LINGER)
    //     ├─→ seconds:int, enabled:bool
    //     └─→ nil, unix.Errno
    size = sizeof(l);
    if (getsockopt(fd, level, optname, &l, &size) != -1) {
      CheckOptvalsize(L, sizeof(l), size);
      lua_pushinteger(L, l.l_linger);
      lua_pushboolean(L, !!l.l_onoff);
      return 1;
    }
  } else {
    einval();
  }
  return SysretErrno(L, "getsockopt", olderr);
}

// unix.socket([family:int[, type:int[, protocol:int]]])
//     ├─→ fd:int
//     └─→ nil, unix.Errno
static int LuaUnixSocket(lua_State *L) {
  int olderr = errno;
  int family = luaL_optinteger(L, 1, AF_INET);
  return SysretInteger(
      L, "socket", olderr,
      socket(family, luaL_optinteger(L, 2, SOCK_STREAM),
             luaL_optinteger(L, 3, family == AF_INET ? IPPROTO_TCP : 0)));
}

// unix.socketpair([family:int[, type:int[, protocol:int]]])
//     ├─→ fd1:int, fd2:int
//     └─→ nil, unix.Errno
static int LuaUnixSocketpair(lua_State *L) {
  int sv[2], olderr = errno;
  if (!socketpair(luaL_optinteger(L, 1, AF_UNIX),
                  luaL_optinteger(L, 2, SOCK_STREAM), luaL_optinteger(L, 3, 0),
                  sv)) {
    lua_pushinteger(L, sv[0]);
    lua_pushinteger(L, sv[1]);
    return 2;
  } else {
    return SysretErrno(L, "socketpair", olderr);
  }
}

// unix.bind(fd:int[, ip:uint32, port:uint16])
// unix.bind(fd:int[, unixpath:str])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixBind(lua_State *L) {
  uint32_t salen;
  struct sockaddr_storage ss;
  int olderr = errno;
  MakeSockaddr(L, 2, &ss, &salen);
  return SysretBool(L, "bind", olderr,
                    bind(luaL_checkinteger(L, 1), &ss, salen));
}

// unix.connect(fd:int, ip:uint32, port:uint16)
// unix.connect(fd:int, unixpath:str)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixConnect(lua_State *L) {
  uint32_t salen;
  struct sockaddr_storage ss;
  int olderr = errno;
  MakeSockaddr(L, 2, &ss, &salen);
  return SysretBool(L, "connect", olderr,
                    connect(luaL_checkinteger(L, 1), &ss, salen));
}

// unix.listen(fd:int[, backlog:int])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixListen(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "listen", olderr,
                    listen(luaL_checkinteger(L, 1), luaL_optinteger(L, 2, 10)));
}

static int LuaUnixGetname(lua_State *L, const char *name,
                          int func(int, void *, uint32_t *)) {
  int olderr;
  uint32_t addrsize;
  struct sockaddr_storage ss = {0};
  olderr = errno;
  addrsize = sizeof(ss) - 1;
  if (!func(luaL_checkinteger(L, 1), &ss, &addrsize)) {
    return PushSockaddr(L, &ss);
  } else {
    return SysretErrno(L, name, olderr);
  }
}

// unix.getsockname(fd:int)
//     ├─→ ip:uint32, port:uint16
//     ├─→ unixpath:str
//     └─→ nil, unix.Errno
static int LuaUnixGetsockname(lua_State *L) {
  return LuaUnixGetname(L, "getsockname", getsockname);
}

// unix.getpeername(fd:int)
//     ├─→ ip:uint32, port:uint16
//     ├─→ unixpath:str
//     └─→ nil, unix.Errno
static int LuaUnixGetpeername(lua_State *L) {
  return LuaUnixGetname(L, "getpeername", getpeername);
}

// unix.siocgifconf()
//     ├─→ {{name:str,ip:uint32,netmask:uint32}, ...}
//     └─→ nil, unix.Errno
static int LuaUnixSiocgifconf(lua_State *L) {
  size_t n;
  char *data;
  int i, fd, olderr;
  struct ifreq *ifr;
  struct ifconf conf;
  olderr = errno;
  data = LuaAllocOrDie(L, (n = 4096));
  if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
    free(data);
    return SysretErrno(L, "siocgifconf", olderr);
  }
  conf.ifc_buf = data;
  conf.ifc_len = n;
  if (ioctl(fd, SIOCGIFCONF, &conf) == -1) {
    close(fd);
    free(data);
    return SysretErrno(L, "siocgifconf", olderr);
  }
  lua_newtable(L);
  i = 0;
  for (ifr = (struct ifreq *)data; (char *)ifr < data + conf.ifc_len; ++ifr) {
    if (ifr->ifr_addr.sa_family != AF_INET) continue;
    lua_createtable(L, 0, 3);
    lua_pushliteral(L, "name");
    lua_pushstring(L, ifr->ifr_name);
    lua_settable(L, -3);
    lua_pushliteral(L, "ip");
    lua_pushinteger(
        L, ntohl(((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr.s_addr));
    lua_settable(L, -3);
    if (ioctl(fd, SIOCGIFNETMASK, ifr) != -1) {
      lua_pushliteral(L, "netmask");
      lua_pushinteger(
          L, ntohl(((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr.s_addr));
      lua_settable(L, -3);
    }
    lua_rawseti(L, -2, ++i);
  }
  close(fd);
  free(data);
  return 1;
}

// sandbox.pledge([promises:str])
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixPledge(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "pledge", olderr, pledge(luaL_checkstring(L, 1), 0));
}

// unix.gethostname()
//     ├─→ host:str
//     └─→ nil, unix.Errno
static int LuaUnixGethostname(lua_State *L) {
  int rc, olderr;
  char buf[DNS_NAME_MAX + 1];
  olderr = errno;
  if ((rc = gethostname(buf, sizeof(buf))) != -1) {
    if (strnlen(buf, sizeof(buf)) < sizeof(buf)) {
      lua_pushstring(L, buf);
      return 1;
    } else {
      enomem();
    }
  }
  return SysretErrno(L, "gethostname", olderr);
}

// unix.accept(serverfd:int[, flags:int])
//     ├─→ clientfd:int, ip:uint32, port:uint16
//     ├─→ clientfd:int, unixpath:str
//     └─→ nil, unix.Errno
static int LuaUnixAccept(lua_State *L) {
  uint32_t addrsize;
  struct sockaddr_storage ss;
  int clientfd, serverfd, olderr, flags;
  olderr = errno;
  addrsize = sizeof(ss);
  serverfd = luaL_checkinteger(L, 1);
  flags = luaL_optinteger(L, 2, 0);
  clientfd = accept4(serverfd, &ss, &addrsize, flags);
  if (clientfd != -1) {
    lua_pushinteger(L, clientfd);
    return 1 + PushSockaddr(L, &ss);
  } else {
    return SysretErrno(L, "accept", olderr);
  }
}

// unix.poll({[fd:int]=events:int, ...}[, timeoutms:int])
//     ├─→ {[fd:int]=revents:int, ...}
//     └─→ nil, unix.Errno
static int LuaUnixPoll(lua_State *L) {
  size_t nfds;
  struct pollfd *fds, *fds2;
  int i, fd, events, timeoutms, olderr = errno;
  timeoutms = luaL_optinteger(L, 2, -1);
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushnil(L);
  for (fds = 0, nfds = 0; lua_next(L, 1);) {
    if (lua_isinteger(L, -2)) {
      if ((fds2 = LuaRealloc(L, fds, (nfds + 1) * sizeof(*fds)))) {
        fds2[nfds].fd = lua_tointeger(L, -2);
        fds2[nfds].events = lua_tointeger(L, -1);
        fds = fds2;
        ++nfds;
      } else {
        free(fds);
        return SysretErrno(L, "poll", olderr);
      }
    } else {
      // ignore non-integer key
    }
    lua_pop(L, 1);
  }
  olderr = errno;
  if ((events = poll(fds, nfds, timeoutms)) != -1) {
    lua_createtable(L, events, 0);
    for (i = 0; i < nfds; ++i) {
      if (fds[i].revents && fds[i].fd >= 0) {
        lua_pushinteger(L, fds[i].revents);
        lua_rawseti(L, -2, fds[i].fd);
      }
    }
    free(fds);
    return 1;
  } else {
    free(fds);
    return SysretErrno(L, "poll", olderr);
  }
}

// unix.recvfrom(fd:int[, bufsiz:int[, flags:int]])
//     ├─→ data:str, ip:uint32, port:uint16
//     ├─→ data:str, unixpath:str
//     └─→ nil, unix.Errno
static int LuaUnixRecvfrom(lua_State *L) {
  char *buf;
  size_t got;
  ssize_t rc;
  uint32_t addrsize;
  lua_Integer bufsiz;
  struct sockaddr_storage ss;
  int fd, flags, pushed, olderr = errno;
  addrsize = sizeof(ss);
  fd = luaL_checkinteger(L, 1);
  bufsiz = luaL_optinteger(L, 2, 1500);
  bufsiz = MIN(bufsiz, 0x7ffff000);
  flags = luaL_optinteger(L, 3, 0);
  buf = LuaAllocOrDie(L, bufsiz);
  if ((rc = recvfrom(fd, buf, bufsiz, flags, &ss, &addrsize)) != -1) {
    got = rc;
    lua_pushlstring(L, buf, got);
    pushed = 1 + PushSockaddr(L, &ss);
    free(buf);
    return pushed;
  } else {
    free(buf);
    return SysretErrno(L, "recvfrom", olderr);
  }
}

// unix.recv(fd:int[, bufsiz:int[, flags:int]])
//     ├─→ data:str
//     └─→ nil, unix.Errno
static int LuaUnixRecv(lua_State *L) {
  char *buf;
  size_t got;
  ssize_t rc;
  lua_Integer bufsiz;
  int fd, flags, pushed, olderr = errno;
  fd = luaL_checkinteger(L, 1);
  bufsiz = luaL_optinteger(L, 2, 1500);
  bufsiz = MIN(bufsiz, 0x7ffff000);
  flags = luaL_optinteger(L, 3, 0);
  buf = LuaAllocOrDie(L, bufsiz);
  rc = recv(fd, buf, bufsiz, flags);
  if (rc != -1) {
    got = rc;
    lua_pushlstring(L, buf, got);
    free(buf);
    return 1;
  } else {
    free(buf);
    return SysretErrno(L, "recv", olderr);
  }
}

// unix.send(fd:int, data:str[, flags:int])
//     ├─→ sent:int
//     └─→ nil, unix.Errno
static int LuaUnixSend(lua_State *L) {
  char *data;
  ssize_t rc;
  size_t sent, size;
  int fd, flags, olderr = errno;
  fd = luaL_checkinteger(L, 1);
  data = luaL_checklstring(L, 2, &size);
  flags = luaL_optinteger(L, 3, 0);
  return SysretInteger(L, "send", olderr, send(fd, data, size, flags));
}

// unix.sendto(fd:int, data:str, ip:uint32, port:uint16[, flags:int])
// unix.sendto(fd:int, data:str, unixpath:str[, flags:int])
//     ├─→ sent:int
//     └─→ nil, unix.Errno
static int LuaUnixSendto(lua_State *L) {
  char *data;
  size_t size;
  uint32_t salen;
  struct sockaddr_storage ss;
  int i, fd, flags, olderr = errno;
  fd = luaL_checkinteger(L, 1);
  data = luaL_checklstring(L, 2, &size);
  i = MakeSockaddr(L, 3, &ss, &salen);
  flags = luaL_optinteger(L, i, 0);
  return SysretInteger(L, "sendto", olderr,
                       sendto(fd, data, size, flags, &ss, salen));
}

// unix.shutdown(fd:int, how:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixShutdown(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "shutdown", olderr,
                    shutdown(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2)));
}

// unix.sigprocmask(how:int, newmask:unix.Sigset)
//     ├─→ oldmask:unix.Sigset
//     └─→ nil, unix.Errno
static int LuaUnixSigprocmask(lua_State *L) {
  uint64_t imask;
  int olderr = errno;
  struct sigset oldmask;
  if (!sigprocmask(luaL_checkinteger(L, 1),
                   luaL_checkudata(L, 2, "unix.Sigset"), &oldmask)) {
    LuaPushSigset(L, oldmask);
    return 1;
  } else {
    return SysretErrno(L, "sigprocmask", olderr);
  }
}

static void LuaUnixOnSignal(int sig, siginfo_t *si, ucontext_t *ctx) {
  int type;
  lua_State *L = GL;
  STRACE("LuaUnixOnSignal(%G)", sig);
  lua_getglobal(L, "__signal_handlers");
  type = lua_rawgeti(L, -1, sig);
  lua_remove(L, -2);  // pop __signal_handlers
  if (type == LUA_TFUNCTION) {
    lua_pushinteger(L, sig);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
      ERRORF("(lua) %s failed: %s", strsignal(sig), lua_tostring(L, -1));
      lua_pop(L, 1);  // pop error
    }
  } else {
    lua_pop(L, 1);  // pop handler
  }
}

// unix.sigaction(sig:int[, handler:func|int[, flags:int[, mask:unix.Sigset]]])
//     ├─→ oldhandler:func|int, flags:int, mask:unix.Sigset
//     └─→ nil, unix.Errno
static int LuaUnixSigaction(lua_State *L) {
  struct sigset *mask;
  int i, n, sig, olderr = errno;
  struct sigaction sa, oldsa, *saptr = &sa;
  sigemptyset(&sa.sa_mask);
  sig = luaL_checkinteger(L, 1);
  if (!(1 <= sig && sig <= NSIG)) {
    luaL_argerror(L, 1, "signal number invalid");
    unreachable;
  }
  if (lua_isnoneornil(L, 2)) {
    // if handler/flags/mask aren't passed,
    // then we're quering the current state
    saptr = 0;
  } else if (lua_isinteger(L, 2)) {
    // bypass handling signals using lua code if possible
    sa.sa_sigaction = (void *)luaL_checkinteger(L, 2);
  } else if (lua_isfunction(L, 2)) {
    sa.sa_sigaction = LuaUnixOnSignal;
    // lua probably isn't async signal safe, so...
    // let's mask all the lua handlers during handling
    sigaddset(&sa.sa_mask, sig);
    lua_getglobal(L, "__signal_handlers");
    luaL_checktype(L, -1, LUA_TTABLE);
    lua_len(L, -1);
    n = lua_tointeger(L, -1);
    lua_pop(L, 1);
    for (i = 1; i <= MIN(n, NSIG); ++i) {
      if (lua_geti(L, -1, i) == LUA_TFUNCTION) {
        sigaddset(&sa.sa_mask, i);
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  } else {
    luaL_argerror(L, 2, "sigaction handler not integer or function");
    unreachable;
  }
  sa.sa_flags = luaL_optinteger(L, 3, 0);
  if (!lua_isnoneornil(L, 4)) {
    mask = luaL_checkudata(L, 4, "unix.Sigset");
    sa.sa_mask.__bits[0] |= mask->__bits[0];
    sa.sa_mask.__bits[1] |= mask->__bits[1];
  }
  if (!sigaction(sig, saptr, &oldsa)) {
    lua_getglobal(L, "__signal_handlers");
    // push the old handler result to stack. if the global lua handler
    // table has a real function, then we prefer to return that. if it's
    // absent or a raw integer value, then we're better off returning
    // what the kernel gave us in &oldsa.
    if (lua_rawgeti(L, -1, sig) != LUA_TFUNCTION) {
      lua_pop(L, 1);
      lua_pushinteger(L, (intptr_t)oldsa.sa_handler);
    }
    if (saptr) {
      // update the global lua table
      if (sa.sa_sigaction == LuaUnixOnSignal) {
        lua_pushvalue(L, -3);
      } else {
        lua_pushnil(L);
      }
      lua_rawseti(L, -3, sig);
    }
    // remove the signal handler table from stack
    lua_remove(L, -2);
    // finish pushing the last 2/3 results
    lua_pushinteger(L, oldsa.sa_flags);
    LuaPushSigset(L, oldsa.sa_mask);
    return 3;
  } else {
    return SysretErrno(L, "sigaction", olderr);
  }
}

// unix.sigsuspend([mask:Sigmask])
//     └─→ nil, unix.Errno
static int LuaUnixSigsuspend(lua_State *L) {
  int olderr = errno;
  sigsuspend(!lua_isnoneornil(L, 1) ? luaL_checkudata(L, 1, "unix.Sigset") : 0);
  return SysretErrno(L, "sigsuspend", olderr);
}

// unix.setitimer(which[, intervalsec, intns, valuesec, valuens])
//     ├─→ intervalsec:int, intervalns:int, valuesec:int, valuens:int
//     └─→ nil, unix.Errno
static int LuaUnixSetitimer(lua_State *L) {
  int which, olderr = errno;
  struct itimerval it, oldit, *itptr;
  which = luaL_checkinteger(L, 1);
  if (!lua_isnoneornil(L, 2)) {
    itptr = &it;
    it.it_interval.tv_sec = luaL_optinteger(L, 2, 0);
    it.it_interval.tv_usec = luaL_optinteger(L, 3, 0) / 1000;
    it.it_value.tv_sec = luaL_optinteger(L, 4, 0);
    it.it_value.tv_usec = luaL_optinteger(L, 5, 0) / 1000;
  } else {
    itptr = 0;
  }
  if (!setitimer(which, itptr, &oldit)) {
    lua_pushinteger(L, oldit.it_interval.tv_sec);
    lua_pushinteger(L, oldit.it_interval.tv_usec * 1000);
    lua_pushinteger(L, oldit.it_value.tv_sec);
    lua_pushinteger(L, oldit.it_value.tv_usec * 1000);
    return 4;
  } else {
    return SysretErrno(L, "setitimer", olderr);
  }
}

static int LuaUnixStr(lua_State *L, char *f(int)) {
  return ReturnString(L, f(luaL_checkinteger(L, 1)));
}

// unix.strsignal(sig:int)
//     └─→ symbol:str
static int LuaUnixStrsignal(lua_State *L) {
  return LuaUnixStr(L, strsignal);
}

// unix.WIFEXITED(wstatus)
//     └─→ bool
static int LuaUnixWifexited(lua_State *L) {
  return ReturnBoolean(L, WIFEXITED(luaL_checkinteger(L, 1)));
}

// unix.WEXITSTATUS(wstatus)
//     └─→ exitcode:uint8
static int LuaUnixWexitstatus(lua_State *L) {
  return ReturnInteger(L, WEXITSTATUS(luaL_checkinteger(L, 1)));
}

// unix.WIFSIGNALED(wstatus)
//     └─→ bool
static int LuaUnixWifsignaled(lua_State *L) {
  return ReturnBoolean(L, WIFSIGNALED(luaL_checkinteger(L, 1)));
}

// unix.WTERMSIG(wstatus)
//     └─→ sig:uint8
static int LuaUnixWtermsig(lua_State *L) {
  return ReturnInteger(L, WTERMSIG(luaL_checkinteger(L, 1)));
}

static dontinline int LuaUnixTime(lua_State *L, const char *call,
                                  struct tm *f(const time_t *, struct tm *)) {
  int64_t ts;
  struct tm tm;
  int olderr = errno;
  ts = luaL_checkinteger(L, 1);
  if (f(&ts, &tm)) {
    lua_pushinteger(L, tm.tm_year + 1900);
    lua_pushinteger(L, tm.tm_mon + 1);  // 1 ≤ mon  ≤ 12
    lua_pushinteger(L, tm.tm_mday);     // 1 ≤ mday ≤ 31
    lua_pushinteger(L, tm.tm_hour);     // 0 ≤ hour ≤ 23
    lua_pushinteger(L, tm.tm_min);      // 0 ≤ min  ≤ 59
    lua_pushinteger(L, tm.tm_sec);      // 0 ≤ sec  ≤ 60
    lua_pushinteger(L, tm.tm_gmtoff);   // ±93600 seconds
    lua_pushinteger(L, tm.tm_wday);     // 0 ≤ wday ≤ 6
    lua_pushinteger(L, tm.tm_yday);     // 0 ≤ yday ≤ 365
    lua_pushinteger(L, tm.tm_isdst);    // daylight savings
    lua_pushstring(L, tm.tm_zone);
    return 11;
  } else {
    return SysretErrno(L, call, olderr);
  }
}

// unix.gmtime(unixsecs:int)
//     ├─→ year,mon,mday,hour,min,sec,gmtoffsec,wday,yday,dst:int,zone:str
//     └─→ nil,unix.Errno
static int LuaUnixGmtime(lua_State *L) {
  return LuaUnixTime(L, "gmtime", gmtime_r);
}

// unix.localtime(unixts:int)
//     ├─→ year,mon,mday,hour,min,sec,gmtoffsec,wday,yday,dst:int,zone:str
//     └─→ nil,unix.Errno
static int LuaUnixLocaltime(lua_State *L) {
  return LuaUnixTime(L, "localtime", localtime_r);
}

// unix.major(rdev:int)
//     └─→ major:int
static int LuaUnixMajor(lua_State *L) {
  return ReturnInteger(L, major(luaL_checkinteger(L, 1)));
}

// unix.minor(rdev:int)
//     └─→ minor:int
static int LuaUnixMinor(lua_State *L) {
  return ReturnInteger(L, minor(luaL_checkinteger(L, 1)));
}

// unix.S_ISDIR(mode:int)
//     └─→ bool
static int LuaUnixSisdir(lua_State *L) {
  lua_pushboolean(L, S_ISDIR(luaL_checkinteger(L, 1)));
  return 1;
}

// unix.S_ISCHR(mode:int)
//     └─→ bool
static int LuaUnixSischr(lua_State *L) {
  lua_pushboolean(L, S_ISCHR(luaL_checkinteger(L, 1)));
  return 1;
}

// unix.S_ISBLK(mode:int)
//     └─→ bool
static int LuaUnixSisblk(lua_State *L) {
  lua_pushboolean(L, S_ISBLK(luaL_checkinteger(L, 1)));
  return 1;
}

// unix.S_ISREG(mode:int)
//     └─→ bool
static int LuaUnixSisreg(lua_State *L) {
  lua_pushboolean(L, S_ISREG(luaL_checkinteger(L, 1)));
  return 1;
}

// unix.S_ISFIFO(mode:int)
//     └─→ bool
static int LuaUnixSisfifo(lua_State *L) {
  lua_pushboolean(L, S_ISFIFO(luaL_checkinteger(L, 1)));
  return 1;
}

// unix.S_ISLNK(mode:int)
//     └─→ bool
static int LuaUnixSislnk(lua_State *L) {
  lua_pushboolean(L, S_ISLNK(luaL_checkinteger(L, 1)));
  return 1;
}

// unix.S_ISSOCK(mode:int)
//     └─→ bool
static int LuaUnixSissock(lua_State *L) {
  lua_pushboolean(L, S_ISSOCK(luaL_checkinteger(L, 1)));
  return 1;
}

// unix.isatty(fd:int)
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixIsatty(lua_State *L) {
  int olderr = errno;
  return SysretBool(L, "isatty", olderr, isatty(luaL_checkinteger(L, 1)));
}

// unix.tiocgwinsz(fd:int)
//     ├─→ rows:int, cols:int
//     └─→ nil, unix.Errno
static int LuaUnixTiocgwinsz(lua_State *L) {
  struct winsize ws;
  int olderr = errno;
  if (!ioctl(luaL_checkinteger(L, 1), TIOCGWINSZ, &ws)) {
    lua_pushinteger(L, ws.ws_row);
    lua_pushinteger(L, ws.ws_col);
    return 2;
  } else {
    return SysretErrno(L, "tiocgwinsz", olderr);
  }
}

////////////////////////////////////////////////////////////////////////////////
// unix.Stat object

static struct stat *GetUnixStat(lua_State *L) {
  return luaL_checkudata(L, 1, "unix.Stat");
}

// unix.Stat:size()
//     └─→ bytes:int
static int LuaUnixStatSize(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_size);
}

// unix.Stat:mode()
//     └─→ mode:int
static int LuaUnixStatMode(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_mode);
}

// unix.Stat:dev()
//     └─→ dev:int
static int LuaUnixStatDev(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_dev);
}

// unix.Stat:ino()
//     └─→ inodeint
static int LuaUnixStatIno(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_ino);
}

// unix.Stat:nlink()
//     └─→ count:int
static int LuaUnixStatNlink(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_nlink);
}

// unix.Stat:rdev()
//     └─→ rdev:int
static int LuaUnixStatRdev(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_rdev);
}

// unix.Stat:uid()
//     └─→ uid:int
static int LuaUnixStatUid(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_uid);
}

// unix.Stat:gid()
//     └─→ gid:int
static int LuaUnixStatGid(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_gid);
}

// unix.Stat:blocks()
//     └─→ count:int
static int LuaUnixStatBlocks(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_blocks);
}

// unix.Stat:blksize()
//     └─→ bytes:int
static int LuaUnixStatBlksize(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_blksize);
}

static dontinline int ReturnTimespec(lua_State *L, struct timespec *ts) {
  lua_pushinteger(L, ts->tv_sec);
  lua_pushinteger(L, ts->tv_nsec);
  return 2;
}

// unix.Stat:atim()
//     └─→ unixts:int, nanos:int
static int LuaUnixStatAtim(lua_State *L) {
  return ReturnTimespec(L, &GetUnixStat(L)->st_atim);
}

// unix.Stat:mtim()
//     └─→ unixts:int, nanos:int
static int LuaUnixStatMtim(lua_State *L) {
  return ReturnTimespec(L, &GetUnixStat(L)->st_mtim);
}

// unix.Stat:ctim()
//     └─→ unixts:int, nanos:int
static int LuaUnixStatCtim(lua_State *L) {
  return ReturnTimespec(L, &GetUnixStat(L)->st_ctim);
}

// unix.Stat:birthtim()
//     └─→ unixts:int, nanos:int
static int LuaUnixStatBirthtim(lua_State *L) {
  return ReturnTimespec(L, &GetUnixStat(L)->st_birthtim);
}

// unix.Stat:gen()
//     └─→ gen:int [xnu/bsd]
static int LuaUnixStatGen(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_gen);
}

// unix.Stat:flags()
//     └─→ flags:int [xnu/bsd]
static int LuaUnixStatFlags(lua_State *L) {
  return ReturnInteger(L, GetUnixStat(L)->st_flags);
}

static int LuaUnixStatToString(lua_State *L) {
  struct stat *st = GetUnixStat(L);
  lua_pushstring(L, "unix.Stat()");
  return 1;
}

static const luaL_Reg kLuaUnixStatMeth[] = {
    {"atim", LuaUnixStatAtim},          //
    {"birthtim", LuaUnixStatBirthtim},  //
    {"blksize", LuaUnixStatBlksize},    //
    {"blocks", LuaUnixStatBlocks},      //
    {"ctim", LuaUnixStatCtim},          //
    {"dev", LuaUnixStatDev},            //
    {"gid", LuaUnixStatGid},            //
    {"ino", LuaUnixStatIno},            //
    {"mode", LuaUnixStatMode},          //
    {"mtim", LuaUnixStatMtim},          //
    {"nlink", LuaUnixStatNlink},        //
    {"rdev", LuaUnixStatRdev},          //
    {"size", LuaUnixStatSize},          //
    {"uid", LuaUnixStatUid},            //
    {"flags", LuaUnixStatFlags},        //
    {"gen", LuaUnixStatGen},            //
    {0},                                //
};

static const luaL_Reg kLuaUnixStatMeta[] = {
    {"__tostring", LuaUnixStatToString},  //
    {0},                                  //
};

static void LuaUnixStatObj(lua_State *L) {
  luaL_newmetatable(L, "unix.Stat");
  luaL_setfuncs(L, kLuaUnixStatMeta, 0);
  luaL_newlibtable(L, kLuaUnixStatMeth);
  luaL_setfuncs(L, kLuaUnixStatMeth, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

////////////////////////////////////////////////////////////////////////////////
// unix.Rusage object

static struct rusage *GetUnixRusage(lua_State *L) {
  return luaL_checkudata(L, 1, "unix.Rusage");
}

static dontinline int ReturnTimeval(lua_State *L, struct timeval *tv) {
  lua_pushinteger(L, tv->tv_sec);
  lua_pushinteger(L, tv->tv_usec * 1000);
  return 2;
}

// unix.Rusage:utime()
//     └─→ unixts:int, nanos:int
static int LuaUnixRusageUtime(lua_State *L) {
  return ReturnTimeval(L, &GetUnixRusage(L)->ru_utime);
}

// unix.Rusage:stime()
//     └─→ unixts:int, nanos:int
static int LuaUnixRusageStime(lua_State *L) {
  return ReturnTimeval(L, &GetUnixRusage(L)->ru_stime);
}

// unix.Rusage:maxrss()
//     └─→ kilobytes:int
static int LuaUnixRusageMaxrss(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_maxrss);
}

// unix.Rusage:ixrss()
//     └─→ integralkilobytes:int
static int LuaUnixRusageIxrss(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_ixrss);
}

// unid.Rusage:idrss()
//     └─→ integralkilobytes:int
static int LuaUnixRusageIdrss(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_idrss);
}

// unis.Rusage:isrss()
//     └─→ integralkilobytes:int
static int LuaUnixRusageIsrss(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_isrss);
}

// unix.Rusage:minflt()
//     └─→ count:int
static int LuaUnixRusageMinflt(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_minflt);
}

// unix.Rusage:majflt()
//     └─→ count:int
static int LuaUnixRusageMajflt(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_majflt);
}

// unix.Rusage:nswap()
//     └─→ count:int
static int LuaUnixRusageNswap(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_nswap);
}

// unix.Rusage:inblock()
//     └─→ count:int
static int LuaUnixRusageInblock(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_inblock);
}

// unix.Rusage:oublock()
//     └─→ count:int
static int LuaUnixRusageOublock(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_oublock);
}

// unix.Rusage:msgsnd()
//     └─→ count:int
static int LuaUnixRusageMsgsnd(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_msgsnd);
}

// unix.Rusage:msgrcv()
//     └─→ count:int
static int LuaUnixRusageMsgrcv(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_msgrcv);
}

// unix.Rusage:nsignals()
//     └─→ count:int
static int LuaUnixRusageNsignals(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_nsignals);
}

// unix.Rusage:nvcsw()
//     └─→ count:int
static int LuaUnixRusageNvcsw(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_nvcsw);
}

// unix.Rusage:nivcsw()
//     └─→ count:int
static int LuaUnixRusageNivcsw(lua_State *L) {
  return ReturnInteger(L, GetUnixRusage(L)->ru_nivcsw);
}

static int LuaUnixRusageToString(lua_State *L) {
  char *b = 0;
  struct rusage *ru = GetUnixRusage(L);
  appends(&b, "{");
  appendf(&b, "%s={%ld, %ld}", "utime", ru->ru_utime.tv_sec,
          ru->ru_utime.tv_usec * 1000);
  if (ru->ru_stime.tv_sec || ru->ru_stime.tv_usec) {
    appendw(&b, READ16LE(", "));
    appendf(&b, "%s={%ld, %ld}", "stime", ru->ru_stime.tv_sec,
            ru->ru_stime.tv_usec * 1000);
  }
  if (ru->ru_maxrss) appendf(&b, ", %s=%ld", "maxrss", ru->ru_maxrss);
  if (ru->ru_ixrss) appendf(&b, ", %s=%ld", "ixrss", ru->ru_ixrss);
  if (ru->ru_idrss) appendf(&b, ", %s=%ld", "idrss", ru->ru_idrss);
  if (ru->ru_isrss) appendf(&b, ", %s=%ld", "isrss", ru->ru_isrss);
  if (ru->ru_minflt) appendf(&b, ", %s=%ld", "minflt", ru->ru_minflt);
  if (ru->ru_majflt) appendf(&b, ", %s=%ld", "majflt", ru->ru_majflt);
  if (ru->ru_nswap) appendf(&b, ", %s=%ld", "nswap", ru->ru_nswap);
  if (ru->ru_inblock) appendf(&b, ", %s=%ld", "inblock", ru->ru_inblock);
  if (ru->ru_oublock) appendf(&b, ", %s=%ld", "oublock", ru->ru_oublock);
  if (ru->ru_msgsnd) appendf(&b, ", %s=%ld", "msgsnd", ru->ru_msgsnd);
  if (ru->ru_msgrcv) appendf(&b, ", %s=%ld", "msgrcv", ru->ru_msgrcv);
  if (ru->ru_nsignals) appendf(&b, ", %s=%ld", "nsignals", ru->ru_nsignals);
  if (ru->ru_nvcsw) appendf(&b, ", %s=%ld", "nvcsw", ru->ru_nvcsw);
  if (ru->ru_nivcsw) appendf(&b, ", %s=%ld", "nivcsw", ru->ru_nivcsw);
  appendw(&b, '}');
  lua_pushlstring(L, b, appendz(b).i);
  return 1;
}

static const luaL_Reg kLuaUnixRusageMeth[] = {
    {"utime", LuaUnixRusageUtime},        //
    {"stime", LuaUnixRusageStime},        //
    {"maxrss", LuaUnixRusageMaxrss},      //
    {"ixrss", LuaUnixRusageIxrss},        //
    {"idrss", LuaUnixRusageIdrss},        //
    {"isrss", LuaUnixRusageIsrss},        //
    {"minflt", LuaUnixRusageMinflt},      //
    {"majflt", LuaUnixRusageMajflt},      //
    {"nswap", LuaUnixRusageNswap},        //
    {"inblock", LuaUnixRusageInblock},    //
    {"oublock", LuaUnixRusageOublock},    //
    {"msgsnd", LuaUnixRusageMsgsnd},      //
    {"msgrcv", LuaUnixRusageMsgrcv},      //
    {"nsignals", LuaUnixRusageNsignals},  //
    {"nvcsw", LuaUnixRusageNvcsw},        //
    {"nivcsw", LuaUnixRusageNivcsw},      //
    {0},                                  //
};

static const luaL_Reg kLuaUnixRusageMeta[] = {
    {"__repr", LuaUnixRusageToString},      //
    {"__tostring", LuaUnixRusageToString},  //
    {0},                                    //
};

static void LuaUnixRusageObj(lua_State *L) {
  luaL_newmetatable(L, "unix.Rusage");
  luaL_setfuncs(L, kLuaUnixRusageMeta, 0);
  luaL_newlibtable(L, kLuaUnixRusageMeth);
  luaL_setfuncs(L, kLuaUnixRusageMeth, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

////////////////////////////////////////////////////////////////////////////////
// unix.Errno object

static struct UnixErrno *GetUnixErrno(lua_State *L) {
  return luaL_checkudata(L, 1, "unix.Errno");
}

// unix.Errno:errno()
//     └─→ errno:int
static int LuaUnixErrnoErrno(lua_State *L) {
  return ReturnInteger(L, GetUnixErrno(L)->errno_);
}

static int LuaUnixErrnoWinerr(lua_State *L) {
  return ReturnInteger(L, GetUnixErrno(L)->winerr);
}

static int LuaUnixErrnoName(lua_State *L) {
  return ReturnString(L, strerrno(GetUnixErrno(L)->errno_));
}

static int LuaUnixErrnoDoc(lua_State *L) {
  return ReturnString(L, strerdoc(GetUnixErrno(L)->errno_));
}

static int LuaUnixErrnoCall(lua_State *L) {
  return ReturnString(L, GetUnixErrno(L)->call);
}

static int LuaUnixErrnoToString(lua_State *L) {
  char msg[256];
  struct UnixErrno *e;
  e = GetUnixErrno(L);
  if (e->call) {
    strerror_wr(e->errno_, e->winerr, msg, sizeof(msg));
    lua_pushfstring(L, "%s() failed: %s", e->call, msg);
  } else {
    lua_pushstring(L, strerrno(e->errno_));
  }
  return 1;
}

static const luaL_Reg kLuaUnixErrnoMeth[] = {
    {"errno", LuaUnixErrnoErrno},    //
    {"winerr", LuaUnixErrnoWinerr},  //
    {"name", LuaUnixErrnoName},      //
    {"call", LuaUnixErrnoCall},      //
    {"doc", LuaUnixErrnoDoc},        //
    {0},                             //
};

static const luaL_Reg kLuaUnixErrnoMeta[] = {
    {"__tostring", LuaUnixErrnoToString},  //
    {0},                                   //
};

static void LuaUnixErrnoObj(lua_State *L) {
  luaL_newmetatable(L, "unix.Errno");
  luaL_setfuncs(L, kLuaUnixErrnoMeta, 0);
  luaL_newlibtable(L, kLuaUnixErrnoMeth);
  luaL_setfuncs(L, kLuaUnixErrnoMeth, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

////////////////////////////////////////////////////////////////////////////////
// unix.Sigset object

// unix.Sigset(sig:int, ...)
//     └─→ unix.Sigset
static int LuaUnixSigset(lua_State *L) {
  int i, n;
  lua_Integer sig;
  struct sigset set;
  sigemptyset(&set);
  n = lua_gettop(L);
  for (i = 1; i <= n; ++i) {
    sig = luaL_checkinteger(L, i);
    if (1 <= sig && sig <= NSIG) {
      set.__bits[(sig - 1) >> 6] |= 1ull << ((sig - 1) & 63);
    }
  }
  LuaPushSigset(L, set);
  return 1;
}

// unix.Sigset:add(sig:int)
static int LuaUnixSigsetAdd(lua_State *L) {
  lua_Integer sig;
  struct sigset *set;
  set = luaL_checkudata(L, 1, "unix.Sigset");
  sig = luaL_checkinteger(L, 2);
  if (1 <= sig && sig <= NSIG) {
    set->__bits[(sig - 1) >> 6] |= 1ull << ((sig - 1) & 63);
  }
  return 0;
}

// unix.Sigset:remove(sig:int)
static int LuaUnixSigsetRemove(lua_State *L) {
  lua_Integer sig;
  struct sigset *set;
  set = luaL_checkudata(L, 1, "unix.Sigset");
  sig = luaL_checkinteger(L, 2);
  if (1 <= sig && sig <= NSIG) {
    set->__bits[(sig - 1) >> 6] &= ~(1ull << ((sig - 1) & 63));
  }
  return 0;
}

// unix.Sigset:fill()
static int LuaUnixSigsetFill(lua_State *L) {
  struct sigset *set;
  set = luaL_checkudata(L, 1, "unix.Sigset");
  memset(set, -1, sizeof(*set));
  return 0;
}

// unix.Sigset:clear()
static int LuaUnixSigsetClear(lua_State *L) {
  struct sigset *set;
  set = luaL_checkudata(L, 1, "unix.Sigset");
  bzero(set, sizeof(*set));
  return 0;
}

// unix.Sigset:contains(sig:int)
//     └─→ bool
static int LuaUnixSigsetContains(lua_State *L) {
  lua_Integer sig;
  struct sigset *set;
  set = luaL_checkudata(L, 1, "unix.Sigset");
  sig = luaL_checkinteger(L, 2);
  return ReturnBoolean(
      L, (1 <= sig && sig <= NSIG)
             ? !!(set->__bits[(sig - 1) >> 6] & (1ull << ((sig - 1) & 63)))
             : false);
}

static int LuaUnixSigsetTostring(lua_State *L) {
  char *b = 0;
  int sig, first;
  struct sigset *ss;
  ss = luaL_checkudata(L, 1, "unix.Sigset");
  appends(&b, "unix.Sigset");
  appendw(&b, '(');
  for (sig = first = 1; sig <= NSIG; ++sig) {
    if (sigismember(ss, sig) == 1) {
      if (!first) {
        appendw(&b, READ16LE(", "));
      } else {
        first = 0;
      }
      appendw(&b, READ64LE("unix.\0\0"));
      appends(&b, strsignal(sig));
    }
  }
  appendw(&b, ')');
  lua_pushlstring(L, b, appendz(b).i);
  free(b);
  return 1;
}

static const luaL_Reg kLuaUnixSigsetMeth[] = {
    {"add", LuaUnixSigsetAdd},            //
    {"fill", LuaUnixSigsetFill},          //
    {"clear", LuaUnixSigsetClear},        //
    {"remove", LuaUnixSigsetRemove},      //
    {"contains", LuaUnixSigsetContains},  //
    {0},                                  //
};

static const luaL_Reg kLuaUnixSigsetMeta[] = {
    {"__tostring", LuaUnixSigsetTostring},  //
    {"__repr", LuaUnixSigsetTostring},      //
    {0},                                    //
};

static void LuaUnixSigsetObj(lua_State *L) {
  luaL_newmetatable(L, "unix.Sigset");
  luaL_setfuncs(L, kLuaUnixSigsetMeta, 0);
  luaL_newlibtable(L, kLuaUnixSigsetMeth);
  luaL_setfuncs(L, kLuaUnixSigsetMeth, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

////////////////////////////////////////////////////////////////////////////////
// unix.Dir object

static DIR **GetUnixDirSelf(lua_State *L) {
  return luaL_checkudata(L, 1, "unix.Dir");
}

static DIR *GetDirOrDie(lua_State *L) {
  DIR **dirp;
  dirp = GetUnixDirSelf(L);
  if (*dirp) return *dirp;
  luaL_argerror(L, 1, "unix.UnixDir is closed");
  unreachable;
}

// unix.Dir:close()
//     ├─→ true
//     └─→ nil, unix.Errno
static int LuaUnixDirClose(lua_State *L) {
  DIR **dirp;
  int rc, olderr;
  dirp = GetUnixDirSelf(L);
  if (*dirp) {
    olderr = errno;
    rc = closedir(*dirp);
    *dirp = 0;
    return SysretBool(L, "closedir", olderr, rc);
  } else {
    lua_pushboolean(L, true);
    return 1;
  }
}

// unix.Dir:read()
//     ├─→ name:str, kind:int, ino:int, off:int
//     └─→ nil
static int LuaUnixDirRead(lua_State *L) {
  struct dirent *ent;
  if ((ent = readdir(GetDirOrDie(L)))) {
    lua_pushlstring(L, ent->d_name, strnlen(ent->d_name, sizeof(ent->d_name)));
    lua_pushinteger(L, ent->d_type);
    lua_pushinteger(L, ent->d_ino);
    lua_pushinteger(L, ent->d_off);
    return 4;
  } else {
    // end of directory stream condition
    // we make the assumption getdents() won't fail
    lua_pushnil(L);
    return 1;
  }
}

// unix.Dir:fd()
//     ├─→ fd:int
//     └─→ nil, unix.Errno
static int LuaUnixDirFd(lua_State *L) {
  int fd, olderr = errno;
  fd = dirfd(GetDirOrDie(L));
  if (fd != -1) {
    lua_pushinteger(L, fd);
    return 1;
  } else {
    return SysretErrno(L, "dirfd", olderr);
  }
}

// unix.Dir:tell()
//     ├─→ off:int
//     └─→ nil, unix.Errno
static int LuaUnixDirTell(lua_State *L) {
  int olderr = errno;
  return SysretInteger(L, "telldir", olderr, telldir(GetDirOrDie(L)));
}

// unix.Dir:rewind()
static int LuaUnixDirRewind(lua_State *L) {
  rewinddir(GetDirOrDie(L));
  return 0;
}

static int ReturnDir(lua_State *L, DIR *dir) {
  DIR **dirp;
  dirp = lua_newuserdatauv(L, sizeof(*dirp), 1);
  luaL_setmetatable(L, "unix.Dir");
  *dirp = dir;
  return 1;
}

// unix.opendir(path:str)
//     ├─→ state:unix.Dir
//     └─→ nil, unix.Errno
static int LuaUnixOpendir(lua_State *L) {
  DIR *dir;
  int olderr = errno;
  if ((dir = opendir(luaL_checkstring(L, 1)))) {
    return ReturnDir(L, dir);
  } else {
    return SysretErrno(L, "opendir", olderr);
  }
}

// unix.fdopendir(fd:int)
//     ├─→ next:function, state:unix.Dir
//     └─→ nil, unix.Errno
static int LuaUnixFdopendir(lua_State *L) {
  DIR *dir;
  int olderr = errno;
  if ((dir = fdopendir(luaL_checkinteger(L, 1)))) {
    return ReturnDir(L, dir);
  } else {
    return SysretErrno(L, "fdopendir", olderr);
  }
}

static const luaL_Reg kLuaUnixDirMeth[] = {
    {"close", LuaUnixDirClose},    //
    {"read", LuaUnixDirRead},      //
    {"fd", LuaUnixDirFd},          //
    {"tell", LuaUnixDirTell},      //
    {"rewind", LuaUnixDirRewind},  //
    {0},                           //
};

static const luaL_Reg kLuaUnixDirMeta[] = {
    {"__call", LuaUnixDirRead},  //
    {"__gc", LuaUnixDirClose},   //
    {0},                         //
};

static void LuaUnixDirObj(lua_State *L) {
  luaL_newmetatable(L, "unix.Dir");
  luaL_setfuncs(L, kLuaUnixDirMeta, 0);
  luaL_newlibtable(L, kLuaUnixDirMeth);
  luaL_setfuncs(L, kLuaUnixDirMeth, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

////////////////////////////////////////////////////////////////////////////////
// UNIX module

static const luaL_Reg kLuaUnix[] = {
    {"S_ISBLK", LuaUnixSisblk},           // is st:mode() a block device?
    {"S_ISCHR", LuaUnixSischr},           // is st:mode() a character device?
    {"S_ISDIR", LuaUnixSisdir},           // is st:mode() a directory?
    {"S_ISFIFO", LuaUnixSisfifo},         // is st:mode() a fifo?
    {"S_ISLNK", LuaUnixSislnk},           // is st:mode() a symbolic link?
    {"S_ISREG", LuaUnixSisreg},           // is st:mode() a regular file?
    {"S_ISSOCK", LuaUnixSissock},         // is st:mode() a socket?
    {"Sigset", LuaUnixSigset},            // creates signal bitmask
    {"WEXITSTATUS", LuaUnixWexitstatus},  // gets exit status from wait status
    {"WIFEXITED", LuaUnixWifexited},      // gets exit code from wait status
    {"WIFSIGNALED", LuaUnixWifsignaled},  // determines if died due to signal
    {"WTERMSIG", LuaUnixWtermsig},        // gets the signal code
    {"accept", LuaUnixAccept},            // create client fd for client
    {"access", LuaUnixAccess},            // check my file authorization
    {"bind", LuaUnixBind},                // reserve network interface address
    {"chdir", LuaUnixChdir},              // change directory
    {"chmod", LuaUnixChmod},              // change mode of file
    {"chown", LuaUnixChown},              // change owner of file
    {"chroot", LuaUnixChroot},            // change root directory
    {"clock_gettime", LuaUnixGettime},    // get timestamp w/ nano precision
    {"close", LuaUnixClose},              // close file or socket
    {"commandv", LuaUnixCommandv},        // resolve program on $PATH
    {"connect", LuaUnixConnect},          // connect to remote address
    {"dup", LuaUnixDup},                  // copy fd to lowest empty slot
    {"environ", LuaUnixEnviron},          // get environment variables
    {"execve", LuaUnixExecve},            // replace process with program
    {"exit", LuaUnixExit},                // exit w/o atexit
    {"fcntl", LuaUnixFcntl},              // manipulate file descriptor
    {"fdatasync", LuaUnixFdatasync},      // flush open file w/o metadata
    {"fdopendir", LuaUnixFdopendir},      // read directory entry list
    {"fork", LuaUnixFork},                // make child process via mitosis
    {"fstat", LuaUnixFstat},              // get file info from fd
    {"fsync", LuaUnixFsync},              // flush open file
    {"ftruncate", LuaUnixFtruncate},      // shrink or extend file medium
    {"futimens", LuaUnixFutimens},        // change access/modified time
    {"getcwd", LuaUnixGetcwd},            // get current directory
    {"getegid", LuaUnixGetegid},          // get effective group id of process
    {"geteuid", LuaUnixGeteuid},          // get effective user id of process
    {"getgid", LuaUnixGetgid},            // get real group id of process
    {"gethostname", LuaUnixGethostname},  // get hostname of this machine
    {"getpeername", LuaUnixGetpeername},  // get address of remote end
    {"getpgid", LuaUnixGetpgid},          // get process group id of pid
    {"getpgrp", LuaUnixGetpgrp},          // get process group id
    {"getpid", LuaUnixGetpid},            // get id of this process
    {"getppid", LuaUnixGetppid},          // get parent process id
    {"getrlimit", LuaUnixGetrlimit},      // query resource limits
    {"getrusage", LuaUnixGetrusage},      // query resource usages
    {"getsid", LuaUnixGetsid},            // get session id of pid
    {"getsockname", LuaUnixGetsockname},  // get address of local end
    {"getsockopt", LuaUnixGetsockopt},    // get socket tunings
    {"tiocgwinsz", LuaUnixTiocgwinsz},    // pseudoteletypewriter dimensions
    {"getuid", LuaUnixGetuid},            // get real user id of process
    {"gmtime", LuaUnixGmtime},            // destructure unix timestamp
    {"isatty", LuaUnixIsatty},            // detects pseudoteletypewriters
    {"kill", LuaUnixKill},                // signal child process
    {"link", LuaUnixLink},                // create hard link
    {"listen", LuaUnixListen},            // begin listening for clients
    {"localtime", LuaUnixLocaltime},      // localize unix timestamp
    {"lseek", LuaUnixLseek},              // seek in file
    {"major", LuaUnixMajor},              // extract device info
    {"makedirs", LuaUnixMakedirs},        // make directory and parents too
    {"minor", LuaUnixMinor},              // extract device info
    {"mkdir", LuaUnixMkdir},              // make directory
    {"nanosleep", LuaUnixNanosleep},      // sleep w/ nano precision
    {"open", LuaUnixOpen},                // open file fd at lowest slot
    {"opendir", LuaUnixOpendir},          // read directory entry list
    {"pipe", LuaUnixPipe},                // create two anon fifo fds
    {"pledge", LuaUnixPledge},            // enables syscall sandbox
    {"poll", LuaUnixPoll},                // waits for file descriptor events
    {"raise", LuaUnixRaise},              // signal this process
    {"read", LuaUnixRead},                // read from file or socket
    {"readlink", LuaUnixReadlink},        // reads symbolic link
    {"realpath", LuaUnixRealpath},        // abspath without dots/symlinks
    {"recv", LuaUnixRecv},                // receive tcp from some address
    {"recvfrom", LuaUnixRecvfrom},        // receive udp from some address
    {"rename", LuaUnixRename},            // rename file or directory
    {"rmdir", LuaUnixRmdir},              // remove empty directory
    {"send", LuaUnixSend},                // send tcp to some address
    {"sendto", LuaUnixSendto},            // send udp to some address
    {"setgid", LuaUnixSetgid},            // set real group id of process
    {"setitimer", LuaUnixSetitimer},      // set alarm clock
    {"setpgid", LuaUnixSetpgid},          // set process group id for pid
    {"setpgrp", LuaUnixSetpgrp},          // sets process group id
    {"setresgid", LuaUnixSetresgid},      // sets real/effective/saved gids
    {"setresuid", LuaUnixSetresuid},      // sets real/effective/saved uids
    {"setrlimit", LuaUnixSetrlimit},      // prevent cpu memory bombs
    {"setsid", LuaUnixSetsid},            // create a new session id
    {"setsockopt", LuaUnixSetsockopt},    // tune socket options
    {"setuid", LuaUnixSetuid},            // set real user id of process
    {"shutdown", LuaUnixShutdown},        // make socket half empty or full
    {"sigaction", LuaUnixSigaction},      // install signal handler
    {"sigprocmask", LuaUnixSigprocmask},  // change signal mask
    {"sigsuspend", LuaUnixSigsuspend},    // wait for signal
    {"siocgifconf", LuaUnixSiocgifconf},  // get list of network interfaces
    {"socket", LuaUnixSocket},            // create network communication fd
    {"socketpair", LuaUnixSocketpair},    // create bidirectional pipe
    {"stat", LuaUnixStat},                // get file info from path
    {"strsignal", LuaUnixStrsignal},      // turn signal into string
    {"symlink", LuaUnixSymlink},          // create symbolic link
    {"sync", LuaUnixSync},                // flushes files and disks
    {"syslog", LuaUnixSyslog},            // logs to system log
    {"truncate", LuaUnixTruncate},        // shrink or extend file medium
    {"umask", LuaUnixUmask},              // set default file mask
    {"unlink", LuaUnixUnlink},            // remove file
    {"utimensat", LuaUnixUtimensat},      // change access/modified time
    {"wait", LuaUnixWait},                // wait for child to change status
    {"write", LuaUnixWrite},              // write to file or socket
    {0},                                  //
};

static void LoadMagnums(lua_State *L, struct MagnumStr *ms, const char *pfx) {
  int i;
  char b[64], *p;
  p = stpcpy(b, pfx);
  for (i = 0; ms[i].x != MAGNUM_TERMINATOR; ++i) {
    stpcpy(p, MAGNUM_STRING(ms, i));
    LuaSetIntField(L, b, MAGNUM_NUMBER(ms, i));
  }
}

int LuaUnix(lua_State *L) {
  GL = L;
  luaL_newlib(L, kLuaUnix);
  LuaUnixSigsetObj(L);
  LuaUnixRusageObj(L);
  LuaUnixErrnoObj(L);
  LuaUnixStatObj(L);
  LuaUnixDirObj(L);
  lua_newtable(L);
  lua_setglobal(L, "__signal_handlers");

  LoadMagnums(L, kErrnoNames, "");
  LoadMagnums(L, kOpenFlags, "O_");
  LoadMagnums(L, kSignalNames, "");
  LoadMagnums(L, kIpOptnames, "IP_");
  LoadMagnums(L, kTcpOptnames, "TCP_");
  LoadMagnums(L, kSockOptnames, "SO_");
  LoadMagnums(L, kClockNames, "CLOCK_");

  // seek() whence
  LuaSetIntField(L, "SEEK_SET", SEEK_SET);
  LuaSetIntField(L, "SEEK_CUR", SEEK_CUR);
  LuaSetIntField(L, "SEEK_END", SEEK_END);

  // fcntl() stuff
  LuaSetIntField(L, "F_GETFD", F_GETFD);
  LuaSetIntField(L, "F_SETFD", F_SETFD);
  LuaSetIntField(L, "F_GETFL", F_GETFL);
  LuaSetIntField(L, "F_SETFL", F_SETFL);
  LuaSetIntField(L, "F_UNLCK", F_UNLCK);
  LuaSetIntField(L, "F_RDLCK", F_RDLCK);
  LuaSetIntField(L, "F_WRLCK", F_WRLCK);
  LuaSetIntField(L, "F_SETLK", F_SETLK);
  LuaSetIntField(L, "F_SETLKW", F_SETLKW);
  LuaSetIntField(L, "FD_CLOEXEC", FD_CLOEXEC);

  // access() mode
  LuaSetIntField(L, "R_OK", R_OK);
  LuaSetIntField(L, "W_OK", W_OK);
  LuaSetIntField(L, "X_OK", X_OK);
  LuaSetIntField(L, "F_OK", F_OK);

  // rlimit() resources
  LuaSetIntField(L, "RLIMIT_AS", RLIMIT_AS);
  LuaSetIntField(L, "RLIMIT_RSS", RLIMIT_RSS);
  LuaSetIntField(L, "RLIMIT_CPU", RLIMIT_CPU);
  LuaSetIntField(L, "RLIMIT_FSIZE", RLIMIT_FSIZE);
  LuaSetIntField(L, "RLIMIT_NPROC", RLIMIT_NPROC);
  LuaSetIntField(L, "RLIMIT_NOFILE", RLIMIT_NOFILE);

  // wait() options
  LuaSetIntField(L, "WNOHANG", WNOHANG);

  // socket() family
  LuaSetIntField(L, "AF_UNSPEC", AF_UNSPEC);
  LuaSetIntField(L, "AF_UNIX", AF_UNIX);
  LuaSetIntField(L, "AF_INET", AF_INET);

  // socket() type
  LuaSetIntField(L, "SOCK_STREAM", SOCK_STREAM);
  LuaSetIntField(L, "SOCK_DGRAM", SOCK_DGRAM);
  LuaSetIntField(L, "SOCK_RAW", SOCK_RAW);
  LuaSetIntField(L, "SOCK_RDM", SOCK_RDM);
  LuaSetIntField(L, "SOCK_SEQPACKET", SOCK_SEQPACKET);
  LuaSetIntField(L, "SOCK_CLOEXEC", SOCK_CLOEXEC);
  LuaSetIntField(L, "SOCK_NONBLOCK", SOCK_NONBLOCK);

  // socket() protocol
  LuaSetIntField(L, "IPPROTO_IP", IPPROTO_IP);
  LuaSetIntField(L, "IPPROTO_ICMP", IPPROTO_ICMP);
  LuaSetIntField(L, "IPPROTO_TCP", IPPROTO_TCP);
  LuaSetIntField(L, "IPPROTO_UDP", IPPROTO_UDP);
  LuaSetIntField(L, "IPPROTO_RAW", IPPROTO_RAW);

  // shutdown() how
  LuaSetIntField(L, "SHUT_RD", SHUT_RD);
  LuaSetIntField(L, "SHUT_WR", SHUT_WR);
  LuaSetIntField(L, "SHUT_RDWR", SHUT_RDWR);

  // recvfrom() / sendto() flags
  LuaSetIntField(L, "MSG_WAITALL", MSG_WAITALL);
  LuaSetIntField(L, "MSG_DONTROUTE", MSG_DONTROUTE);
  LuaSetIntField(L, "MSG_PEEK", MSG_PEEK);
  LuaSetIntField(L, "MSG_OOB", MSG_OOB);
  LuaSetIntField(L, "MSG_NOSIGNAL", MSG_NOSIGNAL);
  LuaSetIntField(L, "MSG_MORE", MSG_MORE);

  // readdir() type
  LuaSetIntField(L, "DT_UNKNOWN", DT_UNKNOWN);
  LuaSetIntField(L, "DT_REG", DT_REG);
  LuaSetIntField(L, "DT_DIR", DT_DIR);
  LuaSetIntField(L, "DT_BLK", DT_BLK);
  LuaSetIntField(L, "DT_LNK", DT_LNK);
  LuaSetIntField(L, "DT_CHR", DT_CHR);
  LuaSetIntField(L, "DT_FIFO", DT_FIFO);
  LuaSetIntField(L, "DT_SOCK", DT_SOCK);

  // poll() flags
  LuaSetIntField(L, "POLLIN", POLLIN);
  LuaSetIntField(L, "POLLPRI", POLLPRI);
  LuaSetIntField(L, "POLLOUT", POLLOUT);
  LuaSetIntField(L, "POLLERR", POLLERR);
  LuaSetIntField(L, "POLLHUP", POLLHUP);
  LuaSetIntField(L, "POLLNVAL", POLLNVAL);
  LuaSetIntField(L, "POLLRDBAND", POLLRDBAND);
  LuaSetIntField(L, "POLLRDNORM", POLLRDNORM);
  LuaSetIntField(L, "POLLWRBAND", POLLWRBAND);
  LuaSetIntField(L, "POLLWRNORM", POLLWRNORM);
  LuaSetIntField(L, "POLLRDHUP", POLLRDHUP);

  // i/o options
  LuaSetIntField(L, "AT_FDCWD", AT_FDCWD);
  LuaSetIntField(L, "AT_EACCESS", AT_EACCESS);
  LuaSetIntField(L, "AT_SYMLINK_NOFOLLOW", AT_SYMLINK_NOFOLLOW);

  // sigprocmask() handlers
  LuaSetIntField(L, "SIG_BLOCK", SIG_BLOCK);
  LuaSetIntField(L, "SIG_UNBLOCK", SIG_UNBLOCK);
  LuaSetIntField(L, "SIG_SETMASK", SIG_SETMASK);

  // sigaction() handlers
  LuaSetIntField(L, "SIG_DFL", (intptr_t)SIG_DFL);
  LuaSetIntField(L, "SIG_IGN", (intptr_t)SIG_IGN);

  // utimensat() magnums
  LuaSetIntField(L, "UTIME_NOW", UTIME_NOW);
  LuaSetIntField(L, "UTIME_OMIT", UTIME_OMIT);

  // setitimer() which
  LuaSetIntField(L, "ITIMER_REAL", ITIMER_REAL);  // portable
  LuaSetIntField(L, "ITIMER_PROF", ITIMER_PROF);
  LuaSetIntField(L, "ITIMER_VIRTUAL", ITIMER_VIRTUAL);

  // syslog() stuff
  LuaSetIntField(L, "LOG_EMERG", LOG_EMERG);
  LuaSetIntField(L, "LOG_ALERT", LOG_ALERT);
  LuaSetIntField(L, "LOG_CRIT", LOG_CRIT);
  LuaSetIntField(L, "LOG_ERR", LOG_ERR);
  LuaSetIntField(L, "LOG_WARNING", LOG_WARNING);
  LuaSetIntField(L, "LOG_NOTICE", LOG_NOTICE);
  LuaSetIntField(L, "LOG_INFO", LOG_INFO);
  LuaSetIntField(L, "LOG_DEBUG", LOG_DEBUG);

  // setsockopt() level
  LuaSetIntField(L, "SOL_IP", SOL_IP);
  LuaSetIntField(L, "SOL_SOCKET", SOL_SOCKET);
  LuaSetIntField(L, "SOL_TCP", SOL_TCP);
  LuaSetIntField(L, "SOL_UDP", SOL_UDP);

  // sigaction() flags
  LuaSetIntField(L, "SA_RESTART", SA_RESTART);
  LuaSetIntField(L, "SA_RESETHAND", SA_RESETHAND);
  LuaSetIntField(L, "SA_NODEFER", SA_NODEFER);
  LuaSetIntField(L, "SA_NOCLDWAIT", SA_NOCLDWAIT);
  LuaSetIntField(L, "SA_NOCLDSTOP", SA_NOCLDSTOP);

  // getrusage() who
  LuaSetIntField(L, "RUSAGE_SELF", RUSAGE_SELF);
  LuaSetIntField(L, "RUSAGE_THREAD", RUSAGE_THREAD);
  LuaSetIntField(L, "RUSAGE_CHILDREN", RUSAGE_CHILDREN);
  LuaSetIntField(L, "RUSAGE_BOTH", RUSAGE_BOTH);

  LuaSetIntField(L, "ARG_MAX", __arg_max());
  LuaSetIntField(L, "BUFSIZ", BUFSIZ);
  LuaSetIntField(L, "CLK_TCK", CLK_TCK);
  LuaSetIntField(L, "NAME_MAX", _NAME_MAX);
  LuaSetIntField(L, "NSIG", _NSIG);
  LuaSetIntField(L, "PATH_MAX", _PATH_MAX);
  LuaSetIntField(L, "PIPE_BUF", PIPE_BUF);

  return 1;
}
