
# Simple LZW (Lempel-Ziv-Welch) Compressor & Decompressor

A basic headerless LZW compressor and decompressor. Supports variable length codes between
9 and 12 bits per default, but up to 15-bits work.

The algorithm implemented by this code was widely distributed in the
old DOS days in places like [Dr.Dobbs](https://marknelson.us/posts/1989/10/01/lzw-data-compression.html) and a popular book on compression,
probably due to its use in GIF. This resulted in it being used in [all sorts of places](https://www.giantbomb.com/profile/eloj/blog/technical-notes-on-the-level-format-of-puzznic-for/114881/).

This code was developed using the note "[LZW and GIF explained](https://www.eecis.udel.edu/~amer/CISC651/lzw.and.gif.explained.html)"
by Steve Blackstock as a reference.

This code was written to be bit-compatible with Puzznic (MS-DOS).

All code is provided under the [MIT License](LICENSE).

[![Build status](https://github.com/eloj/lzw-eddy/workflows/build/badge.svg)](https://github.com/eloj/lzw-eddy/actions/workflows/c-cpp.yml)

## Features

* Single-Header Library.
* Fixed memory requirements:
	* Uses ~16KiB for state/string table.
	* ~4KiB destination buffer recommended, but can go much lower in practice.
	* Low stack usage.
* Compressor can be 'short-stroked' to limit decompression buffer size requirement.
* No scratch buffer used when decompressing.
* Releases are [Valgrind](https://valgrind.org/) and [AFL](https://lcamtuf.coredump.cx/afl/) clean (at least one cycle)

## C interface

```c
ssize_t lzw_decompress(struct lzw_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen);
ssize_t lzw_compress(struct lzw_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen);
const char *lzw_strerror(enum lzw_errors errnum);
```

* State must be zero-initialized.
* The return value is the number of bytes compressed or decompressed into `dest`. Once all input has been processed, `0` is returned.
* On error, a negative integer is returned.

All input is assumed to be available at `src`; e.g it is NOT allowed to switch `src` during encoding/decoding. A function
to 'hand over' state to new input could be added, but I don't have the need.

## Usage

In your code, define `LZW_EDDY_IMPLEMENTATION` and then `#include "lzw.h"`. This will give you a decoder/encoder _specific_
for 9-12 bit codes. You can optionally define `LZW_MAX_CODE_WIDTH` to a value between 9 and 15 before including the header to
change this default. In general, due to the way the dictionary is reconstructed during decompression, a decoder is only
compatible with data generated for the _exact_ same bit range.

## CLI compressor

`lzw-eddy` is a simple command-line compressor built using the library.

You can pass BITWIDTH=<num> to build it with a non-default string table size.

```bash
$ make -B BITWIDTH=14 && ./lzw-eddy -c lzw.h -o /dev/null
lzw-eddy compressing file dummy (9-14 bits)
Compressing 14283 bytes.
6798 bytes written to output, reduction=52.40% (longest prefix=16).
```

## Example

```c
	#define LZW_EDDY_IMPLEMENTATION
	#include "lzw.h"

	struct lzw_state state = { 0 };

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

## Unlikely To Do

* Add Google Benchmark.
* Use hashing for lookups in `lzw_string_table_lookup`.
* Support changing inputs during processing.
* Gather/Scatter alternative interface.
