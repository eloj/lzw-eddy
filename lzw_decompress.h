/*
	Variable-length code LZW decompressor for fixed-memory decoding.
	(C)2020 Eddy L O Jansson. Licensed under The MIT License.

	See https://github.com/eloj/lzw-eddy
*/
#pragma once

// Going outside of 9- to 12-bit codes is untested, and beyond 16-bit codes will require code changes.
#define LZW_MIN_CODE_WIDTH 9
#define LZW_MAX_CODE_WIDTH 12
#define LZW_MAX_CODE (1UL << LZW_MAX_CODE_WIDTH)

enum lzw_errors {
	LZWD_NOERROR = 0,
	LZWD_DESTINATION_TOO_SMALL = -1,
	LZWD_INVALID_CODE_STREAM = -2,
	LZWD_STRING_TABLE_FULL = -3,
};

// This type must be large enough for SYMBOL_BITS + LZW_MAX_CODE_WIDTH*2 bits.
typedef uint32_t lzw_node;
typedef uint32_t bitres_t;

struct lzw_string_table {
	uint32_t code_width;
	uint16_t next_code;
	uint16_t prev_code;
	lzw_node node[LZW_MAX_CODE]; // 16K at 12-bit codes.
};

struct lzw_state {
	struct lzw_string_table tree;

	bool	 was_init;
	bool	 must_reset;

	size_t rptr;
	size_t wptr;
	// Bit reservoir, need room for LZW_MAX_CODE_WIDTH*2-1 bits.
	bitres_t bitres;
	uint32_t bitres_len;

	// Tracks the longest prefix used, which is equal to the minimum output buffer required for decompression.
	size_t longest_prefix;
	// TODO: Restrict the longest_prefix to this -- optimize for decode buffer size.
	size_t longest_prefix_allowed;
};

// Translate error code to message.
const char *lzw_strerror(enum lzw_errors errnum);

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
*/
ssize_t lzw_decompress(struct lzw_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen);

/*
*/
ssize_t lzw_compress(struct lzw_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen);

