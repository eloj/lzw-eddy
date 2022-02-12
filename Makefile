ARCH:=x86-64-v3
OPT=-O3 -fomit-frame-pointer -funroll-loops -fstrict-aliasing -march=$(ARCH) -mtune=native -msse4.2 -mavx
LTOFLAGS=-flto -fno-fat-lto-objects -fuse-linker-plugin
WARNFLAGS=-Wall -Wextra -Wshadow -Wstrict-aliasing -Wcast-qual -Wcast-align -Wpointer-arith -Wredundant-decls -Wfloat-equal -Wswitch-enum
CWARNFLAGS=-Wstrict-prototypes -Wmissing-prototypes
MISCFLAGS=-fstack-protector -fvisibility=hidden
DEVFLAGS=-Wno-unused-parameter -Wno-unused-variable

YELLOW='\033[1;33m'
NC='\033[0m'

ifdef MEMCHECK
	TEST_PREFIX:=valgrind --tool=memcheck --leak-check=full --track-origins=yes
endif

ifdef PERF
	TEST_PREFIX:=perf stat
endif

ifndef OPTIMIZED
	MISCFLAGS+=-g -DDEBUG $(DEVFLAGS)
else
	MISCFLAGS+=-DNDEBUG
endif

CFLAGS=-std=c11 $(OPT) $(CWARNFLAGS) $(WARNFLAGS) $(MISCFLAGS)
CXXFLAGS=-std=gnu++17 -fno-rtti $(OPT) $(WARNFLAGS) $(MISCFLAGS)

.PHONY: clean test fuzz

all: lzw-eddy

lzw-eddy: lzw-eddy.c lzw.h
	$(CC) $(CFLAGS) $< -o $@

afl-%: fuzzing/afl_*.c lzw.h
	afl-gcc $(CFLAGS) -I. fuzzing/afl_$(subst -,_,$*).c -o $@

fuzz-%:
	make afl-$*
	AFL_SKIP_CPUFREQ=1 afl-fuzz -i tests -o findings -- ./afl-$*

fuzz: fuzz-roundtrip-driver

test: lzw-eddy
	${TEST_PREFIX} ./run-tests.sh

backup:
	@echo -e $(YELLOW)Making backup$(NC)
	tar -cJf ../$(notdir $(CURDIR))-`date +"%Y-%m"`.tar.xz ../$(notdir $(CURDIR))

clean:
	@echo -e $(YELLOW)Cleaning$(NC)
	rm -f lzw-eddy afl-*-driver core core.*
