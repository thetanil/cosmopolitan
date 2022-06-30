#ifndef COSMOPOLITAN_LIBC_RUNTIME_RUNTIME_H_
#define COSMOPOLITAN_LIBC_RUNTIME_RUNTIME_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_
/*───────────────────────────────────────────────────────────────────────────│─╗
│ cosmopolitan § runtime                                                   ─╬─│┼
╚────────────────────────────────────────────────────────────────────────────│*/

typedef long jmp_buf[8] forcealign(CACHELINE);

extern char **environ;                              /* CRT */
extern int __argc;                                  /* CRT */
extern char **__argv;                               /* CRT */
extern char **__envp;                               /* CRT */
extern unsigned long *__auxv;                       /* CRT */
extern intptr_t __oldstack;                         /* CRT */
extern uint64_t __nosync;                           /* SYS */
extern _Atomic(int) __ftrace;                       /* SYS */
extern _Atomic(int) __strace;                       /* SYS */
extern char *program_invocation_name;               /* RII */
extern char *program_invocation_short_name;         /* RII */
extern uint64_t __syscount;                         /* RII */
extern uint64_t kStartTsc;                          /* RII */
extern const char kTmpPath[];                       /* RII */
extern const char kNtSystemDirectory[];             /* RII */
extern const char kNtWindowsDirectory[];            /* RII */
extern unsigned char _base[] forcealign(PAGESIZE);  /* αpε */
extern unsigned char _ehead[] forcealign(PAGESIZE); /* αpε */
extern unsigned char _etext[] forcealign(PAGESIZE); /* αpε */
extern unsigned char _edata[] forcealign(PAGESIZE); /* αpε */
extern unsigned char _ezip[];                       /* αpε */
extern unsigned char _end[] forcealign(FRAMESIZE);  /* αpε */
extern unsigned char _ereal[];                      /* αpε */
extern unsigned char __privileged_start[];          /* αpε */
extern unsigned char __privileged_addr[];           /* αpε */
extern unsigned char __privileged_size[];           /* αpε */
extern unsigned char __privileged_end[];            /* αpε */
extern unsigned char __test_start[];                /* αpε */
extern unsigned char __ro[];                        /* αpε */
extern unsigned char *__relo_start[];               /* αpε */
extern unsigned char *__relo_end[];                 /* αpε */
extern uint8_t __zip_start[];                       /* αpε */
extern uint8_t __zip_end[];                         /* αpε */
extern size_t __virtualmax;
extern bool __isworker;

void mcount(void);
unsigned long getauxval(unsigned long);
void *mapanon(size_t) attributeallocsize((1));
int setjmp(jmp_buf) libcesque returnstwice paramsnonnull();
void longjmp(jmp_buf, int) libcesque wontreturn paramsnonnull();
axdx_t setlongerjmp(jmp_buf) libcesque returnstwice paramsnonnull();
void longerjmp(jmp_buf, intptr_t) libcesque wontreturn paramsnonnull();
int _setjmp(jmp_buf) libcesque returnstwice paramsnonnull();
void _longjmp(jmp_buf, int) libcesque wontreturn paramsnonnull();
void exit(int) wontreturn;
void _exit(int) libcesque wontreturn;
void _Exit(int) libcesque wontreturn;
void _Exit1(int) libcesque wontreturn;
void quick_exit(int) wontreturn;
void abort(void) wontreturn noinstrument;
int __cxa_atexit(void *, void *, void *) libcesque;
int atfork(void *, void *) libcesque;
int atexit(void (*)(void)) libcesque;
char *getenv(const char *) paramsnonnull() nosideeffect libcesque;
int putenv(char *) paramsnonnull();
int setenv(const char *, const char *, int) paramsnonnull();
int unsetenv(const char *);
int clearenv(void);
void fpreset(void);
int issetugid(void);
void *mmap(void *, uint64_t, int32_t, int32_t, int32_t, int64_t);
void *mremap(void *, size_t, size_t, int, ...);
int munmap(void *, uint64_t);
int mprotect(void *, uint64_t, int) privileged;
int msync(void *, size_t, int);
void *sbrk(intptr_t);
int brk(void *);
long fpathconf(int, int);
long pathconf(const char *, int);
int getgroups(int, uint32_t[]);
long gethostid(void);
int sethostid(long);
char *getlogin(void);
int getlogin_r(char *, size_t);
int lchown(const char *, uint32_t, uint32_t);
int getpagesize(void);
int syncfs(int);
int vhangup(void);
int getdtablesize(void);
int sethostname(const char *, size_t);
int acct(const char *);
void longsort(long *, size_t);

bool _isheap(void *);
int NtGetVersion(void) pureconst;
long missingno();
void __oom_hook(size_t);
void _loadxmm(void *);
void _peekall(void);
void _savexmm(void *);
void _weakfree(void *);
void free_s(void *) paramsnonnull() libcesque;
int close_s(int *) paramsnonnull() libcesque;
int OpenExecutable(void);
int ftrace_install(void);
long GetResourceLimit(int);
long GetMaxFd(void);
char *GetProgramExecutableName(void);
char *GetInterpreterExecutableName(char *, size_t);
void __printargs(const char *);
void __paginate(int, const char *);
int __arg_max(void);
void __morph_begin(void);
void __morph_end(void);
unsigned char *GetFirstInstruction(void);
unsigned char *GetInstructionLengths(void);
void __print_maps(void);
void __warn_if_powersave(void);

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_RUNTIME_RUNTIME_H_ */
