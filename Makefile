OPT=-O3 -fomit-frame-pointer -funroll-loops -fstrict-aliasing -march=native -mtune=native -msse4.2 -mavx
LTOFLAGS=-flto -fno-fat-lto-objects -fuse-linker-plugin
WARNFLAGS=-Wall -Wextra -Wshadow -Wstrict-aliasing -Wcast-qual -Wcast-align -Wpointer-arith -Wredundant-decls -Wfloat-equal -Wswitch-enum
CWARNFLAGS=-Wstrict-prototypes -Wmissing-prototypes
MISCFLAGS=-fstack-protector -fvisibility=hidden -D_GNU_SOURCE=1
DEVFLAGS=-Wno-unused-parameter -Wno-unused-variable

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

lzw-eddy: lzw-eddy.c lzw.c lzw.h
	$(CC) $(CFLAGS) $< -o $@

afl-%: fuzzing/afl_*.c lzw.c lzw.h
	afl-gcc $(CFLAGS) -I. fuzzing/afl_$(subst -,_,$*).c -o $@

fuzz-%:
	make afl-$*
	AFL_SKIP_CPUFREQ=1 afl-fuzz -i tests -o findings -- ./afl-$*

fuzz: fuzz-roundtrip-driver

test: lzw-eddy
	${TEST_PREFIX} ./run-tests.sh

backup:
	tar -cJf ../eddy-lzw-`date +"%Y-%m"`.tar.xz ../lzw-eddy

clean:
	rm -f lzw-eddy afl-*-driver core core.*
