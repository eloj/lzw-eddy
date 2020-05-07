/*
	Variable-length code LZW decompressor for fixed-memory decoding.
	(C)2020 Eddy L O Jansson. Licensed under The MIT License.

	See https://github.com/eloj/lzw-eddy
*/
#pragma once

#define LZW_MIN_CODE_WIDTH 9
#define LZW_MAX_CODE_WIDTH 12
#define LZW_MAX_CODE (1UL << LZW_MAX_CODE_WIDTH)

enum lzwd_errors {
	LZWD_NOERROR = 0,
	LZWD_DESTINATION_TOO_SMALL = -1,
	LZWD_INVALID_CODE_STREAM = -2,
	LZWD_STRING_TABLE_FULL = -3,
};

// This type must be large enough for SYMBOL_BITS + LZW_MAX_CODE_WIDTH*2 bits.
typedef uint32_t lzw_node;

struct lzwd_state {
	uint32_t code_width;
	uint16_t next_code;
	uint16_t prev_code;
	lzw_node tree[LZW_MAX_CODE]; // 16K

	bool	 was_init;
	bool	 must_reset;

	size_t readptr;
	// Bit reservoir, need room for LZW_MAX_CODE_WIDTH*2-1 bits.
	uint32_t bitres;
	uint32_t bitres_len;

	// Tracks the longest prefix used, which is equal to the minimum output buffer required for decompression.
	size_t longest_prefix;
};

// Translate error code to message.
const char *lzwd_strerror(enum lzwd_errors errnum);

/*
	Decompress `slen` bytes from `src` into `dest` of size `dlen`.

	Returns the number of bytes decompressed into `dest`.
	Once all input has been consumed, 0 is returned.
	On error, a negative integer is returned.

	`state`should be zero-initialized.

	`dlen` should be at least 4096 bytes, unless the input is known to
	require less.

	-1 will be returned if the output buffer is too small, in which case
	you'd have to restart from the beginning with a larger `dest`.

	All that said, even a file consisting of 80K zeros requires only 400 bytes,
	so we're being very conservative here. A 'normal' file may need only
	128 bytes or so.

	`src` and `slen` should not change between calls.
*/
ssize_t lzw_decompress(struct lzwd_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen);
