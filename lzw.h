/*
	Variable-length code LZW compressor and decompressor for fixed-memory decoding.
	Copyright (c) 2020-2022, Eddy L O Jansson. Licensed under The MIT License.

	See https://github.com/eloj/lzw-eddy
*/
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h> // for ssize_t

// Going outside of 9- to 12-bit codes is untested, and beyond 16-bit codes will require code changes.
#define LZW_MIN_CODE_WIDTH 9
#define LZW_MAX_CODE_WIDTH 12
#define LZW_MAX_CODE (1UL << LZW_MAX_CODE_WIDTH)

enum lzw_errors {
	LZW_NOERROR = 0,
	LZW_DESTINATION_TOO_SMALL = -1,
	LZW_INVALID_CODE_STREAM = -2,
	LZW_STRING_TABLE_FULL = -3,
};

// This type must be large enough for SYMBOL_BITS + LZW_MAX_CODE_WIDTH*2 bits.
typedef uint32_t lzw_node;
typedef uint32_t bitres_t;

struct lzw_string_table {
	uint32_t code_width;
	uint16_t next_code;
	uint16_t prev_code;
	lzw_node node[LZW_MAX_CODE + 1]; // 16K at 12-bit codes.
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
	// Restrict the longest_prefix to this -- optimize for decode buffer size.
	size_t longest_prefix_allowed;
};

// Translate error code to message.
const char *lzw_strerror(enum lzw_errors errnum);

/*
	Decompress `slen` bytes from `src` into `dest` of size `dlen`.

	Returns the number of bytes decompressed into `dest`.
	Once all input has been consumed, 0 is returned.
	On error, a negative integer is returned.

	Neither `src` nor `dest` may be NULL.

	`state`should be zero-initialized.

	`dlen` should be at least 4096 bytes, unless the input is known to
	require less.

	`LZWD_DESTINATION_TOO_SMALL` will be returned if the output buffer is too small, in which case
	you'd have to restart from the beginning with a larger `dest`.

	All that said, even a file consisting of 80K zeros requires only 400 bytes,
	so we're being very conservative here. A 'normal' file may need only 128 bytes or so.
*/
ssize_t lzw_decompress(struct lzw_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen);

/*
	Compress `slen` bytes from `src` into `dest` of size `dlen`.

	Returns the number of bytes compressed into `dest`.
	Once all input has been consumed, 0 is returned.
	On error, a negative integer is returned.

	Neither `src` nor `dest` may be NULL.

	`state`should be zero-initialized.
*/
ssize_t lzw_compress(struct lzw_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen);

#ifdef LZW_EDDY_IMPLEMENTATION

/*
	Variable-length code LZW compressor and decompressor for fixed-memory decoding.
	Copyright (c) 2020-2022, Eddy L O Jansson. Licensed under The MIT License.

	See https://github.com/eloj/lzw-eddy
*/
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#define SYMBOL_BITS 8
#define SYMBOL_SHIFT 0
#define SYMBOL_MASK ((1UL << SYMBOL_BITS)-1)
#define PARENT_BITS LZW_MAX_CODE_WIDTH
#define PARENT_SHIFT SYMBOL_BITS
#define PARENT_MASK ((1UL << PARENT_BITS)-1)
#define PREFIXLEN_BITS LZW_MAX_CODE_WIDTH
#define PREFIXLEN_SHIFT (PARENT_BITS+SYMBOL_BITS)
#define PREFIXLEN_MASK ((1UL << PREFIXLEN_BITS)-1)

#define CODE_CLEAR (1UL << SYMBOL_BITS)
#define CODE_EOF (CODE_CLEAR+1)
#define CODE_FIRST (CODE_CLEAR+2)

static_assert((LZW_MAX_CODE_WIDTH >= LZW_MIN_CODE_WIDTH), "");
static_assert((SYMBOL_BITS + PARENT_BITS + PREFIXLEN_BITS) <= sizeof(lzw_node)*8, "lzw_node type too small");
static_assert((LZW_MAX_CODE_WIDTH*2 - 1) < sizeof(bitres_t)*8, "bitres_t type too small");

static inline uint8_t lzw_node_symbol(lzw_node node) {
	return (node >> SYMBOL_SHIFT) & SYMBOL_MASK;
}

static inline uint16_t lzw_node_parent(lzw_node node) {
	return (node >> PARENT_SHIFT) & PARENT_MASK;
}

static inline uint16_t lzw_node_prefix_len(lzw_node node) {
	return (node >> PREFIXLEN_SHIFT) & PREFIXLEN_MASK;
}

static inline lzw_node lzw_make_node(uint8_t symbol, uint16_t parent, uint16_t len) {
	lzw_node node = (len << PREFIXLEN_SHIFT) | (parent << PARENT_SHIFT) | (symbol << SYMBOL_SHIFT);
	return node;
}

static inline uint32_t mask_from_width(uint32_t width) {
	return (1UL << width)-1;
}

static void lzw_reset(struct lzw_state *state) {
	state->tree.next_code = CODE_FIRST;
	state->tree.prev_code = CODE_EOF;
	state->tree.code_width = LZW_MIN_CODE_WIDTH;
	state->must_reset = false;
}

static void lzw_init(struct lzw_state *state) {
	for (size_t i=0 ; i < (1UL << SYMBOL_BITS) ; ++i) {
		state->tree.node[i] = lzw_make_node(i, 0, 0);
	}
	state->rptr = 0;
	state->bitres = 0;
	state->bitres_len = 0;
	state->was_init = true;
	lzw_reset(state);
}

const char *lzw_strerror(enum lzw_errors errnum) {
	const char *errstr = "Unknown error";

	switch (errnum) {
		case LZW_NOERROR:
			errstr = "No error";
			break;
		case LZW_DESTINATION_TOO_SMALL:
			errstr = "Destination buffer too small";
			break;
		case LZW_INVALID_CODE_STREAM:
			errstr = "Invalid code stream";
			break;
		case LZW_STRING_TABLE_FULL:
			errstr = "String table full";
			break;

	}
	return errstr;
}

ssize_t lzw_decompress(struct lzw_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen) {
	if (state->was_init == false)
		lzw_init(state);

	// Keep local copies so that we can exit and continue without losing bits.
	uint32_t bitres = state->bitres;
	uint32_t bitres_len = state->bitres_len;

	uint32_t code = 0;
	size_t wptr = 0;

	while (state->rptr < slen) {
		// Fill bit-reservoir.
		while ((bitres_len < state->tree.code_width) && (state->rptr < slen)) {
			bitres |= src[state->rptr++] << bitres_len;
			bitres_len += 8;
		}

		state->bitres = bitres;
		state->bitres_len = bitres_len;

		if (state->bitres_len < state->tree.code_width) {
			return LZW_INVALID_CODE_STREAM;
		}

		code = bitres & mask_from_width(state->tree.code_width);
		bitres >>= state->tree.code_width;
		bitres_len -= state->tree.code_width;

		if (code == CODE_CLEAR) {
			if (state->tree.next_code != CODE_FIRST)
				lzw_reset(state);
			continue;
		} else if (code == CODE_EOF) {
			break;
		} else if (state->must_reset) {
			// ERROR: Ran out of space in string table
			return LZW_STRING_TABLE_FULL;
		}

		if (code <= state->tree.next_code) {
			bool known_code = code < state->tree.next_code;
			uint16_t tcode = known_code ? code : state->tree.prev_code;
			size_t prefix_len = 1 + lzw_node_prefix_len(state->tree.node[tcode]);
			uint8_t symbol = 0;

			assert(prefix_len > 0);

			// Invalid state, invalid input.
			if (!known_code && state->tree.prev_code == CODE_EOF) {
				return LZW_INVALID_CODE_STREAM;
			}

			// Track longest prefix seen.
			if (prefix_len > state->longest_prefix) {
				state->longest_prefix = prefix_len;
			}

			// Check if prefix alone too large for output buffer. User could start over with a larger buffer.
			if (prefix_len + (known_code ? 0 : 1) > dlen) {
				return LZW_DESTINATION_TOO_SMALL;
			}

			// Check if room in output buffer, else return early.
			if (wptr + prefix_len + (known_code ? 0 : 1) > dlen) {
				return wptr;
			}

			// Write out prefix to destination
			for (size_t i=0 ; i < prefix_len ; ++i) {
				symbol = lzw_node_symbol(state->tree.node[tcode]);
				dest[wptr + prefix_len - 1 - i] = symbol;
				tcode = lzw_node_parent(state->tree.node[tcode]);
			}
			wptr += prefix_len;

			// Add the first character of the prefix as a new code with prev_code as the parent.
			if (state->tree.prev_code != CODE_EOF) {
				if (!known_code) {
					assert(code == state->tree.next_code);
					assert(wptr < dlen);
					dest[wptr++] = symbol; // Special case for new codes.
				}

				state->tree.node[state->tree.next_code] = lzw_make_node(symbol, state->tree.prev_code, 1 + lzw_node_prefix_len(state->tree.node[state->tree.prev_code]));

				// TODO: Change to ==
				if (state->tree.next_code >= mask_from_width(state->tree.code_width)) {
					if (state->tree.code_width == LZW_MAX_CODE_WIDTH) {
						// Out of bits in code, next code MUST be a reset!
						state->must_reset = true;
						state->tree.prev_code = code;
						continue;
					}
					++state->tree.code_width;
				}
				state->tree.next_code++;
			}
			state->tree.prev_code = code;
		} else {
			// Desynchronized, probably corrupt/invalid input.
			return LZW_INVALID_CODE_STREAM;
		}
	}
	return wptr;
}

static bool lzw_string_table_lookup(struct lzw_state *state, uint8_t *prefix, size_t len, uint16_t *code) {
	// printf("Looking up prefix '%.*s' from %p to %p (len=%zu)\n", (int)(len), prefix, prefix, prefix+len, len);
	assert (len > 0);

	if (len == 1) {
		*code = state->tree.node[prefix[0]];
		return true;
	}

	// PERF: This is slow, we should store an array of hashes to use as an initial comparison before walking the tree.
	// NOTE: It's imperative that we search newest to oldest. When limiting the prefix length, we'll
	// end up with duplicate prefixes, and only the newest code is valid for the decoder to stay in sync.
	for (size_t i=state->tree.next_code - 1 ; i >= CODE_FIRST ; --i) {
		lzw_node node = state->tree.node[i];

		if (len - 1 == lzw_node_prefix_len(node)) {
			for (size_t j=0 ; j < len ; ++j) {
				if (prefix[len-j-1] != lzw_node_symbol(node)) {
					break;
				}
				if (lzw_node_prefix_len(node) == 0) {
					*code = i;
					assert(j == len - 1);
					return true;
				}
				node = state->tree.node[lzw_node_parent(node)];
			}
		}
	}

	return false;
}

inline static void lzw_output_code(struct lzw_state *state, uint16_t code) {
	assert(state->bitres_len + state->tree.code_width < sizeof(bitres_t)*8);
	state->bitres |= code << state->bitres_len;
	state->bitres_len += state->tree.code_width;

	// printf("<CODE:%d width=%d reservoir:%02d/%zu:%02x>\n", code, state->tree.code_width, state->bitres_len, sizeof(bitres_t)*8, state->bitres);
}

static void lzw_flush_reservoir(struct lzw_state *state, uint8_t *dest, bool final) {
	// SECURITY: We assume we have enough space left in dest!

	// Write codes to output.
	while (state->bitres_len >= 8) {
		dest[state->wptr++] = state->bitres & 0xFF;
		state->bitres >>= 8;
		state->bitres_len -= 8;
		// printf("DEBUG: Flushed: %02x, reservoir:%02d/%zu:%02x\n", dest[state->wptr-1], state->bitres_len, sizeof(bitres_t)*8, state->bitres);
	}

	if (final && state->bitres_len > 0) {
		// printf("DEBUG: Flushing last %d bits.\n", state->bitres_len);
		dest[state->wptr++] = state->bitres;
		state->bitres = 0;
		state->bitres_len = 0;
		// printf("DEBUG: Flushed: %02x, reservoir:%02d/%zu:%02x\n", dest[state->wptr-1], state->bitres_len, sizeof(bitres_t)*8, state->bitres);
	}
}

ssize_t lzw_compress(struct lzw_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen) {
	if (state->was_init == false) {
		lzw_init(state);
		lzw_output_code(state, CODE_CLEAR);
	}

	uint16_t code = CODE_EOF;
	size_t prefix_end = 0;
	state->wptr = 0;

	size_t old_wptr __attribute__((unused)) = 0; // DEBUG AID.

	while (state->rptr + prefix_end < slen) {
		// Ensure we have enough space for flushing codes.
		if (state->wptr + (state->tree.code_width >> 3) + 1 + 2 + 2 > dlen) { // Also reserve bits for worst-case 16-bit CLEAR + EOF code
			return state->wptr;
		}
		old_wptr = state->wptr;

		++prefix_end;
		// lookup prefix in string table
		bool overlong = ((state->longest_prefix_allowed > 0) && (prefix_end >= state->longest_prefix_allowed));
		bool existing_code = lzw_string_table_lookup(state, src + state->rptr, prefix_end, &code);
		if (!existing_code || overlong) {
			assert(code != CODE_CLEAR);
			assert(code != CODE_EOF);

			uint8_t symbol = src[state->rptr + prefix_end - 1];
			uint16_t parent = code;
			uint16_t parent_len = 1 + lzw_node_prefix_len(state->tree.node[parent]);

			assert(state->tree.next_code <= LZW_MAX_CODE);

			// printf("New prefix from src[%zu], adding symbol '%c' (%02x) as code %d /w parent %d\n", state->rptr + prefix_end, symbol, symbol, state->tree.next_code, parent);
			state->tree.node[state->tree.next_code] = lzw_make_node(symbol, parent, parent_len);
			if (parent_len >= state->longest_prefix) {
				state->longest_prefix = parent_len;
			}

			// Output code _before_ we potentially change the bit-width.
			lzw_output_code(state, parent);

			// Handle code width expansion.
			if (state->tree.next_code > mask_from_width(state->tree.code_width)) {
				// printf("DEBUG: Expanding bitwidth to %d\n", state->tree.code_width + 1);
				if (state->tree.code_width >= LZW_MAX_CODE_WIDTH) {
					// printf("DEBUG: Max code-width reached -- Issuing clear/reset\n");
					lzw_output_code(state, CODE_CLEAR);
					lzw_reset(state);
					lzw_flush_reservoir(state, dest, false);
					state->tree.next_code--; // Undo later increase
				} else {
					++state->tree.code_width;
				}
			}
			state->tree.prev_code = state->tree.next_code;
			state->tree.next_code++;

			state->rptr += parent_len;
			prefix_end = 0;

			lzw_flush_reservoir(state, dest, false);
		}
	}
	if (prefix_end != 0) {
		// printf("DEBUG: Last prefix existed, writing existing code %d to stream\n", code);
		lzw_output_code(state, code);
		lzw_flush_reservoir(state, dest, false);
		state->tree.prev_code = code;
		state->rptr += prefix_end;
		prefix_end = 0;
	}

	// WARN: Problem with this is that we can't chain encodes, add 'final' flag to compression call?
	// NIGHTMARE: Handle zero-input
	if ((state->rptr + prefix_end == slen && state->tree.prev_code != CODE_EOF)
		// This happens to be true if we're called with slen=0, but only the first time as we now flush the bits.
		|| (state->wptr == 0 && state->bitres_len > 0)) {
		lzw_output_code(state, CODE_EOF);
		lzw_flush_reservoir(state, dest, true);
		state->tree.prev_code = CODE_EOF;
	}

	// if we didn't write anything, there shouldn't be any bits left in reservoir.
	assert(!(state->wptr == 0 && state->bitres_len > 0));
	assert(state->wptr - old_wptr < 1 + 2 + 2);

	// printf("DEBUG: Returning %zu bytes written to caller.\n", state->wptr);

	return state->wptr;
}
#endif // LZW_EDDY_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
