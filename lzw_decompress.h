/*
	Variable-length code LZW decompressor for fixed-memory decoding.
	(C)2020 Eddy L O Jansson. Licensed under The MIT License.

	See https://github.com/eloj/lzw-eddy
*/
#pragma once

#define LZW_MIN_CODE_WIDTH 9
#define LZW_MAX_CODE_WIDTH 12
#define LZW_MAX_CODE (1UL << LZW_MAX_CODE_WIDTH)

struct lzwd_state {
	uint32_t code_width;
	uint16_t next_code;
	uint16_t prev_code;
	uint32_t tree[LZW_MAX_CODE]; // 16K

	bool	 was_init;
	bool	 must_reset;

	size_t readptr;
	// Bit reservoir, need room for LZW_MAX_CODE_WIDTH*2-1 bits.
	uint32_t bitres;
	uint32_t bitres_len;
};

// Decompress `slen` bytes from `src` into `dest` of size `dlen`.
//
// Returns the number of bytes decompressed into `dest`.
// Once all input has been consumed, 0 is returned.
// On error, a negative integer is returned.
//
// `state`should be zero-initialized.
//
// `dlen` should be at least 4096 bytes, unless the input
// is known to require less. Technically, we need room
// for the longest prefix used.
//
// `src` and `slen` should not change between calls.
//
ssize_t lzw_decompress(struct lzwd_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen);
