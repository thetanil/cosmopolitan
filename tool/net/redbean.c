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
#include "libc/bits/atomic.h"
#include "libc/bits/safemacros.internal.h"
#include "libc/calls/calls.h"
#include "libc/calls/ioctl.h"
#include "libc/calls/math.h"
#include "libc/calls/struct/dirent.h"
#include "libc/calls/struct/filter.h"
#include "libc/calls/struct/flock.h"
#include "libc/calls/struct/iovec.h"
#include "libc/calls/struct/rusage.h"
#include "libc/calls/struct/sigaction.h"
#include "libc/calls/struct/stat.h"
#include "libc/calls/struct/termios.h"
#include "libc/dce.h"
#include "libc/dns/dns.h"
#include "libc/dns/hoststxt.h"
#include "libc/dos.h"
#include "libc/fmt/conv.h"
#include "libc/fmt/itoa.h"
#include "libc/intrin/kprintf.h"
#include "libc/intrin/nomultics.internal.h"
#include "libc/intrin/spinlock.h"
#include "libc/intrin/wait0.internal.h"
#include "libc/log/check.h"
#include "libc/log/log.h"
#include "libc/macros.internal.h"
#include "libc/math.h"
#include "libc/mem/alloca.h"
#include "libc/mem/mem.h"
#include "libc/nexgen32e/bsr.h"
#include "libc/nexgen32e/crc32.h"
#include "libc/nexgen32e/nt2sysv.h"
#include "libc/nexgen32e/rdtsc.h"
#include "libc/nexgen32e/threaded.h"
#include "libc/nexgen32e/x86feature.h"
#include "libc/nt/enum/fileflagandattributes.h"
#include "libc/nt/thread.h"
#include "libc/rand/rand.h"
#include "libc/runtime/clktck.h"
#include "libc/runtime/gc.h"
#include "libc/runtime/gc.internal.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/runtime.h"
#include "libc/runtime/stack.h"
#include "libc/sock/goodsocket.internal.h"
#include "libc/sock/sock.h"
#include "libc/stdio/append.internal.h"
#include "libc/stdio/hex.internal.h"
#include "libc/str/slice.h"
#include "libc/sysv/consts/af.h"
#include "libc/sysv/consts/clone.h"
#include "libc/sysv/consts/dt.h"
#include "libc/sysv/consts/ex.h"
#include "libc/sysv/consts/exit.h"
#include "libc/sysv/consts/f.h"
#include "libc/sysv/consts/inaddr.h"
#include "libc/sysv/consts/ipproto.h"
#include "libc/sysv/consts/madv.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/poll.h"
#include "libc/sysv/consts/pr.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/consts/rusage.h"
#include "libc/sysv/consts/s.h"
#include "libc/sysv/consts/sa.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/consts/sock.h"
#include "libc/sysv/consts/termios.h"
#include "libc/sysv/consts/w.h"
#include "libc/sysv/errfuns.h"
#include "libc/x/x.h"
#include "libc/zip.h"
#include "net/http/escape.h"
#include "net/http/http.h"
#include "net/http/ip.h"
#include "net/https/https.h"
#include "third_party/getopt/getopt.h"
#include "third_party/lua/cosmo.h"
#include "third_party/lua/lauxlib.h"
#include "third_party/lua/lrepl.h"
#include "third_party/lua/lualib.h"
#include "third_party/lua/lunix.h"
#include "third_party/mbedtls/ctr_drbg.h"
#include "third_party/mbedtls/debug.h"
#include "third_party/mbedtls/iana.h"
#include "third_party/mbedtls/net_sockets.h"
#include "third_party/mbedtls/oid.h"
#include "third_party/mbedtls/san.h"
#include "third_party/mbedtls/ssl.h"
#include "third_party/mbedtls/ssl_ticket.h"
#include "third_party/mbedtls/x509.h"
#include "third_party/mbedtls/x509_crt.h"
#include "third_party/zlib/zlib.h"
#include "tool/args/args.h"
#include "tool/build/lib/case.h"
#include "tool/net/lfuncs.h"
#include "tool/net/luacheck.h"
#include "tool/net/sandbox.h"

STATIC_STACK_SIZE(0x40000);
STATIC_YOINK("zip_uri_support");
#if !IsTiny()
STATIC_YOINK("ShowCrashReportsEarly");
#endif

/**
 * @fileoverview redbean - single-file distributable web server
 *
 * redbean makes it possible to share web applications that run offline
 * as a single-file αcτµαlly pδrταblε εxεcµταblε zip archive which
 * contains your assets. All you need to do is download the redbean.com
 * program below, change the filename to .zip, add your content in a zip
 * editing tool, and then change the extension back to .com.
 *
 * redbean can serve 1 million+ gzip encoded responses per second on a
 * cheap personal computer. That performance is thanks to zip and gzip
 * using the same compression format, which enables kernelspace copies.
 * Another reason redbean goes fast is that it's a tiny static binary,
 * which makes fork memory paging nearly free.
 *
 * redbean is also easy to modify to suit your own needs. The program
 * itself is written as a single .c file. It embeds the Lua programming
 * language and SQLite which let you write dynamic pages.
 */

#ifndef REDBEAN
#define REDBEAN "redbean"
#endif

#define VERSION          0x020008
#define HEARTBEAT        5000 /*ms*/
#define HASH_LOAD_FACTOR /* 1. / */ 4
#define MONITOR_MICROS   150000
#define READ(F, P, N)    readv(F, &(struct iovec){P, N}, 1)
#define WRITE(F, P, N)   writev(F, &(struct iovec){P, N}, 1)
#define LockInc(P)       asm volatile("lock incq\t%0" : "=m"(*(P)))
#define LockDec(P)       asm volatile("lock decq\t%0" : "=m"(*(P)))
#define AppendCrlf(P)    mempcpy(P, "\r\n", 2)
#define HasHeader(H)     (!!msg.headers[H].a)
#define HeaderData(H)    (inbuf.p + msg.headers[H].a)
#define HeaderLength(H)  (msg.headers[H].b - msg.headers[H].a)
#define HeaderEqualCase(H, S) \
  SlicesEqualCase(S, strlen(S), HeaderData(H), HeaderLength(H))

#define TRACE_BEGIN                                                    \
  do {                                                                 \
    if (!IsTiny()) {                                                   \
      if (funtrace) {                                                  \
        atomic_fetch_add_explicit(&__ftrace, 1, memory_order_relaxed); \
      }                                                                \
      if (systrace) {                                                  \
        atomic_fetch_add_explicit(&__strace, 1, memory_order_relaxed); \
      }                                                                \
    }                                                                  \
  } while (0)

#define TRACE_END                                                      \
  do {                                                                 \
    if (!IsTiny()) {                                                   \
      if (funtrace) {                                                  \
        atomic_fetch_sub_explicit(&__ftrace, 1, memory_order_relaxed); \
      }                                                                \
      if (systrace) {                                                  \
        atomic_fetch_sub_explicit(&__strace, 1, memory_order_relaxed); \
      }                                                                \
    }                                                                  \
  } while (0)

// letters not used: EINOQYnoqwxy
// digits not used:  0123456789
// puncts not used:  !"#$&'()*+,-./;<=>@[\]^_`{|}~
#define GETOPTS "%BJSVXZabdfghijkmsuvzA:C:D:F:G:H:K:L:M:P:R:T:U:W:c:e:l:p:r:t:"

static const uint8_t kGzipHeader[] = {
    0x1F,        // MAGNUM
    0x8B,        // MAGNUM
    0x08,        // CM: DEFLATE
    0x00,        // FLG: NONE
    0x00,        // MTIME: NONE
    0x00,        //
    0x00,        //
    0x00,        //
    0x00,        // XFL
    kZipOsUnix,  // OS
};

static const char *const kIndexPaths[] = {
#ifndef STATIC
    "index.lua",
#endif
    "index.html",
};

static const char *const kAlpn[] = {
    "http/1.1",
    NULL,
};

struct Buffer {
  size_t n, c;
  char *p;
};

struct TlsBio {
  int fd, c;
  unsigned a, b;
  unsigned char t[4000];
  unsigned char u[1430];
};

struct Strings {
  size_t n, c;
  struct String {
    size_t n;
    const char *s;
  } * p;
};

struct DeflateGenerator {
  int t;
  void *b;
  size_t i;
  uint32_t c;
  uint32_t z;
  z_stream s;
  struct Asset *a;
};

static struct Ips {
  size_t n;
  uint32_t *p;
} ips;

static struct Ports {
  size_t n;
  uint16_t *p;
} ports;

static struct Servers {
  size_t n;
  struct Server {
    int fd;
    struct sockaddr_in addr;
  } * p;
} servers;

static struct Freelist {
  size_t n, c;
  void **p;
} freelist;

static struct Unmaplist {
  size_t n, c;
  struct Unmap {
    int f;
    void *p;
    size_t n;
  } * p;
} unmaplist;

static struct Psks {
  size_t n;
  struct Psk {
    char *key;
    size_t key_len;
    char *identity;
    size_t identity_len;
    char *s;
  } * p;
} psks;

static struct Suites {
  size_t n;
  uint16_t *p;
} suites;

static struct Certs {
  size_t n;
  struct Cert *p;
} certs;

static struct Redirects {
  size_t n;
  struct Redirect {
    int code;
    struct String path;
    struct String location;
  } * p;
} redirects;

static struct Assets {
  uint32_t n;
  struct Asset {
    bool istext;
    uint32_t hash;
    uint64_t cf;
    uint64_t lf;
    int64_t lastmodified;
    char *lastmodifiedstr;
    struct File {
      struct String path;
      struct stat st;
    } * file;
  } * p;
} assets;

static struct Shared {
  int workers;
  long double nowish;
  long double lastreindex;
  long double lastmeltdown;
  char currentdate[32];
  struct rusage server;
  struct rusage children;
  struct Counters {
#define C(x) long x;
#include "tool/net/counters.inc"
#undef C
  } c;
  _Alignas(64) char montermlock;
} * shared;

static const char kCounterNames[] =
#define C(x) #x "\0"
#include "tool/net/counters.inc"
#undef C
    ;

typedef ssize_t (*reader_f)(int, void *, size_t);
typedef ssize_t (*writer_f)(int, struct iovec *, int);

static bool suiteb;
static bool killed;
static bool istext;
static bool zombied;
static bool gzipped;
static bool branded;
static bool usingssl;
static bool funtrace;
static bool systrace;
static bool meltdown;
static bool unsecure;
static bool norsagen;
static bool printport;
static bool daemonize;
static bool logrusage;
static bool logbodies;
static bool requiressl;
static bool isterminal;
static bool sslcliused;
static bool loglatency;
static bool terminated;
static bool uniprocess;
static bool invalidated;
static bool logmessages;
static bool isinitialized;
static bool checkedmethod;
static bool sslinitialized;
static bool sslfetchverify;
static bool hascontenttype;
static bool gotcachecontrol;
static bool interpretermode;
static bool sslclientverify;
static bool connectionclose;
static bool hasonworkerstop;
static bool isexitingworker;
static bool hasonworkerstart;
static bool leakcrashreports;
static bool hasonhttprequest;
static bool ishandlingrequest;
static bool listeningonport443;
static bool hasonprocesscreate;
static bool hasonprocessdestroy;
static bool loggednetworkorigin;
static bool ishandlingconnection;
static bool hasonclientconnection;
static bool gotxcontenttypeoptions;
static bool evadedragnetsurveillance;
static _Atomic(bool) terminatemonitor;

static int zfd;
static int frags;
static int gmtoff;
static int client;
static int mainpid;
static int sandboxed;
static int changeuid;
static int changegid;
static int isyielding;
static int statuscode;
static int shutdownsig;
static int sslpskindex;
static int oldloglevel;
static int maxpayloadsize;
static int messageshandled;
static int sslticketlifetime;
static uint32_t clientaddrsize;

static size_t zsize;
static char *outbuf;
static lua_State *GL;
static lua_State *YL;
static char *content;
static uint8_t *zmap;
static char *repltls;
static uint8_t *zbase;
static uint8_t *zcdir;
static size_t hdrsize;
static size_t msgsize;
static size_t amtread;
static char *replstack;
static reader_f reader;
static writer_f writer;
static char *extrahdrs;
static char *monitortls;
static char *luaheaderp;
static const char *zpath;
static const char *brand;
static char *monitorstack;
static char gzip_footer[8];
static const char *pidpath;
static const char *logpath;
static const char *histpath;
static struct pollfd *polls;
static struct Strings loops;
static size_t payloadlength;
static size_t contentlength;
static int64_t cacheseconds;
static const char *monitortty;
static const char *serverheader;
static struct Strings stagedirs;
static struct Strings hidepaths;
static const char *launchbrowser;
static const char *referrerpolicy;
static ssize_t (*generator)(struct iovec[3]);

static struct Buffer inbuf_actual;
static struct Buffer inbuf;
static struct Buffer oldin;
static struct Buffer hdrbuf;
static struct timeval timeout;
static struct Buffer effectivepath;

static struct Url url;
static struct HttpMessage msg;

static struct stat zst;
static long double startread;
static long double lastrefresh;
static long double startserver;
static long double startrequest;
static long double lastheartbeat;
static long double startconnection;
static struct sockaddr_in clientaddr;
static struct sockaddr_in *serveraddr;

static mbedtls_ssl_config conf;
static mbedtls_ssl_context ssl;
static mbedtls_ctr_drbg_context rng;
static mbedtls_ssl_ticket_context ssltick;

static mbedtls_ssl_config confcli;
static mbedtls_ssl_context sslcli;
static mbedtls_ctr_drbg_context rngcli;

static struct TlsBio g_bio;
static char slashpath[PATH_MAX];
static struct DeflateGenerator dg;

static char *Route(const char *, size_t, const char *, size_t);
static char *RouteHost(const char *, size_t, const char *, size_t);
static char *RoutePath(const char *, size_t);
static char *HandleAsset(struct Asset *, const char *, size_t);
static char *ServeAsset(struct Asset *, const char *, size_t);
static char *SetStatus(unsigned, const char *);

static void TlsInit(void);

static void OnChld(void) {
  zombied = true;
}

static void OnUsr1(void) {
  invalidated = true;
}

static void OnUsr2(void) {
  meltdown = true;
}

static void OnTerm(int sig) {
  if (!terminated) {
    shutdownsig = sig;
    terminated = true;
  } else {
    killed = true;
  }
}

static void OnInt(int sig) {
  OnTerm(sig);
}

static void OnHup(int sig) {
  if (daemonize) {
    OnUsr1();
  } else {
    OnTerm(sig);
  }
}

static void Free(void *p) {
  free(*(void **)p);
  *(void **)p = 0;
}

static long ParseInt(const char *s) {
  return strtol(s, 0, 0);
}

static void *FreeLater(void *p) {
  if (p) {
    if (++freelist.n > freelist.c) {
      freelist.c = freelist.n + (freelist.n >> 1);
      freelist.p = xrealloc(freelist.p, freelist.c * sizeof(*freelist.p));
    }
    freelist.p[freelist.n - 1] = p;
  }
  return p;
}

static void UnmapLater(int f, void *p, size_t n) {
  if (++unmaplist.n > unmaplist.c) {
    unmaplist.c = unmaplist.n + (unmaplist.n >> 1);
    unmaplist.p = xrealloc(unmaplist.p, unmaplist.c * sizeof(*unmaplist.p));
  }
  unmaplist.p[unmaplist.n - 1].f = f;
  unmaplist.p[unmaplist.n - 1].p = p;
  unmaplist.p[unmaplist.n - 1].n = n;
}

static void CollectGarbage(void) {
  __log_level = oldloglevel;
  DestroyHttpMessage(&msg);
  while (freelist.n) {
    free(freelist.p[--freelist.n]);
  }
  while (unmaplist.n) {
    --unmaplist.n;
    LOGIFNEG1(munmap(unmaplist.p[unmaplist.n].p, unmaplist.p[unmaplist.n].n));
    LOGIFNEG1(close(unmaplist.p[unmaplist.n].f));
  }
}

static void UseOutput(void) {
  content = FreeLater(outbuf);
  contentlength = appendz(outbuf).i;
  outbuf = 0;
}

static void DropOutput(void) {
  FreeLater(outbuf);
  outbuf = 0;
}

static char *MergePaths(const char *p, size_t n, const char *q, size_t m,
                        size_t *z) {
  char *r;
  if (n && p[n - 1] == '/') --n;
  if (m && q[0] == '/') ++q, --m;
  r = xmalloc(n + 1 + m + 1);
  mempcpy(mempcpy(mempcpy(mempcpy(r, p, n), "/", 1), q, m), "", 1);
  if (z) *z = n + 1 + m;
  return r;
}

static long FindRedirect(const char *s, size_t n) {
  int c, m, l, r, z;
  l = 0;
  r = redirects.n - 1;
  while (l <= r) {
    m = (l + r) >> 1;
    c = CompareSlices(redirects.p[m].path.s, redirects.p[m].path.n, s, n);
    if (c < 0) {
      l = m + 1;
    } else if (c > 0) {
      r = m - 1;
    } else {
      return m;
    }
  }
  return -1;
}

static mbedtls_x509_crt *GetTrustedCertificate(mbedtls_x509_name *name) {
  size_t i;
  for (i = 0; i < certs.n; ++i) {
    if (certs.p[i].cert &&
        !mbedtls_x509_name_cmp(name, &certs.p[i].cert->subject)) {
      return certs.p[i].cert;
    }
  }
  return 0;
}

static void UseCertificate(mbedtls_ssl_config *c, struct Cert *kp,
                           const char *role) {
  VERBOSEF("(ssl) using %s certificate %`'s for HTTPS %s",
           mbedtls_pk_get_name(&kp->cert->pk),
           gc(FormatX509Name(&kp->cert->subject)), role);
  CHECK_EQ(0, mbedtls_ssl_conf_own_cert(c, kp->cert, kp->key));
}

static void AppendCert(mbedtls_x509_crt *cert, mbedtls_pk_context *key) {
  certs.p = realloc(certs.p, ++certs.n * sizeof(*certs.p));
  certs.p[certs.n - 1].cert = cert;
  certs.p[certs.n - 1].key = key;
}

static void InternCertificate(mbedtls_x509_crt *cert, mbedtls_x509_crt *prev) {
  int r;
  size_t i;
  if (cert->next) InternCertificate(cert->next, cert);
  if (prev) {
    if (mbedtls_x509_crt_check_parent(prev, cert, 1)) {
      DEBUGF("(ssl) unbundling %`'s from %`'s",
             gc(FormatX509Name(&prev->subject)),
             gc(FormatX509Name(&cert->subject)));
      prev->next = 0;
    } else if ((r = mbedtls_x509_crt_check_signature(prev, cert, 0))) {
      WARNF("(ssl) invalid signature for %`'s -> %`'s (-0x%04x)",
            gc(FormatX509Name(&prev->subject)),
            gc(FormatX509Name(&cert->subject)), -r);
    }
  }
  if (mbedtls_x509_time_is_past(&cert->valid_to)) {
    WARNF("(ssl) certificate %`'s is expired",
          gc(FormatX509Name(&cert->subject)));
  } else if (mbedtls_x509_time_is_future(&cert->valid_from)) {
    WARNF("(ssl) certificate %`'s is from the future",
          gc(FormatX509Name(&cert->subject)));
  }
  for (i = 0; i < certs.n; ++i) {
    if (!certs.p[i].cert) continue;
    if (mbedtls_pk_get_type(&cert->pk) ==
            mbedtls_pk_get_type(&certs.p[i].cert->pk) &&
        !mbedtls_x509_name_cmp(&cert->subject, &certs.p[i].cert->subject)) {
      VERBOSEF("(ssl) %s %`'s is already loaded",
               mbedtls_pk_get_name(&cert->pk),
               gc(FormatX509Name(&cert->subject)));
      return;
    }
  }
  for (i = 0; i < certs.n; ++i) {
    if (!certs.p[i].cert && certs.p[i].key &&
        !mbedtls_pk_check_pair(&cert->pk, certs.p[i].key)) {
      certs.p[i].cert = cert;
      return;
    }
  }
  LogCertificate("loaded certificate", cert);
  if (!cert->next && !IsSelfSigned(cert) && cert->max_pathlen) {
    for (i = 0; i < certs.n; ++i) {
      if (!certs.p[i].cert) continue;
      if (mbedtls_pk_can_do(&cert->pk, certs.p[i].cert->sig_pk) &&
          !mbedtls_x509_crt_check_parent(cert, certs.p[i].cert, 1) &&
          !IsSelfSigned(certs.p[i].cert)) {
        if (ChainCertificate(cert, certs.p[i].cert)) break;
      }
    }
  }
  if (!IsSelfSigned(cert)) {
    for (i = 0; i < certs.n; ++i) {
      if (!certs.p[i].cert) continue;
      if (certs.p[i].cert->next) continue;
      if (certs.p[i].cert->max_pathlen &&
          mbedtls_pk_can_do(&certs.p[i].cert->pk, cert->sig_pk) &&
          !mbedtls_x509_crt_check_parent(certs.p[i].cert, cert, 1)) {
        ChainCertificate(certs.p[i].cert, cert);
      }
    }
  }
  AppendCert(cert, 0);
}

static void ProgramCertificate(const char *p, size_t n) {
  int rc;
  unsigned char *waqapi;
  mbedtls_x509_crt *cert;
  waqapi = malloc(n + 1);
  memcpy(waqapi, p, n);
  waqapi[n] = 0;
  cert = calloc(1, sizeof(mbedtls_x509_crt));
  rc = mbedtls_x509_crt_parse(cert, waqapi, n + 1);
  mbedtls_platform_zeroize(waqapi, n);
  free(waqapi);
  if (rc < 0) {
    WARNF("(ssl) failed to load certificate (grep -0x%04x)", rc);
    return;
  } else if (rc > 0) {
    VERBOSEF("(ssl) certificate bundle partially loaded");
  }
  InternCertificate(cert, 0);
}

static void ProgramPrivateKey(const char *p, size_t n) {
  int rc;
  size_t i;
  unsigned char *waqapi;
  mbedtls_pk_context *key;
  waqapi = malloc(n + 1);
  memcpy(waqapi, p, n);
  waqapi[n] = 0;
  key = calloc(1, sizeof(mbedtls_pk_context));
  rc = mbedtls_pk_parse_key(key, waqapi, n + 1, 0, 0);
  mbedtls_platform_zeroize(waqapi, n);
  free(waqapi);
  if (rc != 0) FATALF("(ssl) error: load key (grep -0x%04x)", -rc);
  for (i = 0; i < certs.n; ++i) {
    if (certs.p[i].cert && !certs.p[i].key &&
        !mbedtls_pk_check_pair(&certs.p[i].cert->pk, key)) {
      certs.p[i].key = key;
      return;
    }
  }
  VERBOSEF("(ssl) loaded private key");
  AppendCert(0, key);
}

static void ProgramFile(const char *path, void program(const char *, size_t)) {
  char *p;
  size_t n;
  DEBUGF("(srvr) ProgramFile(%`'s)", path);
  if ((p = xslurp(path, &n))) {
    program(p, n);
    mbedtls_platform_zeroize(p, n);
    free(p);
  } else {
    FATALF("(srvr) error: failed to read file %`'s", path);
  }
}

static void ProgramPort(long port) {
  if (!(0 <= port && port <= 65535)) {
    FATALF("(cfg) error: bad port: %d", port);
  }
  if (port == 443) listeningonport443 = true;
  ports.p = realloc(ports.p, ++ports.n * sizeof(*ports.p));
  ports.p[ports.n - 1] = port;
}

static void ProgramMaxPayloadSize(long x) {
  maxpayloadsize = MAX(1450, x);
}

static void ProgramSslTicketLifetime(long x) {
  sslticketlifetime = x;
}

static void ProgramAddr(const char *addr) {
  ssize_t rc;
  int64_t ip;
  if ((ip = ParseIp(addr, -1)) == -1) {
    if (!IsTiny()) {
      struct addrinfo *ai = NULL;
      struct addrinfo hint = {AI_NUMERICSERV, AF_INET, SOCK_STREAM,
                              IPPROTO_TCP};
      if ((rc = getaddrinfo(addr, "0", &hint, &ai)) != EAI_SUCCESS) {
        FATALF("(cfg) error: bad addr: %s (EAI_%s)", addr, gai_strerror(rc));
      }
      ip = ntohl(ai->ai_addr4->sin_addr.s_addr);
      freeaddrinfo(ai);
    } else {
      FATALF("(cfg) error: ProgramAddr() needs an IP in MODE=tiny: %s", addr);
    }
  }
  ips.p = realloc(ips.p, ++ips.n * sizeof(*ips.p));
  ips.p[ips.n - 1] = ip;
}

static void ProgramRedirect(int code, const char *sp, size_t sn, const char *dp,
                            size_t dn) {
  long i, j;
  struct Redirect r;
  if (code && code != 301 && code != 302 && code != 307 && code != 308) {
    FATALF("(cfg) error: unsupported redirect code %d", code);
  }

  if (!(FreeLater(EncodeHttpHeaderValue(dp, dn, 0)))) {
    FATALF("(cfg) error: invalid location %s", dp);
  }

  r.code = code;
  r.path.s = sp;
  r.path.n = sn;
  r.location.s = dp;
  r.location.n = dn;
  if ((i = FindRedirect(r.path.s, r.path.n)) != -1) {
    redirects.p[i] = r;
  } else {
    i = redirects.n;
    redirects.p = xrealloc(redirects.p, (i + 1) * sizeof(*redirects.p));
    for (j = i; j; --j) {
      if (CompareSlices(r.path.s, r.path.n, redirects.p[j - 1].path.s,
                        redirects.p[j - 1].path.n) < 0) {
        redirects.p[j] = redirects.p[j - 1];
      } else {
        break;
      }
    }
    redirects.p[j] = r;
    ++redirects.n;
  }
}

static void ProgramRedirectArg(int code, const char *s) {
  size_t n;
  const char *p;
  n = strlen(s);
  if (!(p = memchr(s, '=', n))) {
    FATALF("(cfg) error: redirect arg missing '='");
  }
  ProgramRedirect(code, s, p - s, p + 1, n - (p - s + 1));
}

static void DescribeAddress(char buf[40], uint32_t addr, uint16_t port) {
  char *p;
  const char *s;
  p = buf;
  p = FormatUint32(p, (addr & 0xFF000000) >> 030), *p++ = '.';
  p = FormatUint32(p, (addr & 0x00FF0000) >> 020), *p++ = '.';
  p = FormatUint32(p, (addr & 0x0000FF00) >> 010), *p++ = '.';
  p = FormatUint32(p, (addr & 0x000000FF) >> 000), *p++ = ':';
  p = FormatUint32(p, port);
  *p = '\0';
  assert(p - buf < 40);
}

static inline void GetServerAddr(uint32_t *ip, uint16_t *port) {
  *ip = ntohl(serveraddr->sin_addr.s_addr);
  if (port) *port = ntohs(serveraddr->sin_port);
}

static inline void GetClientAddr(uint32_t *ip, uint16_t *port) {
  *ip = ntohl(clientaddr.sin_addr.s_addr);
  if (port) *port = ntohs(clientaddr.sin_port);
}

static inline void GetRemoteAddr(uint32_t *ip, uint16_t *port) {
  GetClientAddr(ip, port);
  if (HasHeader(kHttpXForwardedFor) &&
      (IsPrivateIp(*ip) || IsLoopbackIp(*ip))) {
    if (ParseForwarded(HeaderData(kHttpXForwardedFor),
                       HeaderLength(kHttpXForwardedFor), ip, port) == -1)
      WARNF("(srvr) invalid X-Forwarded-For value: %`'.*s",
            HeaderLength(kHttpXForwardedFor), HeaderData(kHttpXForwardedFor));
  }
}

static char *DescribeClient(void) {
  uint32_t ip;
  uint16_t port;
  static char clientaddrstr[40];
  GetRemoteAddr(&ip, &port);
  DescribeAddress(clientaddrstr, ip, port);
  return clientaddrstr;
}

static char *DescribeServer(void) {
  uint32_t ip;
  uint16_t port;
  static char serveraddrstr[40];
  GetServerAddr(&ip, &port);
  DescribeAddress(serveraddrstr, ip, port);
  return serveraddrstr;
}

static void ProgramBrand(const char *s) {
  char *p;
  free(brand);
  free(serverheader);
  if (!(p = EncodeHttpHeaderValue(s, -1, 0))) {
    FATALF("(cfg) error: brand isn't latin1 encodable: %`'s", s);
  }
  brand = strdup(s);
  serverheader = xasprintf("Server: %s\r\n", p);
  free(p);
}

static void ProgramUid(long x) {
  changeuid = x;
}

static void ProgramGid(long x) {
  changegid = x;
}

#define MINTIMEOUT 10
static void ProgramTimeout(long ms) {
  ldiv_t d;
  if (ms < 0) {
    timeout.tv_sec = ms; /* -(keepalive seconds) */
    timeout.tv_usec = 0;
  } else {
    if (ms < MINTIMEOUT) {
      FATALF("(cfg) error: timeout needs to be %dms or greater", MINTIMEOUT);
    }
    d = ldiv(ms, 1000);
    timeout.tv_sec = d.quot;
    timeout.tv_usec = d.rem * 1000;
  }
}

static void ProgramCache(long x) {
  cacheseconds = x;
}

static void SetDefaults(void) {
  ProgramBrand(gc(xasprintf("%s/%hhd.%hhd.%hhd", REDBEAN, VERSION >> 020,
                            VERSION >> 010, VERSION >> 000)));
  __log_level = kLogInfo;
  maxpayloadsize = 64 * 1024;
  ProgramCache(-1);
  ProgramTimeout(60 * 1000);
  ProgramSslTicketLifetime(24 * 60 * 60);
  sslfetchverify = true;
}

static void AddString(struct Strings *l, const char *s, size_t n) {
  if (++l->n > l->c) {
    l->c = l->n + (l->n >> 1);
    l->p = realloc(l->p, l->c * sizeof(*l->p));
  }
  l->p[l->n - 1].s = s;
  l->p[l->n - 1].n = n;
}

static bool HasString(struct Strings *l, const char *s, size_t n) {
  size_t i;
  for (i = 0; i < l->n; ++i) {
    if (SlicesEqual(l->p[i].s, l->p[i].n, s, n)) {
      return true;
    }
  }
  return false;
}

static void ProgramDirectory(const char *path) {
  char *s;
  size_t n;
  struct stat st;
  if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
    FATALF("(cfg) error: not a directory: %`'s", path);
  }
  s = strdup(path);
  n = strlen(s);
  INFOF("(cfg) program directory: %s", s);
  AddString(&stagedirs, s, n);
}

static void ProgramHeader(const char *s) {
  char *p, *v, *h;
  if ((p = strchr(s, ':')) && IsValidHttpToken(s, p - s) &&
      (v = EncodeLatin1(p + 1, -1, 0, kControlC0 | kControlC1 | kControlWs))) {
    switch (GetHttpHeader(s, p - s)) {
      case kHttpDate:
      case kHttpConnection:
      case kHttpContentLength:
      case kHttpContentEncoding:
      case kHttpContentRange:
      case kHttpLocation:
        FATALF("(cfg) error: can't program header: %`'s", s);
      case kHttpServer:
        ProgramBrand(p + 1);
        break;
      default:
        p = xasprintf("%s%.*s:%s\r\n", extrahdrs ? extrahdrs : "", p - s, s, v);
        free(extrahdrs);
        extrahdrs = p;
        break;
    }
    free(v);
  } else {
    FATALF("(cfg) error: illegal header: %`'s", s);
  }
}

static void ProgramLogPath(const char *s) {
  logpath = strdup(s);
}

static void ProgramPidPath(const char *s) {
  pidpath = strdup(s);
}

static bool IsServerFd(int fd) {
  size_t i;
  for (i = 0; i < servers.n; ++i) {
    if (servers.p[i].fd == fd) {
      return true;
    }
  }
  return false;
}

static void ChangeUser(void) {
  if (changegid) {
    if (setgid(changegid)) {
      FATALF("(cfg) setgid() failed: %m");
    }
  }
  // order matters
  if (changeuid) {
    if (setuid(changeuid)) {
      FATALF("(cfg) setuid() failed: %m");
    }
  }
}

static void Daemonize(void) {
  char ibuf[21];
  int i, fd, pid;
  for (i = 0; i < 256; ++i) {
    if (!IsServerFd(i)) {
      close(i);
    }
  }
  if ((pid = fork()) > 0) exit(0);
  setsid();
  if ((pid = fork()) > 0) _exit(0);
  umask(0);
  if (pidpath) {
    fd = open(pidpath, O_CREAT | O_WRONLY, 0644);
    WRITE(fd, ibuf, FormatInt32(ibuf, getpid()) - ibuf);
    close(fd);
  }
  if (!logpath) ProgramLogPath("/dev/null");
  open("/dev/null", O_RDONLY);
  open(logpath, O_APPEND | O_WRONLY | O_CREAT, 0640);
  dup2(1, 2);
  ChangeUser();
}

static void LogLuaError(char *hook, char *err) {
  ERRORF("(lua) failed to run %s: %s", hook, err);
}

static bool LuaEvalCode(const char *code) {
  lua_State *L = GL;
  int status = luaL_loadstring(L, code);
  if (status != LUA_OK || LuaCallWithTrace(L, 0, 0, NULL) != LUA_OK) {
    LogLuaError("lua code", lua_tostring(L, -1));
    lua_pop(L, 1);  // pop error
    return false;
  }
  AssertLuaStackIsAt(L, 0);
  return true;
}

static bool LuaEvalFile(const char *path) {
  return LuaEvalCode(gc(xslurp(path, 0)));
}

static bool LuaOnClientConnection(void) {
#ifndef STATIC
  bool dropit;
  uint32_t ip, serverip;
  uint16_t port, serverport;
  lua_State *L = GL;
  lua_getglobal(L, "OnClientConnection");
  GetClientAddr(&ip, &port);
  GetServerAddr(&serverip, &serverport);
  lua_pushinteger(L, ip);
  lua_pushinteger(L, port);
  lua_pushinteger(L, serverip);
  lua_pushinteger(L, serverport);
  if (LuaCallWithTrace(L, 4, 1, NULL) == LUA_OK) {
    dropit = lua_toboolean(L, -1);
  } else {
    LogLuaError("OnClientConnection", lua_tostring(L, -1));
    dropit = false;
  }
  lua_pop(L, 1);  // pop result or error
  AssertLuaStackIsAt(L, 0);
  return dropit;
#else
  return false;
#endif
}

static void LuaOnProcessCreate(int pid) {
#ifndef STATIC
  uint32_t ip, serverip;
  uint16_t port, serverport;
  lua_State *L = GL;
  lua_getglobal(L, "OnProcessCreate");
  GetClientAddr(&ip, &port);
  GetServerAddr(&serverip, &serverport);
  lua_pushinteger(L, pid);
  lua_pushinteger(L, ip);
  lua_pushinteger(L, port);
  lua_pushinteger(L, serverip);
  lua_pushinteger(L, serverport);
  if (LuaCallWithTrace(L, 5, 0, NULL) != LUA_OK) {
    LogLuaError("OnProcessCreate", lua_tostring(L, -1));
    lua_pop(L, 1);  // pop error
  }
  AssertLuaStackIsAt(L, 0);
#endif
}

static void LuaOnProcessDestroy(int pid) {
#ifndef STATIC
  lua_State *L = GL;
  lua_getglobal(L, "OnProcessDestroy");
  lua_pushinteger(L, pid);
  if (LuaCallWithTrace(L, 1, 0, NULL) != LUA_OK) {
    LogLuaError("OnProcessDestroy", lua_tostring(L, -1));
    lua_pop(L, 1);  // pop error
  }
  AssertLuaStackIsAt(L, 0);
#endif
}

static inline bool IsHookDefined(const char *s) {
#ifndef STATIC
  lua_State *L = GL;
  bool res = !!lua_getglobal(L, s);
  lua_pop(L, 1);
  return res;
#else
  return false;
#endif
}

static void CallSimpleHook(const char *s) {
#ifndef STATIC
  lua_State *L = GL;
  int n = lua_gettop(L);
  lua_getglobal(L, s);
  if (LuaCallWithTrace(L, 0, 0, NULL) != LUA_OK) {
    LogLuaError(s, lua_tostring(L, -1));
    lua_pop(L, 1);  // pop error
  }
  AssertLuaStackIsAt(L, n);
#endif
}

static void CallSimpleHookIfDefined(const char *s) {
  if (IsHookDefined(s)) {
    CallSimpleHook(s);
  }
}

static void ReportWorkerExit(int pid, int ws) {
  int workers;
  workers = atomic_fetch_sub(&shared->workers, 1) - 1;
  if (WIFEXITED(ws)) {
    if (WEXITSTATUS(ws)) {
      LockInc(&shared->c.failedchildren);
      WARNF("(stat) %d exited with %d (%,d workers remain)", pid,
            WEXITSTATUS(ws), workers);
    } else {
      DEBUGF("(stat) %d exited (%,d workers remain)", pid, workers);
    }
  } else {
    LockInc(&shared->c.terminatedchildren);
    WARNF("(stat) %d terminated with %s (%,d workers remain)", pid,
          strsignal(WTERMSIG(ws)), workers);
  }
}

static void ReportWorkerResources(int pid, struct rusage *ru) {
  char *s, *b = 0;
  if (logrusage || LOGGABLE(kLogDebug)) {
    AppendResourceReport(&b, ru, "\n");
    if (b) {
      if ((s = IndentLines(b, appendz(b).i - 1, 0, 1))) {
        LOGF(kLogDebug, "(stat) resource report for pid %d\n%s", pid, s);
        free(s);
      }
      free(b);
    }
  }
}

static void HandleWorkerExit(int pid, int ws, struct rusage *ru) {
  LockInc(&shared->c.connectionshandled);
  AddRusage(&shared->children, ru);
  ReportWorkerExit(pid, ws);
  ReportWorkerResources(pid, ru);
  if (hasonprocessdestroy) {
    LuaOnProcessDestroy(pid);
  }
}

static void KillGroupImpl(int sig) {
  LOGIFNEG1(kill(0, sig));
}

static void KillGroup(void) {
  KillGroupImpl(SIGTERM);
}

static void WaitAll(void) {
  int ws, pid;
  struct rusage ru;
  for (;;) {
    if ((pid = wait4(-1, &ws, 0, &ru)) != -1) {
      HandleWorkerExit(pid, ws, &ru);
    } else {
      if (errno == ECHILD) {
        errno = 0;
        break;
      }
      if (errno == EINTR) {
        if (killed) {
          killed = false;
          terminated = false;
          WARNF("(srvr) server shall terminate harder");
          KillGroup();
        }
        errno = 0;
        continue;
      }
      DIEF("(srvr) wait error: %m");
    }
  }
}

static void ReapZombies(void) {
  int ws, pid;
  struct rusage ru;
  do {
    zombied = false;
    if ((pid = wait4(-1, &ws, WNOHANG, &ru)) != -1) {
      if (pid) {
        HandleWorkerExit(pid, ws, &ru);
      } else {
        break;
      }
    } else {
      if (errno == ECHILD) {
        errno = 0;
        break;
      }
      if (errno == EINTR) {
        errno = 0;
        continue;
      }
      DIEF("(srvr) wait error: %m");
    }
  } while (!terminated);
}

static ssize_t ReadAll(int fd, char *p, size_t n) {
  ssize_t rc;
  size_t i, got;
  for (i = 0; i < n;) {
    rc = READ(fd, p + i, n - i);
    if (rc != -1) {
      got = rc;
      i += got;
    } else if (errno != EINTR) {
      WARNF("(file) read error: %m");
      return -1;
    }
  }
  return i;
}

static ssize_t WritevAll(int fd, struct iovec *iov, int iovlen) {
  int i;
  ssize_t rc;
  size_t wrote, total;
  i = 0;
  total = 0;
  do {
    if (i) {
      while (i < iovlen && !iov[i].iov_len) ++i;
      if (i == iovlen) break;
    }
    if ((rc = writev(fd, iov + i, iovlen - i)) != -1) {
      wrote = rc;
      total += wrote;
      do {
        if (wrote >= iov[i].iov_len) {
          wrote -= iov[i++].iov_len;
        } else {
          iov[i].iov_base = (char *)iov[i].iov_base + wrote;
          iov[i].iov_len -= wrote;
          wrote = 0;
        }
      } while (wrote);
    } else if (errno == EINTR) {
      errno = 0;
      LockInc(&shared->c.writeinterruputs);
      if (killed || (meltdown && nowl() - startread > 2)) {
        return total ? total : -1;
      }
    } else {
      return total ? total : -1;
    }
  } while (i < iovlen);
  return total;
}

static int TlsFlush(struct TlsBio *bio, const unsigned char *buf, size_t len) {
  struct iovec v[2];
  if (len || bio->c > 0) {
    v[0].iov_base = bio->u;
    v[0].iov_len = MAX(0, bio->c);
    v[1].iov_base = buf;
    v[1].iov_len = len;
    if (WritevAll(bio->fd, v, 2) != -1) {
      if (bio->c > 0) bio->c = 0;
    } else if (errno == EINTR) {
      errno = 0;
      return MBEDTLS_ERR_NET_CONN_RESET;
    } else if (errno == EAGAIN) {
      errno = 0;
      return MBEDTLS_ERR_SSL_TIMEOUT;
    } else if (errno == EPIPE || errno == ECONNRESET || errno == ENETRESET) {
      return MBEDTLS_ERR_NET_CONN_RESET;
    } else {
      WARNF("(ssl) TlsSend error: %m");
      return MBEDTLS_ERR_NET_SEND_FAILED;
    }
  }
  return 0;
}

static int TlsSend(void *ctx, const unsigned char *buf, size_t len) {
  int rc;
  struct iovec v[2];
  struct TlsBio *bio = ctx;
  if (bio->c >= 0 && bio->c + len <= sizeof(bio->u)) {
    memcpy(bio->u + bio->c, buf, len);
    bio->c += len;
    return len;
  }
  if ((rc = TlsFlush(bio, buf, len)) < 0) return rc;
  return len;
}

static int TlsRecvImpl(void *ctx, unsigned char *p, size_t n, uint32_t o) {
  int r;
  ssize_t s;
  struct iovec v[2];
  struct TlsBio *bio = ctx;
  if ((r = TlsFlush(bio, 0, 0)) < 0) return r;
  if (bio->a < bio->b) {
    r = MIN(n, bio->b - bio->a);
    memcpy(p, bio->t + bio->a, r);
    if ((bio->a += r) == bio->b) bio->a = bio->b = 0;
    return r;
  }
  v[0].iov_base = p;
  v[0].iov_len = n;
  v[1].iov_base = bio->t;
  v[1].iov_len = sizeof(bio->t);
  while ((r = readv(bio->fd, v, 2)) == -1) {
    if (errno == EINTR) {
      errno = 0;
      return MBEDTLS_ERR_SSL_WANT_READ;
    } else if (errno == EAGAIN) {
      errno = 0;
      return MBEDTLS_ERR_SSL_TIMEOUT;
    } else if (errno == EPIPE || errno == ECONNRESET || errno == ENETRESET) {
      return MBEDTLS_ERR_NET_CONN_RESET;
    } else {
      WARNF("(ssl) tls read() error: %m");
      return MBEDTLS_ERR_NET_RECV_FAILED;
    }
  }
  if (r > n) bio->b = r - n;
  return MIN(n, r);
}

static int TlsRecv(void *ctx, unsigned char *buf, size_t len, uint32_t tmo) {
  int rc;
  struct TlsBio *bio = ctx;
  if (oldin.n) {
    rc = MIN(oldin.n, len);
    memcpy(buf, oldin.p, rc);
    oldin.p += rc;
    oldin.n -= rc;
    return rc;
  }
  return TlsRecvImpl(ctx, buf, len, tmo);
}

static ssize_t SslRead(int fd, void *buf, size_t size) {
  int rc;
  rc = mbedtls_ssl_read(&ssl, buf, size);
  if (!rc) {
    errno = ECONNRESET;
    rc = -1;
  } else if (rc < 0) {
    if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
      rc = 0;
    } else if (rc == MBEDTLS_ERR_NET_CONN_RESET ||
               rc == MBEDTLS_ERR_SSL_TIMEOUT) {
      errno = ECONNRESET;
      rc = -1;
    } else if (rc == MBEDTLS_ERR_SSL_WANT_READ) {
      errno = EINTR;
      rc = -1;
      errno = 0;
    } else if (rc == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE) {
      WARNF("(ssl) %s SslRead error -0x%04x (%s)", DescribeClient(), -rc,
            "fatal alert message");
      errno = EIO;
      rc = -1;
    } else if (rc == MBEDTLS_ERR_SSL_INVALID_RECORD) {
      WARNF("(ssl) %s SslRead error -0x%04x (%s)", DescribeClient(), -rc,
            "invalid record");
      errno = EIO;
      rc = -1;
    } else if (rc == MBEDTLS_ERR_SSL_INVALID_MAC) {
      WARNF("(ssl) %s SslRead error -0x%04x (%s)", DescribeClient(), -rc,
            "hmac verification failed");
      errno = EIO;
      rc = -1;
    } else {
      WARNF("(ssl) %s SslRead error -0x%04x", DescribeClient(), -rc);
      errno = EIO;
      rc = -1;
    }
  }
  return rc;
}

static ssize_t SslWrite(int fd, struct iovec *iov, int iovlen) {
  int i;
  size_t n;
  ssize_t rc;
  const unsigned char *p;
  for (i = 0; i < iovlen; ++i) {
    p = iov[i].iov_base;
    n = iov[i].iov_len;
    while (n) {
      if ((rc = mbedtls_ssl_write(&ssl, p, n)) > 0) {
        p += rc;
        n -= rc;
      } else if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
        errno = ECONNRESET;
        return -1;
      } else if (rc == MBEDTLS_ERR_SSL_TIMEOUT) {
        errno = ETIMEDOUT;
        return -1;
      } else {
        WARNF("(ssl) %s SslWrite error -0x%04x", DescribeClient(), -rc);
        errno = EIO;
        return -1;
      }
    }
  }
  return 0;
}

static void NotifyClose(void) {
#ifndef UNSECURE
  if (usingssl) {
    DEBUGF("(ssl) SSL notifying close");
    mbedtls_ssl_close_notify(&ssl);
  }
#endif
}

static void WipeSigningKeys(void) {
  size_t i;
  if (uniprocess) return;
  for (i = 0; i < certs.n; ++i) {
    if (!certs.p[i].key) continue;
    if (!certs.p[i].cert) continue;
    if (!certs.p[i].cert->ca_istrue) continue;
    mbedtls_pk_free(certs.p[i].key);
    Free(&certs.p[i].key);
  }
}

static void PsksDestroy(void) {
  size_t i;
  for (i = 0; i < psks.n; ++i) {
    mbedtls_platform_zeroize(psks.p[i].key, psks.p[i].key_len);
    free(psks.p[i].key);
    free(psks.p[i].identity);
  }
  Free(&psks.p);
  psks.n = 0;
}

static void CertsDestroy(void) {
  size_t i;
  // break up certificate chains to prevent double free
  for (i = 0; i < certs.n; ++i) {
    if (certs.p[i].cert) {
      certs.p[i].cert->next = 0;
    }
  }
  for (i = 0; i < certs.n; ++i) {
    mbedtls_x509_crt_free(certs.p[i].cert);
    free(certs.p[i].cert);
    mbedtls_pk_free(certs.p[i].key);
    free(certs.p[i].key);
  }
  Free(&certs.p);
  certs.n = 0;
}

static void WipeServingKeys(void) {
  size_t i;
  long double t = nowl();
  if (uniprocess) return;
  mbedtls_ssl_ticket_free(&ssltick);
  mbedtls_ssl_key_cert_free(conf.key_cert), conf.key_cert = 0;
  CertsDestroy();
  PsksDestroy();
}

static bool CertHasCommonName(const mbedtls_x509_crt *cert, const void *s,
                              size_t n) {
  const mbedtls_x509_name *name;
  for (name = &cert->subject; name; name = name->next) {
    if (!MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &name->oid)) {
      if (SlicesEqualCase(s, n, name->val.p, name->val.len)) {
        return true;
      }
      break;
    }
  }
  return false;
}

static bool TlsRouteFind(mbedtls_pk_type_t type, mbedtls_ssl_context *ssl,
                         const unsigned char *host, size_t size, int64_t ip) {
  int i;
  for (i = 0; i < certs.n; ++i) {
    if (IsServerCert(certs.p + i, type) &&
        (((certs.p[i].cert->ext_types & MBEDTLS_X509_EXT_SUBJECT_ALT_NAME) &&
          (ip == -1 ? CertHasHost(certs.p[i].cert, host, size)
                    : CertHasIp(certs.p[i].cert, ip))) ||
         CertHasCommonName(certs.p[i].cert, host, size))) {
      CHECK_EQ(
          0, mbedtls_ssl_set_hs_own_cert(ssl, certs.p[i].cert, certs.p[i].key));
      DEBUGF("(ssl) TlsRoute(%s, %`'.*s) %s %`'s", mbedtls_pk_type_name(type),
             size, host, mbedtls_pk_get_name(&certs.p[i].cert->pk),
             gc(FormatX509Name(&certs.p[i].cert->subject)));
      return true;
    }
  }
  return false;
}

static bool TlsRouteFirst(mbedtls_pk_type_t type, mbedtls_ssl_context *ssl) {
  int i;
  for (i = 0; i < certs.n; ++i) {
    if (IsServerCert(certs.p + i, type)) {
      CHECK_EQ(
          0, mbedtls_ssl_set_hs_own_cert(ssl, certs.p[i].cert, certs.p[i].key));
      DEBUGF("(ssl) TlsRoute(%s) %s %`'s", mbedtls_pk_type_name(type),
             mbedtls_pk_get_name(&certs.p[i].cert->pk),
             gc(FormatX509Name(&certs.p[i].cert->subject)));
      return true;
    }
  }
  return false;
}

static int TlsRoute(void *ctx, mbedtls_ssl_context *ssl,
                    const unsigned char *host, size_t size) {
  int64_t ip;
  bool ok1, ok2;
  ip = ParseIp((const char *)host, size);
  ok1 = TlsRouteFind(MBEDTLS_PK_ECKEY, ssl, host, size, ip);
  ok2 = TlsRouteFind(MBEDTLS_PK_RSA, ssl, host, size, ip);
  if (!ok1 && !ok2) {
    WARNF("(ssl) TlsRoute(%`'.*s) not found", size, host);
    ok1 = TlsRouteFirst(MBEDTLS_PK_ECKEY, ssl);
    ok2 = TlsRouteFirst(MBEDTLS_PK_RSA, ssl);
  }
  return ok1 || ok2 ? 0 : -1;
}

static int TlsRoutePsk(void *ctx, mbedtls_ssl_context *ssl,
                       const unsigned char *identity, size_t identity_len) {
  size_t i;
  for (i = 0; i < psks.n; ++i) {
    if (SlicesEqual((void *)identity, identity_len, psks.p[i].identity,
                    psks.p[i].identity_len)) {
      DEBUGF("(ssl) TlsRoutePsk(%`'.*s)", identity_len, identity);
      mbedtls_ssl_set_hs_psk(ssl, psks.p[i].key, psks.p[i].key_len);
      // keep track of selected psk to report its identity
      sslpskindex = i + 1;  // use index+1 to check against 0 (when not set)
      return 0;
    }
  }
  WARNF("(ssl) TlsRoutePsk(%`'.*s) not found", identity_len, identity);
  return -1;
}

static bool TlsSetup(void) {
  int r;
  oldin.p = inbuf.p;
  oldin.n = amtread;
  inbuf.p += amtread;
  inbuf.n -= amtread;
  inbuf.c = amtread;
  amtread = 0;
  g_bio.fd = client;
  g_bio.a = 0;
  g_bio.b = 0;
  g_bio.c = 0;
  sslpskindex = 0;
  for (;;) {
    if (!(r = mbedtls_ssl_handshake(&ssl)) && TlsFlush(&g_bio, 0, 0) != -1) {
      LockInc(&shared->c.sslhandshakes);
      g_bio.c = -1;
      usingssl = true;
      reader = SslRead;
      writer = SslWrite;
      WipeServingKeys();
      VERBOSEF("(ssl) shaken %s %s %s%s %s", DescribeClient(),
               mbedtls_ssl_get_ciphersuite(&ssl), mbedtls_ssl_get_version(&ssl),
               ssl.session->compression ? " COMPRESSED" : "",
               ssl.curve ? ssl.curve->name : "uncurved");
      DEBUGF("(ssl) client ciphersuite preference was %s",
             gc(FormatSslClientCiphers(&ssl)));
      return true;
    } else if (r == MBEDTLS_ERR_SSL_WANT_READ) {
      LockInc(&shared->c.handshakeinterrupts);
      if (terminated || killed || (meltdown && nowl() - startread > 2)) {
        return false;
      }
    } else {
      LockInc(&shared->c.sslhandshakefails);
      mbedtls_ssl_session_reset(&ssl);
      switch (r) {
        case MBEDTLS_ERR_SSL_CONN_EOF:
          DEBUGF("(ssl) %s SSL handshake EOF", DescribeClient());
          return false;
        case MBEDTLS_ERR_NET_CONN_RESET:
          DEBUGF("(ssl) %s SSL handshake reset", DescribeClient());
          return false;
        case MBEDTLS_ERR_SSL_TIMEOUT:
          LockInc(&shared->c.ssltimeouts);
          DEBUGF("(ssl) %s %s", DescribeClient(), "ssltimeouts");
          return false;
        case MBEDTLS_ERR_SSL_NO_CIPHER_CHOSEN:
          LockInc(&shared->c.sslnociphers);
          WARNF("(ssl) %s %s %s", DescribeClient(), "sslnociphers",
                gc(FormatSslClientCiphers(&ssl)));
          return false;
        case MBEDTLS_ERR_SSL_NO_USABLE_CIPHERSUITE:
          LockInc(&shared->c.sslcantciphers);
          WARNF("(ssl) %s %s %s", DescribeClient(), "sslcantciphers",
                gc(FormatSslClientCiphers(&ssl)));
          return false;
        case MBEDTLS_ERR_SSL_BAD_HS_PROTOCOL_VERSION:
          LockInc(&shared->c.sslnoversion);
          WARNF("(ssl) %s %s %s", DescribeClient(), "sslnoversion",
                mbedtls_ssl_get_version(&ssl));
          return false;
        case MBEDTLS_ERR_SSL_INVALID_MAC:
          LockInc(&shared->c.sslshakemacs);
          WARNF("(ssl) %s %s", DescribeClient(), "sslshakemacs");
          return false;
        case MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE:
          LockInc(&shared->c.sslnoclientcert);
          WARNF("(ssl) %s %s", DescribeClient(), "sslnoclientcert");
          NotifyClose();
          return false;
        case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
          LockInc(&shared->c.sslverifyfailed);
          WARNF("(ssl) %s SSL %s", DescribeClient(),
                gc(DescribeSslVerifyFailure(
                    ssl.session_negotiate->verify_result)));
          return false;
        case MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE:
          switch (ssl.fatal_alert) {
            case MBEDTLS_SSL_ALERT_MSG_CERT_UNKNOWN:
              LockInc(&shared->c.sslunknowncert);
              DEBUGF("(ssl) %s %s", DescribeClient(), "sslunknowncert");
              return false;
            case MBEDTLS_SSL_ALERT_MSG_UNKNOWN_CA:
              LockInc(&shared->c.sslunknownca);
              DEBUGF("(ssl) %s %s", DescribeClient(), "sslunknownca");
              return false;
            default:
              WARNF("(ssl) %s SSL shakealert %s", DescribeClient(),
                    GetAlertDescription(ssl.fatal_alert));
              return false;
          }
        default:
          WARNF("(ssl) %s SSL handshake failed -0x%04x", DescribeClient(), -r);
          return false;
      }
    }
  }
}

static void ConfigureCertificate(mbedtls_x509write_cert *cw, struct Cert *ca,
                                 int usage, int type) {
  int r;
  const char *s;
  bool isduplicate;
  size_t i, j, k, nsan;
  struct HostsTxt *htxt;
  struct mbedtls_san *san;
  const mbedtls_x509_name *xname;
  char *name, *subject, *issuer, notbefore[16], notafter[16], hbuf[256];
  san = 0;
  nsan = 0;
  name = 0;
  htxt = GetHostsTxt();
  strcpy(hbuf, "localhost");
  gethostname(hbuf, sizeof(hbuf));
  for (i = 0; i < htxt->entries.i; ++i) {
    for (j = 0; j < ips.n; ++j) {
      if (ips.p[j] == READ32BE(htxt->entries.p[i].ip)) {
        isduplicate = false;
        s = htxt->strings.p + htxt->entries.p[i].name;
        if (!name) name = s;
        for (k = 0; k < nsan; ++k) {
          if (san[k].tag == MBEDTLS_X509_SAN_DNS_NAME &&
              !strcasecmp(s, san[k].val)) {
            isduplicate = true;
            break;
          }
        }
        if (!isduplicate) {
          san = realloc(san, (nsan += 2) * sizeof(*san));
          san[nsan - 2].tag = MBEDTLS_X509_SAN_DNS_NAME;
          san[nsan - 2].val = s;
          san[nsan - 1].tag = MBEDTLS_X509_SAN_DNS_NAME;
          san[nsan - 1].val = gc(xasprintf("*.%s", s));
        }
      }
    }
  }
  for (i = 0; i < ips.n; ++i) {
    san = realloc(san, ++nsan * sizeof(*san));
    san[nsan - 1].tag = MBEDTLS_X509_SAN_IP_ADDRESS;
    san[nsan - 1].ip4 = ips.p[i];
  }
  ChooseCertificateLifetime(notbefore, notafter);
  subject = xasprintf("CN=%s", name ? name : hbuf);
  if (ca) {
    issuer = calloc(1, 1000);
    CHECK_GT(mbedtls_x509_dn_gets(issuer, 1000, &ca->cert->subject), 0);
  } else {
    issuer = strdup(subject);
  }
  if ((r = mbedtls_x509write_crt_set_subject_alternative_name(cw, san, nsan)) ||
      (r = mbedtls_x509write_crt_set_validity(cw, notbefore, notafter)) ||
      (r = mbedtls_x509write_crt_set_basic_constraints(cw, false, -1)) ||
#if defined(MBEDTLS_SHA1_C)
      (r = mbedtls_x509write_crt_set_subject_key_identifier(cw)) ||
      (r = mbedtls_x509write_crt_set_authority_key_identifier(cw)) ||
#endif
      (r = mbedtls_x509write_crt_set_key_usage(cw, usage)) ||
      (r = mbedtls_x509write_crt_set_ext_key_usage(cw, type)) ||
      (r = mbedtls_x509write_crt_set_subject_name(cw, subject)) ||
      (r = mbedtls_x509write_crt_set_issuer_name(cw, issuer))) {
    FATALF("(ssl) configure certificate (grep -0x%04x)", -r);
  }
  free(subject);
  free(issuer);
  free(san);
}

static struct Cert GetKeySigningKey(void) {
  size_t i;
  for (i = 0; i < certs.n; ++i) {
    if (!certs.p[i].key) continue;
    if (!certs.p[i].cert) continue;
    if (!certs.p[i].cert->ca_istrue) continue;
    if (mbedtls_x509_crt_check_key_usage(certs.p[i].cert,
                                         MBEDTLS_X509_KU_KEY_CERT_SIGN)) {
      continue;
    }
    return certs.p[i];
  }
  return (struct Cert){0};
}

static struct Cert GenerateEcpCertificate(struct Cert *ca) {
  mbedtls_pk_context *key;
  mbedtls_md_type_t md_alg;
  mbedtls_x509write_cert wcert;
  md_alg = suiteb ? MBEDTLS_MD_SHA384 : MBEDTLS_MD_SHA256;
  key = InitializeKey(ca, &wcert, md_alg, MBEDTLS_PK_ECKEY);
  CHECK_EQ(0, mbedtls_ecp_gen_key(
                  suiteb ? MBEDTLS_ECP_DP_SECP384R1 : MBEDTLS_ECP_DP_SECP256R1,
                  mbedtls_pk_ec(*key), GenerateHardRandom, 0));
  GenerateCertificateSerial(&wcert);
  ConfigureCertificate(&wcert, ca, MBEDTLS_X509_KU_DIGITAL_SIGNATURE,
                       MBEDTLS_X509_NS_CERT_TYPE_SSL_SERVER |
                           MBEDTLS_X509_NS_CERT_TYPE_SSL_CLIENT);
  return FinishCertificate(ca, &wcert, key);
}

static struct Cert GenerateRsaCertificate(struct Cert *ca) {
  mbedtls_pk_context *key;
  mbedtls_md_type_t md_alg;
  mbedtls_x509write_cert wcert;
  md_alg = suiteb ? MBEDTLS_MD_SHA384 : MBEDTLS_MD_SHA256;
  key = InitializeKey(ca, &wcert, md_alg, MBEDTLS_PK_RSA);
  CHECK_EQ(0, mbedtls_rsa_gen_key(mbedtls_pk_rsa(*key), GenerateHardRandom, 0,
                                  suiteb ? 4096 : 2048, 65537));
  GenerateCertificateSerial(&wcert);
  ConfigureCertificate(
      &wcert, ca,
      MBEDTLS_X509_KU_DIGITAL_SIGNATURE | MBEDTLS_X509_KU_KEY_ENCIPHERMENT,
      MBEDTLS_X509_NS_CERT_TYPE_SSL_SERVER |
          MBEDTLS_X509_NS_CERT_TYPE_SSL_CLIENT);
  return FinishCertificate(ca, &wcert, key);
}

static void LoadCertificates(void) {
  size_t i;
  struct Cert ksk, ecp, rsa;
  bool havecert, haveclientcert;
  havecert = false;
  haveclientcert = false;
  for (i = 0; i < certs.n; ++i) {
    if (certs.p[i].key && certs.p[i].cert && !certs.p[i].cert->ca_istrue &&
        !mbedtls_x509_crt_check_key_usage(certs.p[i].cert,
                                          MBEDTLS_X509_KU_DIGITAL_SIGNATURE)) {
      if (!mbedtls_x509_crt_check_extended_key_usage(
              certs.p[i].cert, MBEDTLS_OID_SERVER_AUTH,
              MBEDTLS_OID_SIZE(MBEDTLS_OID_SERVER_AUTH))) {
        LogCertificate("using server certificate", certs.p[i].cert);
        UseCertificate(&conf, certs.p + i, "server");
        havecert = true;
      }
      if (!mbedtls_x509_crt_check_extended_key_usage(
              certs.p[i].cert, MBEDTLS_OID_CLIENT_AUTH,
              MBEDTLS_OID_SIZE(MBEDTLS_OID_CLIENT_AUTH))) {
        LogCertificate("using client certificate", certs.p[i].cert);
        UseCertificate(&confcli, certs.p + i, "client");
        haveclientcert = true;
      }
    }
  }
  if (!havecert && (!psks.n || ksk.key)) {
    if ((ksk = GetKeySigningKey()).key) {
      DEBUGF("(ssl) generating ssl certificates using %`'s",
             gc(FormatX509Name(&ksk.cert->subject)));
    } else {
      VERBOSEF("(ssl) could not find non-CA SSL certificate key pair with"
               " -addext keyUsage=digitalSignature"
               " -addext extendedKeyUsage=serverAuth");
      VERBOSEF("(ssl) could not find CA key signing key pair with"
               " -addext keyUsage=keyCertSign");
      VERBOSEF("(ssl) generating self-signed ssl certificates");
    }
#ifdef MBEDTLS_ECP_C
    ecp = GenerateEcpCertificate(ksk.key ? &ksk : 0);
    if (!havecert) UseCertificate(&conf, &ecp, "server");
    if (!haveclientcert && ksk.key) {
      UseCertificate(&confcli, &ecp, "client");
    }
    AppendCert(ecp.cert, ecp.key);
#endif
#ifdef MBEDTLS_RSA_C
    if (!norsagen) {
      rsa = GenerateRsaCertificate(ksk.key ? &ksk : 0);
      if (!havecert) UseCertificate(&conf, &rsa, "server");
      if (!haveclientcert && ksk.key) {
        UseCertificate(&confcli, &rsa, "client");
      }
      AppendCert(rsa.cert, rsa.key);
    }
#endif
  }
  WipeSigningKeys();
}

static bool ClientAcceptsGzip(void) {
  return msg.version >= 10 && /* RFC1945 § 3.5 */
         HeaderHas(&msg, inbuf.p, kHttpAcceptEncoding, "gzip", 4);
}

char *FormatUnixHttpDateTime(char *s, int64_t t) {
  struct tm tm;
  gmtime_r(&t, &tm);
  FormatHttpDateTime(s, &tm);
  return s;
}

static void UpdateCurrentDate(long double now) {
  int64_t t;
  struct tm tm;
  t = now;
  shared->nowish = now;
  gmtime_r(&t, &tm);
  FormatHttpDateTime(shared->currentdate, &tm);
}

static int64_t GetGmtOffset(int64_t t) {
  struct tm tm;
  localtime_r(&t, &tm);
  return tm.tm_gmtoff;
}

forceinline bool IsCompressed(struct Asset *a) {
  return !a->file &&
         ZIP_LFILE_COMPRESSIONMETHOD(zbase + a->lf) == kZipCompressionDeflate;
}

forceinline int GetMode(struct Asset *a) {
  return a->file ? a->file->st.st_mode : GetZipCfileMode(zbase + a->cf);
}

forceinline bool IsCompressionMethodSupported(int method) {
  return method == kZipCompressionNone || method == kZipCompressionDeflate;
}

static inline unsigned Hash(const void *p, unsigned long n) {
  unsigned h, i;
  for (h = i = 0; i < n; i++) {
    h += ((unsigned char *)p)[i];
    h *= 0x9e3779b1;
  }
  return MAX(1, h);
}

static void FreeAssets(void) {
  size_t i;
  for (i = 0; i < assets.n; ++i) {
    Free(&assets.p[i].lastmodifiedstr);
  }
  Free(&assets.p);
  assets.n = 0;
}

static void FreeStrings(struct Strings *l) {
  size_t i;
  for (i = 0; i < l->n; ++i) {
    Free(&l->p[i].s);
  }
  Free(&l->p);
  l->n = 0;
}

static void IndexAssets(void) {
  uint64_t cf;
  struct Asset *p;
  struct timespec lm;
  uint32_t i, n, m, step, hash;
  DEBUGF("(zip) indexing assets (inode %#lx)", zst.st_ino);
  FreeAssets();
  CHECK_GE(HASH_LOAD_FACTOR, 2);
  CHECK(READ32LE(zcdir) == kZipCdir64HdrMagic ||
        READ32LE(zcdir) == kZipCdirHdrMagic);
  n = GetZipCdirRecords(zcdir);
  m = roundup2pow(MAX(1, n) * HASH_LOAD_FACTOR);
  p = xcalloc(m, sizeof(struct Asset));
  for (cf = GetZipCdirOffset(zcdir); n--; cf += ZIP_CFILE_HDRSIZE(zbase + cf)) {
    CHECK_EQ(kZipCfileHdrMagic, ZIP_CFILE_MAGIC(zbase + cf));
    if (!IsCompressionMethodSupported(
            ZIP_CFILE_COMPRESSIONMETHOD(zbase + cf))) {
      WARNF("(zip) don't understand zip compression method %d used by %`'.*s",
            ZIP_CFILE_COMPRESSIONMETHOD(zbase + cf),
            ZIP_CFILE_NAMESIZE(zbase + cf), ZIP_CFILE_NAME(zbase + cf));
      continue;
    }
    hash = Hash(ZIP_CFILE_NAME(zbase + cf), ZIP_CFILE_NAMESIZE(zbase + cf));
    step = 0;
    do {
      i = (hash + (step * (step + 1)) >> 1) & (m - 1);
      ++step;
    } while (p[i].hash);
    GetZipCfileTimestamps(zbase + cf, &lm, 0, 0, gmtoff);
    p[i].hash = hash;
    p[i].cf = cf;
    p[i].lf = GetZipCfileOffset(zbase + cf);
    p[i].istext = !!(ZIP_CFILE_INTERNALATTRIBUTES(zbase + cf) & kZipIattrText);
    p[i].lastmodified = lm.tv_sec;
    p[i].lastmodifiedstr = FormatUnixHttpDateTime(xmalloc(30), lm.tv_sec);
  }
  assets.p = p;
  assets.n = m;
}

static bool OpenZip(bool force) {
  int fd;
  size_t n;
  struct stat st;
  uint8_t *m, *b, *d, *p;
  if (stat(zpath, &st) != -1) {
    if (force || st.st_ino != zst.st_ino || st.st_size > zst.st_size) {
      if (st.st_ino == zst.st_ino) {
        fd = zfd;
      } else if ((fd = open(zpath, O_RDWR)) == -1) {
        WARNF("(zip) open() error: %m");
        return false;
      }
      if ((m = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) !=
          MAP_FAILED) {
        n = st.st_size;
        if ((p = FindEmbeddedApe(m, n))) {
          b = p;
          n -= p - m;
        } else {
          b = m;
        }
        if ((d = GetZipCdir(b, n))) {
          if (zmap) {
            LOGIFNEG1(munmap(zmap, zbase + zsize - zmap));
          }
          zmap = m;
          zbase = b;
          zsize = n;
          zcdir = d;
          DCHECK(IsZipCdir32(zbase, zsize, zcdir - zbase) ||
                 IsZipCdir64(zbase, zsize, zcdir - zbase));
          memcpy(&zst, &st, sizeof(st));
          IndexAssets();
          return true;
        } else {
          WARNF("(zip) couldn't locate central directory");
        }
      } else {
        WARNF("(zip) mmap() error: %m");
      }
    }
  } else {
    WARNF("(zip) stat() error: %m");
  }
  return false;
}

static struct Asset *GetAssetZip(const char *path, size_t pathlen) {
  uint32_t i, step, hash;
  if (pathlen > 1 && path[0] == '/') ++path, --pathlen;
  hash = Hash(path, pathlen);
  for (step = 0;; ++step) {
    i = (hash + (step * (step + 1)) >> 1) & (assets.n - 1);
    if (!assets.p[i].hash) return NULL;
    if (hash == assets.p[i].hash &&
        pathlen == ZIP_CFILE_NAMESIZE(zbase + assets.p[i].cf) &&
        memcmp(path, ZIP_CFILE_NAME(zbase + assets.p[i].cf), pathlen) == 0) {
      return &assets.p[i];
    }
  }
}

static struct Asset *GetAssetFile(const char *path, size_t pathlen) {
  size_t i;
  struct Asset *a;
  if (stagedirs.n) {
    a = FreeLater(xcalloc(1, sizeof(struct Asset)));
    a->file = FreeLater(xmalloc(sizeof(struct File)));
    for (i = 0; i < stagedirs.n; ++i) {
      LockInc(&shared->c.stats);
      a->file->path.s = FreeLater(MergePaths(stagedirs.p[i].s, stagedirs.p[i].n,
                                             path, pathlen, &a->file->path.n));
      if (stat(a->file->path.s, &a->file->st) != -1) {
        a->lastmodifiedstr = FormatUnixHttpDateTime(
            FreeLater(xmalloc(30)),
            (a->lastmodified = a->file->st.st_mtim.tv_sec));
        return a;
      } else {
        LockInc(&shared->c.statfails);
      }
    }
  }
  return NULL;
}

static struct Asset *GetAsset(const char *path, size_t pathlen) {
  char *path2;
  struct Asset *a;
  if (!(a = GetAssetFile(path, pathlen))) {
    if (!(a = GetAssetZip(path, pathlen))) {
      if (pathlen > 1 && path[pathlen - 1] != '/' &&
          pathlen + 1 <= sizeof(slashpath)) {
        memcpy(mempcpy(slashpath, path, pathlen), "/", 1);
        a = GetAssetZip(slashpath, pathlen + 1);
      }
    }
  }
  return a;
}

static char *AppendHeader(char *p, const char *k, const char *v) {
  if (!v) return p;
  return AppendCrlf(stpcpy(stpcpy(stpcpy(p, k), ": "), v));
}

static char *AppendContentType(char *p, const char *ct) {
  p = stpcpy(p, "Content-Type: ");
  p = stpcpy(p, ct);
  if ((istext = startswith(ct, "text/"))) {
    if (!strchr(ct + 5, ';')) {
      p = stpcpy(p, "; charset=utf-8");
    }
    if (!referrerpolicy && startswith(ct + 5, "html")) {
      referrerpolicy = "no-referrer-when-downgrade";
    }
  }
  hascontenttype = true;
  return AppendCrlf(p);
}

static char *AppendExpires(char *p, int64_t t) {
  struct tm tm;
  gmtime_r(&t, &tm);
  p = stpcpy(p, "Expires: ");
  p = FormatHttpDateTime(p, &tm);
  return AppendCrlf(p);
}

static char *AppendCache(char *p, int64_t seconds) {
  if (seconds < 0) return p;
  p = stpcpy(p, "Cache-Control: max-age=");
  p = FormatUint64(p, seconds);
  if (!seconds) {
    p = stpcpy(p, ", no-store");
  } else {
    p = stpcpy(p, ", must-revalidate");
  }
  p = AppendCrlf(p);
  return AppendExpires(p, (int64_t)shared->nowish + seconds);
}

static inline char *AppendContentLength(char *p, size_t n) {
  p = stpcpy(p, "Content-Length: ");
  p = FormatUint64(p, n);
  return AppendCrlf(p);
}

static char *AppendContentRange(char *p, long a, long b, long c) {
  p = stpcpy(p, "Content-Range: bytes ");
  if (a >= 0 && b > 0) {
    p = FormatUint64(p, a);
    *p++ = '-';
    p = FormatUint64(p, a + b - 1);
  } else {
    *p++ = '*';
  }
  *p++ = '/';
  p = FormatUint64(p, c);
  return AppendCrlf(p);
}

static bool Inflate(void *dp, size_t dn, const void *sp, size_t sn) {
  LockInc(&shared->c.inflates);
  return !__inflate(dp, dn, sp, sn);
}

static bool Verify(void *data, size_t size, uint32_t crc) {
  uint32_t got;
  LockInc(&shared->c.verifies);
  if (crc == (got = crc32_z(0, data, size))) {
    return true;
  } else {
    LockInc(&shared->c.thiscorruption);
    WARNF("(zip) corrupt zip file at %`'.*s had crc 0x%08x but expected 0x%08x",
          msg.uri.b - msg.uri.a, inbuf.p + msg.uri.a, got, crc);
    return false;
  }
}

static void *Deflate(const void *data, size_t size, size_t *out_size) {
  void *res;
  z_stream zs;
  LockInc(&shared->c.deflates);
  zs.zfree = 0;
  zs.zalloc = 0;
  CHECK_EQ(Z_OK, deflateInit2(&zs, 4, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL,
                              Z_DEFAULT_STRATEGY));
  zs.next_in = data;
  zs.avail_in = size;
  zs.avail_out = compressBound(size);
  zs.next_out = res = xmalloc(zs.avail_out);
  CHECK_EQ(Z_STREAM_END, deflate(&zs, Z_FINISH));
  CHECK_EQ(Z_OK, deflateEnd(&zs));
  *out_size = zs.total_out;
  return xrealloc(res, zs.total_out);
}

static void *LoadAsset(struct Asset *a, size_t *out_size) {
  int mode;
  size_t size;
  uint8_t *data;
  if (S_ISDIR(GetMode(a))) {
    WARNF("(srvr) can't load directory");
    return NULL;
  }
  if (!a->file) {
    size = GetZipLfileUncompressedSize(zbase + a->lf);
    if (size == SIZE_MAX || !(data = malloc(size + 1))) return NULL;
    if (IsCompressed(a)) {
      if (!Inflate(data, size, ZIP_LFILE_CONTENT(zbase + a->lf),
                   GetZipCfileCompressedSize(zbase + a->cf))) {
        free(data);
        return NULL;
      }
    } else {
      memcpy(data, ZIP_LFILE_CONTENT(zbase + a->lf), size);
    }
    if (!Verify(data, size, ZIP_LFILE_CRC32(zbase + a->lf))) {
      free(data);
      return NULL;
    }
    data[size] = '\0';
    if (out_size) *out_size = size;
    return data;
  } else {
    LockInc(&shared->c.slurps);
    return xslurp(a->file->path.s, out_size);
  }
}

static wontreturn void PrintUsage(int fd, int rc) {
  size_t n;
  const char *p;
  struct Asset *a;
  if (!(a = GetAssetZip("/help.txt", 9)) || !(p = LoadAsset(a, &n))) {
    fprintf(stderr, "error: /help.txt is not a zip asset\n");
    exit(1);
  }
  __paginate(fd, p);
  exit(rc);
}

static void AppendLogo(void) {
  size_t n;
  char *p, *q;
  struct Asset *a;
  if ((a = GetAsset("/redbean.png", 12)) && (p = LoadAsset(a, &n))) {
    if ((q = EncodeBase64(p, n, &n))) {
      appends(&outbuf, "<img alt=\"[logo]\" src=\"data:image/png;base64,");
      appendd(&outbuf, q, n);
      appends(&outbuf, "\">\r\n");
      free(q);
    }
    free(p);
  }
}

static ssize_t Send(struct iovec *iov, int iovlen) {
  ssize_t rc;
  if ((rc = writer(client, iov, iovlen)) == -1) {
    if (errno == ECONNRESET) {
      LockInc(&shared->c.writeresets);
      DEBUGF("(rsp) %s write reset", DescribeClient());
    } else if (errno == EAGAIN) {
      LockInc(&shared->c.writetimeouts);
      WARNF("(rsp) %s write timeout", DescribeClient());
      errno = 0;
    } else {
      LockInc(&shared->c.writeerrors);
      WARNF("(rsp) %s write error: %m", DescribeClient());
    }
    connectionclose = true;
  }
  return rc;
}

static bool IsSslCompressed(void) {
  return usingssl && ssl.session->compression;
}

static char *CommitOutput(char *p) {
  uint32_t crc;
  size_t outbuflen;
  if (!contentlength) {
    outbuflen = appendz(outbuf).i;
    if (istext && outbuflen >= 100) {
      if (!IsTiny() && !IsSslCompressed()) {
        p = stpcpy(p, "Vary: Accept-Encoding\r\n");
      }
      if (!IsTiny() && !IsSslCompressed() && ClientAcceptsGzip()) {
        gzipped = true;
        crc = crc32_z(0, outbuf, outbuflen);
        WRITE32LE(gzip_footer + 0, crc);
        WRITE32LE(gzip_footer + 4, outbuflen);
        content = FreeLater(Deflate(outbuf, outbuflen, &contentlength));
        DropOutput();
      } else {
        UseOutput();
      }
    } else {
      UseOutput();
    }
  } else {
    DropOutput();
  }
  return p;
}

static char *ServeDefaultErrorPage(char *p, unsigned code, const char *reason,
                                   const char *details) {
  p = AppendContentType(p, "text/html; charset=ISO-8859-1");
  reason = FreeLater(EscapeHtml(reason, -1, 0));
  appends(&outbuf, "\
<!doctype html>\r\n\
<title>");
  appendf(&outbuf, "%d %s", code, reason);
  appends(&outbuf, "\
</title>\r\n\
<style>\r\n\
html { color: #111; font-family: sans-serif; }\r\n\
img { vertical-align: middle; }\r\n\
</style>\r\n\
<h1>\r\n");
  AppendLogo();
  appendf(&outbuf, "%d %s\r\n", code, reason);
  appends(&outbuf, "</h1>\r\n");
  if (details) {
    appendf(&outbuf, "<pre>%s</pre>\r\n",
            FreeLater(EscapeHtml(details, -1, 0)));
  }
  UseOutput();
  return p;
}

static char *ServeErrorImpl(unsigned code, const char *reason,
                            const char *details) {
  size_t n;
  char *p, *s;
  struct Asset *a;
  LockInc(&shared->c.errors);
  DropOutput();
  p = SetStatus(code, reason);
  s = xasprintf("/%d.html", code);
  a = GetAsset(s, strlen(s));
  free(s);
  if (!a) {
    return ServeDefaultErrorPage(p, code, reason, details);
  } else if (a->file) {
    LockInc(&shared->c.slurps);
    content = FreeLater(xslurp(a->file->path.s, &contentlength));
    return AppendContentType(p, "text/html; charset=utf-8");
  } else {
    content = (char *)ZIP_LFILE_CONTENT(zbase + a->lf);
    contentlength = GetZipCfileCompressedSize(zbase + a->cf);
    if (IsCompressed(a)) {
      n = GetZipLfileUncompressedSize(zbase + a->lf);
      if ((s = FreeLater(malloc(n))) && Inflate(s, n, content, contentlength)) {
        content = s;
        contentlength = n;
      } else {
        return ServeDefaultErrorPage(p, code, reason, details);
      }
    }
    if (Verify(content, contentlength, ZIP_LFILE_CRC32(zbase + a->lf))) {
      return AppendContentType(p, "text/html; charset=utf-8");
    } else {
      return ServeDefaultErrorPage(p, code, reason, details);
    }
  }
}

static char *ServeErrorWithDetail(unsigned code, const char *reason,
                                  const char *details) {
  ERRORF("(srvr) server error: %d %s", code, reason);
  return ServeErrorImpl(code, reason, details);
}

static char *ServeError(unsigned code, const char *reason) {
  return ServeErrorWithDetail(code, reason, NULL);
}

static char *ServeFailure(unsigned code, const char *reason) {
  ERRORF("(srvr) failure: %d %s %s HTTP%02d %.*s %`'.*s %`'.*s %`'.*s %`'.*s",
         code, reason, DescribeClient(), msg.version,
         msg.xmethod.b - msg.xmethod.a, inbuf.p + msg.xmethod.a,
         HeaderLength(kHttpHost), HeaderData(kHttpHost), msg.uri.b - msg.uri.a,
         inbuf.p + msg.uri.a, HeaderLength(kHttpReferer),
         HeaderData(kHttpReferer), HeaderLength(kHttpUserAgent),
         HeaderData(kHttpUserAgent));
  return ServeErrorImpl(code, reason, NULL);
}

static ssize_t YieldGenerator(struct iovec v[3]) {
  int nresults, status;
  if (isyielding > 1) {
    do {
      if (!YL || lua_status(YL) != LUA_YIELD) return 0;  // done yielding
      contentlength = 0;
      status = lua_resume(YL, NULL, 0, &nresults);
      if (status != LUA_OK && status != LUA_YIELD) {
        LogLuaError("resume", lua_tostring(YL, -1));
        lua_pop(YL, 1);
        return -1;
      }
      lua_pop(YL, nresults);
      if (!contentlength) UseOutput();
      // continue yielding if nothing to return to keep generator running
    } while (!contentlength);
  }
  DEBUGF("(lua) yielded with %ld bytes generated", contentlength);
  isyielding++;
  v[0].iov_base = content;
  v[0].iov_len = contentlength;
  return contentlength;
}

static void OnLuaServerPageCtrlc(int i) {
  lua_sigint(GL, i);
}

static int LuaCallWithYield(lua_State *L) {
  int status;
  // since yield may happen in OnHttpRequest and in ServeLua,
  // need to fully restart the yield generator;
  // the second set of headers is not going to be sent
  struct sigaction sa, saold;
  lua_State *co = lua_newthread(L);
  if (__replmode) {
    sa.sa_flags = SA_RESETHAND;
    sa.sa_handler = OnLuaServerPageCtrlc;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, &saold);
  }
  status = LuaCallWithTrace(L, 0, 0, co);
  if (__replmode) {
    sigaction(SIGINT, &saold, 0);
  }
  if (status == LUA_YIELD) {
    CHECK_GT(lua_gettop(L), 0);  // make sure that coroutine is anchored
    YL = co;
    generator = YieldGenerator;
    if (!isyielding) isyielding = 1;
    istext = false;  // reset istext flag to avoid zipping yielded chunk
    status = LUA_OK;
  }
  return status;
}

static ssize_t DeflateGenerator(struct iovec v[3]) {
  int i, rc;
  size_t no;
  void *res;
  int level;
  i = 0;
  if (!dg.t) {
    v[0].iov_base = kGzipHeader;
    v[0].iov_len = sizeof(kGzipHeader);
    ++dg.t;
    ++i;
  } else if (dg.t == 3) {
    return 0;
  }
  if (dg.t != 2) {
    CHECK_EQ(0, dg.s.avail_in);
    dg.s.next_in = (void *)(content + dg.i);
    dg.s.avail_in = MIN(dg.z, contentlength - dg.i);
    dg.c = crc32_z(dg.c, dg.s.next_in, dg.s.avail_in);
    dg.i += dg.s.avail_in;
  }
  dg.s.next_out = dg.b;
  dg.s.avail_out = dg.z;
  no = dg.s.avail_in;
  rc = deflate(&dg.s, dg.i < contentlength ? Z_SYNC_FLUSH : Z_FINISH);
  if (rc != Z_OK && rc != Z_STREAM_END) {
    DIEF("(zip) deflate()→%d oldin:%,zu/%,zu in:%,zu/%,zu out:%,zu/%,zu", rc,
         no, dg.z, dg.s.avail_in, dg.z, dg.s.avail_out, dg.z);
  } else {
    NOISEF("(zip) deflate()→%d oldin:%,zu/%,zu in:%,zu/%,zu out:%,zu/%,zu", rc,
           no, dg.z, dg.s.avail_in, dg.z, dg.s.avail_out, dg.z);
  }
  no = dg.z - dg.s.avail_out;
  if (no) {
    v[i].iov_base = dg.b;
    v[i].iov_len = no;
    ++i;
  }
  if (rc == Z_OK) {
    CHECK_GT(no, 0);
    if (dg.s.avail_out) {
      dg.t = 1;
    } else {
      dg.t = 2;
    }
  } else if (rc == Z_STREAM_END) {
    CHECK_EQ(contentlength, dg.i);
    CHECK_EQ(Z_OK, deflateEnd(&dg.s));
    WRITE32LE(gzip_footer + 0, dg.c);
    WRITE32LE(gzip_footer + 4, contentlength);
    v[i].iov_base = gzip_footer;
    v[i].iov_len = sizeof(gzip_footer);
    dg.t = 3;
  }
  return v[0].iov_len + v[1].iov_len + v[2].iov_len;
}

static char *ServeAssetCompressed(struct Asset *a) {
  char *p;
  uint32_t crc;
  LockInc(&shared->c.deflates);
  LockInc(&shared->c.compressedresponses);
  DEBUGF("(srvr) ServeAssetCompressed()");
  dg.t = 0;
  dg.i = 0;
  dg.c = 0;
  if (usingssl) {
    dg.z = 512 + (rand64() & 1023);
  } else {
    dg.z = 65536;
  }
  gzipped = true;
  generator = DeflateGenerator;
  bzero(&dg.s, sizeof(dg.s));
  CHECK_EQ(Z_OK, deflateInit2(&dg.s, 4, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL,
                              Z_DEFAULT_STRATEGY));
  dg.b = FreeLater(malloc(dg.z));
  p = SetStatus(200, "OK");
  p = stpcpy(p, "Content-Encoding: gzip\r\n");
  return p;
}

static ssize_t InflateGenerator(struct iovec v[3]) {
  int i, rc;
  size_t no;
  void *res;
  i = 0;
  if (!dg.t) {
    ++dg.t;
  } else if (dg.t == 3) {
    return 0;
  }
  if (dg.t != 2) {
    CHECK_EQ(0, dg.s.avail_in);
    dg.s.next_in = (void *)(content + dg.i);
    dg.s.avail_in = MIN(dg.z, contentlength - dg.i);
    dg.i += dg.s.avail_in;
  }
  dg.s.next_out = dg.b;
  dg.s.avail_out = dg.z;
  rc = inflate(&dg.s, Z_NO_FLUSH);
  if (rc != Z_OK && rc != Z_STREAM_END) DIEF("(zip) inflate()→%d", rc);
  no = dg.z - dg.s.avail_out;
  if (no) {
    v[i].iov_base = dg.b;
    v[i].iov_len = no;
    dg.c = crc32_z(dg.c, dg.b, no);
    ++i;
  }
  if (rc == Z_OK) {
    CHECK_GT(no, 0);
    dg.t = dg.s.avail_out ? 1 : 2;
  } else if (rc == Z_STREAM_END) {
    CHECK_EQ(Z_OK, inflateEnd(&dg.s));
    CHECK_EQ(ZIP_CFILE_CRC32(zbase + dg.a->cf), dg.c);
    dg.t = 3;
  }
  return v[0].iov_len + v[1].iov_len + v[2].iov_len;
}

static char *ServeAssetDecompressed(struct Asset *a) {
  char *p;
  size_t size;
  uint32_t crc;
  LockInc(&shared->c.inflates);
  LockInc(&shared->c.decompressedresponses);
  size = GetZipCfileUncompressedSize(zbase + a->cf);
  DEBUGF("(srvr) ServeAssetDecompressed(%ld)→%ld", contentlength, size);
  if (msg.method == kHttpHead) {
    content = 0;
    contentlength = size;
    return SetStatus(200, "OK");
  } else if (!IsTiny()) {
    dg.t = 0;
    dg.i = 0;
    dg.c = 0;
    dg.a = a;
    dg.z = 65536;
    CHECK_EQ(Z_OK, inflateInit2(&dg.s, -MAX_WBITS));
    generator = InflateGenerator;
    dg.b = FreeLater(malloc(dg.z));
    return SetStatus(200, "OK");
  } else if ((p = FreeLater(malloc(size))) &&
             Inflate(p, size, content, contentlength) &&
             Verify(p, size, ZIP_CFILE_CRC32(zbase + a->cf))) {
    content = p;
    contentlength = size;
    return SetStatus(200, "OK");
  } else {
    return ServeError(500, "Internal Server Error");
  }
}

static inline char *ServeAssetIdentity(struct Asset *a, const char *ct) {
  LockInc(&shared->c.identityresponses);
  DEBUGF("(srvr) ServeAssetIdentity(%`'s)", ct);
  return SetStatus(200, "OK");
}

static inline char *ServeAssetPrecompressed(struct Asset *a) {
  size_t size;
  uint32_t crc;
  DEBUGF("(srvr) ServeAssetPrecompressed()");
  LockInc(&shared->c.precompressedresponses);
  gzipped = true;
  crc = ZIP_CFILE_CRC32(zbase + a->cf);
  size = GetZipCfileUncompressedSize(zbase + a->cf);
  WRITE32LE(gzip_footer + 0, crc);
  WRITE32LE(gzip_footer + 4, size);
  return SetStatus(200, "OK");
}

static char *ServeAssetRange(struct Asset *a) {
  char *p;
  long rangestart, rangelength;
  DEBUGF("(srvr) ServeAssetRange()");
  if (ParseHttpRange(HeaderData(kHttpRange), HeaderLength(kHttpRange),
                     contentlength, &rangestart, &rangelength) &&
      rangestart >= 0 && rangelength >= 0 && rangestart < contentlength &&
      rangestart + rangelength <= contentlength) {
    LockInc(&shared->c.partialresponses);
    p = SetStatus(206, "Partial Content");
    p = AppendContentRange(p, rangestart, rangelength, contentlength);
    content += rangestart;
    contentlength = rangelength;
    return p;
  } else {
    LockInc(&shared->c.badranges);
    WARNF("(client) bad range %`'.*s", HeaderLength(kHttpRange),
          HeaderData(kHttpRange));
    p = SetStatus(416, "Range Not Satisfiable");
    p = AppendContentRange(p, -1, -1, contentlength);
    content = "";
    contentlength = 0;
    return p;
  }
}

static char *GetAssetPath(uint8_t *zcf, size_t *out_size) {
  char *p1, *p2;
  size_t n1, n2;
  p1 = ZIP_CFILE_NAME(zcf);
  n1 = ZIP_CFILE_NAMESIZE(zcf);
  p2 = xmalloc(1 + n1 + 1);
  n2 = 1 + n1 + 1;
  p2[0] = '/';
  memcpy(p2 + 1, p1, n1);
  p2[1 + n1] = '\0';
  if (out_size) *out_size = 1 + n1;
  return p2;
}

static bool IsHiddenPath(const char *s, size_t n) {
  size_t i;
  for (i = 0; i < hidepaths.n; ++i) {
    if (n >= hidepaths.p[i].n &&
        !memcmp(s, hidepaths.p[i].s, hidepaths.p[i].n)) {
      return true;
    }
  }
  return false;
}

static char *GetBasicAuthorization(size_t *z) {
  size_t n;
  const char *p, *q;
  struct HttpSlice *g;
  g = msg.headers + (HasHeader(kHttpProxyAuthorization)
                         ? kHttpProxyAuthorization
                         : kHttpAuthorization);
  p = inbuf.p + g->a;
  n = g->b - g->a;
  if ((q = memchr(p, ' ', n)) && SlicesEqualCase(p, q - p, "Basic", 5)) {
    return DecodeBase64(q + 1, n - (q + 1 - p), z);
  } else {
    return NULL;
  }
}

static const char *GetSystemUrlLauncherCommand(void) {
  if (IsWindows()) {
    return "explorer";
  } else if (IsXnu()) {
    return "open";
  } else {
    return "xdg-open";
  }
}

static void LaunchBrowser(const char *path) {
  int pid, ws;
  struct in_addr addr;
  const char *u, *prog;
  sigset_t chldmask, savemask;
  struct sigaction ignore, saveint, savequit;
  uint16_t port = 80;
  path = firstnonnull(path, "/");
  // use the first server address if there is at least one server
  if (servers.n) {
    addr = servers.p[0].addr.sin_addr;
    port = ntohs(servers.p[0].addr.sin_port);
  }
  // assign a loopback address if no server or unknown server address
  if (!servers.n || !addr.s_addr) addr.s_addr = htonl(INADDR_LOOPBACK);
  if (*path != '/') path = gc(xasprintf("/%s", path));
  if ((prog = commandv(GetSystemUrlLauncherCommand(), gc(malloc(PATH_MAX)),
                       PATH_MAX))) {
    u = gc(xasprintf("http://%s:%d%s", inet_ntoa(addr), port,
                     gc(EscapePath(path, -1, 0))));
    DEBUGF("(srvr) opening browser with command %`'s %s", prog, u);
    ignore.sa_flags = 0;
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    sigaction(SIGINT, &ignore, &saveint);
    sigaction(SIGQUIT, &ignore, &savequit);
    sigemptyset(&chldmask);
    sigaddset(&chldmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &chldmask, &savemask);
    CHECK_NE(-1, (pid = fork()));
    if (!pid) {
      setpgrp();  // ctrl-c'ing redbean shouldn't kill browser
      sigaction(SIGINT, &saveint, 0);
      sigaction(SIGQUIT, &savequit, 0);
      sigprocmask(SIG_SETMASK, &savemask, 0);
      execv(prog, (char *const[]){prog, u, 0});
      _Exit(127);
    }
    while (wait4(pid, &ws, 0, 0) == -1) {
      CHECK_EQ(EINTR, errno);
      errno = 0;
    }
    sigaction(SIGINT, &saveint, 0);
    sigaction(SIGQUIT, &savequit, 0);
    sigprocmask(SIG_SETMASK, &savemask, 0);
    if (!(WIFEXITED(ws) && WEXITSTATUS(ws) == 0)) {
      WARNF("(srvr) command %`'s exited with %d", GetSystemUrlLauncherCommand(),
            WIFEXITED(ws) ? WEXITSTATUS(ws) : 128 + WEXITSTATUS(ws));
    }
  } else {
    WARNF("(srvr) can't launch browser because %`'s isn't installed",
          GetSystemUrlLauncherCommand());
  }
}

static char *BadMethod(void) {
  LockInc(&shared->c.badmethods);
  return stpcpy(ServeError(405, "Method Not Allowed"), "Allow: GET, HEAD\r\n");
}

static int GetDecimalWidth(long x) {
  return LengthInt64Thousands(x);
}

static int GetOctalWidth(int x) {
  return !x ? 1 : x < 8 ? 2 : 1 + bsr(x) / 3;
}

static const char *DescribeCompressionRatio(char rb[8], uint8_t *zcf) {
  long percent;
  if (ZIP_CFILE_COMPRESSIONMETHOD(zcf) == kZipCompressionNone) {
    return "n/a";
  } else {
    percent = lround(100 - (double)GetZipCfileCompressedSize(zcf) /
                               GetZipCfileUncompressedSize(zcf) * 100);
    sprintf(rb, "%ld%%", MIN(999, MAX(-999, percent)));
    return rb;
  }
}

static char *ServeListing(void) {
  long x;
  ldiv_t y;
  int w[3];
  uint8_t *zcf;
  struct tm tm;
  const char *and;
  struct rusage ru;
  char *p, *q, *path;
  struct timespec lastmod;
  char rb[8], tb[20], *rp[6];
  size_t i, n, pathlen, rn[6];
  LockInc(&shared->c.listingrequests);
  if (msg.method != kHttpGet && msg.method != kHttpHead) return BadMethod();
  appends(&outbuf, "\
<!doctype html>\r\n\
<meta charset=\"utf-8\">\r\n\
<title>redbean zip listing</title>\r\n\
<style>\r\n\
html { color: #111; font-family: sans-serif; }\r\n\
a { text-decoration: none; }\r\n\
pre a:hover { color: #00e; border-bottom: 1px solid #ccc; }\r\n\
h1 a { color: #111; }\r\n\
img { vertical-align: middle; }\r\n\
footer { color: #555; font-size: 10pt; }\r\n\
td { padding-right: 3em; }\r\n\
.eocdcomment { max-width: 800px; color: #333; font-size: 11pt; }\r\n\
</style>\r\n\
<header><h1>\r\n");
  AppendLogo();
  rp[0] = EscapeHtml(brand, -1, &rn[0]);
  appendd(&outbuf, rp[0], rn[0]);
  free(rp[0]);
  appendf(&outbuf,
          "</h1>\r\n"
          "<div class=\"eocdcomment\">%.*s</div>\r\n"
          "<hr>\r\n"
          "</header>\r\n"
          "<pre>\r\n",
          strnlen(GetZipCdirComment(zcdir), GetZipCdirCommentSize(zcdir)),
          GetZipCdirComment(zcdir));
  bzero(w, sizeof(w));
  n = GetZipCdirRecords(zcdir);
  for (zcf = zbase + GetZipCdirOffset(zcdir); n--;
       zcf += ZIP_CFILE_HDRSIZE(zcf)) {
    CHECK_EQ(kZipCfileHdrMagic, ZIP_CFILE_MAGIC(zcf));
    path = GetAssetPath(zcf, &pathlen);
    if (!IsHiddenPath(path, pathlen)) {
      w[0] = min(80, max(w[0], strwidth(path, 0) + 2));
      w[1] = max(w[1], GetOctalWidth(GetZipCfileMode(zcf)));
      w[2] = max(w[2], GetDecimalWidth(GetZipCfileUncompressedSize(zcf)));
    }
    free(path);
  }
  n = GetZipCdirRecords(zcdir);
  for (zcf = zbase + GetZipCdirOffset(zcdir); n--;
       zcf += ZIP_CFILE_HDRSIZE(zcf)) {
    CHECK_EQ(kZipCfileHdrMagic, ZIP_CFILE_MAGIC(zcf));
    path = GetAssetPath(zcf, &pathlen);
    if (!IsHiddenPath(path, pathlen)) {
      rp[0] = VisualizeControlCodes(path, pathlen, &rn[0]);
      rp[1] = EscapePath(path, pathlen, &rn[1]);
      rp[2] = EscapeHtml(rp[1], rn[1], &rn[2]);
      rp[3] = VisualizeControlCodes(
          ZIP_CFILE_COMMENT(zcf),
          strnlen(ZIP_CFILE_COMMENT(zcf), ZIP_CFILE_COMMENTSIZE(zcf)), &rn[3]);
      rp[4] = EscapeHtml(rp[0], rn[0], &rn[4]);
      GetZipCfileTimestamps(zcf, &lastmod, 0, 0, gmtoff);
      localtime_r(&lastmod.tv_sec, &tm);
      iso8601(tb, &tm);
      if (IsCompressionMethodSupported(ZIP_CFILE_COMPRESSIONMETHOD(zcf)) &&
          IsAcceptablePath(path, pathlen)) {
        appendf(&outbuf,
                "<a href=\"%.*s\">%-*.*s</a> %s  %0*o %4s  %,*ld  %'s\r\n",
                rn[2], rp[2], w[0], rn[4], rp[4], tb, w[1],
                GetZipCfileMode(zcf), DescribeCompressionRatio(rb, zcf), w[2],
                GetZipCfileUncompressedSize(zcf), rp[3]);
      } else {
        appendf(&outbuf, "%-*.*s %s  %0*o %4s  %,*ld  %'s\r\n", w[0], rn[4],
                rp[4], tb, w[1], GetZipCfileMode(zcf),
                DescribeCompressionRatio(rb, zcf), w[2],
                GetZipCfileUncompressedSize(zcf), rp[3]);
      }
      free(rp[4]);
      free(rp[3]);
      free(rp[2]);
      free(rp[1]);
      free(rp[0]);
    }
    free(path);
  }
  appends(&outbuf, "\
</pre><footer><hr>\r\n\
<table border=\"0\"><tr>\r\n\
<td valign=\"top\">\r\n\
<a href=\"/statusz\">/statusz</a>\r\n\
");
  if (shared->c.connectionshandled) {
    appends(&outbuf, "says your redbean<br>\r\n");
    AppendResourceReport(&outbuf, &shared->children, "<br>\r\n");
  }
  appends(&outbuf, "<td valign=\"top\">\r\n");
  and = "";
  x = nowl() - startserver;
  y = ldiv(x, 24L * 60 * 60);
  if (y.quot) {
    appendf(&outbuf, "%,ld day%s ", y.quot, y.quot == 1 ? "" : "s");
    and = "and ";
  }
  y = ldiv(y.rem, 60 * 60);
  if (y.quot) {
    appendf(&outbuf, "%,ld hour%s ", y.quot, y.quot == 1 ? "" : "s");
    and = "and ";
  }
  y = ldiv(y.rem, 60);
  if (y.quot) {
    appendf(&outbuf, "%,ld minute%s ", y.quot, y.quot == 1 ? "" : "s");
    and = "and ";
  }
  appendf(&outbuf, "%s%,ld second%s of operation<br>\r\n", and, y.rem,
          y.rem == 1 ? "" : "s");
  x = shared->c.messageshandled;
  appendf(&outbuf, "%,ld message%s handled<br>\r\n", x, x == 1 ? "" : "s");
  x = shared->c.connectionshandled;
  appendf(&outbuf, "%,ld connection%s handled<br>\r\n", x, x == 1 ? "" : "s");
  x = shared->workers;
  appendf(&outbuf, "%,ld connection%s active<br>\r\n", x, x == 1 ? "" : "s");
  appends(&outbuf, "</table>\r\n");
  appends(&outbuf, "</footer>\r\n");
  p = SetStatus(200, "OK");
  p = AppendContentType(p, "text/html");
  if (msg.version >= 11) {
    p = stpcpy(p, "Cache-Control: no-store\r\n");
  }
  return CommitOutput(p);
}

static const char *MergeNames(const char *a, const char *b) {
  return FreeLater(xasprintf("%s.%s", a, b));
}

static void AppendLong1(const char *a, long x) {
  if (x) appendf(&outbuf, "%s: %ld\r\n", a, x);
}

static void AppendLong2(const char *a, const char *b, long x) {
  if (x) appendf(&outbuf, "%s.%s: %ld\r\n", a, b, x);
}

static void AppendTimeval(const char *a, struct timeval *tv) {
  AppendLong2(a, "tv_sec", tv->tv_sec);
  AppendLong2(a, "tv_usec", tv->tv_usec);
}

static void AppendRusage(const char *a, struct rusage *ru) {
  AppendTimeval(MergeNames(a, "ru_utime"), &ru->ru_utime);
  AppendTimeval(MergeNames(a, "ru_stime"), &ru->ru_stime);
  AppendLong2(a, "ru_maxrss", ru->ru_maxrss);
  AppendLong2(a, "ru_ixrss", ru->ru_ixrss);
  AppendLong2(a, "ru_idrss", ru->ru_idrss);
  AppendLong2(a, "ru_isrss", ru->ru_isrss);
  AppendLong2(a, "ru_minflt", ru->ru_minflt);
  AppendLong2(a, "ru_majflt", ru->ru_majflt);
  AppendLong2(a, "ru_nswap", ru->ru_nswap);
  AppendLong2(a, "ru_inblock", ru->ru_inblock);
  AppendLong2(a, "ru_oublock", ru->ru_oublock);
  AppendLong2(a, "ru_msgsnd", ru->ru_msgsnd);
  AppendLong2(a, "ru_msgrcv", ru->ru_msgrcv);
  AppendLong2(a, "ru_nsignals", ru->ru_nsignals);
  AppendLong2(a, "ru_nvcsw", ru->ru_nvcsw);
  AppendLong2(a, "ru_nivcsw", ru->ru_nivcsw);
}

static void ServeCounters(void) {
  const long *c;
  const char *s;
  for (c = (const long *)&shared->c, s = kCounterNames; *s;
       ++c, s += strlen(s) + 1) {
    AppendLong1(s, *c);
  }
}

static char *ServeStatusz(void) {
  char *p;
  LockInc(&shared->c.statuszrequests);
  if (msg.method != kHttpGet && msg.method != kHttpHead) return BadMethod();
  AppendLong1("pid", getpid());
  AppendLong1("ppid", getppid());
  AppendLong1("now", nowl());
  AppendLong1("nowish", shared->nowish);
  AppendLong1("gmtoff", gmtoff);
  AppendLong1("CLK_TCK", CLK_TCK);
  AppendLong1("startserver", startserver);
  AppendLong1("lastmeltdown", shared->lastmeltdown);
  AppendLong1("workers", shared->workers);
  AppendLong1("assets.n", assets.n);
#ifndef STATIC
  lua_State *L = GL;
  AppendLong1("lua.memory",
              lua_gc(L, LUA_GCCOUNT) * 1024 + lua_gc(L, LUA_GCCOUNTB));
#endif
  ServeCounters();
  AppendRusage("server", &shared->server);
  AppendRusage("children", &shared->children);
  p = SetStatus(200, "OK");
  p = AppendContentType(p, "text/plain");
  if (msg.version >= 11) {
    p = stpcpy(p, "Cache-Control: no-store\r\n");
  }
  return CommitOutput(p);
}

static char *RedirectSlash(void) {
  size_t n;
  char *p, *e;
  LockInc(&shared->c.redirects);
  p = SetStatus(307, "Temporary Redirect");
  p = stpcpy(p, "Location: ");
  e = EscapePath(url.path.p, url.path.n, &n);
  p = mempcpy(p, e, n);
  p = stpcpy(p, "/\r\n");
  free(e);
  return p;
}

static char *ServeIndex(const char *path, size_t pathlen) {
  size_t i, n;
  char *p, *q;
  for (p = 0, i = 0; !p && i < ARRAYLEN(kIndexPaths); ++i) {
    q = MergePaths(path, pathlen, kIndexPaths[i], strlen(kIndexPaths[i]), &n);
    p = RoutePath(q, n);
    free(q);
  }
  return p;
}

static char *GetLuaResponse(void) {
  return luaheaderp ? luaheaderp : SetStatus(200, "OK");
}

static bool ShouldServeCrashReportDetails(void) {
  uint32_t ip;
  uint16_t port;
  if (leakcrashreports) {
    return true;
  } else {
    GetRemoteAddr(&ip, &port);
    return IsLoopbackIp(ip) || IsPrivateIp(ip);
  }
}

static char *LuaOnHttpRequest(void) {
  char *error;
  lua_State *L = GL;
  effectivepath.p = url.path.p;
  effectivepath.n = url.path.n;
  lua_settop(L, 0);  // clear Lua stack, as it needs to start fresh
  lua_getglobal(L, "OnHttpRequest");
  if (LuaCallWithYield(L) == LUA_OK) {
    return CommitOutput(GetLuaResponse());
  } else {
    LogLuaError("OnHttpRequest", lua_tostring(L, -1));
    error = ServeErrorWithDetail(
        500, "Internal Server Error",
        ShouldServeCrashReportDetails() ? lua_tostring(L, -1) : NULL);
    lua_pop(L, 1);  // pop error
    return error;
  }
}

static char *ServeLua(struct Asset *a, const char *s, size_t n) {
  char *code;
  size_t codelen;
  lua_State *L = GL;
  LockInc(&shared->c.dynamicrequests);
  effectivepath.p = s;
  effectivepath.n = n;
  if ((code = FreeLater(LoadAsset(a, &codelen)))) {
    int status =
        luaL_loadbuffer(L, code, codelen,
                        FreeLater(xasprintf("@%s", FreeLater(strndup(s, n)))));
    if (status == LUA_OK && LuaCallWithYield(L) == LUA_OK) {
      return CommitOutput(GetLuaResponse());
    } else {
      char *error;
      LogLuaError("lua code", lua_tostring(L, -1));
      error = ServeErrorWithDetail(
          500, "Internal Server Error",
          ShouldServeCrashReportDetails() ? lua_tostring(L, -1) : NULL);
      lua_pop(L, 1);  // pop error
      return error;
    }
  }
  return ServeError(500, "Internal Server Error");
}

static char *HandleRedirect(struct Redirect *r) {
  int code;
  struct Asset *a;
  if (!r->code && (a = GetAsset(r->location.s, r->location.n))) {
    LockInc(&shared->c.rewrites);
    DEBUGF("(rsp) internal redirect to %`'s", r->location.s);
    if (!HasString(&loops, r->location.s, r->location.n)) {
      AddString(&loops, r->location.s, r->location.n);
      return RoutePath(r->location.s, r->location.n);
    } else {
      LockInc(&shared->c.loops);
      return SetStatus(508, "Loop Detected");
    }
  } else if (msg.version < 10) {
    return ServeError(505, "HTTP Version Not Supported");
  } else {
    LockInc(&shared->c.redirects);
    code = r->code;
    if (!code) code = 307;
    DEBUGF("(rsp) %d redirect to %`'s", code, r->location.s);
    return AppendHeader(
        SetStatus(code, GetHttpReason(code)), "Location",
        FreeLater(EncodeHttpHeaderValue(r->location.s, r->location.n, 0)));
  }
}

static char *HandleFolder(const char *path, size_t pathlen) {
  char *p;
  if (url.path.n && url.path.p[url.path.n - 1] != '/' &&
      SlicesEqual(path, pathlen, url.path.p, url.path.n)) {
    return RedirectSlash();
  }
  if ((p = ServeIndex(path, pathlen))) {
    return p;
  } else {
    LockInc(&shared->c.forbiddens);
    WARNF("(srvr) directory %`'.*s lacks index page", pathlen, path);
    return ServeError(403, "Forbidden");
  }
}

static bool Reindex(void) {
  if (OpenZip(false)) {
    LockInc(&shared->c.reindexes);
    return true;
  } else {
    return false;
  }
}

static const char *LuaCheckPath(lua_State *L, int idx, size_t *pathlen) {
  const char *path;
  if (lua_isnoneornil(L, idx)) {
    path = url.path.p;
    *pathlen = url.path.n;
  } else {
    path = luaL_checklstring(L, idx, pathlen);
    if (!IsReasonablePath(path, *pathlen)) {
      WARNF("(srvr) bad path %`'.*s", *pathlen, path);
      luaL_argerror(L, idx, "bad path");
      unreachable;
    }
  }
  return path;
}

static const char *LuaCheckHost(lua_State *L, int idx, size_t *hostlen) {
  const char *host;
  if (lua_isnoneornil(L, idx)) {
    host = url.host.p;
    *hostlen = url.host.n;
  } else {
    host = luaL_checklstring(L, idx, hostlen);
    if (!IsAcceptableHost(host, *hostlen)) {
      WARNF("(srvr) bad host %`'.*s", *hostlen, host);
      luaL_argerror(L, idx, "bad host");
      unreachable;
    }
  }
  return host;
}

static void OnlyCallFromInitLua(lua_State *L, const char *api) {
  if (isinitialized) {
    luaL_error(L, "%s() should be called %s", api,
               "from the global scope of .init.lua");
    unreachable;
  }
}

static void OnlyCallFromMainProcess(lua_State *L, const char *api) {
  if (__isworker) {
    luaL_error(L, "%s() should be called %s", api,
               "from .init.lua or the repl");
    unreachable;
  }
}

static void OnlyCallDuringConnection(lua_State *L, const char *api) {
  if (!ishandlingconnection) {
    luaL_error(L, "%s() can only be called ", api,
               "while handling a connection");
    unreachable;
  }
}

static void OnlyCallDuringRequest(lua_State *L, const char *api) {
  if (!ishandlingrequest) {
    luaL_error(L, "%s() can only be called %s", api,
               "while handling a request");
    unreachable;
  }
}

static int LuaServe(lua_State *L, const char *api, char *impl(void)) {
  OnlyCallDuringRequest(L, api);
  luaheaderp = impl();
  return 0;
}

static int LuaServeListing(lua_State *L) {
  return LuaServe(L, "ServeListing", ServeListing);
}

static int LuaServeStatusz(lua_State *L) {
  return LuaServe(L, "ServeStatusz", ServeStatusz);
}

static int LuaServeAsset(lua_State *L) {
  size_t pathlen;
  struct Asset *a;
  const char *path;
  OnlyCallDuringRequest(L, "ServeAsset");
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen)) && !S_ISDIR(GetMode(a))) {
    luaheaderp = ServeAsset(a, path, pathlen);
    lua_pushboolean(L, true);
  } else {
    lua_pushboolean(L, false);
  }
  return 1;
}

static int LuaServeIndex(lua_State *L) {
  size_t pathlen;
  const char *path;
  OnlyCallDuringRequest(L, "ServeIndex");
  path = LuaCheckPath(L, 1, &pathlen);
  lua_pushboolean(L, !!(luaheaderp = ServeIndex(path, pathlen)));
  return 1;
}

static int LuaServeRedirect(lua_State *L) {
  size_t loclen;
  const char *location, *eval;
  int code;
  OnlyCallDuringRequest(L, "ServeRedirect");

  code = luaL_checkinteger(L, 1);
  if (!(300 <= code && code <= 399)) {
    luaL_argerror(L, 1, "bad status code");
    unreachable;
  }
  location = luaL_checklstring(L, 2, &loclen);
  if (msg.version < 10) {
    (void)ServeError(505, "HTTP Version Not Supported");
    lua_pushboolean(L, false);
  } else {
    if (!(eval = EncodeHttpHeaderValue(location, loclen, 0))) {
      luaL_argerror(L, 2, "invalid location");
      unreachable;
    }
    VERBOSEF("(rsp) %d redirect to %`'s", code, location);
    luaheaderp =
        AppendHeader(SetStatus(code, GetHttpReason(code)), "Location", eval);
    free(eval);
    lua_pushboolean(L, true);
  }
  return 1;
}

static int LuaRoutePath(lua_State *L) {
  size_t pathlen;
  const char *path;
  OnlyCallDuringRequest(L, "RoutePath");
  path = LuaCheckPath(L, 1, &pathlen);
  lua_pushboolean(L, !!(luaheaderp = RoutePath(path, pathlen)));
  return 1;
}

static int LuaRouteHost(lua_State *L) {
  size_t hostlen, pathlen;
  const char *host, *path;
  OnlyCallDuringRequest(L, "RouteHost");
  host = LuaCheckHost(L, 1, &hostlen);
  path = LuaCheckPath(L, 2, &pathlen);
  lua_pushboolean(L, !!(luaheaderp = RouteHost(host, hostlen, path, pathlen)));
  return 1;
}

static int LuaRoute(lua_State *L) {
  size_t hostlen, pathlen;
  const char *host, *path;
  OnlyCallDuringRequest(L, "Route");
  host = LuaCheckHost(L, 1, &hostlen);
  path = LuaCheckPath(L, 2, &pathlen);
  lua_pushboolean(L, !!(luaheaderp = Route(host, hostlen, path, pathlen)));
  return 1;
}

static int LuaRespond(lua_State *L, char *R(unsigned, const char *)) {
  char *p;
  int code;
  size_t reasonlen;
  const char *reason;
  OnlyCallDuringRequest(L, "Respond");
  code = luaL_checkinteger(L, 1);
  if (!(100 <= code && code <= 999)) {
    luaL_argerror(L, 1, "bad status code");
    unreachable;
  }
  if (lua_isnoneornil(L, 2)) {
    luaheaderp = R(code, GetHttpReason(code));
  } else {
    reason = lua_tolstring(L, 2, &reasonlen);
    if ((p = EncodeHttpHeaderValue(reason, MIN(reasonlen, 128), 0))) {
      luaheaderp = R(code, p);
      free(p);
    } else {
      luaL_argerror(L, 2, "invalid");
      unreachable;
    }
  }
  return 0;
}

static int LuaSetStatus(lua_State *L) {
  return LuaRespond(L, SetStatus);
}

static int LuaGetStatus(lua_State *L) {
  OnlyCallDuringRequest(L, "GetStatus");
  if (!luaheaderp) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, statuscode);
  }
  return 1;
}

static int LuaGetSslIdentity(lua_State *L) {
  const mbedtls_x509_crt *cert;
  OnlyCallDuringRequest(L, "GetSslIdentity");
  if (!usingssl) {
    lua_pushnil(L);
  } else {
    if (sslpskindex) {
      CHECK((sslpskindex - 1) >= 0 && (sslpskindex - 1) < psks.n);
      lua_pushlstring(L, psks.p[sslpskindex - 1].identity,
                      psks.p[sslpskindex - 1].identity_len);
    } else {
      cert = mbedtls_ssl_get_peer_cert(&ssl);
      lua_pushstring(L, cert ? gc(FormatX509Name(&cert->subject)) : "");
    }
  }
  return 1;
}

static int LuaServeError(lua_State *L) {
  return LuaRespond(L, ServeError);
}

static int LuaLoadAsset(lua_State *L) {
  void *data;
  struct Asset *a;
  const char *path;
  size_t size, pathlen;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen))) {
    if (!a->file && !IsCompressed(a)) {
      /* fast path: this avoids extra copy */
      data = ZIP_LFILE_CONTENT(zbase + a->lf);
      size = GetZipLfileUncompressedSize(zbase + a->lf);
      if (Verify(data, size, ZIP_LFILE_CRC32(zbase + a->lf))) {
        lua_pushlstring(L, data, size);
        return 1;
      }
      // any error from Verify has already been reported
    } else if ((data = LoadAsset(a, &size))) {
      lua_pushlstring(L, data, size);
      free(data);
      return 1;
    } else {
      WARNF("(srvr) could not load asset: %`'.*s", pathlen, path);
    }
  } else {
    WARNF("(srvr) could not find asset: %`'.*s", pathlen, path);
  }
  return 0;
}

static void GetDosLocalTime(int64_t utcunixts, uint16_t *out_time,
                            uint16_t *out_date) {
  struct tm tm;
  CHECK_NOTNULL(localtime_r(&utcunixts, &tm));
  *out_time = DOS_TIME(tm.tm_hour, tm.tm_min, tm.tm_sec);
  *out_date = DOS_DATE(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday + 1);
}

static void StoreAsset(char *path, size_t pathlen, char *data, size_t datalen,
                       int mode) {
  int64_t ft;
  int i;
  uint32_t crc;
  char *comp, *p;
  long double now;
  struct Asset *a;
  struct iovec v[13];
  uint8_t *cdir, era;
  const char *use;
  uint16_t gflags, iattrs, mtime, mdate, dosmode, method, disk;
  size_t oldcdirsize, oldcdiroffset, records, cdiroffset, cdirsize, complen,
      uselen;
  if (IsOpenbsd() || IsNetbsd() || IsWindows()) {
    FATALF("(cfg) StoreAsset() not available on Windows/NetBSD/OpenBSD yet");
  }
  INFOF("(srvr) storing asset %`'s", path);
  disk = gflags = iattrs = 0;
  if (_isutf8(path, pathlen)) gflags |= kZipGflagUtf8;
  if (_istext(data, datalen)) iattrs |= kZipIattrText;
  crc = crc32_z(0, data, datalen);
  if (datalen < 100) {
    method = kZipCompressionNone;
    comp = 0;
    use = data;
    uselen = datalen;
    era = kZipEra1989;
  } else {
    comp = Deflate(data, datalen, &complen);
    if (complen < datalen) {
      method = kZipCompressionDeflate;
      use = comp;
      uselen = complen;
      era = kZipEra1993;
    } else {
      method = kZipCompressionNone;
      use = data;
      uselen = datalen;
      era = kZipEra1989;
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  if (-1 == fcntl(zfd, F_SETLKW, &(struct flock){F_WRLCK})) {
    WARNF("can't place write lock on file descriptor %d: %s", zfd,
          strerror(errno));
    return;
  }
  OpenZip(false);
  now = nowl();
  a = GetAssetZip(path, pathlen);
  if (!mode) mode = a ? GetMode(a) : 0644;
  if (!(mode & S_IFMT)) mode |= S_IFREG;
  if (pathlen > 1 && path[0] == '/') ++path, --pathlen;
  dosmode = !(mode & 0200) ? kNtFileAttributeReadonly : 0;
  ft = (now + MODERNITYSECONDS) * HECTONANOSECONDS;
  GetDosLocalTime(now, &mtime, &mdate);
  // local file header
  if (uselen >= 0xffffffff || datalen >= 0xffffffff) {
    era = kZipEra2001;
    v[2].iov_base = p = alloca((v[2].iov_len = 2 + 2 + 8 + 8));
    p = WRITE16LE(p, kZipExtraZip64);
    p = WRITE16LE(p, 8 + 8);
    p = WRITE64LE(p, uselen);
    p = WRITE64LE(p, datalen);
  } else {
    v[2].iov_len = 0;
    v[2].iov_base = 0;
  }
  v[0].iov_base = p = alloca((v[0].iov_len = kZipLfileHdrMinSize));
  p = WRITE32LE(p, kZipLfileHdrMagic);
  *p++ = era;
  *p++ = kZipOsDos;
  p = WRITE16LE(p, gflags);
  p = WRITE16LE(p, method);
  p = WRITE16LE(p, mtime);
  p = WRITE16LE(p, mdate);
  p = WRITE32LE(p, crc);
  p = WRITE32LE(p, MIN(uselen, 0xffffffff));
  p = WRITE32LE(p, MIN(datalen, 0xffffffff));
  p = WRITE16LE(p, pathlen);
  p = WRITE16LE(p, v[2].iov_len);
  v[1].iov_len = pathlen;
  v[1].iov_base = path;
  // file data
  v[3].iov_len = uselen;
  v[3].iov_base = use;
  // old central directory entries
  oldcdirsize = GetZipCdirSize(zcdir);
  oldcdiroffset = GetZipCdirOffset(zcdir);
  if (a) {
    // to remove an existing asset,
    // first copy the central directory part before its record
    v[4].iov_base = zbase + oldcdiroffset;
    v[4].iov_len = a->cf - oldcdiroffset;
    // and then the rest of the central directory
    v[5].iov_base = zbase + oldcdiroffset +
                    (v[4].iov_len + ZIP_CFILE_HDRSIZE(zbase + a->cf));
    v[5].iov_len =
        oldcdirsize - (v[4].iov_len + ZIP_CFILE_HDRSIZE(zbase + a->cf));
  } else {
    v[4].iov_base = zbase + oldcdiroffset;
    v[4].iov_len = oldcdirsize;
    v[5].iov_base = 0;
    v[5].iov_len = 0;
  }
  // new central directory entry
  if (uselen >= 0xffffffff || datalen >= 0xffffffff || zsize >= 0xffffffff) {
    v[8].iov_base = p = alloca((v[8].iov_len = 2 + 2 + 8 + 8 + 8));
    p = WRITE16LE(p, kZipExtraZip64);
    p = WRITE16LE(p, 8 + 8 + 8);
    p = WRITE64LE(p, uselen);
    p = WRITE64LE(p, datalen);
    p = WRITE64LE(p, zsize);
  } else {
    v[8].iov_len = 0;
    v[8].iov_base = 0;
  }
  v[9].iov_base = p = alloca((v[9].iov_len = 2 + 2 + 4 + 2 + 2 + 8 + 8 + 8));
  p = WRITE16LE(p, kZipExtraNtfs);
  p = WRITE16LE(p, 4 + 2 + 2 + 8 + 8 + 8);
  p = WRITE32LE(p, 0);
  p = WRITE16LE(p, 1);
  p = WRITE16LE(p, 8 + 8 + 8);
  p = WRITE64LE(p, ft);
  p = WRITE64LE(p, ft);
  p = WRITE64LE(p, ft);
  v[6].iov_base = p = alloca((v[6].iov_len = kZipCfileHdrMinSize));
  p = WRITE32LE(p, kZipCfileHdrMagic);
  *p++ = kZipCosmopolitanVersion;
  *p++ = kZipOsUnix;
  *p++ = era;
  *p++ = kZipOsDos;
  p = WRITE16LE(p, gflags);
  p = WRITE16LE(p, method);
  p = WRITE16LE(p, mtime);
  p = WRITE16LE(p, mdate);
  p = WRITE32LE(p, crc);
  p = WRITE32LE(p, MIN(uselen, 0xffffffff));
  p = WRITE32LE(p, MIN(datalen, 0xffffffff));
  p = WRITE16LE(p, pathlen);
  p = WRITE16LE(p, v[8].iov_len + v[9].iov_len);
  p = WRITE16LE(p, 0);
  p = WRITE16LE(p, disk);
  p = WRITE16LE(p, iattrs);
  p = WRITE16LE(p, dosmode);
  p = WRITE16LE(p, mode);
  p = WRITE32LE(p, MIN(zsize, 0xffffffff));
  v[7].iov_len = pathlen;
  v[7].iov_base = path;
  // zip64 end of central directory
  cdiroffset =
      zsize + v[0].iov_len + v[1].iov_len + v[2].iov_len + v[3].iov_len;
  cdirsize = v[4].iov_len + v[5].iov_len + v[6].iov_len + v[7].iov_len +
             v[8].iov_len + v[9].iov_len;
  records = GetZipCdirRecords(zcdir) + !a;
  if (records >= 0xffff || cdiroffset >= 0xffffffff || cdirsize >= 0xffffffff) {
    v[10].iov_base = p =
        alloca((v[10].iov_len = kZipCdir64HdrMinSize + kZipCdir64LocatorSize));
    p = WRITE32LE(p, kZipCdir64HdrMagic);
    p = WRITE64LE(p, 2 + 2 + 4 + 4 + 8 + 8 + 8 + 8);
    p = WRITE16LE(p, kZipCosmopolitanVersion);
    p = WRITE16LE(p, kZipEra2001);
    p = WRITE32LE(p, disk);
    p = WRITE32LE(p, disk);
    p = WRITE64LE(p, records);
    p = WRITE64LE(p, records);
    p = WRITE64LE(p, cdirsize);
    p = WRITE64LE(p, cdiroffset);
    p = WRITE32LE(p, kZipCdir64LocatorMagic);
    p = WRITE32LE(p, disk);
    p = WRITE64LE(p, cdiroffset + cdirsize);
    p = WRITE32LE(p, disk);
  } else {
    v[10].iov_len = 0;
    v[10].iov_base = 0;
  }
  // end of central directory
  v[12].iov_base = GetZipCdirComment(zcdir);
  v[12].iov_len = GetZipCdirCommentSize(zcdir);
  v[11].iov_base = p = alloca((v[11].iov_len = kZipCdirHdrMinSize));
  p = WRITE32LE(p, kZipCdirHdrMagic);
  p = WRITE16LE(p, disk);
  p = WRITE16LE(p, disk);
  p = WRITE16LE(p, MIN(records, 0xffff));
  p = WRITE16LE(p, MIN(records, 0xffff));
  p = WRITE32LE(p, MIN(cdirsize, 0xffffffff));
  p = WRITE32LE(p, MIN(cdiroffset, 0xffffffff));
  p = WRITE16LE(p, v[12].iov_len);
  CHECK_NE(-1, lseek(zfd, zbase + zsize - zmap, SEEK_SET));
  CHECK_NE(-1, WritevAll(zfd, v, 13));
  CHECK_NE(-1, fcntl(zfd, F_SETLK, &(struct flock){F_UNLCK}));
  //////////////////////////////////////////////////////////////////////////////
  OpenZip(false);
  free(comp);
}

static void StoreFile(char *path) {
  char *p;
  size_t plen, tlen;
  struct stat st;
  char *target = path;
  if (startswith(target, "./")) target += 2;
  tlen = strlen(target);
  if (!IsReasonablePath(target, tlen))
    FATALF("(cfg) error: can't store %`'s: contains '.' or '..' segments",
           target);
  if (lstat(path, &st) == -1) FATALF("(cfg) error: can't stat %`'s: %m", path);
  if (!(p = xslurp(path, &plen)))
    FATALF("(cfg) error: can't read %`'s: %m", path);
  StoreAsset(target, tlen, p, plen, st.st_mode & 0777);
  free(p);
}

static void StorePath(const char *dirpath) {
  DIR *d;
  char *path;
  struct dirent *e;
  if (!isdirectory(dirpath) && !endswith(dirpath, "/"))
    return StoreFile(dirpath);
  if (!(d = opendir(dirpath))) FATALF("(cfg) error: can't open %`'s", dirpath);
  while ((e = readdir(d))) {
    if (strcmp(e->d_name, ".") == 0) continue;
    if (strcmp(e->d_name, "..") == 0) continue;
    path = gc(xjoinpaths(dirpath, e->d_name));
    if (e->d_type == DT_DIR) {
      StorePath(path);
    } else {
      StoreFile(path);
    }
  }
  closedir(d);
}

static int LuaStoreAsset(lua_State *L) {
  const char *path, *data;
  size_t pathlen, datalen;
  int mode;
  path = LuaCheckPath(L, 1, &pathlen);
  if (pathlen > 0xffff) {
    return luaL_argerror(L, 1, "path too long");
  }
  data = luaL_checklstring(L, 2, &datalen);
  mode = luaL_optinteger(L, 3, 0);
  StoreAsset(path, pathlen, data, datalen, mode);
  return 0;
}

static void ReseedRng(mbedtls_ctr_drbg_context *r, const char *s) {
#ifndef UNSECURE
  if (unsecure) return;
  CHECK_EQ(0, mbedtls_ctr_drbg_reseed(r, (void *)s, strlen(s)));
#endif
}

static wontreturn void LuaThrowTlsError(lua_State *L, const char *s, int r) {
  const char *code;
  code = gc(xasprintf("-0x%04x", -r));
  if (!IsTiny()) {
    luaL_error(L, "tls %s failed (%s %s)", s, code, GetTlsError(r));
  } else {
    luaL_error(L, "tls %s failed (grep %s)", s, code);
  }
  unreachable;
}

static void LogMessage(const char *d, const char *s, size_t n) {
  size_t n2, n3;
  char *s2, *s3;
  if (!LOGGABLE(kLogInfo)) return;
  while (n && (s[n - 1] == '\r' || s[n - 1] == '\n')) --n;
  if ((s2 = DecodeLatin1(s, n, &n2))) {
    if ((s3 = IndentLines(s2, n2, &n3, 1))) {
      INFOF("(stat) %s %,ld byte message\r\n%.*s", d, n, n3, s3);
      free(s3);
    }
    free(s2);
  }
}

static void LogBody(const char *d, const char *s, size_t n) {
  char *s2, *s3;
  size_t n2, n3;
  if (!n) return;
  if (!LOGGABLE(kLogInfo)) return;
  while (n && (s[n - 1] == '\r' || s[n - 1] == '\n')) --n;
  if ((s2 = VisualizeControlCodes(s, n, &n2))) {
    if ((s3 = IndentLines(s2, n2, &n3, 1))) {
      INFOF("(stat) %s %,ld byte payload\r\n%.*s", d, n, n3, s3);
      free(s3);
    }
    free(s2);
  }
}

static int LuaFetch(lua_State *L) {
#define ssl nope  // TODO(jart): make this file less huge
  char *p;
  ssize_t rc;
  bool usingssl;
  uint32_t ip;
  struct Url url;
  int t, ret, sock, methodidx, hdridx;
  char *host, *port;
  struct TlsBio *bio;
  struct addrinfo *addr;
  struct Buffer inbuf;     // shadowing intentional
  struct HttpMessage msg;  // shadowing intentional
  struct HttpUnchunker u;
  const char *urlarg, *request, *body, *method;
  char *conlenhdr = "";
  char *headers = 0;
  char *hosthdr = 0;
  char *agenthdr = brand;
  char *key, *val, *hdr;
  size_t keylen, vallen;
  size_t urlarglen, requestlen, paylen, bodylen;
  size_t g, i, n, hdrsize;
  int numredirects = 0, maxredirects = 5;
  bool followredirect = true;
  struct addrinfo hints = {.ai_family = AF_INET,
                           .ai_socktype = SOCK_STREAM,
                           .ai_protocol = IPPROTO_TCP,
                           .ai_flags = AI_NUMERICSERV};

  /*
   * Get args: url [, body | {method = "PUT", body = "..."}]
   */
  urlarg = luaL_checklstring(L, 1, &urlarglen);
  if (lua_istable(L, 2)) {
    lua_settop(L, 2);  // discard any extra arguments
    lua_getfield(L, 2, "body");
    body = luaL_optlstring(L, -1, "", &bodylen);
    lua_getfield(L, 2, "method");
    // use GET by default if no method is provided
    method = strtoupper(luaL_optstring(L, -1, kHttpMethod[kHttpGet]));
    lua_getfield(L, 2, "followredirect");
    if (lua_isboolean(L, -1)) followredirect = lua_toboolean(L, -1);
    lua_getfield(L, 2, "maxredirects");
    maxredirects = luaL_optinteger(L, -1, maxredirects);
    lua_getfield(L, 2, "numredirects");
    numredirects = luaL_optinteger(L, -1, numredirects);
    lua_getfield(L, 2, "headers");
    if (!lua_isnil(L, -1)) {
      if (!lua_istable(L, -1))
        return luaL_argerror(L, 2, "invalid headers value; table expected");

      lua_pushnil(L);
      while (lua_next(L, -2)) {
        if (lua_type(L, -2) == LUA_TSTRING) {  // skip any non-string keys
          key = lua_tolstring(L, -2, &keylen);
          if (!IsValidHttpToken(key, keylen))
            return luaL_argerror(L, 2, "invalid header name");

          val = lua_tolstring(L, -1, &vallen);
          if (!(hdr = gc(EncodeHttpHeaderValue(val, vallen, 0))))
            return luaL_argerror(L, 2, "invalid header value encoding");

          // Content-Length and Connection will be overwritten;
          // skip them to avoid duplicates;
          // also allow unknown headers
          if ((hdridx = GetHttpHeader(key, keylen)) == -1 ||
              hdridx != kHttpContentLength && hdridx != kHttpConnection) {
            if (hdridx == kHttpUserAgent) {
              agenthdr = hdr;
            } else if (hdridx == kHttpHost) {
              hosthdr = hdr;
            } else {
              appendd(&headers, key, keylen);
              appendw(&headers, READ16LE(": "));
              appends(&headers, hdr);
              appendw(&headers, READ16LE("\r\n"));
            }
          }
        }
        lua_pop(L, 1);  // pop the value, keep the key for the next iteration
      }
    }
    lua_settop(L, 2);  // drop all added elements to keep the stack balanced
  } else if (lua_isnoneornil(L, 2)) {
    body = "";
    bodylen = 0;
    method = kHttpMethod[kHttpGet];
  } else {
    body = luaL_checklstring(L, 2, &bodylen);
    method = kHttpMethod[kHttpPost];
  }
  // provide Content-Length header unless it's zero and not expected
  methodidx = GetHttpMethod(method, -1);
  if (bodylen > 0 || !(methodidx == kHttpGet || methodidx == kHttpHead ||
                       methodidx == kHttpTrace || methodidx == kHttpDelete ||
                       methodidx == kHttpConnect)) {
    conlenhdr = gc(xasprintf("Content-Length: %zu\r\n", bodylen));
  }

  /*
   * Parse URL.
   */
  gc(ParseUrl(urlarg, urlarglen, &url));
  gc(url.params.p);
  usingssl = false;
  if (url.scheme.n) {
#ifndef UNSECURE
    if (!unsecure && url.scheme.n == 5 &&
        !memcasecmp(url.scheme.p, "https", 5)) {
      usingssl = true;
    } else
#endif
        if (!(url.scheme.n == 4 && !memcasecmp(url.scheme.p, "http", 4))) {
      luaL_argerror(L, 1, "bad scheme");
      unreachable;
    }
  }

#ifndef UNSECURE
  if (usingssl && !sslinitialized) TlsInit();
#endif

  if (url.host.n) {
    host = gc(strndup(url.host.p, url.host.n));
    if (url.port.n) {
      port = gc(strndup(url.port.p, url.port.n));
#ifndef UNSECURE
    } else if (usingssl) {
      port = "443";
#endif
    } else {
      port = "80";
    }
  } else {
    ip = servers.n ? ntohl(servers.p[0].addr.sin_addr.s_addr) : INADDR_LOOPBACK;
    host =
        gc(xasprintf("%hhu.%hhu.%hhu.%hhu", ip >> 24, ip >> 16, ip >> 8, ip));
    port =
        gc(xasprintf("%d", servers.n ? ntohs(servers.p[0].addr.sin_port) : 80));
  }
  if (!IsAcceptableHost(host, -1)) {
    luaL_argerror(L, 1, "invalid host");
    unreachable;
  }
  if (!hosthdr) hosthdr = gc(xasprintf("%s:%s", host, port));

  url.fragment.p = 0, url.fragment.n = 0;
  url.scheme.p = 0, url.scheme.n = 0;
  url.user.p = 0, url.user.n = 0;
  url.pass.p = 0, url.pass.n = 0;
  url.host.p = 0, url.host.n = 0;
  url.port.p = 0, url.port.n = 0;
  if (!url.path.n || url.path.p[0] != '/') {
    p = gc(xmalloc(1 + url.path.n));
    mempcpy(mempcpy(p, "/", 1), url.path.p, url.path.n);
    url.path.p = p;
    ++url.path.n;
  }

  /*
   * Create HTTP message.
   */
  request = gc(xasprintf("%s %s HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "Connection: close\r\n"
                         "User-Agent: %s\r\n"
                         "%s%s"
                         "\r\n%s",
                         method, gc(EncodeUrl(&url, 0)), hosthdr, agenthdr,
                         conlenhdr, headers ? headers : "", body));
  requestlen = strlen(request);

  /*
   * Perform DNS lookup.
   */
  DEBUGF("(ftch) client resolving %s", host);
  if ((rc = getaddrinfo(host, port, &hints, &addr)) != EAI_SUCCESS) {
    luaL_error(L, "getaddrinfo(%s:%s) error: EAI_%s %s", host, port,
               gai_strerror(rc), strerror(errno));
    unreachable;
  }

  /*
   * Connect to server.
   */
  ip = ntohl(((struct sockaddr_in *)addr->ai_addr)->sin_addr.s_addr);
  DEBUGF("(ftch) client connecting %hhu.%hhu.%hhu.%hhu:%d", ip >> 24, ip >> 16,
         ip >> 8, ip, ntohs(((struct sockaddr_in *)addr->ai_addr)->sin_port));
  CHECK_NE(-1, (sock = GoodSocket(addr->ai_family, addr->ai_socktype,
                                  addr->ai_protocol, false, &timeout)));
  rc = connect(sock, addr->ai_addr, addr->ai_addrlen);
  freeaddrinfo(addr), addr = 0;
  if (rc == -1) {
    close(sock);
    luaL_error(L, "connect(%s:%s) error: %s", host, port, strerror(errno));
    unreachable;
  }

#ifndef UNSECURE
  if (usingssl) {
    if (sslcliused) {
      mbedtls_ssl_session_reset(&sslcli);
    } else {
      ReseedRng(&rngcli, "child");
    }
    sslcliused = true;
    DEBUGF("(ftch) client handshaking %`'s", host);
    if (!evadedragnetsurveillance) {
      mbedtls_ssl_set_hostname(&sslcli, host);
    }
    bio = gc(malloc(sizeof(struct TlsBio)));
    bio->fd = sock;
    bio->a = 0;
    bio->b = 0;
    bio->c = -1;
    mbedtls_ssl_set_bio(&sslcli, bio, TlsSend, 0, TlsRecvImpl);
    while ((ret = mbedtls_ssl_handshake(&sslcli))) {
      switch (ret) {
        case MBEDTLS_ERR_SSL_WANT_READ:
          break;
        case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
          goto VerifyFailed;
        default:
          close(sock);
          LuaThrowTlsError(L, "handshake", ret);
          unreachable;
      }
    }
    LockInc(&shared->c.sslhandshakes);
    VERBOSEF("(ftch) shaken %s:%s %s %s", host, port,
             mbedtls_ssl_get_ciphersuite(&sslcli),
             mbedtls_ssl_get_version(&sslcli));
  }
#endif /* UNSECURE */

  /*
   * Send HTTP Message.
   */
  DEBUGF("(ftch) client sending %s request", method);
#ifndef UNSECURE
  if (usingssl) {
    ret = mbedtls_ssl_write(&sslcli, request, requestlen);
    if (ret != requestlen) {
      if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) goto VerifyFailed;
      close(sock);
      LuaThrowTlsError(L, "write", ret);
      unreachable;
    }
  } else
#endif
      if (WRITE(sock, request, requestlen) != requestlen) {
    close(sock);
    luaL_error(L, "write error: %s", strerror(errno));
    unreachable;
  }
  if (logmessages) {
    LogMessage("sent", request, requestlen);
  }

  /*
   * Handle response.
   */
  bzero(&inbuf, sizeof(inbuf));
  InitHttpMessage(&msg, kHttpResponse);
  for (hdrsize = paylen = t = 0;;) {
    if (inbuf.n == inbuf.c) {
      inbuf.c += 1000;
      inbuf.c += inbuf.c >> 1;
      inbuf.p = realloc(inbuf.p, inbuf.c);
    }
    NOISEF("(ftch) client reading");
#ifndef UNSECURE
    if (usingssl) {
      if ((rc = mbedtls_ssl_read(&sslcli, inbuf.p + inbuf.n,
                                 inbuf.c - inbuf.n)) < 0) {
        if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
          rc = 0;
        } else {
          close(sock);
          free(inbuf.p);
          DestroyHttpMessage(&msg);
          LuaThrowTlsError(L, "read", rc);
          unreachable;
        }
      }
    } else
#endif
        if ((rc = READ(sock, inbuf.p + inbuf.n, inbuf.c - inbuf.n)) == -1) {
      close(sock);
      free(inbuf.p);
      DestroyHttpMessage(&msg);
      luaL_error(L, "read error: %s", strerror(errno));
      unreachable;
    }
    g = rc;
    inbuf.n += g;
    switch (t) {
      case kHttpClientStateHeaders:
        if (!g) {
          WARNF("(ftch) HTTP client %s error", "EOF headers");
          goto TransportError;
        }
        rc = ParseHttpMessage(&msg, inbuf.p, inbuf.n);
        if (rc == -1) {
          WARNF("(ftch) HTTP client %s error", "ParseHttpMessage");
          goto TransportError;
        }
        if (rc) {
          hdrsize = rc;
          if (logmessages) {
            LogMessage("received", inbuf.p, hdrsize);
          }
          if (100 <= msg.status && msg.status <= 199) {
            if ((HasHeader(kHttpContentLength) &&
                 !HeaderEqualCase(kHttpContentLength, "0")) ||
                (HasHeader(kHttpTransferEncoding) &&
                 !HeaderEqualCase(kHttpTransferEncoding, "identity"))) {
              WARNF("(ftch) HTTP client %s error", "Content-Length #1");
              goto TransportError;
            }
            DestroyHttpMessage(&msg);
            InitHttpMessage(&msg, kHttpResponse);
            memmove(inbuf.p, inbuf.p + hdrsize, inbuf.n - hdrsize);
            inbuf.n -= hdrsize;
            break;
          }
          if (msg.status == 204 || msg.status == 304) {
            goto Finished;
          }
          if (HasHeader(kHttpTransferEncoding) &&
              !HeaderEqualCase(kHttpTransferEncoding, "identity")) {
            if (HeaderEqualCase(kHttpTransferEncoding, "chunked")) {
              t = kHttpClientStateBodyChunked;
              bzero(&u, sizeof(u));
              goto Chunked;
            } else {
              WARNF("(ftch) HTTP client %s error", "Transfer-Encoding");
              goto TransportError;
            }
          } else if (HasHeader(kHttpContentLength)) {
            rc = ParseContentLength(HeaderData(kHttpContentLength),
                                    HeaderLength(kHttpContentLength));
            if (rc == -1) {
              WARNF("(ftch) HTTP client %s error", "Content-Length #2");
              goto TransportError;
            }
            if ((paylen = rc) <= inbuf.n - hdrsize) {
              goto Finished;
            } else {
              t = kHttpClientStateBodyLengthed;
            }
          } else {
            t = kHttpClientStateBody;
          }
        }
        break;
      case kHttpClientStateBody:
        if (!g) {
          paylen = inbuf.n;
          goto Finished;
        }
        break;
      case kHttpClientStateBodyLengthed:
        if (!g) {
          WARNF("(ftch) HTTP client %s error", "EOF body");
          goto TransportError;
        }
        if (inbuf.n - hdrsize >= paylen) {
          goto Finished;
        }
        break;
      case kHttpClientStateBodyChunked:
      Chunked:
        rc = Unchunk(&u, inbuf.p + hdrsize, inbuf.n - hdrsize, &paylen);
        if (rc == -1) {
          WARNF("(ftch) HTTP client %s error", "Unchunk");
          goto TransportError;
        }
        if (rc) goto Finished;
        break;
      default:
        unreachable;
    }
  }

Finished:
  if (paylen && logbodies) LogBody("received", inbuf.p + hdrsize, paylen);
  VERBOSEF("(ftch) completed %s HTTP%02d %d %s %`'.*s", method, msg.version,
           msg.status, urlarg, HeaderLength(kHttpServer),
           HeaderData(kHttpServer));
  if (followredirect && HasHeader(kHttpLocation) &&
      (msg.status == 301 || msg.status == 308 ||  // permanent redirects
       msg.status == 302 || msg.status == 307 ||  // temporary redirects
       msg.status == 303 /* see other; non-GET changes to GET, body lost */) &&
      numredirects < maxredirects) {
    // if 303, then remove body and set method to GET
    if (msg.status == 303) {
      body = "";
      bodylen = 0;
      method = kHttpMethod[kHttpGet];
    }
    // create table if needed
    if (!lua_istable(L, 2)) {
      lua_settop(L, 1);          // pop body if present
      lua_createtable(L, 0, 3);  // body, method, numredirects
    }
    lua_pushlstring(L, body, bodylen);
    lua_setfield(L, -2, "body");

    lua_pushstring(L, method);
    lua_setfield(L, -2, "method");

    lua_pushinteger(L, numredirects + 1);
    lua_setfield(L, -2, "numredirects");
    // replace URL with Location header
    lua_pushlstring(L, HeaderData(kHttpLocation), HeaderLength(kHttpLocation));
    lua_replace(L, -3);

    DestroyHttpMessage(&msg);
    free(inbuf.p);
    close(sock);
    return LuaFetch(L);
  } else {
    lua_pushinteger(L, msg.status);
    LuaPushHeaders(L, &msg, inbuf.p);
    lua_pushlstring(L, inbuf.p + hdrsize, paylen);
    DestroyHttpMessage(&msg);
    free(inbuf.p);
    close(sock);
    return 3;
  }
TransportError:
  DestroyHttpMessage(&msg);
  free(inbuf.p);
  close(sock);
  luaL_error(L, "transport error");
  unreachable;
#ifndef UNSECURE
VerifyFailed:
  LockInc(&shared->c.sslverifyfailed);
  close(sock);
  LuaThrowTlsError(
      L, gc(DescribeSslVerifyFailure(sslcli.session_negotiate->verify_result)),
      ret);
  unreachable;
#endif
#undef ssl
}

static int LuaGetDate(lua_State *L) {
  lua_pushinteger(L, shared->nowish);
  return 1;
}

static int LuaGetHttpVersion(lua_State *L) {
  OnlyCallDuringRequest(L, "GetHttpVersion");
  lua_pushinteger(L, msg.version);
  return 1;
}

static int LuaGetRedbeanVersion(lua_State *L) {
  lua_pushinteger(L, VERSION);
  return 1;
}

static int LuaGetMethod(lua_State *L) {
  OnlyCallDuringRequest(L, "GetMethod");
  if (msg.method) {
    lua_pushstring(L, kHttpMethod[msg.method]);
  } else {
    lua_pushlstring(L, inbuf.p + msg.xmethod.a, msg.xmethod.b - msg.xmethod.a);
  }
  return 1;
}

static int LuaGetAddr(lua_State *L, void GetAddr(uint32_t *, uint16_t *)) {
  uint32_t ip;
  uint16_t port;
  GetAddr(&ip, &port);
  lua_pushinteger(L, ip);
  lua_pushinteger(L, port);
  return 2;
}

static int LuaGetServerAddr(lua_State *L) {
  OnlyCallDuringConnection(L, "GetServerAddr");
  return LuaGetAddr(L, GetServerAddr);
}

static int LuaGetClientAddr(lua_State *L) {
  OnlyCallDuringConnection(L, "GetClientAddr");
  return LuaGetAddr(L, GetClientAddr);
}

static int LuaGetRemoteAddr(lua_State *L) {
  OnlyCallDuringRequest(L, "GetRemoteAddr");
  return LuaGetAddr(L, GetRemoteAddr);
}

static int LuaLog(lua_State *L) {
  int level, line;
  lua_Debug ar;
  const char *msg, *module;
  level = luaL_checkinteger(L, 1);
  if (LOGGABLE(level)) {
    msg = luaL_checkstring(L, 2);
    if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
      module = ar.short_src;
      line = ar.currentline;
    } else {
      module = gc(strndup(effectivepath.p, effectivepath.n));
      line = -1;
    }
    flogf(level, module, line, NULL, "%s", msg);
  }
  return 0;
}

static int LuaEncodeSmth(lua_State *L,
                         int Encoder(lua_State *, char **, char *, int)) {
  int useoutput = false;
  int maxdepth = 64;
  char *numformat = "%.14g";
  char *p = 0;
  if (lua_istable(L, 2)) {
    lua_settop(L, 2);  // discard any extra arguments
    lua_getfield(L, 2, "useoutput");
    // ignore useoutput outside of request handling
    if (ishandlingrequest && lua_isboolean(L, -1)) {
      useoutput = lua_toboolean(L, -1);
    }
    lua_getfield(L, 2, "numformat");
    numformat = luaL_optstring(L, -1, numformat);
  }
  lua_settop(L, 1);  // keep the passed argument on top
  Encoder(L, useoutput ? &outbuf : &p, numformat, -1);
  if (useoutput) {
    lua_pushnil(L);
  } else {
    lua_pushstring(L, p);
    free(p);
  }
  return 1;
}

static int LuaEncodeJson(lua_State *L) {
  return LuaEncodeSmth(L, LuaEncodeJsonData);
}

static int LuaEncodeLua(lua_State *L) {
  return LuaEncodeSmth(L, LuaEncodeLuaData);
}

static int LuaGetUrl(lua_State *L) {
  char *p;
  size_t n;
  OnlyCallDuringRequest(L, "GetUrl");
  p = EncodeUrl(&url, &n);
  lua_pushlstring(L, p, n);
  free(p);
  return 1;
}

static int LuaGetScheme(lua_State *L) {
  OnlyCallDuringRequest(L, "GetScheme");
  LuaPushUrlView(L, &url.scheme);
  return 1;
}

static int LuaGetPath(lua_State *L) {
  OnlyCallDuringRequest(L, "GetPath");
  LuaPushUrlView(L, &url.path);
  return 1;
}

static int LuaGetEffectivePath(lua_State *L) {
  OnlyCallDuringRequest(L, "GetEffectivePath");
  lua_pushlstring(L, effectivepath.p, effectivepath.n);
  return 1;
}

static int LuaGetFragment(lua_State *L) {
  OnlyCallDuringRequest(L, "GetFragment");
  LuaPushUrlView(L, &url.fragment);
  return 1;
}

static int LuaGetUser(lua_State *L) {
  size_t n;
  const char *p, *q;
  OnlyCallDuringRequest(L, "GetUser");
  if (url.user.p) {
    LuaPushUrlView(L, &url.user);
  } else if ((p = GetBasicAuthorization(&n))) {
    if (!(q = memchr(p, ':', n))) q = p + n;
    lua_pushlstring(L, p, q - p);
    free(p);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaGetPass(lua_State *L) {
  size_t n;
  const char *p, *q;
  OnlyCallDuringRequest(L, "GetPass");
  if (url.user.p) {
    LuaPushUrlView(L, &url.pass);
  } else if ((p = GetBasicAuthorization(&n))) {
    if ((q = memchr(p, ':', n))) {
      lua_pushlstring(L, q + 1, p + n - (q + 1));
    } else {
      lua_pushnil(L);
    }
    free(p);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaGetHost(lua_State *L) {
  char b[16];
  OnlyCallDuringRequest(L, "GetHost");
  if (url.host.n) {
    lua_pushlstring(L, url.host.p, url.host.n);
  } else {
    inet_ntop(AF_INET, &serveraddr->sin_addr.s_addr, b, sizeof(b));
    lua_pushstring(L, b);
  }
  return 1;
}

static int LuaGetPort(lua_State *L) {
  int i, x = 0;
  OnlyCallDuringRequest(L, "GetPort");
  for (i = 0; i < url.port.n; ++i) x = url.port.p[i] - '0' + x * 10;
  if (!x) x = ntohs(serveraddr->sin_port);
  lua_pushinteger(L, x);
  return 1;
}

static int LuaGetBody(lua_State *L) {
  OnlyCallDuringRequest(L, "GetBody");
  lua_pushlstring(L, inbuf.p + hdrsize, payloadlength);
  return 1;
}

static int LuaGetHeader(lua_State *L) {
  int h;
  const char *key;
  size_t i, keylen;
  OnlyCallDuringRequest(L, "GetHeader");
  key = luaL_checklstring(L, 1, &keylen);
  if ((h = GetHttpHeader(key, keylen)) != -1) {
    if (msg.headers[h].a) {
      return LuaPushHeader(L, &msg, inbuf.p, h);
    }
  } else {
    for (i = 0; i < msg.xheaders.n; ++i) {
      if (SlicesEqualCase(key, keylen, inbuf.p + msg.xheaders.p[i].k.a,
                          msg.xheaders.p[i].k.b - msg.xheaders.p[i].k.a)) {
        LuaPushLatin1(L, inbuf.p + msg.xheaders.p[i].v.a,
                      msg.xheaders.p[i].v.b - msg.xheaders.p[i].v.a);
        return 1;
      }
    }
  }
  lua_pushnil(L);
  return 1;
}

static int LuaGetHeaders(lua_State *L) {
  OnlyCallDuringRequest(L, "GetHeaders");
  return LuaPushHeaders(L, &msg, inbuf.p);
}

static int LuaSetHeader(lua_State *L) {
  int h;
  ssize_t rc;
  char *p, *q;
  const char *key, *val, *eval;
  size_t i, keylen, vallen, evallen;
  OnlyCallDuringRequest(L, "SetHeader");
  key = luaL_checklstring(L, 1, &keylen);
  val = luaL_checklstring(L, 2, &vallen);
  if ((h = GetHttpHeader(key, keylen)) == -1) {
    if (!IsValidHttpToken(key, keylen)) {
      luaL_argerror(L, 1, "invalid");
      unreachable;
    }
  }
  if (!(eval = EncodeHttpHeaderValue(val, vallen, &evallen))) {
    luaL_argerror(L, 2, "invalid");
    unreachable;
  }
  p = GetLuaResponse();
  while (p - hdrbuf.p + keylen + 2 + evallen + 2 + 512 > hdrbuf.n) {
    hdrbuf.n += hdrbuf.n >> 1;
    q = xrealloc(hdrbuf.p, hdrbuf.n);
    luaheaderp = p = q + (p - hdrbuf.p);
    hdrbuf.p = q;
  }
  switch (h) {
    case kHttpConnection:
      if (!SlicesEqualCase(eval, evallen, "close", 5)) {
        luaL_argerror(L, 2, "unsupported");
        unreachable;
      }
      connectionclose = true;
      break;
    case kHttpContentType:
      p = AppendContentType(p, eval);
      break;
    case kHttpReferrerPolicy:
      referrerpolicy = FreeLater(strdup(eval));
      break;
    case kHttpServer:
      branded = true;
      p = AppendHeader(p, "Server", eval);
      break;
    case kHttpExpires:
    case kHttpCacheControl:
      gotcachecontrol = true;
      p = AppendHeader(p, key, eval);
      break;
    case kHttpXContentTypeOptions:
      gotxcontenttypeoptions = true;
      p = AppendHeader(p, key, eval);
      break;
    default:
      p = AppendHeader(p, key, eval);
      break;
  }
  luaheaderp = p;
  free(eval);
  return 0;
}

static int LuaGetCookie(lua_State *L) {
  char *cookie = 0, *cookietmpl, *cookieval;
  OnlyCallDuringRequest(L, "GetCookie");
  cookietmpl = gc(xasprintf(" %s=", luaL_checkstring(L, 1)));
  if (HasHeader(kHttpCookie)) {
    appends(&cookie, " ");  // prepend space to simplify cookie search
    appendd(&cookie, HeaderData(kHttpCookie), HeaderLength(kHttpCookie));
  }
  if (cookie && (cookieval = strstr(cookie, cookietmpl))) {
    cookieval += strlen(cookietmpl);
    lua_pushlstring(L, cookieval, strchrnul(cookieval, ';') - cookieval);
  } else {
    lua_pushnil(L);
  }
  if (cookie) free(cookie);
  return 1;
}

static int LuaSetCookie(lua_State *L) {
  const char *key, *val;
  size_t keylen, vallen;
  char *expires, *samesite = "";
  char *buf = 0;
  bool ishostpref, issecurepref;
  const char *hostpref = "__Host-";
  const char *securepref = "__Secure-";

  OnlyCallDuringRequest(L, "SetCookie");
  key = luaL_checklstring(L, 1, &keylen);
  val = luaL_checklstring(L, 2, &vallen);

  if (!IsValidHttpToken(key, keylen)) {
    luaL_argerror(L, 1, "invalid");
    unreachable;
  }
  if (!IsValidCookieValue(val, vallen)) {
    luaL_argerror(L, 2, "invalid");
    unreachable;
  }

  ishostpref = keylen > strlen(hostpref) &&
               SlicesEqual(key, strlen(hostpref), hostpref, strlen(hostpref));
  issecurepref =
      keylen > strlen(securepref) &&
      SlicesEqual(key, strlen(securepref), securepref, strlen(securepref));
  if ((ishostpref || issecurepref) && !usingssl) {
    luaL_argerror(
        L, 1,
        gc(xasprintf("%s and %s prefixes require SSL", hostpref, securepref)));
    unreachable;
  }

  appends(&buf, key);
  appends(&buf, "=");
  appends(&buf, val);

  if (lua_istable(L, 3)) {
    if (lua_getfield(L, 3, "expires") != LUA_TNIL ||
        lua_getfield(L, 3, "Expires") != LUA_TNIL) {
      if (lua_isnumber(L, -1)) {
        expires =
            FormatUnixHttpDateTime(FreeLater(xmalloc(30)), lua_tonumber(L, -1));
      } else {
        expires = lua_tostring(L, -1);
        if (!ParseHttpDateTime(expires, -1)) {
          luaL_argerror(L, 3, "invalid data format in Expires");
          unreachable;
        }
      }
      appends(&buf, "; Expires=");
      appends(&buf, expires);
    }

    if ((lua_getfield(L, 3, "maxage") == LUA_TNUMBER ||
         lua_getfield(L, 3, "MaxAge") == LUA_TNUMBER) &&
        lua_isinteger(L, -1)) {
      appends(&buf, "; Max-Age=");
      appends(&buf, lua_tostring(L, -1));
    }

    if (lua_getfield(L, 3, "samesite") == LUA_TSTRING ||
        lua_getfield(L, 3, "SameSite") == LUA_TSTRING) {
      samesite = lua_tostring(L, -1);  // also used in the Secure check
      appends(&buf, "; SameSite=");
      appends(&buf, samesite);
    }

    // Secure attribute is required for __Host and __Secure prefixes
    // as well as for the SameSite=None
    if (ishostpref || issecurepref || !strcmp(samesite, "None") ||
        ((lua_getfield(L, 3, "secure") == LUA_TBOOLEAN ||
          lua_getfield(L, 3, "Secure") == LUA_TBOOLEAN) &&
         lua_toboolean(L, -1))) {
      appends(&buf, "; Secure");
    }

    if (!ishostpref && (lua_getfield(L, 3, "domain") == LUA_TSTRING ||
                        lua_getfield(L, 3, "Domain") == LUA_TSTRING)) {
      appends(&buf, "; Domain=");
      appends(&buf, lua_tostring(L, -1));
    }

    if (ishostpref || lua_getfield(L, 3, "path") == LUA_TSTRING ||
        lua_getfield(L, 3, "Path") == LUA_TSTRING) {
      appends(&buf, "; Path=");
      appends(&buf, ishostpref ? "/" : lua_tostring(L, -1));
    }

    if ((lua_getfield(L, 3, "httponly") == LUA_TBOOLEAN ||
         lua_getfield(L, 3, "HttpOnly") == LUA_TBOOLEAN) &&
        lua_toboolean(L, -1)) {
      appends(&buf, "; HttpOnly");
    }
  }
  DEBUGF("(srvr) Set-Cookie: %s", buf);

  // empty the stack and push header name/value
  lua_settop(L, 0);
  lua_pushliteral(L, "Set-Cookie");
  lua_pushstring(L, buf);
  free(buf);
  return LuaSetHeader(L);
}

static int LuaHasParam(lua_State *L) {
  size_t i, n;
  const char *s;
  OnlyCallDuringRequest(L, "HasParam");
  s = luaL_checklstring(L, 1, &n);
  for (i = 0; i < url.params.n; ++i) {
    if (SlicesEqual(s, n, url.params.p[i].key.p, url.params.p[i].key.n)) {
      lua_pushboolean(L, true);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

static int LuaGetParam(lua_State *L) {
  size_t i, n;
  const char *s;
  OnlyCallDuringRequest(L, "GetParam");
  s = luaL_checklstring(L, 1, &n);
  for (i = 0; i < url.params.n; ++i) {
    if (SlicesEqual(s, n, url.params.p[i].key.p, url.params.p[i].key.n)) {
      if (url.params.p[i].val.p) {
        lua_pushlstring(L, url.params.p[i].val.p, url.params.p[i].val.n);
        return 1;
      } else {
        break;
      }
    }
  }
  lua_pushnil(L);
  return 1;
}

static int LuaGetParams(lua_State *L) {
  OnlyCallDuringRequest(L, "GetParams");
  LuaPushUrlParams(L, &url.params);
  return 1;
}

static int LuaWrite(lua_State *L) {
  size_t size;
  const char *data;
  OnlyCallDuringRequest(L, "Write");
  if (!lua_isnil(L, 1)) {
    data = luaL_checklstring(L, 1, &size);
    appendd(&outbuf, data, size);
  }
  return 0;
}

static dontinline int LuaProgramInt(lua_State *L, void P(long)) {
  P(luaL_checkinteger(L, 1));
  return 0;
}

static int LuaProgramPort(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramPort");
  return LuaProgramInt(L, ProgramPort);
}

static int LuaProgramCache(lua_State *L) {
  OnlyCallFromMainProcess(L, "ProgramCache");
  return LuaProgramInt(L, ProgramCache);
}

static int LuaProgramTimeout(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramTimeout");
  return LuaProgramInt(L, ProgramTimeout);
}

static int LuaProgramUid(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramUid");
  return LuaProgramInt(L, ProgramUid);
}

static int LuaProgramGid(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramGid");
  return LuaProgramInt(L, ProgramGid);
}

static int LuaProgramMaxPayloadSize(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramMaxPayloadSize");
  return LuaProgramInt(L, ProgramMaxPayloadSize);
}

static int LuaGetClientFd(lua_State *L) {
  OnlyCallDuringConnection(L, "GetClientFd");
  lua_pushinteger(L, client);
  return 1;
}

static int LuaIsClientUsingSsl(lua_State *L) {
  OnlyCallDuringConnection(L, "IsClientUsingSsl");
  lua_pushboolean(L, usingssl);
  return 1;
}

static int LuaProgramSslTicketLifetime(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramSslTicketLifetime");
  return LuaProgramInt(L, ProgramSslTicketLifetime);
}

static int LuaProgramUniprocess(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramUniprocess");
  if (!lua_isboolean(L, 1) && !lua_isnoneornil(L, 1)) {
    return luaL_argerror(L, 1, "invalid uniprocess mode; boolean expected");
  }
  lua_pushboolean(L, uniprocess);
  if (lua_isboolean(L, 1)) uniprocess = lua_toboolean(L, 1);
  return 1;
}

static int LuaProgramAddr(lua_State *L) {
  uint32_t ip;
  OnlyCallFromInitLua(L, "ProgramAddr");
  if (lua_isinteger(L, 1)) {
    ip = luaL_checkinteger(L, 1);
    ips.p = realloc(ips.p, ++ips.n * sizeof(*ips.p));
    ips.p[ips.n - 1] = ip;
  } else {
    ProgramAddr(luaL_checkstring(L, 1));
  }
  return 0;
}

static dontinline int LuaProgramString(lua_State *L, void P(const char *)) {
  P(luaL_checkstring(L, 1));
  return 0;
}

static int LuaProgramBrand(lua_State *L) {
  OnlyCallFromMainProcess(L, "ProgramBrand");
  return LuaProgramString(L, ProgramBrand);
}

static int LuaProgramDirectory(lua_State *L) {
  return LuaProgramString(L, ProgramDirectory);
}

static int LuaProgramLogPath(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramLogPath");
  return LuaProgramString(L, ProgramLogPath);
}

static int LuaProgramPidPath(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramPidPath");
  return LuaProgramString(L, ProgramPidPath);
}

static int LuaProgramSslPresharedKey(lua_State *L) {
  struct Psk psk;
  size_t n1, n2, i;
  const char *p1, *p2;
  OnlyCallFromMainProcess(L, "ProgramSslPresharedKey");
  p1 = luaL_checklstring(L, 1, &n1);
  p2 = luaL_checklstring(L, 2, &n2);
  if (!n1 || n1 > MBEDTLS_PSK_MAX_LEN || !n2) {
    luaL_argerror(L, 1, "bad preshared key length");
    unreachable;
  }
  psk.key = memcpy(malloc(n1), p1, n1);
  psk.key_len = n1;
  psk.identity = memcpy(malloc(n2), p2, n2);
  psk.identity_len = n2;
  for (i = 0; i < psks.n; ++i) {
    if (SlicesEqual(psk.identity, psk.identity_len, psks.p[i].identity,
                    psks.p[i].identity_len)) {
      mbedtls_platform_zeroize(psks.p[i].key, psks.p[i].key_len);
      free(psks.p[i].key);
      free(psks.p[i].identity);
      psks.p[i] = psk;
      return 0;
    }
  }
  psks.p = realloc(psks.p, ++psks.n * sizeof(*psks.p));
  psks.p[psks.n - 1] = psk;
  return 0;
}

static int LuaProgramSslCiphersuite(lua_State *L) {
  mbedtls_ssl_ciphersuite_t *suite;
  OnlyCallFromInitLua(L, "ProgramSslCiphersuite");
  if (!(suite = GetCipherSuite(luaL_checkstring(L, 1)))) {
    luaL_argerror(L, 1, "unsupported or unknown ciphersuite");
    unreachable;
  }
  suites.p = realloc(suites.p, (++suites.n + 1) * sizeof(*suites.p));
  suites.p[suites.n - 1] = suite->id;
  suites.p[suites.n - 0] = 0;
  return 0;
}

static int LuaProgramPrivateKey(lua_State *L) {
  size_t n;
  const char *p;
  OnlyCallFromInitLua(L, "ProgramPrivateKey");
  p = luaL_checklstring(L, 1, &n);
  ProgramPrivateKey(p, n);
  return 0;
}

static int LuaProgramCertificate(lua_State *L) {
  size_t n;
  const char *p;
  OnlyCallFromInitLua(L, "ProgramCertificate");
  p = luaL_checklstring(L, 1, &n);
  ProgramCertificate(p, n);
  return 0;
}

static int LuaProgramHeader(lua_State *L) {
  ProgramHeader(
      gc(xasprintf("%s: %s", luaL_checkstring(L, 1), luaL_checkstring(L, 2))));
  return 0;
}

static int LuaProgramRedirect(lua_State *L) {
  int code;
  const char *from, *to;
  code = luaL_checkinteger(L, 1);
  from = luaL_checkstring(L, 2);
  to = luaL_checkstring(L, 3);
  ProgramRedirect(code, strdup(from), strlen(from), strdup(to), strlen(to));
  return 0;
}

static dontinline int LuaProgramBool(lua_State *L, bool *b) {
  *b = lua_toboolean(L, 1);
  return 0;
}

static int LuaProgramSslClientVerify(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramSslClientVerify");
  return LuaProgramBool(L, &sslclientverify);
}

static int LuaProgramSslRequired(lua_State *L) {
  OnlyCallFromInitLua(L, "ProgramSslRequired");
  return LuaProgramBool(L, &requiressl);
}

static int LuaProgramSslFetchVerify(lua_State *L) {
  OnlyCallFromMainProcess(L, "ProgramSslFetchVerify");
  return LuaProgramBool(L, &sslfetchverify);
}

static int LuaProgramSslInit(lua_State *L) {
  OnlyCallFromInitLua(L, "SslInit");
  TlsInit();
  return 0;
}

static int LuaProgramLogMessages(lua_State *L) {
  return LuaProgramBool(L, &logmessages);
}

static int LuaProgramLogBodies(lua_State *L) {
  return LuaProgramBool(L, &logbodies);
}

static int LuaEvadeDragnetSurveillance(lua_State *L) {
  return LuaProgramBool(L, &evadedragnetsurveillance);
}

static int LuaHidePath(lua_State *L) {
  size_t pathlen;
  const char *path;
  path = luaL_checklstring(L, 1, &pathlen);
  AddString(&hidepaths, memcpy(malloc(pathlen), path, pathlen), pathlen);
  return 0;
}

static int LuaIsHiddenPath(lua_State *L) {
  size_t n;
  const char *s;
  s = luaL_checklstring(L, 1, &n);
  lua_pushboolean(L, IsHiddenPath(s, n));
  return 1;
}

static int LuaGetZipPaths(lua_State *L) {
  char *path;
  uint8_t *zcf;
  size_t i, n, pathlen, prefixlen;
  char *prefix = luaL_optlstring(L, 1, "", &prefixlen);
  lua_newtable(L);
  i = 0;
  n = GetZipCdirRecords(zcdir);
  for (zcf = zbase + GetZipCdirOffset(zcdir); n--;
       zcf += ZIP_CFILE_HDRSIZE(zcf)) {
    CHECK_EQ(kZipCfileHdrMagic, ZIP_CFILE_MAGIC(zcf));
    path = GetAssetPath(zcf, &pathlen);
    if (prefixlen == 0 || startswith(path, prefix)) {
      lua_pushlstring(L, path, pathlen);
      lua_seti(L, -2, ++i);
    }
    free(path);
  }
  return 1;
}

static int LuaGetAssetMode(lua_State *L) {
  size_t pathlen;
  struct Asset *a;
  const char *path;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen))) {
    lua_pushinteger(L, GetMode(a));
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaGetAssetLastModifiedTime(lua_State *L) {
  size_t pathlen;
  struct Asset *a;
  const char *path;
  struct timespec lm;
  int64_t zuluseconds;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen))) {
    if (a->file) {
      zuluseconds = a->file->st.st_mtim.tv_sec;
    } else {
      GetZipCfileTimestamps(zbase + a->cf, &lm, 0, 0, gmtoff);
      zuluseconds = lm.tv_sec;
    }
    lua_pushinteger(L, zuluseconds);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaGetAssetSize(lua_State *L) {
  size_t pathlen;
  struct Asset *a;
  const char *path;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen))) {
    if (a->file) {
      lua_pushinteger(L, a->file->st.st_size);
    } else {
      lua_pushinteger(L, GetZipLfileUncompressedSize(zbase + a->lf));
    }
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaIsAssetCompressed(lua_State *L) {
  size_t pathlen;
  struct Asset *a;
  const char *path;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen))) {
    lua_pushboolean(L, IsCompressed(a));
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaIsDaemon(lua_State *L) {
  lua_pushboolean(L, daemonize);
  return 1;
}

static int LuaGetAssetComment(lua_State *L) {
  struct Asset *a;
  const char *path;
  size_t pathlen, m;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAssetZip(path, pathlen)) &&
      (m = strnlen(ZIP_CFILE_COMMENT(zbase + a->cf),
                   ZIP_CFILE_COMMENTSIZE(zbase + a->cf)))) {
    lua_pushlstring(L, ZIP_CFILE_COMMENT(zbase + a->cf), m);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaLaunchBrowser(lua_State *L) {
  OnlyCallFromInitLua(L, "LaunchBrowser");
  launchbrowser = strdup(luaL_optstring(L, 1, "/"));
  return 0;
}

static bool LuaRunAsset(const char *path, bool mandatory) {
  int status;
  struct Asset *a;
  const char *code;
  size_t pathlen, codelen;
  pathlen = strlen(path);
  if ((a = GetAsset(path, pathlen))) {
    if ((code = FreeLater(LoadAsset(a, &codelen)))) {
      lua_State *L = GL;
      effectivepath.p = path;
      effectivepath.n = pathlen;
      DEBUGF("(lua) LuaRunAsset(%`'s)", path);
      status = luaL_loadbuffer(
          L, code, codelen,
          FreeLater(xasprintf("@%s%s", a->file ? "" : "/zip", path)));
      if (status != LUA_OK || LuaCallWithTrace(L, 0, 0, NULL) != LUA_OK) {
        LogLuaError("lua code", lua_tostring(L, -1));
        lua_pop(L, 1);  // pop error
        if (mandatory) exit(1);
      }
    }
  }
  return !!a;
}

// <SORTED>
// list of functions that can't be run from the repl
static const char *const kDontAutoComplete[] = {
    "GetBody",                   //
    "GetClientAddr",             //
    "GetClientFd",               //
    "GetComment",                // deprecated
    "GetCookie",                 //
    "GetEffectivePath",          //
    "GetFragment",               //
    "GetHeader",                 //
    "GetHeaders",                //
    "GetHost",                   //
    "GetHttpVersion",            //
    "GetLastModifiedTime",       // deprecated
    "GetMethod",                 //
    "GetParam",                  //
    "GetParams",                 //
    "GetPass",                   //
    "GetPath",                   //
    "GetPayload",                // deprecated
    "GetPort",                   //
    "GetRemoteAddr",             //
    "GetScheme",                 //
    "GetServerAddr",             //
    "GetSslIdentity",            //
    "GetStatus",                 //
    "GetUrl",                    //
    "GetUser",                   //
    "GetVersion",                // deprecated
    "HasParam",                  //
    "IsClientUsingSsl",          //
    "IsCompressed",              // deprecated
    "LaunchBrowser",             //
    "LuaProgramSslRequired",     // TODO
    "ProgramAddr",               // TODO
    "ProgramBrand",              //
    "ProgramCertificate",        // TODO
    "ProgramGid",                //
    "ProgramLogPath",            // TODO
    "ProgramMaxPayloadSize",     // TODO
    "ProgramPidPath",            // TODO
    "ProgramPort",               // TODO
    "ProgramPrivateKey",         // TODO
    "ProgramSslCiphersuite",     // TODO
    "ProgramSslClientVerify",    // TODO
    "ProgramSslTicketLifetime",  //
    "ProgramTimeout",            // TODO
    "ProgramUid",                //
    "ProgramUniprocess",         //
    "Respond",                   //
    "Route",                     //
    "RouteHost",                 //
    "RoutePath",                 //
    "ServeAsset",                //
    "ServeIndex",                //
    "ServeListing",              //
    "ServeRedirect",             //
    "ServeStatusz",              //
    "SetCookie",                 //
    "SetHeader",                 //
    "SslInit",                   // TODO
    "Write",                     //
};
// </SORTED>

static const luaL_Reg kLuaFuncs[] = {
    {"Benchmark", LuaBenchmark},                                //
    {"Bsf", LuaBsf},                                            //
    {"Bsr", LuaBsr},                                            //
    {"CategorizeIp", LuaCategorizeIp},                          //
    {"Compress", LuaCompress},                                  //
    {"Crc32", LuaCrc32},                                        //
    {"Crc32c", LuaCrc32c},                                      //
    {"Decimate", LuaDecimate},                                  //
    {"DecodeBase64", LuaDecodeBase64},                          //
    {"DecodeLatin1", LuaDecodeLatin1},                          //
    {"EncodeBase64", LuaEncodeBase64},                          //
    {"EncodeJson", LuaEncodeJson},                              //
    {"EncodeLatin1", LuaEncodeLatin1},                          //
    {"EncodeLua", LuaEncodeLua},                                //
    {"EncodeUrl", LuaEncodeUrl},                                //
    {"EscapeFragment", LuaEscapeFragment},                      //
    {"EscapeHost", LuaEscapeHost},                              //
    {"EscapeHtml", LuaEscapeHtml},                              //
    {"EscapeIp", LuaEscapeIp},                                  //
    {"EscapeLiteral", LuaEscapeLiteral},                        //
    {"EscapeParam", LuaEscapeParam},                            //
    {"EscapePass", LuaEscapePass},                              //
    {"EscapePath", LuaEscapePath},                              //
    {"EscapeSegment", LuaEscapeSegment},                        //
    {"EscapeUser", LuaEscapeUser},                              //
    {"Fetch", LuaFetch},                                        //
    {"FormatHttpDateTime", LuaFormatHttpDateTime},              //
    {"FormatIp", LuaFormatIp},                                  //
    {"GetAssetComment", LuaGetAssetComment},                    //
    {"GetAssetLastModifiedTime", LuaGetAssetLastModifiedTime},  //
    {"GetAssetMode", LuaGetAssetMode},                          //
    {"GetAssetSize", LuaGetAssetSize},                          //
    {"GetBody", LuaGetBody},                                    //
    {"GetClientAddr", LuaGetClientAddr},                        //
    {"GetClientFd", LuaGetClientFd},                            //
    {"GetCookie", LuaGetCookie},                                //
    {"GetCpuCore", LuaGetCpuCore},                              //
    {"GetCpuCount", LuaGetCpuCount},                            //
    {"GetCpuNode", LuaGetCpuNode},                              //
    {"GetCryptoHash", LuaGetCryptoHash},                        //
    {"GetDate", LuaGetDate},                                    //
    {"GetEffectivePath", LuaGetEffectivePath},                  //
    {"GetFragment", LuaGetFragment},                            //
    {"GetHeader", LuaGetHeader},                                //
    {"GetHeaders", LuaGetHeaders},                              //
    {"GetHost", LuaGetHost},                                    //
    {"GetHostOs", LuaGetHostOs},                                //
    {"GetHttpReason", LuaGetHttpReason},                        //
    {"GetHttpVersion", LuaGetHttpVersion},                      //
    {"GetLogLevel", LuaGetLogLevel},                            //
    {"GetMethod", LuaGetMethod},                                //
    {"GetMonospaceWidth", LuaGetMonospaceWidth},                //
    {"GetParam", LuaGetParam},                                  //
    {"GetParams", LuaGetParams},                                //
    {"GetPass", LuaGetPass},                                    //
    {"GetPath", LuaGetPath},                                    //
    {"GetPort", LuaGetPort},                                    //
    {"GetRandomBytes", LuaGetRandomBytes},                      //
    {"GetRedbeanVersion", LuaGetRedbeanVersion},                //
    {"GetRemoteAddr", LuaGetRemoteAddr},                        //
    {"GetScheme", LuaGetScheme},                                //
    {"GetServerAddr", LuaGetServerAddr},                        //
    {"GetStatus", LuaGetStatus},                                //
    {"GetTime", LuaGetTime},                                    //
    {"GetUrl", LuaGetUrl},                                      //
    {"GetUser", LuaGetUser},                                    //
    {"GetZipPaths", LuaGetZipPaths},                            //
    {"HasControlCodes", LuaHasControlCodes},                    //
    {"HasParam", LuaHasParam},                                  //
    {"HidePath", LuaHidePath},                                  //
    {"IndentLines", LuaIndentLines},                            //
    {"IsAcceptableHost", LuaIsAcceptableHost},                  //
    {"IsAcceptablePath", LuaIsAcceptablePath},                  //
    {"IsAcceptablePort", LuaIsAcceptablePort},                  //
    {"IsClientUsingSsl", LuaIsClientUsingSsl},                  //
    {"IsAssetCompressed", LuaIsAssetCompressed},                //
    {"IsDaemon", LuaIsDaemon},                                  //
    {"IsHeaderRepeatable", LuaIsHeaderRepeatable},              //
    {"IsHiddenPath", LuaIsHiddenPath},                          //
    {"IsLoopbackIp", LuaIsLoopbackIp},                          //
    {"IsPrivateIp", LuaIsPrivateIp},                            //
    {"IsPublicIp", LuaIsPublicIp},                              //
    {"IsReasonablePath", LuaIsReasonablePath},                  //
    {"IsValidHttpToken", LuaIsValidHttpToken},                  //
    {"LaunchBrowser", LuaLaunchBrowser},                        //
    {"Lemur64", LuaLemur64},                                    //
    {"LoadAsset", LuaLoadAsset},                                //
    {"Log", LuaLog},                                            //
    {"Md5", LuaMd5},                                            //
    {"MeasureEntropy", LuaMeasureEntropy},                      //
    {"ParseHost", LuaParseHost},                                //
    {"ParseHttpDateTime", LuaParseHttpDateTime},                //
    {"ParseIp", LuaParseIp},                                    //
    {"ParseParams", LuaParseParams},                            //
    {"ParseUrl", LuaParseUrl},                                  //
    {"Popcnt", LuaPopcnt},                                      //
    {"ProgramAddr", LuaProgramAddr},                            //
    {"ProgramBrand", LuaProgramBrand},                          //
    {"ProgramCache", LuaProgramCache},                          //
    {"ProgramDirectory", LuaProgramDirectory},                  //
    {"ProgramGid", LuaProgramGid},                              //
    {"ProgramHeader", LuaProgramHeader},                        //
    {"ProgramLogBodies", LuaProgramLogBodies},                  //
    {"ProgramLogMessages", LuaProgramLogMessages},              //
    {"ProgramLogPath", LuaProgramLogPath},                      //
    {"ProgramMaxPayloadSize", LuaProgramMaxPayloadSize},        //
    {"ProgramPidPath", LuaProgramPidPath},                      //
    {"ProgramPort", LuaProgramPort},                            //
    {"ProgramRedirect", LuaProgramRedirect},                    //
    {"ProgramTimeout", LuaProgramTimeout},                      //
    {"ProgramUid", LuaProgramUid},                              //
    {"ProgramUniprocess", LuaProgramUniprocess},                //
    {"Rand64", LuaRand64},                                      //
    {"Rdrand", LuaRdrand},                                      //
    {"Rdseed", LuaRdseed},                                      //
    {"Rdtsc", LuaRdtsc},                                        //
    {"ResolveIp", LuaResolveIp},                                //
    {"Route", LuaRoute},                                        //
    {"RouteHost", LuaRouteHost},                                //
    {"RoutePath", LuaRoutePath},                                //
    {"ServeAsset", LuaServeAsset},                              //
    {"ServeError", LuaServeError},                              //
    {"ServeIndex", LuaServeIndex},                              //
    {"ServeListing", LuaServeListing},                          //
    {"ServeRedirect", LuaServeRedirect},                        //
    {"ServeStatusz", LuaServeStatusz},                          //
    {"SetCookie", LuaSetCookie},                                //
    {"SetHeader", LuaSetHeader},                                //
    {"SetLogLevel", LuaSetLogLevel},                            //
    {"SetStatus", LuaSetStatus},                                //
    {"Sha1", LuaSha1},                                          //
    {"Sha224", LuaSha224},                                      //
    {"Sha256", LuaSha256},                                      //
    {"Sha384", LuaSha384},                                      //
    {"Sha512", LuaSha512},                                      //
    {"Sleep", LuaSleep},                                        //
    {"Slurp", LuaSlurp},                                        //
    {"StoreAsset", LuaStoreAsset},                              //
    {"Uncompress", LuaUncompress},                              //
    {"Underlong", LuaUnderlong},                                //
    {"VisualizeControlCodes", LuaVisualizeControlCodes},        //
    {"Write", LuaWrite},                                        //
    {"bin", LuaBin},                                            //
    {"hex", LuaHex},                                            //
    {"oct", LuaOct},                                            //
#ifndef UNSECURE
    {"EvadeDragnetSurveillance", LuaEvadeDragnetSurveillance},  //
    {"GetSslIdentity", LuaGetSslIdentity},                      //
    {"ProgramCertificate", LuaProgramCertificate},              //
    {"ProgramPrivateKey", LuaProgramPrivateKey},                //
    {"ProgramSslCiphersuite", LuaProgramSslCiphersuite},        //
    {"ProgramSslClientVerify", LuaProgramSslClientVerify},      //
    {"ProgramSslFetchVerify", LuaProgramSslFetchVerify},        //
    {"ProgramSslInit", LuaProgramSslInit},                      //
    {"ProgramSslPresharedKey", LuaProgramSslPresharedKey},      //
    {"ProgramSslRequired", LuaProgramSslRequired},              //
    {"ProgramSslTicketLifetime", LuaProgramSslTicketLifetime},  //
#endif
    // deprecated
    {"GetPayload", LuaGetBody},                            //
    {"GetComment", LuaGetAssetComment},                    //
    {"GetVersion", LuaGetHttpVersion},                     //
    {"IsCompressed", LuaIsAssetCompressed},                //
    {"GetLastModifiedTime", LuaGetAssetLastModifiedTime},  //
};

static const luaL_Reg kLuaLibs[] = {
    {"argon2", luaopen_argon2},      //
    {"lsqlite3", luaopen_lsqlite3},  //
    {"maxmind", LuaMaxmind},         //
    {"re", LuaRe},                   //
    {"unix", LuaUnix},               //
};

static void LuaSetArgv(lua_State *L) {
  int i, j = -1;
  lua_newtable(L);
  lua_pushstring(L, __argv[0]);
  lua_seti(L, -2, j++);
  if (!interpretermode) {
    lua_pushstring(L, "/zip/.init.lua");
    lua_seti(L, -2, j++);
  }
  for (i = optind; i < __argc; ++i) {
    lua_pushstring(L, __argv[i]);
    lua_seti(L, -2, j++);
  }
  lua_pushvalue(L, -1);
  lua_setglobal(L, "argv");  // deprecated
  lua_setglobal(L, "arg");
}

static void LuaSetConstant(lua_State *L, const char *s, long x) {
  lua_pushinteger(L, x);
  lua_setglobal(L, s);
}

static char *GetDefaultLuaPath(void) {
  char *s;
  size_t i;
  for (s = 0, i = 0; i < stagedirs.n; ++i) {
    appendf(&s, "%s/.lua/?.lua;%s/.lua/?/init.lua;", stagedirs.p[i].s,
            stagedirs.p[i].s);
  }
  appends(&s, "/zip/.lua/?.lua;/zip/.lua/?/init.lua");
  return s;
}

static void LuaStart(void) {
#ifndef STATIC
  size_t i;
  lua_State *L = GL = luaL_newstate();
  g_lua_path_default = GetDefaultLuaPath();
  luaL_openlibs(L);
  for (i = 0; i < ARRAYLEN(kLuaLibs); ++i) {
    luaL_requiref(L, kLuaLibs[i].name, kLuaLibs[i].func, 1);
    lua_pop(L, 1);
  }
  for (i = 0; i < ARRAYLEN(kLuaFuncs); ++i) {
    lua_pushcfunction(L, kLuaFuncs[i].func);
    lua_setglobal(L, kLuaFuncs[i].name);
  }
  LuaSetConstant(L, "kLogDebug", kLogDebug);
  LuaSetConstant(L, "kLogVerbose", kLogVerbose);
  LuaSetConstant(L, "kLogInfo", kLogInfo);
  LuaSetConstant(L, "kLogWarn", kLogWarn);
  LuaSetConstant(L, "kLogError", kLogError);
  LuaSetConstant(L, "kLogFatal", kLogFatal);
#endif
}

static bool ShouldAutocomplete(const char *s) {
  int c, m, l, r;
  l = 0;
  r = ARRAYLEN(kDontAutoComplete) - 1;
  while (l <= r) {
    m = (l + r) >> 1;
    c = strcmp(kDontAutoComplete[m], s);
    if (c < 0) {
      l = m + 1;
    } else if (c > 0) {
      r = m - 1;
    } else {
      return false;
    }
  }
  return true;
}

static void HandleCompletions(const char *p, linenoiseCompletions *c) {
  size_t i, j;
  for (j = i = 0; i < c->len; ++i) {
    if (ShouldAutocomplete(c->cvec[i])) {
      c->cvec[j++] = c->cvec[i];
    } else {
      free(c->cvec[i]);
    }
  }
  c->len = j;
}

static void LuaPrint(lua_State *L) {
  int i, n;
  char *b = 0;
  const char *s;
  n = lua_gettop(L);
  if (n > 0) {
    for (i = 1; i <= n; i++) {
      if (i > 1) appendw(&b, '\t');
      LuaEncodeLuaData(L, &b, "g", i);
    }
    appendw(&b, '\n');
    WRITE(1, b, appendz(b).i);
    free(b);
  }
}

static void EnableRawMode(void) {
  if (lua_repl_isterminal) {
    --__strace;
    linenoiseEnableRawMode(0);
    ++__strace;
  }
}

static void DisableRawMode(void) {
  --__strace;
  linenoiseDisableRawMode();
  ++__strace;
}

static void LuaInterpreter(lua_State *L) {
  int i, n, sig, status;
  const char *script;
  if (optind < __argc) {
    script = __argv[optind];
    if (!strcmp(script, "-")) script = 0;
    if ((status = luaL_loadfile(L, script)) == LUA_OK) {
      lua_getglobal(L, "arg");
      n = luaL_len(L, -1);
      luaL_checkstack(L, n + 3, "too many script args");
      for (i = 1; i <= n; i++) lua_rawgeti(L, -i, i);
      lua_remove(L, -i);  // remove arg table from stack
      TRACE_BEGIN;
      status = lua_runchunk(L, n, LUA_MULTRET);
      TRACE_END;
    }
    lua_report(L, status);
  } else {
    lua_repl_blocking = true;
    lua_repl_completions_callback = HandleCompletions;
    lua_initrepl(GL, "redbean");
    EnableRawMode();
    for (;;) {
      status = lua_loadline(L);
      if (status == -1) break;  // eof
      if (status == -2) {
        if (errno == EINTR) {
          if ((sig = linenoiseGetInterrupt())) {
            kill(0, sig);
          }
        }
        fprintf(stderr, "i/o error: %m\n");
        exit(1);
      }
      if (status == LUA_OK) {
        TRACE_BEGIN;
        status = lua_runchunk(GL, 0, LUA_MULTRET);
        TRACE_END;
      }
      if (status == LUA_OK) {
        LuaPrint(GL);
      } else {
        lua_report(GL, status);
      }
    }
    DisableRawMode();
    lua_freerepl();
    lua_settop(GL, 0);  // clear stack
    if ((sig = linenoiseGetInterrupt())) {
      raise(sig);
    }
  }
}

static void LuaInit(void) {
#ifndef STATIC
  lua_State *L = GL;
  LuaSetArgv(L);
  if (interpretermode) {
    LuaInterpreter(L);
    exit(0);
  }
  if (LuaRunAsset("/.init.lua", true)) {
    hasonhttprequest = IsHookDefined("OnHttpRequest");
    hasonclientconnection = IsHookDefined("OnClientConnection");
    hasonprocesscreate = IsHookDefined("OnProcessCreate");
    hasonprocessdestroy = IsHookDefined("OnProcessDestroy");
    hasonworkerstart = IsHookDefined("OnWorkerStart");
    hasonworkerstop = IsHookDefined("OnWorkerStop");
  } else {
    DEBUGF("(srvr) no /.init.lua defined");
  }
#endif
}

static void LuaReload(void) {
#ifndef STATIC
  if (!LuaRunAsset("/.reload.lua", false)) {
    DEBUGF("(srvr) no /.reload.lua defined");
  }
#endif
}

static void LuaDestroy(void) {
#ifndef STATIC
  lua_State *L = GL;
  lua_close(L);
  free(g_lua_path_default);
#endif
}

static const char *DescribeClose(void) {
  if (killed) return "killed";
  if (meltdown) return "meltdown";
  if (terminated) return "terminated";
  if (connectionclose) return "connectionclose";
  return "destroyed";
}

static void LogClose(const char *reason) {
  if (amtread || meltdown || killed) {
    LockInc(&shared->c.fumbles);
    INFOF("(stat) %s %s with %,ld unprocessed and %,d handled (%,d workers)",
          DescribeClient(), reason, amtread, messageshandled, shared->workers);
  } else {
    DEBUGF("(stat) %s %s with %,d messages handled", DescribeClient(), reason,
           messageshandled);
  }
}

static ssize_t SendString(const char *s) {
  size_t n;
  ssize_t rc;
  struct iovec iov;
  n = strlen(s);
  iov.iov_base = s;
  iov.iov_len = n;
  if (logmessages) {
    LogMessage("sending", s, n);
  }
  for (;;) {
    if ((rc = writer(client, &iov, 1)) != -1 || errno != EINTR) {
      return rc;
    }
    errno = 0;
  }
}

static ssize_t SendContinue(void) {
  return SendString("\
HTTP/1.1 100 Continue\r\n\
\r\n");
}

static ssize_t SendTimeout(void) {
  return SendString("\
HTTP/1.1 408 Request Timeout\r\n\
Connection: close\r\n\
Content-Length: 0\r\n\
\r\n");
}

static ssize_t SendServiceUnavailable(void) {
  return SendString("\
HTTP/1.1 503 Service Unavailable\r\n\
Connection: close\r\n\
Content-Length: 0\r\n\
\r\n");
}

static void EnterMeltdownMode(void) {
  if (shared->lastmeltdown && nowl() - shared->lastmeltdown < 1) return;
  WARNF("(srvr) server is melting down (%,d workers)", shared->workers);
  LOGIFNEG1(kill(0, SIGUSR2));
  shared->lastmeltdown = nowl();
  ++shared->c.meltdowns;
}

static char *HandlePayloadDisconnect(void) {
  LockInc(&shared->c.payloaddisconnects);
  LogClose("payload disconnect");
  return ServeFailure(400, "Bad Request"); /* XXX */
}

static char *HandlePayloadDrop(void) {
  LockInc(&shared->c.dropped);
  LogClose(DescribeClose());
  return ServeFailure(503, "Service Unavailable");
}

static char *HandleBadContentLength(void) {
  LockInc(&shared->c.badlengths);
  return ServeFailure(400, "Bad Content Length");
}

static char *HandleLengthRequired(void) {
  LockInc(&shared->c.missinglengths);
  return ServeFailure(411, "Length Required");
}

static char *HandleVersionNotSupported(void) {
  LockInc(&shared->c.http12);
  return ServeFailure(505, "HTTP Version Not Supported");
}

static char *HandleConnectRefused(void) {
  LockInc(&shared->c.connectsrefused);
  return ServeFailure(501, "Not Implemented");
}

static char *HandleExpectFailed(void) {
  LockInc(&shared->c.expectsrefused);
  return ServeFailure(417, "Expectation Failed");
}

static char *HandleHugePayload(void) {
  LockInc(&shared->c.hugepayloads);
  return ServeFailure(413, "Payload Too Large");
}

static char *HandlePayloadSlowloris(void) {
  LockInc(&shared->c.slowloris);
  LogClose("payload slowloris");
  return ServeFailure(408, "Request Timeout");
}

static char *HandleTransferRefused(void) {
  LockInc(&shared->c.transfersrefused);
  return ServeFailure(501, "Not Implemented");
}

static char *HandleMapFailed(struct Asset *a, int fd) {
  LockInc(&shared->c.mapfails);
  WARNF("(srvr) mmap(%`'s) error: %m", a->file->path);
  close(fd);
  return ServeError(500, "Internal Server Error");
}

static void LogAcceptError(const char *s) {
  LockInc(&shared->c.accepterrors);
  WARNF("(srvr) %s accept error: %s", DescribeServer(), s);
}

static char *HandleOpenFail(struct Asset *a) {
  LockInc(&shared->c.openfails);
  WARNF("(srvr) open(%`'s) error: %m", a->file->path);
  if (errno == ENFILE) {
    LockInc(&shared->c.enfiles);
    return ServeError(503, "Service Unavailable");
  } else if (errno == EMFILE) {
    LockInc(&shared->c.emfiles);
    return ServeError(503, "Service Unavailable");
  } else {
    return ServeError(500, "Internal Server Error");
  }
}

static char *HandlePayloadReadError(void) {
  if (errno == ECONNRESET) {
    LockInc(&shared->c.readresets);
    LogClose("payload reset");
    return ServeFailure(400, "Bad Request"); /* XXX */
  } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
    LockInc(&shared->c.readtimeouts);
    LogClose("payload read timeout");
    return ServeFailure(408, "Request Timeout");
  } else {
    LockInc(&shared->c.readerrors);
    INFOF("(clnt) %s payload read error: %m", DescribeClient());
    return ServeFailure(500, "Internal Server Error");
  }
}

static void HandleForkFailure(void) {
  LockInc(&shared->c.forkerrors);
  LockInc(&shared->c.dropped);
  EnterMeltdownMode();
  SendServiceUnavailable();
  close(client);
  WARNF("(srvr) too many processes: %m");
}

static void HandleFrag(size_t got) {
  LockInc(&shared->c.frags);
  if (++frags == 32) {
    SendTimeout();
    LogClose("slowloris");
    LockInc(&shared->c.slowloris);
    return;
  } else {
    DEBUGF("(stat) %s fragged msg added %,ld bytes to %,ld byte buffer",
           DescribeClient(), amtread, got);
  }
}

static void HandleReload(void) {
  LockInc(&shared->c.reloads);
  Reindex();
  LuaReload();
}

static void HandleHeartbeat(void) {
  size_t i;
  sigset_t mask;
  if (nowl() - lastrefresh > 60 * 60) RefreshTime();
  UpdateCurrentDate(nowl());
  Reindex();
  getrusage(RUSAGE_SELF, &shared->server);
#ifndef STATIC
  LuaRunAsset("/.heartbeat.lua", false);
  CollectGarbage();
#endif
  for (i = 1; i < servers.n; ++i) {
    if (polls[i].fd < 0) {
      polls[i].fd = -polls[i].fd;
    }
  }
}

// returns 0 on success or response on error
static char *OpenAsset(struct Asset *a) {
  int fd;
  void *data;
  size_t size;
  struct stat *st;
  if (a->file->st.st_size) {
    size = a->file->st.st_size;
    if (msg.method == kHttpHead) {
      content = 0;
      contentlength = size;
    } else {
    OpenAgain:
      if ((fd = open(a->file->path.s, O_RDONLY)) != -1) {
        data = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data != MAP_FAILED) {
          LockInc(&shared->c.maps);
          UnmapLater(fd, data, size);
          content = data;
          contentlength = size;
        } else if ((st = _gc(malloc(sizeof(struct stat)))) &&
                   fstat(fd, st) != -1 && (data = malloc(st->st_size))) {
          /* probably empty file or zipos handle */
          LockInc(&shared->c.slurps);
          FreeLater(data);
          if (ReadAll(fd, data, st->st_size) != -1) {
            content = data;
            contentlength = st->st_size;
            close(fd);
          } else {
            return HandleMapFailed(a, fd);
          }
        } else {
          return HandleMapFailed(a, fd);
        }
      } else if (errno == EINTR) {
        errno = 0;
        goto OpenAgain;
      } else {
        return HandleOpenFail(a);
      }
    }
  } else {
    content = "";
    contentlength = 0;
  }
  return 0;
}

static char *ServeServerOptions(void) {
  char *p;
  LockInc(&shared->c.serveroptions);
  p = SetStatus(200, "OK");
#ifdef STATIC
  p = stpcpy(p, "Allow: GET, HEAD, OPTIONS\r\n");
#else
  p = stpcpy(p, "Accept: */*\r\n"
                "Accept-Charset: utf-8,ISO-8859-1;q=0.7,*;q=0.5\r\n"
                "Allow: GET, HEAD, POST, PUT, DELETE, OPTIONS\r\n");
#endif
  return p;
}

static void SendContinueIfNeeded(void) {
  if (msg.version >= 11 && HeaderEqualCase(kHttpExpect, "100-continue")) {
    LockInc(&shared->c.continues);
    SendContinue();
  }
}

static char *ReadMore(void) {
  size_t got;
  ssize_t rc;
  LockInc(&shared->c.frags);
  if (++frags == 64) return HandlePayloadSlowloris();
  if ((rc = reader(client, inbuf.p + amtread, inbuf.n - amtread)) != -1) {
    if (!(got = rc)) return HandlePayloadDisconnect();
    amtread += got;
  } else if (errno == EINTR) {
    LockInc(&shared->c.readinterrupts);
    if (killed || ((meltdown || terminated) && nowl() - startread > 1)) {
      return HandlePayloadDrop();
    }
  } else {
    return HandlePayloadReadError();
  }
  return NULL;
}

static char *SynchronizeLength(void) {
  char *p;
  if (hdrsize + payloadlength > amtread) {
    if (hdrsize + payloadlength > inbuf.n) return HandleHugePayload();
    SendContinueIfNeeded();
    while (amtread < hdrsize + payloadlength) {
      if ((p = ReadMore())) return p;
    }
  }
  msgsize = hdrsize + payloadlength;
  return NULL;
}

static char *SynchronizeChunked(void) {
  char *p;
  ssize_t transferlength;
  struct HttpUnchunker u = {0};
  SendContinueIfNeeded();
  while (!(transferlength = Unchunk(&u, inbuf.p + hdrsize, amtread - hdrsize,
                                    &payloadlength))) {
    if ((p = ReadMore())) return p;
  }
  if (transferlength == -1) return HandleHugePayload();
  msgsize = hdrsize + transferlength;
  return NULL;
}

static char *SynchronizeStream(void) {
  int64_t cl;
  if (HasHeader(kHttpTransferEncoding) &&
      !HeaderEqualCase(kHttpTransferEncoding, "identity")) {
    if (HeaderEqualCase(kHttpTransferEncoding, "chunked")) {
      return SynchronizeChunked();
    } else {
      return HandleTransferRefused();
    }
  } else if (HasHeader(kHttpContentLength)) {
    if ((cl = ParseContentLength(HeaderData(kHttpContentLength),
                                 HeaderLength(kHttpContentLength))) != -1) {
      payloadlength = cl;
      return SynchronizeLength();
    } else {
      return HandleBadContentLength();
    }
  } else if (msg.method == kHttpPost || msg.method == kHttpPut) {
    return HandleLengthRequired();
  } else {
    msgsize = hdrsize;
    payloadlength = 0;
    return NULL;
  }
}

static void ParseRequestParameters(void) {
  uint32_t ip;
  FreeLater(ParseRequestUri(inbuf.p + msg.uri.a, msg.uri.b - msg.uri.a, &url));
  if (!url.host.p) {
    GetRemoteAddr(&ip, 0);
    if (HasHeader(kHttpXForwardedHost) &&
        (IsPrivateIp(ip) || IsLoopbackIp(ip))) {
      FreeLater(ParseHost(HeaderData(kHttpXForwardedHost),
                          HeaderLength(kHttpXForwardedHost), &url));
    } else if (HasHeader(kHttpHost)) {
      FreeLater(
          ParseHost(HeaderData(kHttpHost), HeaderLength(kHttpHost), &url));
    } else {
      FreeLater(ParseHost(DescribeServer(), -1, &url));
    }
  } else if (!url.path.n) {
    url.path.p = "/";
    url.path.n = 1;
  }
  if (!url.scheme.n) {
    if (usingssl) {
      url.scheme.p = "https";
      url.scheme.n = 5;
    } else {
      url.scheme.p = "http";
      url.scheme.n = 4;
    }
  }
}

static bool HasAtMostThisElement(int h, const char *s) {
  size_t i, n;
  struct HttpHeader *x;
  if (HasHeader(h)) {
    n = strlen(s);
    if (!SlicesEqualCase(s, n, HeaderData(h), HeaderLength(h))) {
      return false;
    }
    for (i = 0; i < msg.xheaders.n; ++i) {
      x = msg.xheaders.p + i;
      if (GetHttpHeader(inbuf.p + x->k.a, x->k.b - x->k.a) == h &&
          !SlicesEqualCase(inbuf.p + x->v.a, x->v.b - x->v.a, s, n)) {
        return false;
      }
    }
  }
  return true;
}

static char *HandleRequest(void) {
  char *p;
  if (msg.version == 11) {
    LockInc(&shared->c.http11);
  } else if (msg.version < 10) {
    LockInc(&shared->c.http09);
  } else if (msg.version == 10) {
    LockInc(&shared->c.http10);
  } else {
    return HandleVersionNotSupported();
  }
  if ((p = SynchronizeStream())) return p;
  if (logbodies) LogBody("received", inbuf.p + hdrsize, payloadlength);
  if (msg.version < 11 || HeaderEqualCase(kHttpConnection, "close")) {
    connectionclose = true;
  }
  if (msg.method == kHttpOptions &&
      SlicesEqual(inbuf.p + msg.uri.a, msg.uri.b - msg.uri.a, "*", 1)) {
    return ServeServerOptions();
  }
  if (msg.method == kHttpConnect) {
    return HandleConnectRefused();
  }
  if (!HasAtMostThisElement(kHttpExpect, "100-continue")) {
    return HandleExpectFailed();
  }
  ParseRequestParameters();
  if (!url.host.n || !url.path.n || url.path.p[0] != '/' ||
      !IsAcceptablePath(url.path.p, url.path.n) ||
      !IsAcceptableHost(url.host.p, url.host.n) ||
      !IsAcceptablePort(url.port.p, url.port.n)) {
    free(url.params.p);
    LockInc(&shared->c.urisrefused);
    return ServeFailure(400, "Bad URI");
  }
  INFOF("(req) received %s HTTP%02d %.*s %s %`'.*s %`'.*s", DescribeClient(),
        msg.version, msg.xmethod.b - msg.xmethod.a, inbuf.p + msg.xmethod.a,
        FreeLater(EncodeUrl(&url, 0)), HeaderLength(kHttpReferer),
        HeaderData(kHttpReferer), HeaderLength(kHttpUserAgent),
        HeaderData(kHttpUserAgent));
  if (HasHeader(kHttpContentType) &&
      IsMimeType(HeaderData(kHttpContentType), HeaderLength(kHttpContentType),
                 "application/x-www-form-urlencoded")) {
    FreeLater(ParseParams(inbuf.p + hdrsize, payloadlength, &url.params));
  }
  FreeLater(url.params.p);
#ifndef STATIC
  if (hasonhttprequest) return LuaOnHttpRequest();
#endif
  return Route(url.host.p, url.host.n, url.path.p, url.path.n);
}

static char *Route(const char *host, size_t hostlen, const char *path,
                   size_t pathlen) {
  char *p;
  // reset the redirect loop check, as it can only be looping inside
  // this function (as it always serves something); otherwise
  // successful RoutePath and Route may fail with "508 loop detected"
  loops.n = 0;
  if (logmessages) LogMessage("received", inbuf.p, hdrsize);
  if (hostlen && (p = RouteHost(host, hostlen, path, pathlen))) {
    return p;
  }
  if (SlicesEqual(path, pathlen, "/", 1)) {
    if ((p = ServeIndex("/", 1))) return p;
    return ServeListing();
  } else if ((p = RoutePath(path, pathlen))) {
    return p;
  } else if (SlicesEqual(path, pathlen, "/statusz", 8)) {
    return ServeStatusz();
  } else {
    LockInc(&shared->c.notfounds);
    return ServeError(404, "Not Found");
  }
}

static char *RoutePath(const char *path, size_t pathlen) {
  int m;
  long r;
  char *p;
  struct Asset *a;
  DEBUGF("(srvr) RoutePath(%`'.*s)", pathlen, path);
  if ((a = GetAsset(path, pathlen))) {
    // only allow "read other" permissions for security
    // and consistency with handling of "external" files
    // in this and other webservers
    if ((m = GetMode(a)) & 0004) {
      if (!S_ISDIR(m)) {
        return HandleAsset(a, path, pathlen);
      } else {
        return HandleFolder(path, pathlen);
      }
    } else {
      LockInc(&shared->c.forbiddens);
      WARNF("(srvr) asset %`'.*s %#o isn't readable", pathlen, path, m);
      return ServeError(403, "Forbidden");
    }
  } else if ((r = FindRedirect(path, pathlen)) != -1) {
    return HandleRedirect(redirects.p + r);
  } else {
    return NULL;
  }
}

static char *RouteHost(const char *host, size_t hostlen, const char *path,
                       size_t pathlen) {
  size_t hn, hm;
  char *hp, *p, b[96];
  if (hostlen) {
    hn = 1 + hostlen + url.path.n;
    hm = 3 + 1 + hn;
    hp = hm <= sizeof(b) ? b : FreeLater(xmalloc(hm));
    hp[0] = '/';
    mempcpy(mempcpy(hp + 1, host, hostlen), path, pathlen);
    if ((p = RoutePath(hp, hn))) return p;
    if (!isdigit(host[0])) {
      if (hostlen > 4 &&
          READ32LE(host) == ('w' | 'w' << 8 | 'w' << 16 | '.' << 24)) {
        mempcpy(mempcpy(hp + 1, host + 4, hostlen - 4), path, pathlen);
        if ((p = RoutePath(hp, hn - 4))) return p;
      } else {
        mempcpy(mempcpy(mempcpy(hp + 1, "www.", 4), host, hostlen), path,
                pathlen);
        if ((p = RoutePath(hp, hn + 4))) return p;
      }
    }
  }
  return NULL;
}

static inline bool IsLua(struct Asset *a) {
  size_t n;
  const char *p;
  if (a->file && a->file->path.n >= 4 &&
      READ32LE(a->file->path.s + a->file->path.n - 4) ==
          ('.' | 'l' << 8 | 'u' << 16 | 'a' << 24)) {
    return true;
  }
  p = ZIP_CFILE_NAME(zbase + a->cf);
  n = ZIP_CFILE_NAMESIZE(zbase + a->cf);
  return n > 4 &&
         READ32LE(p + n - 4) == ('.' | 'l' << 8 | 'u' << 16 | 'a' << 24);
}

static char *HandleAsset(struct Asset *a, const char *path, size_t pathlen) {
  char *p;
#ifndef STATIC
  if (IsLua(a)) return ServeLua(a, path, pathlen);
#endif
  if (msg.method == kHttpGet || msg.method == kHttpHead) {
    LockInc(&shared->c.staticrequests);
    p = ServeAsset(a, path, pathlen);
    if (!gotxcontenttypeoptions) {
      p = stpcpy(p, "X-Content-Type-Options: nosniff\r\n");
    }
    return p;
  } else {
    return BadMethod();
  }
}

static const char *GetContentType(struct Asset *a, const char *path, size_t n) {
  const char *r;
  if (a->file && (r = FindContentType(a->file->path.s, a->file->path.n))) {
    return r;
  }
  return firstnonnull(
      FindContentType(path, n),
      firstnonnull(FindContentType(ZIP_CFILE_NAME(zbase + a->cf),
                                   ZIP_CFILE_NAMESIZE(zbase + a->cf)),
                   a->istext ? "text/plain" : "application/octet-stream"));
}

static bool IsNotModified(struct Asset *a) {
  if (msg.version < 10) return false;
  if (!HasHeader(kHttpIfModifiedSince)) return false;
  return a->lastmodified >=
         ParseHttpDateTime(HeaderData(kHttpIfModifiedSince),
                           HeaderLength(kHttpIfModifiedSince));
}

static char *ServeAsset(struct Asset *a, const char *path, size_t pathlen) {
  char *p;
  uint32_t crc;
  const char *ct;
  ct = GetContentType(a, path, pathlen);
  if (IsNotModified(a)) {
    LockInc(&shared->c.notmodifieds);
    p = SetStatus(304, "Not Modified");
  } else {
    if (!a->file) {
      content = (char *)ZIP_LFILE_CONTENT(zbase + a->lf);
      contentlength = GetZipCfileCompressedSize(zbase + a->cf);
    } else if ((p = OpenAsset(a))) {
      return p;
    }
    if (IsCompressed(a)) {
      if (ClientAcceptsGzip()) {
        p = ServeAssetPrecompressed(a);
      } else {
        p = ServeAssetDecompressed(a);
      }
    } else if (msg.version >= 11 && HasHeader(kHttpRange)) {
      p = ServeAssetRange(a);
    } else if (!a->file) {
      LockInc(&shared->c.identityresponses);
      DEBUGF("(zip) ServeAssetZipIdentity(%`'s)", ct);
      if (Verify(content, contentlength, ZIP_LFILE_CRC32(zbase + a->lf))) {
        p = SetStatus(200, "OK");
      } else {
        return ServeError(500, "Internal Server Error");
      }
    } else if (!IsTiny() && msg.method != kHttpHead && !IsSslCompressed() &&
               ClientAcceptsGzip() &&
               ((contentlength >= 100 && startswithi(ct, "text/")) ||
                (contentlength >= 1000 && MeasureEntropy(content, 1000) < 7))) {
      p = ServeAssetCompressed(a);
    } else {
      p = ServeAssetIdentity(a, ct);
    }
  }
  p = AppendContentType(p, ct);
  p = stpcpy(p, "Vary: Accept-Encoding\r\n");
  p = AppendHeader(p, "Last-Modified", a->lastmodifiedstr);
  if (msg.version >= 11) {
    if (!gotcachecontrol) {
      p = AppendCache(p, cacheseconds);
    }
    if (!IsCompressed(a)) {
      p = stpcpy(p, "Accept-Ranges: bytes\r\n");
    }
  }
  return p;
}

static char *SetStatus(unsigned code, const char *reason) {
  if (msg.version == 10) {
    if (code == 307) code = 302;
    if (code == 308) code = 301;
  }
  statuscode = code;
  hascontenttype = istext = false;  // reset, as the headers are reset
  gotxcontenttypeoptions = 0;
  gotcachecontrol = 0;
  referrerpolicy = 0;
  branded = 0;
  stpcpy(hdrbuf.p, "HTTP/1.0 000 ");
  hdrbuf.p[7] += msg.version & 1;
  hdrbuf.p[9] += code / 100;
  hdrbuf.p[10] += code / 10 % 10;
  hdrbuf.p[11] += code % 10;
  VERBOSEF("(rsp) %d %s", code, reason);
  return AppendCrlf(stpcpy(hdrbuf.p + 13, reason));
}

static inline bool MustNotIncludeMessageBody(void) { /* RFC2616 § 4.4 */
  return msg.method == kHttpHead || (100 <= statuscode && statuscode <= 199) ||
         statuscode == 204 || statuscode == 304;
}

static bool TransmitResponse(char *p) {
  int iovlen;
  struct iovec iov[4];
  long actualcontentlength;
  if (msg.version >= 10) {
    actualcontentlength = contentlength;
    if (gzipped) {
      actualcontentlength += sizeof(kGzipHeader) + sizeof(gzip_footer);
      p = stpcpy(p, "Content-Encoding: gzip\r\n");
    }
    p = AppendContentLength(p, actualcontentlength);
    p = AppendCrlf(p);
    CHECK_LE(p - hdrbuf.p, hdrbuf.n);
    if (logmessages) {
      LogMessage("sending", hdrbuf.p, p - hdrbuf.p);
    }
    iov[0].iov_base = hdrbuf.p;
    iov[0].iov_len = p - hdrbuf.p;
    iovlen = 1;
    if (!MustNotIncludeMessageBody()) {
      if (gzipped) {
        iov[iovlen].iov_base = kGzipHeader;
        iov[iovlen].iov_len = sizeof(kGzipHeader);
        ++iovlen;
      }
      iov[iovlen].iov_base = content;
      iov[iovlen].iov_len = contentlength;
      ++iovlen;
      if (gzipped) {
        iov[iovlen].iov_base = gzip_footer;
        iov[iovlen].iov_len = sizeof(gzip_footer);
        ++iovlen;
      }
    }
  } else {
    iov[0].iov_base = content;
    iov[0].iov_len = contentlength;
    iovlen = 1;
  }
  Send(iov, iovlen);
  LockInc(&shared->c.messageshandled);
  ++messageshandled;
  return true;
}

static bool StreamResponse(char *p) {
  int rc;
  struct iovec iov[6];
  char *s, chunkbuf[23];
  assert(!MustNotIncludeMessageBody());
  if (msg.version >= 11) {
    p = stpcpy(p, "Transfer-Encoding: chunked\r\n");
  } else {
    assert(connectionclose);
  }
  p = AppendCrlf(p);
  CHECK_LE(p - hdrbuf.p, hdrbuf.n);
  if (logmessages) {
    LogMessage("sending", hdrbuf.p, p - hdrbuf.p);
  }
  bzero(iov, sizeof(iov));
  if (msg.version >= 10) {
    iov[0].iov_base = hdrbuf.p;
    iov[0].iov_len = p - hdrbuf.p;
  }
  if (msg.version >= 11) {
    iov[5].iov_base = "\r\n";
    iov[5].iov_len = 2;
  }
  for (;;) {
    iov[2].iov_base = 0;
    iov[2].iov_len = 0;
    iov[3].iov_base = 0;
    iov[3].iov_len = 0;
    iov[4].iov_base = 0;
    iov[4].iov_len = 0;
    if ((rc = generator(iov + 2)) <= 0) break;
    if (msg.version >= 11) {
      s = chunkbuf;
      s += uint64toarray_radix16(rc, s);
      s = AppendCrlf(s);
      iov[1].iov_base = chunkbuf;
      iov[1].iov_len = s - chunkbuf;
    }
    if (Send(iov, 6) == -1) break;
    iov[0].iov_base = 0;
    iov[0].iov_len = 0;
  }
  if (rc != -1) {
    if (msg.version >= 11) {
      iov[0].iov_base = "0\r\n\r\n";
      iov[0].iov_len = 5;
      Send(iov, 1);
    }
  } else {
    connectionclose = true;
  }
  return true;
}

static bool HandleMessageActual(void) {
  int rc;
  char *p;
  long double now;
  if ((rc = ParseHttpMessage(&msg, inbuf.p, amtread)) != -1) {
    if (!rc) return false;
    hdrsize = rc;
    p = HandleRequest();
  } else {
    LockInc(&shared->c.badmessages);
    connectionclose = true;
    if ((p = DumpHexc(inbuf.p, MIN(amtread, 256), 0))) {
      INFOF("(clnt) %s sent garbage %s", DescribeClient(), p);
    }
    return true;
  }
  if (!msgsize) {
    amtread = 0;
    connectionclose = true;
    LockInc(&shared->c.synchronizationfailures);
    DEBUGF("(clnt) could not synchronize message stream");
  }
  if (msg.version >= 10) {
    p = AppendCrlf(stpcpy(stpcpy(p, "Date: "), shared->currentdate));
    if (!branded) p = stpcpy(p, serverheader);
    if (extrahdrs) p = stpcpy(p, extrahdrs);
    if (connectionclose) {
      p = stpcpy(p, "Connection: close\r\n");
    } else if (timeout.tv_sec < 0 && msg.version >= 11) {
      p = stpcpy(p, "Connection: keep-alive\r\n");
    }
  }
  // keep content-type update *before* referrerpolicy
  // https://datatracker.ietf.org/doc/html/rfc2616#section-7.2.1
  if (!hascontenttype && contentlength > 0) {
    p = AppendContentType(p, "text/html");
  }
  if (referrerpolicy) {
    p = stpcpy(p, "Referrer-Policy: ");
    p = stpcpy(p, referrerpolicy);
    p = stpcpy(p, "\r\n");
  }
  if (loglatency || LOGGABLE(kLogDebug)) {
    now = nowl();
    LOGF(kLogDebug, "(stat) %`'.*s latency r: %,ldµs c: %,ldµs",
         msg.uri.b - msg.uri.a, inbuf.p + msg.uri.a,
         (long)((now - startrequest) * 1e6L),
         (long)((now - startconnection) * 1e6L));
  }
  if (!generator) {
    return TransmitResponse(p);
  } else {
    return StreamResponse(p);
  }
}

static bool HandleMessage(void) {
  bool r;
  ishandlingrequest = true;
  r = HandleMessageActual();
  ishandlingrequest = false;
  return r;
}

static void InitRequest(void) {
  assert(!outbuf);
  frags = 0;
  outbuf = 0;
  gzipped = 0;
  branded = 0;
  content = 0;
  msgsize = 0;
  loops.n = 0;
  generator = 0;
  luaheaderp = 0;
  isyielding = 0;
  contentlength = 0;
  referrerpolicy = 0;
  gotcachecontrol = 0;
  gotxcontenttypeoptions = 0;
  hascontenttype = istext = false;
  InitHttpMessage(&msg, kHttpRequest);
}

static bool IsSsl(unsigned char c) {
  if (c == 22) return true;
  if (!(c & 128)) return false;
  /* RHEL5 sends SSLv2 hello but supports TLS */
  DEBUGF("(ssl) %s SSLv2 hello D:", DescribeClient());
  return true;
}

static void HandleMessages(void) {
  char *p;
  bool once;
  ssize_t rc;
  size_t got;
  for (once = false;;) {
    InitRequest();
    startread = nowl();
    for (;;) {
      if (!msg.i && amtread) {
        startrequest = nowl();
        if (HandleMessage()) break;
      }
      if ((rc = reader(client, inbuf.p + amtread, inbuf.n - amtread)) != -1) {
        startrequest = nowl();
        got = rc;
        amtread += got;
        if (amtread) {
#ifndef UNSECURE
          if (!once) {
            once = true;
            if (!unsecure) {
              if (IsSsl(inbuf.p[0])) {
                if (TlsSetup()) {
                  continue;
                } else {
                  return;
                }
              } else if (requiressl) {
                INFOF("(clnt) %s didn't send an ssl hello", DescribeClient());
                return;
              } else {
                WipeServingKeys();
              }
            }
          }
#endif
          DEBUGF("(stat) %s read %,zd bytes", DescribeClient(), got);
          if (HandleMessage()) {
            break;
          } else if (got) {
            HandleFrag(got);
          }
        }
        if (!got) {
          NotifyClose();
          LogClose("disconnect");
          return;
        }
      } else if (errno == EINTR) {
        LockInc(&shared->c.readinterrupts);
        errno = 0;
      } else if (errno == EAGAIN) {
        LockInc(&shared->c.readtimeouts);
        if (amtread) SendTimeout();
        NotifyClose();
        LogClose("readtimeout");
        return;
      } else if (errno == ECONNRESET) {
        LockInc(&shared->c.readresets);
        LogClose("readreset");
        return;
      } else {
        LockInc(&shared->c.readerrors);
        WARNF("(clnt) %s readerror: %m", DescribeClient());
        return;
      }
      if (killed || (terminated && !amtread) ||
          (meltdown && (!amtread || nowl() - startread > 1))) {
        if (amtread) {
          LockInc(&shared->c.dropped);
          SendServiceUnavailable();
        }
        NotifyClose();
        LogClose(DescribeClose());
        return;
      }
      if (invalidated) {
        HandleReload();
        invalidated = false;
      }
    }
    if (msgsize == amtread) {
      amtread = 0;
      if (killed) {
        LogClose(DescribeClose());
        return;
      } else if (connectionclose || terminated || meltdown) {
        NotifyClose();
        LogClose(DescribeClose());
        return;
      }
    } else {
      CHECK_LT(msgsize, amtread);
      LockInc(&shared->c.pipelinedrequests);
      DEBUGF("(stat) %,ld pipelinedrequest bytes", amtread - msgsize);
      memmove(inbuf.p, inbuf.p + msgsize, amtread - msgsize);
      amtread -= msgsize;
      if (killed) {
        LogClose(DescribeClose());
        return;
      } else if (connectionclose) {
        NotifyClose();
        LogClose(DescribeClose());
        return;
      }
    }
    CollectGarbage();
    if (invalidated) {
      HandleReload();
      invalidated = false;
    }
  }
}

static void CloseServerFds(void) {
  size_t i;
  for (i = 0; i < servers.n; ++i) {
    close(servers.p[i].fd);
  }
}

static int ExitWorker(void) {
  if (IsModeDbg() && !sandboxed) {
    isexitingworker = true;
    return eintr();
  }
  if (monitortty) {
    terminatemonitor = true;
    _wait0((int *)(monitortls + 0x38));
  }
  _Exit(0);
}

static const struct sock_filter kSandboxOnline[] = {
    _SECCOMP_MACHINE(AUDIT_ARCH_X86_64),  //
    _SECCOMP_LOAD_SYSCALL_NR(),           //
    _SECCOMP_ALLOW_SYSCALL(0x013),        // readv
    _SECCOMP_ALLOW_SYSCALL(0x014),        // writev
    _SECCOMP_ALLOW_SYSCALL(0x009),        // mmap
    _SECCOMP_ALLOW_SYSCALL(0x00b),        // munmap
    _SECCOMP_ALLOW_SYSCALL(0x000),        // read
    _SECCOMP_ALLOW_SYSCALL(0x001),        // write
    _SECCOMP_ALLOW_SYSCALL(0x003),        // close
    _SECCOMP_ALLOW_SYSCALL(0x008),        // lseek
    _SECCOMP_ALLOW_SYSCALL(0x04f),        // getcwd
    _SECCOMP_ALLOW_SYSCALL(0x027),        // getpid
    _SECCOMP_ALLOW_SYSCALL(0x066),        // getuid
    _SECCOMP_ALLOW_SYSCALL(0x068),        // getgid
    _SECCOMP_ALLOW_SYSCALL(0x06e),        // getppid
    _SECCOMP_ALLOW_SYSCALL(0x06f),        // getpgrp
    _SECCOMP_ALLOW_SYSCALL(0x07c),        // getsid
    _SECCOMP_ALLOW_SYSCALL(0x06b),        // geteuid
    _SECCOMP_ALLOW_SYSCALL(0x06c),        // getegid
    _SECCOMP_ALLOW_SYSCALL(0x061),        // getrlimit
    _SECCOMP_ALLOW_SYSCALL(0x00f),        // rt_sigreturn
    _SECCOMP_ALLOW_SYSCALL(0x0e7),        // exit_group
    _SECCOMP_ALLOW_SYSCALL(0x106),        // newfstatat
    _SECCOMP_ALLOW_SYSCALL(0x0e4),        // clock_gettime
    _SECCOMP_ALLOW_SYSCALL(0x03f),        // uname
    _SECCOMP_ALLOW_SYSCALL(0x048),        // fcntl
    _SECCOMP_ALLOW_SYSCALL(0x029),        // socket
    _SECCOMP_ALLOW_SYSCALL(0x02a),        // connect
    _SECCOMP_ALLOW_SYSCALL(0x02c),        // sendto
    _SECCOMP_ALLOW_SYSCALL(0x02d),        // recvfrom
    _SECCOMP_ALLOW_SYSCALL(0x036),        // setsockopt
    _SECCOMP_LOG_AND_RETURN_ERRNO(1),     // EPERM
};

static const struct sock_filter kSandboxOffline[] = {
    _SECCOMP_MACHINE(AUDIT_ARCH_X86_64),  //
    _SECCOMP_LOAD_SYSCALL_NR(),           //
    _SECCOMP_ALLOW_SYSCALL(0x013),        // readv
    _SECCOMP_ALLOW_SYSCALL(0x014),        // writev
    _SECCOMP_ALLOW_SYSCALL(0x000),        // read
    _SECCOMP_ALLOW_SYSCALL(0x001),        // write
    _SECCOMP_ALLOW_SYSCALL(0x009),        // mmap
    _SECCOMP_ALLOW_SYSCALL(0x00b),        // munmap
    _SECCOMP_ALLOW_SYSCALL(0x003),        // close
    _SECCOMP_ALLOW_SYSCALL(0x008),        // lseek
    _SECCOMP_ALLOW_SYSCALL(0x04f),        // getcwd
    _SECCOMP_ALLOW_SYSCALL(0x027),        // getpid
    _SECCOMP_ALLOW_SYSCALL(0x066),        // getuid
    _SECCOMP_ALLOW_SYSCALL(0x068),        // getgid
    _SECCOMP_ALLOW_SYSCALL(0x06e),        // getppid
    _SECCOMP_ALLOW_SYSCALL(0x06f),        // getpgrp
    _SECCOMP_ALLOW_SYSCALL(0x07c),        // getsid
    _SECCOMP_ALLOW_SYSCALL(0x06b),        // geteuid
    _SECCOMP_ALLOW_SYSCALL(0x06c),        // getegid
    _SECCOMP_ALLOW_SYSCALL(0x061),        // getrlimit
    _SECCOMP_ALLOW_SYSCALL(0x00f),        // rt_sigreturn
    _SECCOMP_ALLOW_SYSCALL(0x0e7),        // exit_group
    _SECCOMP_ALLOW_SYSCALL(0x106),        // newfstatat
    _SECCOMP_ALLOW_SYSCALL(0x0e4),        // clock_gettime
    _SECCOMP_ALLOW_SYSCALL(0x03f),        // uname
    _SECCOMP_ALLOW_SYSCALL(0x048),        // fcntl
    _SECCOMP_LOG_AND_RETURN_ERRNO(1),     // EPERM
};

static const struct sock_fprog kSandboxOnlineProg = {
    .len = ARRAYLEN(kSandboxOnline),
    .filter = kSandboxOnline,
};

static const struct sock_fprog kSandboxOfflineProg = {
    .len = ARRAYLEN(kSandboxOffline),
    .filter = kSandboxOffline,
};

static int EnableSandbox(void) {
  const struct sock_fprog *sandbox;
  switch (sandboxed) {
    case 0:
      return 0;
    case 1:
      DEBUGF("(stat) applying '%s' sandbox policy", "online");
      sandbox = &kSandboxOnlineProg;
      break;
    default:
      DEBUGF("(stat) applying '%s' sandbox policy", "offline");
      sandbox = &kSandboxOfflineProg;
      break;
  }
  if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, sandbox) != -1) {
    return 0;
  } else {
    return -1;
  }
}

static int MemoryMonitor(void *arg) {
  static struct termios oldterm;
  static int tty;
  sigset_t ss;
  bool done, ok;
  size_t intervals;
  struct winsize ws;
  unsigned char rez;
  struct termios term;
  char *b, *addr, title[128];
  struct MemoryInterval *mi, *mi2;
  long i, j, k, n, x, y, pi, gen, pages;
  int rc, id, color, color2, workers;
  id = atomic_load_explicit(&shared->workers, memory_order_relaxed);
  DEBUGF("(memv) started for pid %d on tid %d", getpid(), gettid());

  sigemptyset(&ss);
  sigaddset(&ss, SIGHUP);
  sigaddset(&ss, SIGINT);
  sigaddset(&ss, SIGQUIT);
  sigaddset(&ss, SIGTERM);
  sigaddset(&ss, SIGPIPE);
  sigaddset(&ss, SIGUSR1);
  sigaddset(&ss, SIGUSR2);
  sigprocmask(SIG_BLOCK, &ss, 0);

  _spinlock(&shared->montermlock);
  if (!id) {
    if ((tty = open(monitortty, O_RDWR | O_NOCTTY)) != -1) {
      ioctl(tty, TCGETS, &oldterm);
      term = oldterm;
      term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
      term.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
      term.c_oflag |= OPOST | ONLCR;
      term.c_iflag |= IUTF8;
      term.c_cflag |= CS8;
      term.c_cc[VMIN] = 1;
      term.c_cc[VTIME] = 0;
      ioctl(tty, TCSETS, &term);
      WRITE(tty, "\e[?25l", 6);
    }
  }
  _spunlock(&shared->montermlock);

  if (tty != -1) {
    for (gen = 0, mi = 0, b = 0; !terminatemonitor;) {
      workers = atomic_load_explicit(&shared->workers, memory_order_relaxed);
      if (id) id = MAX(1, MIN(id, workers));
      if (!id && workers) {
        usleep(50000);
        continue;
      }

      ++gen;
      intervals = atomic_load_explicit(&_mmi.i, memory_order_relaxed);
      if ((mi2 = realloc(mi, (intervals += 3) * sizeof(*mi)))) {
        mi = mi2;
        mi[0].x = (intptr_t)_base >> 16;
        mi[0].size = _etext - _base;
        mi[0].flags = 0;
        mi[1].x = (intptr_t)_etext >> 16;
        mi[1].size = _edata - _etext;
        mi[1].flags = 0;
        mi[2].x = (intptr_t)_edata >> 16;
        mi[2].size = _end - _edata;
        mi[2].flags = 0;
        __mmi_lock();
        if (_mmi.i == intervals - 3) {
          memcpy(mi + 3, _mmi.p, _mmi.i * sizeof(*mi));
          ok = true;
        } else {
          ok = false;
        }
        __mmi_unlock();
        if (!ok) {
          VERBOSEF("(memv) retrying due to contention on mmap table");
          continue;
        }

        ws.ws_col = 80;
        ws.ws_row = 40;
        getttysize(tty, &ws);

        appendr(&b, 0);
        appends(&b, "\e[H\e[1m");

        for (pi = k = x = y = i = 0; i < intervals; ++i) {
          addr = (char *)((int64_t)((uint64_t)mi[i].x << 32) >> 16);
          color = 0;
          appendf(&b, "\e[0m%lx", addr);
          pages = (mi[i].size + PAGESIZE - 1) / PAGESIZE;
          for (j = 0; j < pages; ++j) {
            rc = mincore(addr + j * PAGESIZE, PAGESIZE, &rez);
            if (!rc) {
              if (rez & 1) {
                if (mi[i].flags & MAP_SHARED) {
                  color2 = 105;
                } else {
                  color2 = 42;
                }
              } else {
                color2 = 41;
              }
            } else {
              errno = 0;
              color2 = 0;
            }
            if (color != color2) {
              color = color2;
              appendf(&b, "\e[%dm", color);
            }
            if (mi[i].flags & MAP_ANONYMOUS) {
              appendw(&b, ' ');
            } else {
              appendw(&b, '/');
            }
          }
        }

        appendf(&b,
                "\e[0m ID=%d PID=%d WS=%dx%d WORKERS=%d MODE=" MODE
                " GEN=%ld\e[J",
                id, getpid(), ws.ws_col, ws.ws_row, workers, gen);

        _spinlock(&shared->montermlock);
        WRITE(tty, b, appendz(b).i);
        appendr(&b, 0);
        usleep(MONITOR_MICROS);
        _spunlock(&shared->montermlock);
      } else {
        // running out of memory temporarily is a real possibility here
        // the right thing to do, is stand aside and let lua try to fix
        WARNF("(memv) we require more vespene gas");
        usleep(MONITOR_MICROS);
      }
    }

    if (!id) {
      appendr(&b, 0);
      appends(&b, "\e[H\e[J\e[?25h");
      WRITE(tty, b, appendz(b).i);
      ioctl(tty, TCSETS, &oldterm);
    }

    DEBUGF("(memv) exiting...");
    close(tty);
    free(mi);
    free(b);
  }

  DEBUGF("(memv) done");
  return 0;
}

static void MonitorMemory(void) {
  if ((monitortls = malloc(64))) {
    if ((monitorstack = mmap(0, GetStackSize(), PROT_READ | PROT_WRITE,
                             MAP_STACK | MAP_ANONYMOUS, -1, 0)) != MAP_FAILED) {
      if (clone(MemoryMonitor, monitorstack, GetStackSize(),
                CLONE_VM | CLONE_THREAD | CLONE_FS | CLONE_FILES |
                    CLONE_SIGHAND | CLONE_SETTLS | CLONE_CHILD_SETTID |
                    CLONE_CHILD_CLEARTID,
                0, 0, __initialize_tls(monitortls), 64,
                (int *)(monitortls + 0x38)) != -1) {
        return;
      }
      munmap(monitorstack, GetStackSize());
    }
    free(monitortls);
  }
  WARNF("(memv) failed to start memory monitor %m");
  monitortty = 0;
}

static int HandleConnection(size_t i) {
  int pid, rc = 0;
  clientaddrsize = sizeof(clientaddr);
  if ((client = accept4(servers.p[i].fd, &clientaddr, &clientaddrsize,
                        SOCK_CLOEXEC)) != -1) {
    startconnection = nowl();
    VERBOSEF("(srvr) accept %s via %s", DescribeClient(), DescribeServer());
    messageshandled = 0;
    if (hasonclientconnection && LuaOnClientConnection()) {
      close(client);
      return 0;
    }
    if (uniprocess) {
      pid = -1;
      connectionclose = true;
    } else {
      switch ((pid = fork())) {
        case 0:
          if (!IsTiny() && monitortty) {
            MonitorMemory();
          }
          meltdown = false;
          __isworker = true;
          connectionclose = false;
          if (!IsTiny() && systrace) {
            kStartTsc = rdtsc();
          }
          TRACE_BEGIN;
          if (sandboxed) {
            CHECK_NE(-1, EnableSandbox());
          }
          if (hasonworkerstart) {
            CallSimpleHook("OnWorkerStart");
          }
          break;
        case -1:
          HandleForkFailure();
          return 0;
        default:
          LockInc(&shared->workers);
          close(client);
          ReseedRng(&rng, "parent");
          if (hasonprocesscreate) {
            LuaOnProcessCreate(pid);
          }
          return 0;
      }
    }
    if (!pid && !IsWindows()) {
      CloseServerFds();
    }
    HandleMessages();
    DEBUGF("(stat) %s closing after %,ldµs", DescribeClient(),
           (long)((nowl() - startconnection) * 1e6L));
    if (!pid) {
      if (hasonworkerstop) {
        CallSimpleHook("OnWorkerStop");
      }
      rc = ExitWorker();
    } else {
      close(client);
      oldin.p = 0;
      oldin.n = 0;
      if (inbuf.c) {
        inbuf.p -= inbuf.c;
        inbuf.n += inbuf.c;
        inbuf.c = 0;
      }
#ifndef UNSECURE
      if (usingssl) {
        usingssl = false;
        reader = read;
        writer = WritevAll;
        mbedtls_ssl_session_reset(&ssl);
      }
#endif
    }
    CollectGarbage();
  } else {
    if (errno == EINTR || errno == EAGAIN) {
      LockInc(&shared->c.acceptinterrupts);
    } else if (errno == ENFILE) {
      LockInc(&shared->c.enfiles);
      LogAcceptError("enfile: too many open files");
      meltdown = true;
    } else if (errno == EMFILE) {
      LockInc(&shared->c.emfiles);
      LogAcceptError("emfile: ran out of open file quota");
      meltdown = true;
    } else if (errno == ENOMEM) {
      LockInc(&shared->c.enomems);
      LogAcceptError("enomem: ran out of memory");
      meltdown = true;
    } else if (errno == ENOBUFS) {
      LockInc(&shared->c.enobufs);
      LogAcceptError("enobuf: ran out of buffer");
      meltdown = true;
    } else if (errno == ENONET) {
      LockInc(&shared->c.enonets);
      LogAcceptError("enonet: network gone");
      polls[i].fd = -polls[i].fd;
    } else if (errno == ENETDOWN) {
      LockInc(&shared->c.enetdowns);
      LogAcceptError("enetdown: network down");
      polls[i].fd = -polls[i].fd;
    } else if (errno == ECONNABORTED) {
      LockInc(&shared->c.accepterrors);
      LockInc(&shared->c.acceptresets);
      WARNF("(srvr) %S accept error: %s", DescribeServer(),
            "acceptreset: connection reset before accept");
    } else if (errno == ENETUNREACH || errno == EHOSTUNREACH ||
               errno == EOPNOTSUPP || errno == ENOPROTOOPT || errno == EPROTO) {
      LockInc(&shared->c.accepterrors);
      LockInc(&shared->c.acceptflakes);
      WARNF("(srvr) accept error: %s ephemeral accept error: %m",
            DescribeServer());
    } else {
      DIEF("(srvr) %s accept error: %m", DescribeServer());
    }
    errno = 0;
  }
  return rc;
}

static void MakeExecutableModifiable(void) {
  int ft;
  size_t n;
  extern char ape_rom_vaddr[] __attribute__((__weak__));
  if (!(SUPPORT_VECTOR & (METAL | WINDOWS | XNU))) return;
  if (IsWindows()) return;  // TODO
  if (IsOpenbsd()) return;  // TODO
  if (IsNetbsd()) return;   // TODO
  if (endswith(zpath, ".com.dbg")) return;
  close(zfd);
  ft = __ftrace;
  if ((zfd = OpenExecutable()) == -1) {
    WARNF("(srvr) can't open executable for modification: %m");
  }
  if (ft > 0) {
    __ftrace = 0;
    ftrace_install();
    __ftrace = ft;
  }
}

static int HandleReadline(void) {
  int status;
  lua_State *L = GL;
  for (;;) {
    status = lua_loadline(L);
    if (status < 0) {
      if (status == -1) {
        OnTerm(SIGHUP);  // eof
        VERBOSEF("(repl) eof");
        return -1;
      } else if (errno == EINTR) {
        errno = 0;
        VERBOSEF("(repl) interrupt");
        shutdownsig = SIGINT;
        OnInt(SIGINT);
        kill(0, SIGINT);
        return -1;
      } else if (errno == EAGAIN) {
        errno = 0;
        return 0;
      } else {
        WARNF("(srvr) unexpected terminal error %d% m", status);
        errno = 0;
        return 0;
      }
    }
    DisableRawMode();
    lua_repl_lock();
    if (status == LUA_OK) {
      status = lua_runchunk(L, 0, LUA_MULTRET);
    }
    if (status == LUA_OK) {
      LuaPrint(L);
    } else {
      lua_report(L, status);
    }
    lua_repl_unlock();
    EnableRawMode();
  }
}

static int HandlePoll(int ms) {
  int rc, nfds;
  size_t pollid, serverid;
  if ((nfds = poll(polls, 1 + servers.n, ms)) != -1) {
    if (nfds) {
      // handle pollid/o events
      for (pollid = 0; pollid < 1 + servers.n; ++pollid) {
        if (!polls[pollid].revents) continue;
        if (polls[pollid].fd < 0) continue;
        if (polls[pollid].fd) {
          // handle listen socket
          lua_repl_lock();
          serverid = pollid - 1;
          assert(0 <= serverid && serverid < servers.n);
          serveraddr = &servers.p[serverid].addr;
          ishandlingconnection = true;
          rc = HandleConnection(serverid);
          ishandlingconnection = false;
          lua_repl_unlock();
          if (rc == -1) return -1;
#ifndef STATIC
        } else {
          // handle standard input
          rc = HandleReadline();
          if (rc == -1) return rc;
#endif
        }
      }
#ifndef STATIC
    } else if (__replmode) {
      // handle refresh repl line
      if (!IsWindows()) {
        rc = HandleReadline();
        if (rc < 0) return rc;
      } else {
        --__strace;
        linenoiseRefreshLine(lua_repl_linenoise);
        ++__strace;
      }
#endif
    }
  } else {
    if (errno == EINTR || errno == EAGAIN) {
      LockInc(&shared->c.pollinterrupts);
    } else if (errno == ENOMEM) {
      LockInc(&shared->c.enomems);
      WARNF("(srvr) poll error: ran out of memory");
      meltdown = true;
    } else {
      DIEF("(srvr) poll error: %m");
    }
    errno = 0;
  }
  return 0;
}

static void Listen(void) {
  char ipbuf[16];
  size_t i, j, n;
  uint32_t ip, port, addrsize, *ifs, *ifp;
  if (!ports.n) {
    ProgramPort(8080);
  }
  if (!ips.n) {
    if ((ifs = GetHostIps()) && *ifs) {
      for (ifp = ifs; *ifp; ++ifp) {
        sprintf(ipbuf, "%hhu.%hhu.%hhu.%hhu", *ifp >> 24, *ifp >> 16, *ifp >> 8,
                *ifp);
        ProgramAddr(ipbuf);
      }
    } else {
      ProgramAddr("0.0.0.0");
    }
    free(ifs);
  }
  servers.p = malloc(ips.n * ports.n * sizeof(*servers.p));
  for (n = i = 0; i < ips.n; ++i) {
    for (j = 0; j < ports.n; ++j, ++n) {
      bzero(servers.p + n, sizeof(*servers.p));
      servers.p[n].addr.sin_family = AF_INET;
      servers.p[n].addr.sin_port = htons(ports.p[j]);
      servers.p[n].addr.sin_addr.s_addr = htonl(ips.p[i]);
      if ((servers.p[n].fd = GoodSocket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC,
                                        IPPROTO_TCP, true, &timeout)) == -1) {
        DIEF("(srvr) socket: %m");
      }
      if (bind(servers.p[n].fd, &servers.p[n].addr,
               sizeof(servers.p[n].addr)) == -1) {
        DIEF("(srvr) bind error: %m: %hhu.%hhu.%hhu.%hhu:%hu", ips.p[i] >> 24,
             ips.p[i] >> 16, ips.p[i] >> 8, ips.p[i], ports.p[j]);
      }
      if (listen(servers.p[n].fd, 10) == -1) {
        DIEF("(srvr) listen error: %m");
      }
      addrsize = sizeof(servers.p[n].addr);
      if (getsockname(servers.p[n].fd, &servers.p[n].addr, &addrsize) == -1) {
        DIEF("(srvr) getsockname error: %m");
      }
      port = ntohs(servers.p[n].addr.sin_port);
      ip = ntohl(servers.p[n].addr.sin_addr.s_addr);
      if (ip == INADDR_ANY) ip = INADDR_LOOPBACK;
      INFOF("(srvr) listen http://%hhu.%hhu.%hhu.%hhu:%d", ip >> 24, ip >> 16,
            ip >> 8, ip, port);
      if (printport && !ports.p[j]) {
        printf("%d\r\n", port);
        fflush(stdout);
      }
    }
  }
  servers.n = n;
  polls = malloc((1 + n) * sizeof(*polls));
  polls[0].fd = -1;
  polls[0].events = POLLIN;
  polls[0].revents = 0;
  for (i = 0; i < n; ++i) {
    polls[1 + i].fd = servers.p[i].fd;
    polls[1 + i].events = POLLIN;
    polls[1 + i].revents = 0;
  }
}

static void HandleShutdown(void) {
  CloseServerFds();
  INFOF("(srvr) received %s", strsignal(shutdownsig));
  if (shutdownsig != SIGINT && shutdownsig != SIGQUIT) {
    if (!killed) terminated = false;
    INFOF("(srvr) killing process group");
    KillGroup();
  }
  WaitAll();
}

// this function coroutines with linenoise
static int EventLoop(int ms) {
  long double t;
  DEBUGF("(repl) event loop");
  while (!terminated) {
    errno = 0;
    if (zombied) {
      lua_repl_lock();
      ReapZombies();
      lua_repl_unlock();
    } else if (invalidated) {
      lua_repl_lock();
      HandleReload();
      lua_repl_unlock();
      invalidated = false;
    } else if (meltdown) {
      lua_repl_lock();
      EnterMeltdownMode();
      lua_repl_unlock();
      meltdown = false;
    } else if ((t = nowl()) - lastheartbeat > HEARTBEAT / 1000.) {
      lastheartbeat = t;
      HandleHeartbeat();
    } else if (HandlePoll(ms) == -1) {
      break;
    }
  }
  return -1;
}

static void ReplEventLoop(void) {
  lua_State *L = GL;
  DEBUGF("ReplEventLoop()");
  polls[0].fd = 0;
  lua_repl_completions_callback = HandleCompletions;
  lua_initrepl(L, "redbean");
  EnableRawMode();
  EventLoop(100);
  DisableRawMode();
  lua_freerepl();
  lua_settop(L, 0);  // clear stack
  polls[0].fd = -1;
}

static int WindowsReplThread(void *arg) {
  int sig;
  lua_State *L = GL;
  DEBUGF("(repl) started windows thread");
  lua_repl_blocking = true;
  lua_repl_completions_callback = HandleCompletions;
  lua_initrepl(L, "redbean");
  EnableRawMode();
  while (!terminated) {
    if (HandleReadline() == -1) {
      break;
    }
  }
  DisableRawMode();
  lua_freerepl();
  lua_repl_lock();
  lua_settop(L, 0);  // clear stack
  lua_repl_unlock();
  if ((sig = linenoiseGetInterrupt())) {
    raise(sig);
  }
  DEBUGF("(repl) terminating windows thread");
  return 0;
}

static void InstallSignalHandler(int sig, void *handler) {
  struct sigaction sa = {.sa_sigaction = handler};
  CHECK_NE(-1, sigaction(sig, &sa, 0));
}

static void SigInit(void) {
  InstallSignalHandler(SIGINT, OnInt);
  InstallSignalHandler(SIGHUP, OnHup);
  InstallSignalHandler(SIGTERM, OnTerm);
  InstallSignalHandler(SIGCHLD, OnChld);
  InstallSignalHandler(SIGUSR1, OnUsr1);
  InstallSignalHandler(SIGUSR2, OnUsr2);
  InstallSignalHandler(SIGPIPE, SIG_IGN);
}

static void TlsInit(void) {
#ifndef UNSECURE
  int suite;
  if (unsecure) return;

  if (suiteb && !X86_HAVE(AES)) {
    WARNF("(srvr) requested suite b crypto, but aes-ni is not present");
  }

  if (!sslinitialized) {
    InitializeRng(&rng);
    InitializeRng(&rngcli);
    suite = suiteb ? MBEDTLS_SSL_PRESET_SUITEB : MBEDTLS_SSL_PRESET_SUITEC;
    mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER,
                                MBEDTLS_SSL_TRANSPORT_STREAM, suite);
    mbedtls_ssl_config_defaults(&confcli, MBEDTLS_SSL_IS_CLIENT,
                                MBEDTLS_SSL_TRANSPORT_STREAM, suite);
  }

  // the following setting can be re-applied even when SSL/TLS is initialized
  if (suites.n) {
    mbedtls_ssl_conf_ciphersuites(&conf, suites.p);
    mbedtls_ssl_conf_ciphersuites(&confcli, suites.p);
  }
  if (psks.n) {
    mbedtls_ssl_conf_psk_cb(&conf, TlsRoutePsk, 0);
    DCHECK_EQ(0,
              mbedtls_ssl_conf_psk(&confcli, psks.p[0].key, psks.p[0].key_len,
                                   psks.p[0].identity, psks.p[0].identity_len));
  }
  if (sslticketlifetime > 0) {
    mbedtls_ssl_ticket_setup(&ssltick, mbedtls_ctr_drbg_random, &rng,
                             MBEDTLS_CIPHER_AES_256_GCM, sslticketlifetime);
    mbedtls_ssl_conf_session_tickets_cb(&conf, mbedtls_ssl_ticket_write,
                                        mbedtls_ssl_ticket_parse, &ssltick);
  }

  if (sslinitialized) return;
  sslinitialized = true;

  LoadCertificates();
  mbedtls_ssl_conf_sni(&conf, TlsRoute, 0);
  mbedtls_ssl_conf_dbg(&conf, TlsDebug, 0);
  mbedtls_ssl_conf_dbg(&confcli, TlsDebug, 0);
  mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &rng);
  mbedtls_ssl_conf_rng(&confcli, mbedtls_ctr_drbg_random, &rngcli);
  if (sslclientverify) {
    mbedtls_ssl_conf_ca_chain(&conf, GetSslRoots(), 0);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  }
  if (sslfetchverify) {
    mbedtls_ssl_conf_ca_chain(&confcli, GetSslRoots(), 0);
    mbedtls_ssl_conf_authmode(&confcli, MBEDTLS_SSL_VERIFY_REQUIRED);
  } else {
    mbedtls_ssl_conf_authmode(&confcli, MBEDTLS_SSL_VERIFY_NONE);
  }
  mbedtls_ssl_set_bio(&ssl, &g_bio, TlsSend, 0, TlsRecv);
  conf.disable_compression = confcli.disable_compression = true;
  DCHECK_EQ(0, mbedtls_ssl_conf_alpn_protocols(&conf, kAlpn));
  DCHECK_EQ(0, mbedtls_ssl_conf_alpn_protocols(&confcli, kAlpn));
  DCHECK_EQ(0, mbedtls_ssl_setup(&ssl, &conf));
  DCHECK_EQ(0, mbedtls_ssl_setup(&sslcli, &confcli));
#endif
}

static void TlsDestroy(void) {
#ifndef UNSECURE
  if (unsecure) return;
  mbedtls_ssl_free(&ssl);
  mbedtls_ssl_free(&sslcli);
  mbedtls_ctr_drbg_free(&rng);
  mbedtls_ctr_drbg_free(&rngcli);
  mbedtls_ssl_config_free(&conf);
  mbedtls_ssl_config_free(&confcli);
  mbedtls_ssl_ticket_free(&ssltick);
  CertsDestroy();
  PsksDestroy();
  Free(&suites.p), suites.n = 0;
#endif
}

static void MemDestroy(void) {
  FreeAssets();
  CollectGarbage();
  inbuf.p = 0, inbuf.n = 0, inbuf.c = 0;
  Free(&inbuf_actual.p), inbuf_actual.n = inbuf_actual.c = 0;
  Free(&unmaplist.p), unmaplist.n = unmaplist.c = 0;
  Free(&freelist.p), freelist.n = freelist.c = 0;
  Free(&hdrbuf.p), hdrbuf.n = hdrbuf.c = 0;
  Free(&servers.p), servers.n = 0;
  Free(&ports.p), ports.n = 0;
  Free(&ips.p), ips.n = 0;
  Free(&outbuf);
  FreeStrings(&stagedirs);
  FreeStrings(&hidepaths);
  Free(&launchbrowser);
  Free(&serverheader);
  Free(&extrahdrs);
  Free(&pidpath);
  Free(&logpath);
  Free(&brand);
  Free(&polls);
}

static void GetOpts(int argc, char *argv[]) {
  int opt;
  bool storeasset = false;
  while ((opt = getopt(argc, argv, GETOPTS)) != -1) {
    switch (opt) {
      CASE('S', ++sandboxed);
      CASE('v', ++__log_level);
      CASE('s', --__log_level);
      CASE('X', unsecure = true);
      CASE('%', norsagen = true);
      CASE('Z', systrace = true);
      CASE('b', logbodies = true);
      CASE('z', printport = true);
      CASE('d', daemonize = true);
      CASE('a', logrusage = true);
      CASE('J', requiressl = true);
      CASE('u', uniprocess = true);
      CASE('g', loglatency = true);
      CASE('m', logmessages = true);
      CASE('l', ProgramAddr(optarg));
      CASE('H', ProgramHeader(optarg));
      CASE('L', ProgramLogPath(optarg));
      CASE('P', ProgramPidPath(optarg));
      CASE('D', ProgramDirectory(optarg));
      CASE('U', ProgramUid(atoi(optarg)));
      CASE('G', ProgramGid(atoi(optarg)));
      CASE('p', ProgramPort(ParseInt(optarg)));
      CASE('R', ProgramRedirectArg(0, optarg));
      CASE('c', ProgramCache(ParseInt(optarg)));
      CASE('r', ProgramRedirectArg(307, optarg));
      CASE('t', ProgramTimeout(ParseInt(optarg)));
      CASE('h', PrintUsage(1, EXIT_SUCCESS));
      CASE('M', ProgramMaxPayloadSize(ParseInt(optarg)));
#if !IsTiny()
      CASE('W', monitortty = optarg);
#endif
#ifndef STATIC
      CASE('e', LuaEvalCode(optarg));
      CASE('F', LuaEvalFile(optarg));
      CASE('i', interpretermode = true);
      CASE('E', leakcrashreports = true);
      CASE('A', storeasset = true; StorePath(optarg));
#endif
#ifndef UNSECURE
      CASE('B', suiteb = true);
      CASE('V', ++mbedtls_debug_threshold);
      CASE('k', sslfetchverify = false);
      CASE('j', sslclientverify = true);
      CASE('T', ProgramSslTicketLifetime(ParseInt(optarg)));
      CASE('C', ProgramFile(optarg, ProgramCertificate));
      CASE('K', ProgramFile(optarg, ProgramPrivateKey));
#endif
      case 'f':
        funtrace = true;
        if (ftrace_install() == -1) {
          WARNF("(srvr) ftrace failed to install %m");
        }
        break;
      default:
        PrintUsage(2, EX_USAGE);
    }
  }
  // if storing asset(s) is requested, don't need to continue
  if (storeasset) exit(0);
}

void RedBean(int argc, char *argv[]) {
  if (IsLinux()) {
    // disable sneak privilege since we don't use them
    // seccomp will fail later if this fails
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
  }
  reader = read;
  writer = WritevAll;
  gmtoff = GetGmtOffset((lastrefresh = startserver = nowl()));
  mainpid = getpid();
  CHECK_GT(CLK_TCK, 0);
  CHECK_NE(MAP_FAILED,
           (shared = mmap(NULL, ROUNDUP(sizeof(struct Shared), FRAMESIZE),
                          PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
                          -1, 0)));
  zpath = GetProgramExecutableName();
  CHECK_NE(-1, (zfd = open(zpath, O_RDONLY)));
  CHECK_NE(-1, fstat(zfd, &zst));
  OpenZip(true);
  MakeExecutableModifiable();
  SetDefaults();
  LuaStart();
  GetOpts(argc, argv);
  LuaInit();
  oldloglevel = __log_level;
  if (uniprocess) {
    shared->workers = 1;
  }
  SigInit();
  Listen();
  TlsInit();
  if (launchbrowser) {
    LaunchBrowser(launchbrowser);
  }
  if (daemonize) {
    Daemonize();
  } else {
    if (logpath) {
      close(2);
      open(logpath, O_APPEND | O_WRONLY | O_CREAT, 0640);
    }
    ChangeUser();
  }
  UpdateCurrentDate(nowl());
  CollectGarbage();
  hdrbuf.n = 4 * 1024;
  hdrbuf.p = xmalloc(hdrbuf.n);
  inbuf_actual.n = maxpayloadsize;
  inbuf_actual.p = xmalloc(inbuf_actual.n);
  inbuf = inbuf_actual;
  isinitialized = true;
  CallSimpleHookIfDefined("OnServerStart");
  if (!IsTiny()) {
    if (monitortty && (daemonize || uniprocess)) {
      monitortty = 0;
    }
    if (monitortty) {
      MonitorMemory();
    }
  }
#ifdef STATIC
  EventLoop(HEARTBEAT);
#else
  GetHostsTxt();    // for effect
  GetResolvConf();  // for effect
  if (daemonize || uniprocess || !linenoiseIsTerminal()) {
    EventLoop(HEARTBEAT);
  } else if (IsWindows()) {
    CHECK_NE(MAP_FAILED, (repltls = malloc(64)));
    CHECK_NE(MAP_FAILED,
             (replstack = mmap(0, GetStackSize(), PROT_READ | PROT_WRITE,
                               MAP_STACK | MAP_ANONYMOUS, -1, 0)));
    CHECK_NE(
        -1,
        clone(WindowsReplThread, replstack, GetStackSize(),
              CLONE_VM | CLONE_THREAD | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
                  CLONE_SETTLS | CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID,
              0, 0, __initialize_tls(repltls), 64, (int *)(repltls + 0x38)));
    EventLoop(100);
  } else {
    ReplEventLoop();
  }
#endif
  if (!isexitingworker) {
    HandleShutdown();
    CallSimpleHookIfDefined("OnServerStop");
  }
  if (!IsTiny()) {
    LuaDestroy();
    TlsDestroy();
    MemDestroy();
  }
  if (!isexitingworker) {
    if (!IsTiny()) {
      if (monitortty) {
        terminatemonitor = true;
        _wait0((int *)(monitortls + 0x38));
        munmap(monitorstack, GetStackSize());
        free(monitortls);
      }
    }
#ifndef STATIC
    if (repltls) {
      _wait0((int *)(repltls + 0x38));
      munmap(replstack, GetStackSize());
      free(repltls);
    }
#endif
  }
  if (!isexitingworker) {
    INFOF("(srvr) shutdown complete");
  }
}

int main(int argc, char *argv[]) {
  LoadZipArgs(&argc, &argv);
  RedBean(argc, argv);
  if (IsModeDbg()) {
    CheckForMemoryLeaks();
  }
  return 0;
}
