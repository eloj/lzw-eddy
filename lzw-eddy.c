#define LZW_EDDY_IMPLEMENTATION
// #define LZW_MAX_CODE_WIDTH 14
#include "lzw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "build_const.h"

static const char *infile;
static const char *outfile;
static int compress = 0;
static size_t maxlen = 0;

static void print_version(void) {
	if (build_hash && *build_hash) {
		printf("%s <%.*s>\n", LZW_EDDY_VERSION, 8, build_hash);
	} else {
		printf("%s\n", LZW_EDDY_VERSION);
	}
}

static void print_banner(void) {
	printf("lzw-eddy ");
	print_version();
}

static int parse_args(int argc, char **argv) {
	for (int i=1 ; i < argc ; ++i) {
		const char *arg = argv[i];
		// "argv[argc] shall be a null pointer", section 5.1.2.2.1
		const char *value = argv[i+1];

		if (arg && *arg == '-') {
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
					case 'm':
						maxlen = atoi(value);
						break;
				}
			} else {
				if (*arg == 'v' || *arg == 'V' || strcmp(arg, "-version") == 0) {
					print_version();
					exit(0);
				}
			}
		}
	}

	return 0;
}

static int lzw_compress_file(const char *srcfile, const char *destfile) {
	FILE *ifile = fopen(srcfile, "rb");

	if (!ifile) {
		fprintf(stderr, "Error: %m\n");
		return -1;
	}

	fseek(ifile, 0, SEEK_END);
	long slen = ftell(ifile);
	fseek(ifile, 0, SEEK_SET);

	printf("Compressing %zu bytes.\n", (size_t)slen);

	uint8_t *src = malloc(slen + 1);
	if (!src) {
		fprintf(stderr, "ERROR: memory allocation of %ld bytes failed.\n", slen);
		return -1;
	}

	if ((fread(src, slen, 1, ifile) != 1) && (ferror(ifile) != 0)) {
		fprintf(stderr, "fread '%s': %s", srcfile, strerror(errno));
		return -1;
	}
	fclose(ifile);

	FILE *ofile = fopen(destfile, "wb");
	if (ofile) {
		uint8_t dest[4096];

		struct lzw_state state = { 0 };

		if (maxlen > 0) {
			state.longest_prefix_allowed = maxlen;
			printf("WARNING: Restricting maximum prefix length to %zu.\n", state.longest_prefix_allowed);
		}

		ssize_t res, written = 0;
		while ((res = lzw_compress(&state, src, slen, dest, sizeof(dest))) > 0) {
			fwrite(dest, res, 1, ofile);
			written += res;
		}
		if (res == 0) {
			printf("%zd bytes written to output, reduction=%2.02f%% (longest prefix=%zu).\n",
					written,
					(1.0f - ((float)written/slen)) * 100.0f,
					state.longest_prefix);
		} else if (res < 0) {
			fprintf(stderr, "Compression returned error: %s (err: %zd)\n", lzw_strerror(res), res);
		}
		fclose(ofile);
		free(src);
	} else {
		fprintf(stderr, "Error: %m\n");
		return -1;
	}
	return 0;
}

static int lzw_decompress_file(const char *srcfile, const char *destfile) {
	FILE *ifile = fopen(srcfile, "rb");

	if (!ifile) {
		fprintf(stderr, "Error: %m\n");
		return -1;
	}

	fseek(ifile, 0, SEEK_END);
	long slen = ftell(ifile);
	fseek(ifile, 0, SEEK_SET);

	if (slen < 3) {
		fprintf(stderr, "WARNING: Input file is too small, no output generated.\n");
		return -1;
	}

	printf("Decompressing %zu bytes.\n", (size_t)slen);

	uint8_t *src = malloc(slen);
	if (!src) {
		fprintf(stderr, "ERROR: memory allocation of %ld bytes failed.\n", slen);
		return -1;
	}

	if ((fread(src, slen, 1, ifile) != 1) && (ferror(ifile) != 0)) {
		fprintf(stderr, "fread '%s': %s", srcfile, strerror(errno));
		return -1;
	}
	fclose(ifile);

	FILE *ofile = stdout;
	if (strcmp(destfile, "-") != 0) {
		ofile = fopen(destfile, "wb");
	}
	if (ofile) {
		uint8_t dest[4096];
		size_t dest_len = sizeof(dest);
		if (maxlen > 0 && maxlen + 1 < dest_len) {
			dest_len = maxlen + 1;
			printf("WARNING: Restricting output buffer to %zu bytes.\n", dest_len);
		}

		struct lzw_state state = { 0 };

		ssize_t res, written = 0;
		// Returns 0 when done, otherwise number of bytes written to destination buffer. On error, < 0.
		while ((res = lzw_decompress(&state, src, slen, dest, dest_len)) > 0) {
			fwrite(dest, res, 1, ofile);
			written += res;
		}
		if (res == 0) {
			printf("%zd bytes written to output, expansion=%2.2f%% (longest prefix=%zu).\n",
				written,
				((float)written/slen - 1.0f) * 100.0f,
				state.longest_prefix);
		} else if (res < 0) {
			fprintf(stderr, "Decompression returned error: %s (err: %zd)\n", lzw_strerror(res), res);
		}
		fclose(ofile);
		free(src);
	} else {
		fprintf(stderr, "Error: %m\n");
		return -1;
	}
	return 0;
}

int main(int argc, char *argv []) {
	parse_args(argc, argv);

	print_banner();

	int res = 0;

	if (!infile || !outfile) {
		printf("Usage: %s -c file|-d file -o outfile\n", argv[0]);
		printf("Compiled Configuration:\n LZW_MIN_CODE_WIDTH=%d, LZW_MAX_CODE_WIDTH=%d, LZW_MAX_CODES=%lu, sizeof(lzw_state)=%zu\n",
			LZW_MIN_CODE_WIDTH,
			LZW_MAX_CODE_WIDTH,
			LZW_MAX_CODES,
			sizeof(struct lzw_state)
		);
	} else {
		if (compress) {
			res = lzw_compress_file(infile, outfile);
		} else {
			res = lzw_decompress_file(infile, outfile);
		}
	}

	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
