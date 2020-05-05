#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "lzw_decompress.c"

static const char *infile;
static const char *outfile;
static int compress = 0;

static int parse_args(int argc, char **argv) {
	// TODO: just use getopt.h?
	for (int i=1 ; i < argc ; ++i) {
		const char *arg = argv[i];
		// "argv[argc] shall be a null pointer", section 5.1.2.2.1
		const char *value = argv[i+1];

		if (*arg == '-') {
			++arg;
			if (value) {
				switch (*arg) {
					case 'c':
						compress = 1;
						infile = value;
						break;
					case 'd':
						/* fallthrough */
					case 'x':
						compress = 0;
						infile = value;
						break;
					case 'o':
						outfile = value;
						break;
				}
			}
		}
	}

	return 0;
}

static void lzw_decompress_file(const char *srcfile, const char *destfile) {
	FILE *ifile = fopen(srcfile, "rb");

	if (!ifile) {
		fprintf(stderr, "Error: %m\n");
		return;
	}
	fseek(ifile, 0, SEEK_END);
	long slen = ftell(ifile);
	fseek(ifile, 0, SEEK_SET);

	if (slen > 0) {
		size_t dlen = 8192;
		uint8_t *src = malloc(slen);
		uint8_t *dest = malloc(dlen);

		fread(src, slen, 1, ifile);

		printf("Decompressing %zu bytes.\n", slen);
		struct lzwd_state state = { };
		lzwd_init(&state);

		FILE *ofile = fopen(destfile, "wb");
		ssize_t res;
		while ((res = lzw_decompress(&state, src, slen, dest, dlen)) >= 0) {
			if (res == 0) {
				break;
			}
			// printf("res=%zd\n", res);
			fwrite(dest, res, 1, ofile);
		}
		if (res < 0) {
			fprintf(stderr, "Decompressor returned error %zd\n", res);
		}
		fclose(ofile);

		free(dest);
		free(src);
	}
	fclose(ifile);
}

int main(int argc, char *argv []) {

	parse_args(argc, argv);

	if (!infile || !outfile) {
		printf("Usage: %s -c file|-d file -o outfile\n", argv[0]);
		return EXIT_SUCCESS;
	}

	printf("Running lzw-eddy %s on file %s\n", compress ? "compression" : "decompression", infile);
	if (compress) {
		fprintf(stderr, "Compression not implemented.\n");
	} else {
		lzw_decompress_file(infile, outfile);
	}

	return EXIT_SUCCESS;
}
