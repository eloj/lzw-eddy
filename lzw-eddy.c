#define LZW_EDDY_IMPLEMENTATION
// #define LZW_MAX_CODE_WIDTH 14
#include "lzw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>

#include "build_const.h"

static const char *infile;
static const char *outfile;
static int compress = 0;
static int use_zheader = 0; // Note: We're not 'compress' compatible!
static size_t maxlen = 0;

static void print_version(void) {
	printf("%s <%.*s>\n", LZW_EDDY_VERSION, 8, build_hash);
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
					case 'Z':
						use_zheader = atoi(value);
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

static void lzw_compress_file(const char *srcfile, const char *destfile, int zheader) {
	FILE *ifile = fopen(srcfile, "rb");

	if (!ifile) {
		fprintf(stderr, "Error: %m\n");
		return;
	}
	fseek(ifile, 0, SEEK_END);
	long slen = ftell(ifile);
	fseek(ifile, 0, SEEK_SET);

	printf("Compressing %zu bytes.\n", (size_t)slen);
	FILE *ofile = fopen(destfile, "wb");
	if (ofile) {
		uint8_t *src = malloc(slen);
		if (!src) {
			fprintf(stderr, "ERROR: memory allocation of %ld bytes failed.\n", slen);
			exit(1);
		}
		uint8_t dest[4096];

		struct lzw_state state = { 0 };
		if (maxlen > 0) {
			state.longest_prefix_allowed = maxlen;
			printf("WARNING: Restricting maximum prefix length to %zu.\n", state.longest_prefix_allowed);
		}

		if ((fread(src, slen, 1, ifile) != 1) && (ferror(ifile) != 0)) {
			fprintf(stderr, "fread '%s': %s", srcfile, strerror(errno));
			exit(EXIT_FAILURE);
		}

		ssize_t res, written = 0;

		if (zheader) {
			uint8_t Zheader[3] = { 0x1F, 0x9D, 0x80 | LZW_MAX_CODE_WIDTH };
			fwrite(Zheader, 3, 1, ofile);
			printf(".Z (compress) header written.\n");
			written += 3;
		}

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
	}
	fclose(ifile);
}

static void lzw_decompress_file(const char *srcfile, const char *destfile, int zheader) {
	FILE *ifile = fopen(srcfile, "rb");

	if (!ifile) {
		fprintf(stderr, "Error: %m\n");
		return;
	}
	fseek(ifile, 0, SEEK_END);
	long slen = ftell(ifile);
	fseek(ifile, 0, SEEK_SET);

	if (slen > 0) {
		printf("Decompressing %zu bytes.\n", (size_t)slen);
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
			uint8_t *src = malloc(slen);
			if (!src) {
				fprintf(stderr, "ERROR: memory allocation of %ld bytes failed.\n", slen);
				exit(1);
			}

			if ((fread(src, slen, 1, ifile) != 1) && (ferror(ifile) != 0)) {
				fprintf(stderr, "fread '%s': %s", srcfile, strerror(errno));
				exit(EXIT_FAILURE);
			}

			if (zheader) {
				if (src[0] != 0x1F || src[1] != 0x9D) {
					fprintf(stderr, "ERROR: compress .Z header not detected\n");
					exit(EXIT_FAILURE);
				}
				if ((src[2] & 0x0F) != LZW_MAX_CODE_WIDTH) {
					fprintf(stderr, "ERROR: compress .Z header settings mismatch: %d bits/code indicated, compiled with LZW_MAX_CODE_WIDTH %d\n", (src[3] & 0x0F), LZW_MAX_CODE_WIDTH);
					exit(EXIT_FAILURE);
				}
				if ((src[2] & 0xF0) != 0x80) {
					fprintf(stderr, "ERROR: compress .Z header settings mismatch: block mode not set, or unknown bits\n");
					exit(EXIT_FAILURE);
				}
				slen -= 3;
				printf(".Z (compress) header valid (LZW_MAX_CODE_WIDTH=%d)\n", LZW_MAX_CODE_WIDTH);
			}

			struct lzw_state state = { 0 };

			ssize_t res, written = 0;
			// Returns 0 when done, otherwise number of bytes written to destination buffer. On error, < 0.
			while ((res = lzw_decompress(&state, src + (zheader ? 3 : 0), slen, dest, dest_len)) > 0) {
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
		}
	}
	fclose(ifile);
}

int main(int argc, char *argv []) {
	parse_args(argc, argv);

	print_banner();

	if (!infile || !outfile) {
		printf("Usage: %s -c file|-d file -o outfile\n", argv[0]);
		printf("Compiled Configuration:\n LZW_MIN_CODE_WIDTH=%d, LZW_MAX_CODE_WIDTH=%d, LZW_MAX_CODES=%lu, sizeof(lzw_state)=%zu\n",
			LZW_MIN_CODE_WIDTH,
			LZW_MAX_CODE_WIDTH,
			LZW_MAX_CODES,
			sizeof(struct lzw_state)
		);
		return EXIT_SUCCESS;
	}

	if (compress) {
		lzw_compress_file(infile, outfile, use_zheader);
	} else {
		lzw_decompress_file(infile, outfile, use_zheader);
	}

	return EXIT_SUCCESS;
}
