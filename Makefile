#ARCH:=x86-64-v3 ; github `ubuntu-latest` compiler too old. <sigh>
ARCH:=native
OPT=-O3 -fomit-frame-pointer -funroll-loops -fstrict-aliasing -march=$(ARCH) -mtune=native -msse4.2 -mavx
LTOFLAGS=-flto -fno-fat-lto-objects -fuse-linker-plugin
WARNFLAGS=-Wall -Wextra -Wshadow -Wstrict-aliasing -Wcast-qual -Wcast-align -Wpointer-arith -Wredundant-decls -Wfloat-equal -Wswitch-enum
CWARNFLAGS=-Wstrict-prototypes -Wmissing-prototypes
MISCFLAGS=-fstack-protector -fcf-protection -fvisibility=hidden
DEVFLAGS=-ggdb -DDEBUG -D_FORTIFY_SOURCE=3 -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function

AFLCC?=afl-clang-fast

YELLOW='\033[1;33m'
NC='\033[0m'

ifdef MEMCHECK
	TEST_PREFIX:=valgrind --tool=memcheck --leak-check=full --track-origins=yes
endif

ifdef PERF
	TEST_PREFIX:=perf stat
endif

ifdef OPTIMIZED
# On mingw, -static avoids dep on libssp-0.dll when built with -fstack-protector
	MISCFLAGS+=-DNDEBUG -Werror -static
else
	MISCFLAGS+=$(DEVFLAGS)
endif

BITWIDTH?=12

CFLAGS=-std=c11 $(OPT) $(CWARNFLAGS) $(WARNFLAGS) $(MISCFLAGS) -DLZW_MAX_CODE_WIDTH=$(BITWIDTH)
CXXFLAGS=-std=gnu++17 -fno-rtti $(OPT) $(WARNFLAGS) $(MISCFLAGS)

.PHONY: clean test fuzz

all: lzw-eddy

FORCE:

build_const.h: FORCE
	@git show-ref --head --hash | head -n 1 | awk '{ printf "const char *build_hash = \"%s\";\n",$$1 }' > $@.tmp
	@if test -r $@ ; then \
		(cmp $@.tmp $@ && rm $@.tmp) || mv -f $@.tmp $@ ; \
	else \
		mv $@.tmp $@ ; \
	fi

lzw-eddy: lzw-eddy.c lzw.h build_const.h
	$(CC) $(CFLAGS) $< -o $@

afl-%: fuzzing/afl_*.c lzw.h
	$(AFLCC) $(CFLAGS) -I. fuzzing/afl_$(subst -,_,$*).c -o $@

fuzz-%:
	make afl-$*
	AFL_AUTORESUME=1 AFL_SKIP_CPUFREQ=1 afl-fuzz -m 16 -i tests -o findings -- ./afl-$*

fuzz: fuzz-roundtrip-driver

test: lzw-eddy
	${TEST_PREFIX} ./run-tests.sh

backup:
	@echo -e $(YELLOW)Making backup$(NC)
	tar -cJf ../$(notdir $(CURDIR))-`date +"%Y-%m"`.tar.xz ../$(notdir $(CURDIR))

clean:
	@echo -e $(YELLOW)Cleaning$(NC)
	rm -f lzw-eddy build_const.h afl-*-driver core core.*
	rm -rf packages
