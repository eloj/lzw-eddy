
# Simple LZW (Lempel-Ziv-Welch) Library, Compressor & Decompressor

A single-header library and basic headerless compressor and decompressor. Supports variable length codes
between 9 and 12 bits per default, but the upper bound is a compile-time constant that can be adjusted between
9 and 16 bits.

The algorithm implemented by this code was widely distributed in the old MS-DOS days in places
like [Dr.Dobbs](https://marknelson.us/posts/1989/10/01/lzw-data-compression.html) and a popular book on compression,
probably due to its use in GIF. This resulted in it being used in all sorts of places.

Specifically the code in this repository was written to be [bit-compatible with Puzznic](https://www.giantbomb.com/profile/eloj/blog/technical-notes-on-the-level-format-of-puzznic-for/114881/) (MS-DOS),
and as such does not represent an effort to write "the best" LZW codec.

Code developed using the note "[LZW and GIF explained](https://www.eecis.udel.edu/~amer/CISC651/lzw.and.gif.explained.html)"
by Steve Blackstock as a reference.

All code is provided under the [MIT License](LICENSE).

[![Build status](https://github.com/eloj/lzw-eddy/workflows/build/badge.svg)](https://github.com/eloj/lzw-eddy/actions/workflows/c-cpp.yml)

## Features

* Single-Header Library.
* Fixed memory requirements:
	* Uses ~16KiB for state/string table by default.
	* At least ~4KiB output buffer recommended, but can go _much_ lower in practice.
	* Low stack usage.
* Compressor can be 'short-stroked' to limit decompression buffer size requirement.
* Fast decompression. _Very_ slow compression.
* Releases are:
	* [Valgrind](https://valgrind.org/) clean,
	* [scan-build](https://clang-analyzer.llvm.org/scan-build.html) clean, and
	* [AFL++](https://aflplus.plus/) clean (for some reasonable run-time).

## C interface

```c
ssize_t lzw_decompress(struct lzw_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen);
ssize_t lzw_compress(struct lzw_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen);
const char *lzw_strerror(enum lzw_errors errnum);
```

* State must be zero-initialized.
* The return value is the number of bytes compressed or decompressed into `dest`. Once all input has been processed, `0` is returned. See [example](#example).
* On error, a negative integer is returned.

All input is assumed to be available at `src`; e.g it is NOT allowed to switch `src` during encoding/decoding. A function
to 'hand over' state to new input could be added, but I don't have the need.

## Usage

In your code, define `LZW_EDDY_IMPLEMENTATION` and then `#include "lzw.h"`. This will give you a decoder/encoder _specific_
for 9-12 bit codes, giving a string table of 4096 entries.

You can optionally define `LZW_MAX_CODE_WIDTH` to a value between 9 and 16 before including the header to
change this compile-time default. Due to the way the dictionary is reconstructed during decompression,
a decoder is only compatible with data generated for the _exact_ same size string table.

12-bit codes are probably the sweet spot for most applications. Larger codes means more bits are needed to
encode newer strings, and because the string table is larger, the dictionary doesn't adapt as fast as it
would if it was smaller. This combination means that a larger table can result in worse compression ratio.

The encoder could theoretically be improved to flush or prune the existing string table if few long matches are made over
some window, but no such adaptability is present.

## CLI compressor

`lzw-eddy` is a simple command-line compressor built using the library.

```bash
lzw-eddy 1.1.0-dev <45bf69f1>
Usage: ./lzw-eddy -c file|-d file -o outfile
Compiled Configuration:
 LZW_MIN_CODE_WIDTH=9, LZW_MAX_CODE_WIDTH=12, LZW_MAX_CODES=4096, sizeof(lzw_state)=16440
```

You can pass BITWIDTH=\<num\> to build it with a non-default string table size.

```bash
$ make -B BITWIDTH=14 && ./lzw-eddy -c lzw.h -o /dev/null
lzw-eddy 1.1.0-dev <45bf69f1>
Compressing 'lzw.h', 14566 bytes.
6947 bytes written to output, reduction=52.31% (longest prefix=15).
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
