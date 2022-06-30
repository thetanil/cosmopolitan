#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

PKGS += EXAMPLES

ifeq ($(MODE),tiny)
EXAMPLES_BOOTLOADER = $(CRT) $(APE)
else
EXAMPLES_BOOTLOADER = $(CRT) $(APE_NO_MODIFY_SELF)
endif

EXAMPLES_FILES := $(wildcard examples/*)
EXAMPLES_MAINS_S = $(filter %.S,$(EXAMPLES_FILES))
EXAMPLES_MAINS_C = $(filter %.c,$(EXAMPLES_FILES))
EXAMPLES_MAINS_CC = $(filter %.cc,$(EXAMPLES_FILES))

EXAMPLES_SRCS =									\
	$(EXAMPLES_MAINS_S)							\
	$(EXAMPLES_MAINS_C)							\
	$(EXAMPLES_MAINS_CC)

EXAMPLES_MAINS =								\
	$(EXAMPLES_MAINS_S)							\
	$(EXAMPLES_MAINS_C)							\
	$(EXAMPLES_MAINS_CC)

EXAMPLES_OBJS =									\
	$(EXAMPLES_MAINS_S:%.S=o/$(MODE)/%.o)					\
	$(EXAMPLES_MAINS_C:%.c=o/$(MODE)/%.o)					\
	$(EXAMPLES_MAINS_CC:%.cc=o/$(MODE)/%.o)

EXAMPLES_COMS =									\
	$(EXAMPLES_MAINS_S:%.S=o/$(MODE)/%.com)					\
	$(EXAMPLES_MAINS_C:%.c=o/$(MODE)/%.com)					\
	$(EXAMPLES_MAINS_CC:%.cc=o/$(MODE)/%.com)

EXAMPLES_BINS =									\
	$(EXAMPLES_COMS)							\
	$(EXAMPLES_COMS:%=%.dbg)

EXAMPLES_DIRECTDEPS =								\
	DSP_CORE								\
	DSP_SCALE								\
	DSP_TTY									\
	LIBC_ALG								\
	LIBC_BITS								\
	LIBC_CALLS								\
	LIBC_DNS								\
	LIBC_FMT								\
	LIBC_INTRIN								\
	LIBC_LOG								\
	LIBC_MEM								\
	LIBC_NEXGEN32E								\
	LIBC_NT_IPHLPAPI							\
	LIBC_NT_KERNEL32							\
	LIBC_NT_NTDLL								\
	LIBC_NT_USER32								\
	LIBC_NT_WS2_32								\
	LIBC_NT_ADVAPI32							\
	LIBC_RAND								\
	LIBC_RUNTIME								\
	LIBC_SOCK								\
	LIBC_STDIO								\
	LIBC_STR								\
	LIBC_STUBS								\
	LIBC_SYSV								\
	LIBC_SYSV_CALLS								\
	LIBC_TESTLIB								\
	LIBC_THREAD								\
	LIBC_TIME								\
	LIBC_TINYMATH								\
	LIBC_UNICODE								\
	LIBC_X									\
	LIBC_ZIPOS								\
	NET_HTTP								\
	NET_HTTPS								\
	THIRD_PARTY_COMPILER_RT							\
	THIRD_PARTY_DLMALLOC							\
	THIRD_PARTY_GDTOA							\
	THIRD_PARTY_GETOPT							\
	THIRD_PARTY_LIBCXX							\
	THIRD_PARTY_LINENOISE							\
	THIRD_PARTY_LUA								\
	THIRD_PARTY_MBEDTLS							\
	THIRD_PARTY_MUSL							\
	THIRD_PARTY_QUICKJS							\
	THIRD_PARTY_STB								\
	THIRD_PARTY_XED								\
	THIRD_PARTY_ZLIB							\
	TOOL_BUILD_LIB								\
	TOOL_VIZ_LIB

EXAMPLES_DEPS :=								\
	$(call uniq,$(foreach x,$(EXAMPLES_DIRECTDEPS),$($(x))))

o/$(MODE)/examples/examples.pkg:						\
		$(EXAMPLES_OBJS)						\
		$(foreach x,$(EXAMPLES_DIRECTDEPS),$($(x)_A).pkg)

o/$(MODE)/examples/unbourne.o:							\
		OVERRIDE_CPPFLAGS +=						\
			-DSTACK_FRAME_UNLIMITED					\
			-fpie

o/$(MODE)/examples/%.com.dbg:							\
		$(EXAMPLES_DEPS)						\
		o/$(MODE)/examples/%.o						\
		o/$(MODE)/examples/examples.pkg					\
		$(EXAMPLES_BOOTLOADER)
	@$(APELINK)

o/$(MODE)/examples/nomodifyself.com.dbg:					\
		$(EXAMPLES_DEPS)						\
		o/$(MODE)/examples/nomodifyself.o				\
		o/$(MODE)/examples/examples.pkg					\
		$(CRT)								\
		$(APE_NO_MODIFY_SELF)
	@$(APELINK)

o/$(MODE)/examples/hellolua.com.dbg:						\
		$(EXAMPLES_DEPS)						\
		o/$(MODE)/examples/hellolua.o					\
		o/$(MODE)/examples/hellolua.lua.zip.o				\
		o/$(MODE)/examples/examples.pkg					\
		$(EXAMPLES_BOOTLOADER)
	@$(APELINK)

o/$(MODE)/examples/ispell.com.dbg:						\
		$(EXAMPLES_DEPS)						\
		o/$(MODE)/examples/ispell.o					\
		o/$(MODE)/usr/share/dict/words.zip.o				\
		o/$(MODE)/examples/examples.pkg					\
		$(EXAMPLES_BOOTLOADER)
	@$(APELINK)

o/$(MODE)/examples/nesemu1.com.dbg:						\
		$(EXAMPLES_DEPS)						\
		o/$(MODE)/examples/nesemu1.o					\
		o/$(MODE)/usr/share/rom/mario.nes.zip.o				\
		o/$(MODE)/usr/share/rom/zelda.nes.zip.o				\
		o/$(MODE)/usr/share/rom/tetris.nes.zip.o			\
		o/$(MODE)/examples/examples.pkg					\
		$(EXAMPLES_BOOTLOADER)
	@$(APELINK)

o/$(MODE)/examples/picol.o:					\
		OVERRIDE_CPPFLAGS +=				\
			-DSTACK_FRAME_UNLIMITED

o/$(MODE)/examples/picol.com.dbg:				\
		$(EXAMPLES_DEPS)				\
		o/$(MODE)/examples/picol.o			\
		o/$(MODE)/examples/examples.pkg			\
		$(CRT)						\
		$(APE_NO_MODIFY_SELF)
	@$(APELINK)

o/$(MODE)/examples/nesemu1.com:							\
		o/$(MODE)/examples/nesemu1.com.dbg				\
		o/$(MODE)/third_party/zip/zip.com				\
		o/$(MODE)/tool/build/symtab.com
	@$(COMPILE) -AOBJCOPY -T$@ $(OBJCOPY) -S -O binary $< $@
	@$(COMPILE) -ASYMTAB o/$(MODE)/tool/build/symtab.com			\
		-o o/$(MODE)/examples/.nesemu1/.symtab $<
	@$(COMPILE) -AZIP -T$@ o/$(MODE)/third_party/zip/zip.com -9qj $@	\
		o/$(MODE)/examples/.nesemu1/.symtab

o/$(MODE)/examples/nesemu1.o: QUOTA += -M512m
o/$(MODE)/usr/share/dict/words.zip.o: ZIPOBJ_FLAGS += -C2

$(EXAMPLES_OBJS): examples/examples.mk

o/$(MODE)/usr/share/dict/words:							\
		usr/share/dict/words.gz						\
		o/$(MODE)/tool/build/gzip.com
	@$(MKDIR) $(@D)
	@o/$(MODE)/tool/build/gzip.com $(ZFLAGS) -cd <$< >$@

################################################################################

.PHONY: o/$(MODE)/examples
o/$(MODE)/examples:								\
		o/$(MODE)/examples/package					\
		o/$(MODE)/examples/pylife					\
		o/$(MODE)/examples/pyapp					\
		$(EXAMPLES_BINS)
