
# Simple LZW (Lempel-Ziv-Welch) Decompressor

A basic headerless LZW decompressor. Supports variable length codes between
9 and 12 bits per default. Up to 16-bits should work if the `LZW_MAX_CODE_WIDTH`
define is changed and the `lzw_node` type upgraded to 64 bits, but this is untested.

The algorithm implemented by this code was widely distributed in the
old DOS days in places like Dr.Dobbs and a popular book on compression,
probably due to its use in GIF. This resulted in it being used in [all sorts of places](https://www.giantbomb.com/profile/eloj/blog/technical-notes-on-the-level-format-of-puzznic-for/114881/).

This code was developed using the note "[LZW and GIF explained](https://www.eecis.udel.edu/~amer/CISC651/lzw.and.gif.explained.html)"
by Steve Blackstock as a reference.

## Decompression Features

* Fixed memory requirements:
	* Uses ~16KiB for state/string table.
	* ~4KiB destination buffer recommended, but can go much lower in practice.
	* Low stack usage.
* No wasteful scratch buffer used when writing out strings.
* [Valgrind](https://valgrind.org/) and [AFL](https://lcamtuf.coredump.cx/afl/) clean.

## Usage Example

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
		fprintf(stderr, "Decompression error: %s (err:%zd)\n", lzwd_strerror(res), res);
	}
```

All code is provided under the [MIT License](LICENSE).

[![Build Status](https://travis-ci.org/eloj/lzw-eddy.svg?branch=master)](https://travis-ci.org/eloj/lzw-eddy)
