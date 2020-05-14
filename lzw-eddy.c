#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <err.h>

#include "lzw.c"

static const char *infile;
static const char *outfile;
static int compress = 0;
static size_t maxlen = 0;

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
					case 'm':
						maxlen = atoi(value);
						break;
				}
			}
		}
	}

	return 0;
}

static void lzw_compress_file(const char *srcfile, const char *destfile) {
	FILE *ifile = fopen(srcfile, "rb");

	if (!ifile) {
		fprintf(stderr, "Error: %m\n");
		return;
	}
	fseek(ifile, 0, SEEK_END);
	long slen = ftell(ifile);
	fseek(ifile, 0, SEEK_SET);

	printf("Compressing %zu bytes.\n", slen);
	FILE *ofile = fopen(destfile, "wb");
	if (ofile) {
		uint8_t *src = malloc(slen);
		uint8_t dest[4096];

		struct lzw_state state = { 0 };
		if (maxlen > 0) {
			state.longest_prefix_allowed = maxlen;
			printf("WARNING: Restricting maximum prefix length to %zu.\n", state.longest_prefix_allowed);
		}

		if ((fread(src, slen, 1, ifile) != 1) && (ferror(ifile) != 0)) {
			err(1, "fread: %s", srcfile);
		}

		ssize_t res, written = 0;
		while ((res = lzw_compress(&state, src, slen, dest, sizeof(dest))) > 0) {
			fwrite(dest, res, 1, ofile);
			written += res;
		}
		if (res == 0) {
			printf("%zd bytes written to output (longest prefix=%zu).\n", written, state.longest_prefix);
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
		printf("Decompressing %zu bytes.\n", slen);
		FILE *ofile = stdout;
		if (strcmp(destfile, "-") != 0) {
			ofile = fopen(destfile, "wb");
		}
		if (ofile) {
			size_t dest_len = 4096;
			if (maxlen > 0) {
				dest_len = maxlen + 1;
				printf("WARNING: Restricting output buffer to %zu bytes.\n", dest_len);
			}
			uint8_t *src = malloc(slen);
			uint8_t dest[dest_len];
			if ((fread(src, slen, 1, ifile) != 1) && (ferror(ifile) != 0)) {
				err(1, "fread: %s", srcfile);
			}

			struct lzw_state state = { 0 };

			ssize_t res, written = 0;
			// Returns 0 when done, otherwise number of bytes written to destination buffer. On error, < 0.
			while ((res = lzw_decompress(&state, src, slen, dest, sizeof(dest))) > 0) {
				fwrite(dest, res, 1, ofile);
				written += res;
			}
			if (res == 0) {
				printf("%zd bytes written to output (longest prefix=%zu).\n", written, state.longest_prefix);
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

	if (!infile || !outfile) {
		printf("Usage: %s -c file|-d file -o outfile\n", argv[0]);
		return EXIT_SUCCESS;
	}

	printf("lzw-eddy %s file %s\n", compress ? "compressing" : "decompressing", infile);
	if (compress) {
		lzw_compress_file(infile, outfile);
	} else {
		lzw_decompress_file(infile, outfile);
	}

	return EXIT_SUCCESS;
}
