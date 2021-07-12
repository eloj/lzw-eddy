
# Simple LZW (Lempel-Ziv-Welch) Compressor & Decompressor

A basic headerless LZW compressor and decompressor. Supports variable length codes between
9 and 12 bits per default. Up to 16-bits should work if the `LZW_MAX_CODE_WIDTH`
define is changed and the `lzw_node` type upgraded to 64 bits, but this is untested.

The algorithm implemented by this code was widely distributed in the
old DOS days in places like [Dr.Dobbs](https://marknelson.us/posts/1989/10/01/lzw-data-compression.html) and a popular book on compression,
probably due to its use in GIF. This resulted in it being used in [all sorts of places](https://www.giantbomb.com/profile/eloj/blog/technical-notes-on-the-level-format-of-puzznic-for/114881/).

This code was developed using the note "[LZW and GIF explained](https://www.eecis.udel.edu/~amer/CISC651/lzw.and.gif.explained.html)"
by Steve Blackstock as a reference.

All code is provided under the [MIT License](LICENSE).

[![Build status](https://github.com/eloj/lzw-eddy/workflows/build/badge.svg)](https://github.com/eloj/lzw-eddy/actions/workflows/c-cpp.yml)

## Features

* Fixed memory requirements:
	* Uses ~16KiB for state/string table.
	* ~4KiB destination buffer recommended, but can go much lower in practice.
	* Low stack usage.
* Compressor can be 'short-stroked' to limit decompression buffer size requirement.
* No scratch buffer used when decompressing.
* [Valgrind](https://valgrind.org/) and [AFL](https://lcamtuf.coredump.cx/afl/) clean (at least one cycle)

## To Do

* Add Google Benchmark.
* Use hashing for lookups in `lzw_string_table_lookup`.
* Support changing inputs during processing.
* Gather/Scatter alternative interface.

## Usage Example

```c
	#include "lzw.c"

	struct lzw_state state = { };

	size_t slen = <length of compressed data>
	uint8_t *src = <compressed data>;
	uint8_t dest[4096];

	ssize_t res, written = 0;
	while ((res = lzw_decompress(&state, src, slen, dest, sizeof(dest))) > 0) {
		// Process `res` bytes of output in `dest`, e.g:
		// fwrite(dest, res, 1, outfile);
		written += res;
	}
	if (res == 0) {
		printf("%zd bytes successfully decompressed.\n", written);
	} else if (res < 0) {
		fprintf(stderr, "Decompression error: %s (err:%zd)\n", lzw_strerror(res), res);
	}
```
