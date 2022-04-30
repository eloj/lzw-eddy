/*
	Compression console driver for use with afl-fuzz (fast mode)
*/
#include <unistd.h>
#include <stdio.h>

#define LZW_EDDY_IMPLEMENTATION
#include "lzw.h"

/* this lets the source compile without afl-clang-fast/lto */
#ifndef __AFL_FUZZ_TESTCASE_LEN

ssize_t       fuzz_len;
unsigned char fuzz_buf[1024000];

#define __AFL_FUZZ_TESTCASE_LEN fuzz_len
#define __AFL_FUZZ_TESTCASE_BUF fuzz_buf
#define __AFL_FUZZ_INIT() void sync(void);
#define __AFL_LOOP(x) \
	((fuzz_len = read(0, fuzz_buf, sizeof(fuzz_buf))) > 0 ? 1 : 0)
#define __AFL_INIT() sync()
#endif

__AFL_FUZZ_INIT();

#ifdef __clang__
#pragma clang optimize off
#else
#pragma GCC optimize("O0")
#endif

int main(int argc, char *argv[]) {
	struct lzw_state state;
	uint8_t dest[2048];

	uint8_t *input = __AFL_FUZZ_TESTCASE_BUF;

#ifdef __clang_major__
	while (__AFL_LOOP(1000)) {
#endif
		memset(&state, 0, sizeof(state));

		ssize_t slen = __AFL_FUZZ_TESTCASE_LEN;
		if (slen > 0) {
			ssize_t res, written = 0;
			while ((res = lzw_compress(&state, input, slen, dest, sizeof(dest))) > 0) {
				written += res;
			}
			printf("compressed:%zd (res=%zd)\n", written, res);
		}
#ifdef __clang_major__
	}
#endif
	return EXIT_SUCCESS;
}
