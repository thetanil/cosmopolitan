#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘
#
# SYNOPSIS
#
#   System Call Compatibility Layer
#
# DESCRIPTION
#
#   This package exports a familiar system call interface that works
#   across platforms.

PKGS += LIBC_CALLS

LIBC_CALLS_ARTIFACTS += LIBC_CALLS_A
LIBC_CALLS = $(LIBC_CALLS_A_DEPS) $(LIBC_CALLS_A)
LIBC_CALLS_A = o/$(MODE)/libc/calls/syscalls.a
LIBC_CALLS_A_FILES :=					\
	$(wildcard libc/calls/typedef/*)		\
	$(wildcard libc/calls/struct/*)			\
	$(wildcard libc/calls/*)
LIBC_CALLS_A_HDRS = $(filter %.h,$(LIBC_CALLS_A_FILES))
LIBC_CALLS_A_INCS = $(filter %.inc,$(LIBC_CALLS_A_FILES))
LIBC_CALLS_A_SRCS_S = $(filter %.S,$(LIBC_CALLS_A_FILES))
LIBC_CALLS_A_SRCS_C = $(filter %.c,$(LIBC_CALLS_A_FILES))

LIBC_CALLS_A_SRCS =					\
	$(LIBC_CALLS_A_SRCS_S)				\
	$(LIBC_CALLS_A_SRCS_C)

LIBC_CALLS_A_OBJS =					\
	$(LIBC_CALLS_A_SRCS_S:%.S=o/$(MODE)/%.o)	\
	$(LIBC_CALLS_A_SRCS_C:%.c=o/$(MODE)/%.o)

LIBC_CALLS_A_CHECKS =					\
	$(LIBC_CALLS_A).pkg				\
	$(LIBC_CALLS_A_HDRS:%=o/$(MODE)/%.ok)

LIBC_CALLS_A_DIRECTDEPS =				\
	LIBC_FMT					\
	LIBC_INTRIN					\
	LIBC_NEXGEN32E					\
	LIBC_NT_ADVAPI32				\
	LIBC_NT_IPHLPAPI				\
	LIBC_NT_KERNEL32				\
	LIBC_NT_NTDLL					\
	LIBC_NT_PDH					\
	LIBC_NT_PSAPI					\
	LIBC_NT_POWRPROF				\
	LIBC_NT_WS2_32					\
	LIBC_STR					\
	LIBC_STUBS					\
	LIBC_SYSV_CALLS					\
	LIBC_SYSV

LIBC_CALLS_A_DEPS :=					\
	$(call uniq,$(foreach x,$(LIBC_CALLS_A_DIRECTDEPS),$($(x))))

$(LIBC_CALLS_A):					\
		libc/calls/				\
		$(LIBC_CALLS_A).pkg			\
		$(LIBC_CALLS_A_OBJS)

$(LIBC_CALLS_A).pkg:					\
		$(LIBC_CALLS_A_OBJS)			\
		$(foreach x,$(LIBC_CALLS_A_DIRECTDEPS),$($(x)_A).pkg)

# we can't use asan because:
#   ucontext_t memory is owned by xnu kernel
o/$(MODE)/libc/calls/sigenter-xnu.o:			\
		OVERRIDE_COPTS +=			\
			-ffreestanding			\
			-fno-sanitize=address

# we can't use asan because:
#   vdso memory is owned by linux kernel
o/$(MODE)/libc/calls/vdsofunc.greg.o:			\
		OVERRIDE_COPTS +=			\
			-ffreestanding			\
			-fno-sanitize=address

# we can't use asan because:
#   asan guard pages haven't been allocated yet
o/$(MODE)/libc/calls/directmap.o			\
o/$(MODE)/libc/calls/directmap-nt.o:			\
		OVERRIDE_COPTS +=			\
			-ffreestanding			\
			-fno-sanitize=address

# we can't use asan because:
#   ntspawn allocates 128kb of heap memory via win32
o/$(MODE)/libc/calls/ntspawn.o				\
o/$(MODE)/libc/calls/mkntcmdline.o			\
o/$(MODE)/libc/calls/mkntenvblock.o:			\
		OVERRIDE_COPTS +=			\
			-ffreestanding			\
			-fno-sanitize=address

# we can't use sanitizers because:
#   windows owns the data structure
o/$(MODE)/libc/calls/wincrash.o				\
o/$(MODE)/libc/calls/ntcontext2linux.o:			\
		OVERRIDE_COPTS +=			\
			-fno-sanitize=all

# we always want -O3 because:
#   it makes the code size smaller too
o/$(MODE)/libc/calls/sigenter-freebsd.o			\
o/$(MODE)/libc/calls/sigenter-netbsd.o			\
o/$(MODE)/libc/calls/sigenter-openbsd.o			\
o/$(MODE)/libc/calls/sigenter-xnu.o			\
o/$(MODE)/libc/calls/ntcontext2linux.o:			\
		OVERRIDE_COPTS +=			\
			-O3

# we must disable static stack safety because:
#   these functions use alloca(n)
o/$(MODE)/libc/calls/execl.o				\
o/$(MODE)/libc/calls/execle.o				\
o/$(MODE)/libc/calls/execlp.o				\
o/$(MODE)/libc/calls/execve-sysv.o			\
o/$(MODE)/libc/calls/execve-nt.greg.o			\
o/$(MODE)/libc/calls/mkntenvblock.o:			\
		OVERRIDE_CPPFLAGS +=			\
			-DSTACK_FRAME_UNLIMITED

# we must disable static stack safety because:
#   PATH_MAX*sizeof(char16_t)*2 exceeds 4096 byte frame limit
o/$(MODE)/libc/calls/copyfile.o				\
o/$(MODE)/libc/calls/symlinkat-nt.o			\
o/$(MODE)/libc/calls/readlinkat-nt.o			\
o/$(MODE)/libc/calls/linkat-nt.o			\
o/$(MODE)/libc/calls/renameat-nt.o:			\
		OVERRIDE_CPPFLAGS +=			\
			-DSTACK_FRAME_UNLIMITED

# we must segregate codegen because:
#   file contains multiple independently linkable apis
o/$(MODE)/libc/calls/ioctl-siocgifconf.o		\
o/$(MODE)/libc/calls/ioctl-siocgifconf-nt.o:		\
		OVERRIDE_COPTS +=			\
			-ffunction-sections		\
			-fdata-sections

# we want small code size because:
#   to keep .text.head under 4096 bytes
o/$(MODE)/libc/calls/mman.greg.o:			\
		OVERRIDE_COPTS +=			\
			-Os

# we always want -Os because:
#   va_arg codegen is very bloated in default mode
o//libc/calls/open.o					\
o//libc/calls/openat.o					\
o//libc/calls/prctl.o					\
o//libc/calls/ioctl.o					\
o//libc/calls/ioctl_default.o				\
o//libc/calls/ioctl_fioclex-nt.o			\
o//libc/calls/ioctl_fioclex.o				\
o//libc/calls/ioctl_siocgifconf-nt.o			\
o//libc/calls/ioctl_siocgifconf.o			\
o//libc/calls/ioctl_tcgets-nt.o				\
o//libc/calls/ioctl_tcgets.o				\
o//libc/calls/ioctl_tcsets-nt.o				\
o//libc/calls/ioctl_tcsets.o				\
o//libc/calls/ioctl_tiocgwinsz-nt.o			\
o//libc/calls/ioctl_tiocgwinsz.o			\
o//libc/calls/ioctl_tiocswinsz-nt.o			\
o//libc/calls/ioctl_tiocswinsz.o			\
o//libc/calls/fcntl.o:					\
		OVERRIDE_CFLAGS +=			\
			-Os

# we always want -Os because:
#   it's early runtime mandatory and quite huge without it
o//libc/calls/getcwd.greg.o				\
o//libc/calls/getcwd-nt.greg.o				\
o//libc/calls/getcwd-xnu.greg.o:			\
		OVERRIDE_CFLAGS +=			\
			-Os

o/$(MODE)/libc/calls/pledge.o				\
o/$(MODE)/libc/calls/unveil.o:				\
		OVERRIDE_CFLAGS +=			\
			-DSTACK_FRAME_UNLIMITED

LIBC_CALLS_LIBS = $(foreach x,$(LIBC_CALLS_ARTIFACTS),$($(x)))
LIBC_CALLS_SRCS = $(foreach x,$(LIBC_CALLS_ARTIFACTS),$($(x)_SRCS))
LIBC_CALLS_HDRS = $(foreach x,$(LIBC_CALLS_ARTIFACTS),$($(x)_HDRS))
LIBC_CALLS_INCS = $(foreach x,$(LIBC_CALLS_ARTIFACTS),$($(x)_INCS))
LIBC_CALLS_BINS = $(foreach x,$(LIBC_CALLS_ARTIFACTS),$($(x)_BINS))
LIBC_CALLS_CHECKS = $(foreach x,$(LIBC_CALLS_ARTIFACTS),$($(x)_CHECKS))
LIBC_CALLS_OBJS = $(foreach x,$(LIBC_CALLS_ARTIFACTS),$($(x)_OBJS))
LIBC_CALLS_TESTS = $(foreach x,$(LIBC_CALLS_ARTIFACTS),$($(x)_TESTS))

.PHONY: o/$(MODE)/libc/calls
o/$(MODE)/libc/calls: $(LIBC_CALLS_CHECKS)
