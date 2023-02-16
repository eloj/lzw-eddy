/*
	Roundtrip input->compress->decompress driver for use with afl-fuzz (fast mode)

	This driver takes input, compresses it, then decompresses it, and
	then re-compresses it, checking that returned lengths and contents
	of input and output buffers agree.
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
	struct lzw_state statec0;
	struct lzw_state stated0;
	size_t dest_size = 1UL << 19; // 512KiB
	uint8_t *decomp = malloc(dest_size*2);
	uint8_t *comp = decomp + dest_size;

#ifdef __AFL_HAVE_MANUAL_CONTROL
	__AFL_INIT();
#endif

	uint8_t *input = __AFL_FUZZ_TESTCASE_BUF;

#ifdef __clang_major__
	while (__AFL_LOOP(5000)) {
#endif
		memset(&statec0, 0, sizeof(struct lzw_state));
		memset(&stated0, 0, sizeof(struct lzw_state));

		ssize_t res;
		size_t comp_size = 0;
		size_t decomp_size = 0;

		size_t slen = __AFL_FUZZ_TESTCASE_LEN;
		if (input && slen > 0) {
			// Compress input from fuzzer.
			while ((res = lzw_compress(&statec0, input, slen, comp, dest_size)) > 0) { comp_size += res; };
			printf("compressed:%zu (res=%zd)\n", comp_size, res);
			if (res < 0) {
				abort();
			}

			// Decompress the compressed data...
			while ((res = lzw_decompress(&stated0, comp, comp_size, decomp, dest_size)) > 0) { decomp_size += res; };
			printf("decompressed:%zu (res=%zd)\n", decomp_size, res);
			if (res < 0) {
				abort();
			}

			// Verify input size vs decompressed size.
			if (slen != decomp_size) {
				abort();
			}

			// Compare the decompressed data and the original input; should match obviously.
			int comp0 = memcmp(input, decomp, slen);
			if (comp0 != 0) {
				abort();
			}
		}

#ifdef __clang_major__
	}
#endif
	free(decomp);
	return EXIT_SUCCESS;
}
