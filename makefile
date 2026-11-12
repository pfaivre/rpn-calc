CC		= gcc
SRCDIR	= src
OBJDIR	= $(SRCDIR)
OBJS	= $(addprefix $(OBJDIR)/, main.o calc.o stack.o io.o registers.o number.o tokenize.o)
STD		= c23
CFLAGS	= -c -g -Wall -pedantic -std=$(STD) -MMD -MP -D_FORTIFY_SOURCE=3 -O2
LDFLAGS	= -lgmp
PGM		= rpn

# Cross-compile a Windows binary from Linux (needs mingw64-gcc and mingw64-gmp,
# e.g. dnf install mingw64-gcc mingw64-gmp). Objects go to their own directory
# so they never clash with a native build's. Links statically when the MinGW
# sysroot has a static libgmp.a (MSYS2 does; Fedora's mingw64-gmp is DLL-only,
# in which case rpn.exe needs libgmp-10.dll next to it)
MINGW		= x86_64-w64-mingw32-gcc
MINGW_GMP_A	:= $(shell $(MINGW) -print-sysroot 2>/dev/null)/mingw/lib/libgmp.a
WINOBJDIR	= build-windows
ifeq ($(wildcard $(MINGW_GMP_A)),)
WINDOWS_LDFLAGS	= -lgmp
else
WINDOWS_LDFLAGS	= -lgmp -static
endif

# On Windows (MSYS2/MinGW-w64 make sets OS=Windows_NT): build a statically
# linked rpn.exe, so it runs without the MinGW runtime or GMP DLLs
ifeq ($(OS),Windows_NT)
PGM		= rpn.exe
LDFLAGS	+= -static
endif

.PHONY:	all clean windows dist check valgrind test bench

# Build

all:	$(PGM)

$(PGM):	$(OBJS)
		$(CC) -o $(PGM) $(OBJS) $(LDFLAGS)

$(OBJDIR)/%.o:	$(SRCDIR)/%.c | $(OBJDIR)
		$(CC) $(CFLAGS) -o $@ $<

$(OBJDIR):
		mkdir -p $(OBJDIR)

-include $(OBJS:.o=.d)

# Distribution

windows:
		$(MAKE) CC=$(MINGW) PGM=rpn.exe OBJDIR=$(WINOBJDIR) LDFLAGS="$(WINDOWS_LDFLAGS)" rpn.exe

# Bundle the binary with the examples into dist/ for distribution. The
# release workflow archives dist/ per target as rpn-<target>.tar.gz (Linux)
# or rpn-<target>.zip (Windows); inside the archive the binary keeps its
# plain name (rpn / rpn.exe) with examples/ alongside it
dist:	$(PGM)
		rm -rf dist
		mkdir -p dist
		cp $(PGM) dist/
		cp -r examples dist/

# Housekeeping

clean:
		rm -f $(PGM) rpn rpn.exe
		rm -f $(OBJS) $(OBJS:.o=.d)
		rm -f $(WINOBJDIR)/*.o $(WINOBJDIR)/*.d
		rmdir $(WINOBJDIR) 2>/dev/null || true
		rm -rf dist

# For development

check:
		cppcheck -q --error-exitcode=1 $(SRCDIR)/*.c $(SRCDIR)/*.h

valgrind: $(PGM)
		valgrind ./$(PGM)

test: $(PGM)
		bash ./tests/functional_tests.sh
		bash ./tests/adversarial_tests.sh
		bash ./tests/dc_compliance_tests.sh

# Performance comparison against dc (needs dc installed; BENCH_N=1 for a quick run)
bench: $(PGM)
		bash ./tests/bench.sh
