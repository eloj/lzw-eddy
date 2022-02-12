/*
	Decompression console driver for use with afl-fuzz.
*/
#include <unistd.h>
#include <stdio.h>

#define LZW_EDDY_IMPLEMENTATION
#include "lzw.h"

int main(int argc, char *argv[]) {
	struct lzw_state state = { };
	uint8_t dest[2048];
	unsigned char buf[4096];

#ifdef __clang_major__
	while (__AFL_LOOP(1000)) {
#endif
		memset(&state, 0, sizeof(state));
		memset(buf, 0, sizeof(buf));

		ssize_t slen = read(0, buf, sizeof(buf));

		ssize_t res, written = 0;
		while ((res = lzw_decompress(&state, buf, slen, dest, sizeof(dest))) > 0) {
			written += res;
		}
		printf("decompressed:%zd (res=%zd)\n", written, res);
#ifdef __clang_major__
	}
#endif
	return EXIT_SUCCESS;
}
