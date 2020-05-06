
# Simple LZW (Lempel-Ziv-Welch) Decompressor

A basic headerless LZW decompressor. Supports variable length codes
between 9 and 12 bits per default. Up to 16-bits should work if the
`LZW_MAX_CODE_WIDTH` define is changed, but this is untested.

This is similar to what's used by GIF, but slightly less flexible.

The algorithm implemented by this code was widely distributed in the
old days in places like Dr.Dobbs and a popular book on compression,
which resulted in it being used in [all sorts of places](https://www.giantbomb.com/profile/eloj/blog/technical-notes-on-the-level-format-of-puzznic-for/114881/).

## Features

* No dynamic memory allocations in decompressor.
* Re-entrant design. Decompress into a fixed size buffer (minimum 4096 bytes recommended).
* No scratch buffer used when writing out strings.
* Valgrind clean.

## Usage example

```c
	#include "lzw_decompress.c"

	struct lzwd_state state = { 0 };

	size_t slen = <length of compressed data>
	uint8_t *src = <compressed data>;
	uint8_t dest[4096];

	ssize_t res, written = 0;
	while ((res = lzw_decompress(&state, src, slen, dest, sizeof(dest))) > 0) {
		// Process `res` bytes of output in `dest`.
		fwrite(dest, res, 1, outfile);
		written += res;
	}
	if (res == 0) {
		printf("%zd bytes successfully decompressed.\n", written);
	} else if (res < 0) {
		fprintf(stderr, "Decompression error: %zd\n", res);
	}
```

All code is provided under the [MIT License](LICENSE).

[![Build Status](https://travis-ci.org/eloj/lzw-eddy.svg?branch=master)](https://travis-ci.org/eloj/lzw-eddy)
