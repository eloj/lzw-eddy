/*
	Roundtrip input->compress->decompress console driver for use with afl-fuzz.

	This driver takes input, compresses it, then decompresses it, and
	then re-compresses it, checking that returned lengths and contents
	of input and output buffers agree.
*/
#include <unistd.h>
#include <stdio.h>

#include "lzw.c"

int main(int argc, char *argv[]) {
	uint8_t buf[4096];

	struct lzw_state statec0 = { };
	struct lzw_state stated0 = { };
	size_t dest_size = 1UL << 19; // 512KiB
	uint8_t *decomp = malloc(dest_size*2);
	uint8_t *comp = decomp + dest_size;

#ifdef __clang_major__
	while (__AFL_LOOP(1000)) {
#endif
		memset(&statec0, 0, sizeof(struct lzw_state));
		memset(&stated0, 0, sizeof(struct lzw_state));
		memset(buf, 0, sizeof(buf));

		ssize_t res;
		size_t comp_size = 0;
		size_t decomp_size = 0;

		ssize_t slen = read(0, buf, sizeof(buf));
		if (slen > 0) {
			// Compress input from fuzzer.
			while ((res = lzw_compress(&statec0, buf, slen, comp, dest_size)) > 0) { comp_size += res; };
			printf("compressed:%zd (res=%zd)\n", comp_size, res);
			if (res < 0) {
				abort();
			}

			// Decompress the compressed data...
			while ((res = lzw_decompress(&stated0, comp, comp_size, decomp, dest_size)) > 0) { decomp_size += res; };
			printf("decompressed:%zd (res=%zd)\n", decomp_size, res);
			if (res < 0) {
				abort();
			}

			// Verify input size vs decompressed size.
			if (slen != (ssize_t)decomp_size) {
				abort();
			}

			// Compare the decompressed data and the original input; should match obviously.
			int comp0 = memcmp(buf, decomp, slen);
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
