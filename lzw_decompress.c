/*
	Variable-length code LZW decompressor for fixed-memory decoding.
	(C)2020 Eddy L O Jansson. Licensed under The MIT License.

	See https://github.com/eloj/lzw-eddy
*/
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "lzw_decompress.h"

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

static void lzw_string_table_reset(struct lzw_string_table *tree) {
	tree->next_code = CODE_FIRST;
	tree->prev_code = CODE_EOF;
	tree->code_width = LZW_MIN_CODE_WIDTH;
}

static void lzwd_reset(struct lzwd_state *state) {
	lzw_string_table_reset(&state->tree);
	state->must_reset = false;
}

static void lzwc_reset(struct lzwc_state *state) {
	lzw_string_table_reset(&state->tree);
	state->must_reset = false;
}

static void lzwd_init(struct lzwd_state *state) {
	for (size_t i=0 ; i < (1UL << SYMBOL_BITS) ; ++i) {
		state->tree.node[i] = lzw_make_node(i, 0, 0);
	}
	state->readptr = 0;
	state->bitres = 0;
	state->bitres_len = 0;
	state->was_init = true;
	lzwd_reset(state);
}

static void lzwc_init(struct lzwc_state *state) {
	for (size_t i=0 ; i < (1UL << SYMBOL_BITS) ; ++i) {
		state->tree.node[i] = lzw_make_node(i, 0, 0);
	}
	state->readptr = 0;
	state->wptr = 0;
	state->was_init = true;
	lzwc_reset(state);
}

const char *lzw_strerror(enum lzw_errors errnum) {
	const char *errstr = "Unknown error";

	switch (errnum) {
		case LZWD_NOERROR:
			errstr = "No error";
			break;
		case LZWD_DESTINATION_TOO_SMALL:
			errstr = "Destination buffer too small";
			break;
		case LZWD_INVALID_CODE_STREAM:
			errstr = "Invalid code stream";
			break;
		case LZWD_STRING_TABLE_FULL:
			errstr = "String table full";
			break;

	}
	return errstr;
};

ssize_t lzw_decompress(struct lzwd_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen) {
	if (state->was_init == false)
		lzwd_init(state);

	uint32_t bitres = state->bitres;
	uint32_t bitres_len = state->bitres_len;

	uint32_t code = 0;
	size_t wptr = 0;

	while (state->readptr < slen) {
		// Fill bit-reservoir.
		while (bitres_len < state->tree.code_width) {
			bitres |= src[state->readptr++] << bitres_len;
			bitres_len += 8;
		}

		state->bitres = bitres;
		state->bitres_len = bitres_len;

		code = bitres & mask_from_width(state->tree.code_width);
		bitres >>= state->tree.code_width;
		bitres_len -= state->tree.code_width;

		if (code == CODE_CLEAR) {
			if (state->tree.next_code != CODE_FIRST)
				lzwd_reset(state);
			continue;
		} else if (code == CODE_EOF) {
			break;
		} else if (state->must_reset) {
			// ERROR: Ran out of space in string table
			return LZWD_STRING_TABLE_FULL;
		}

		if (code <= state->tree.next_code) {
			bool known_code = code < state->tree.next_code;
			uint16_t tcode = known_code ? code : state->tree.prev_code;
			size_t prefix_len = lzw_node_prefix_len(state->tree.node[tcode]);
			uint8_t symbol;

			// Invalid state, invalid input.
			if (!known_code && state->tree.prev_code == CODE_EOF) {
				return LZWD_INVALID_CODE_STREAM;
			}

			// Track max output buffer size required. This isn't necessary, but nice to have.
			if (prefix_len + 2 > state->longest_prefix) {
				state->longest_prefix = prefix_len + 2;
			}

			// Check if prefix alone too large for output buffer. User could try again with a larger buffer.
			if (prefix_len + 2 > dlen) {
				return LZWD_DESTINATION_TOO_SMALL;
			}

			// Check if room in output buffer, else return early.
			if (wptr + prefix_len + 2 > dlen) {
				return wptr;
			}

			// Write out prefix to destination
			for (size_t i=0 ; i < prefix_len + 1 ; ++i) {
				symbol = lzw_node_symbol(state->tree.node[tcode]);
				dest[wptr + prefix_len - i] = symbol;
				tcode = lzw_node_parent(state->tree.node[tcode]);
			}
			wptr += prefix_len + 1;

			// Add the first character of the prefix as a new code with prev_code as the parent.
			if (state->tree.prev_code != CODE_EOF) {
				if (!known_code) {
					dest[wptr++] = symbol; // Special case for new codes.
					assert(code == state->tree.next_code);
				}

				state->tree.node[state->tree.next_code] = lzw_make_node(symbol, state->tree.prev_code, 1 + lzw_node_prefix_len(state->tree.node[state->tree.prev_code]));

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
			return LZWD_INVALID_CODE_STREAM;
		}
	}
	return wptr;
}

static bool lzw_string_table_lookup(struct lzwc_state *state, uint8_t *prefix, size_t len, uint16_t *code) {
	printf("Looking up prefix from %p to %p (len=%zu)\n", prefix, prefix+len, len);
	if (len == 1) {
		*code = state->tree.node[*prefix];
		return true;
	}

	return false;
}

static void lzw_output_code(struct lzwc_state *state, uint16_t code) {
	// TODO: Write until less than one octet left, put remains in bit-reservoir.
	assert(state->bitres_len + state->tree.code_width < sizeof(bitres_t)*8);
	state->bitres |= code << state->bitres_len;
	state->bitres_len += state->tree.code_width;

	state->tree.prev_code = code;

	printf("<CODE:%d width=%d reservoir:%02d/%zu:%02x>\n", code, state->tree.code_width, state->bitres_len, sizeof(bitres_t)*8, state->bitres);
}

static void lzw_flush_reservoir(struct lzwc_state *state, uint8_t *dest, bool final) {
	// NOTE: We assume we have enough space left in dest.

	// Write codes to output.
	while (state->bitres_len >= 8) {
		dest[state->wptr++] = state->bitres & 0xFF;
		state->bitres >>= 8;
		state->bitres_len -= 8;
		printf("Flushed: %02x, reservoir:%02d/%zu:%02x\n", dest[state->wptr-1], state->bitres_len, sizeof(bitres_t)*8, state->bitres);
	}

	if (final && state->bitres_len > 0) {
		printf("Flushing last %d bits.\n", state->bitres_len);
		dest[state->wptr++] = state->bitres;
		state->bitres = 0;
		state->bitres_len = 0;
		printf("Flushed: %02x, reservoir:%02d/%zu:%02x\n", dest[state->wptr-1], state->bitres_len, sizeof(bitres_t)*8, state->bitres);
	}
}

ssize_t lzw_compress(struct lzwc_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen) {
	if (state->was_init == false) {
		lzwc_init(state);
		lzw_output_code(state, CODE_CLEAR);
	}

	uint16_t code = 0;
	size_t prefix_end = 0;
	state->wptr = 0;

	while (state->readptr + prefix_end < slen) {
		// Ensure we have enough space for flushing codes.
		if (state->wptr + (state->tree.code_width >> 3) + 1 + 2 > dlen) { // Also reserve bits for 16-bit EOF code.
			return state->wptr;
		}

		++prefix_end;
		// lookup prefix in string table
		bool existing_code = lzw_string_table_lookup(state, src + state->readptr, prefix_end, &code);
		// if not found, add prefix rooted at state->prev_code
		if (!existing_code) {
			printf("New prefix, adding code %d /w parent %d\n", state->tree.next_code, state->tree.prev_code);

			state->readptr += prefix_end;
			prefix_end = 0;
		} else {
			printf("Found prefix as code %d\n", code);
			state->readptr += prefix_end;
			prefix_end = 0;
		}
		lzw_output_code(state, code);

		lzw_flush_reservoir(state, dest, false);
		break; // This tests
	}
	// WARN: Problem with this is that we can't chain encodes, add 'final' flag to compression call?
	if (state->readptr + prefix_end == slen && state->tree.prev_code != CODE_EOF) {
		lzw_output_code(state, CODE_EOF);
		lzw_flush_reservoir(state, dest, true);
	}

	// if we didn't write anything, there shouldn't be any bits left in reservoir.
	assert(!(state->wptr == 0 && state->bitres_len > 0));

	printf("Returning %zu bytes written to caller.\n", state->wptr);
	return state->wptr;
}
