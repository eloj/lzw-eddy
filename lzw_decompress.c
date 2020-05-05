#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "lzw_decompress.h"

#define DATA_BITS   8
#define DATA_SHIFT  0
#define DATA_MASK   ((1UL << DATA_BITS)-1)
#define PARENT_BITS 12
#define PARENT_SHIFT DATA_BITS
#define PARENT_MASK ((1UL << PARENT_BITS)-1)
#define PREFIXLEN_BITS 12
#define PREFIXLEN_SHIFT (PARENT_BITS+DATA_BITS)
#define PREFIXLEN_MASK ((1UL << PREFIXLEN_BITS)-1)

#define CODE_CLEAR 256
#define CODE_EOF 257
#define CODE_FIRST 258

static inline uint8_t lzw_node_data(uint32_t node) {
	return (node >> DATA_SHIFT) & DATA_MASK;
}

static inline uint16_t lzw_node_parent(uint32_t node) {
	return (node >> PARENT_SHIFT) & PARENT_MASK;
}

static inline uint16_t lzw_node_prefix_len(uint32_t node) {
	return (node >> PREFIXLEN_SHIFT) & PREFIXLEN_MASK;
}

static inline uint32_t lzw_make_node(uint8_t ch, uint16_t parent, uint16_t len) {
	uint32_t node = (len << PREFIXLEN_SHIFT) | (parent << PARENT_SHIFT) | (ch << DATA_SHIFT);
	return node;
}

static void lzwd_reset(struct lzwd_state *state) {
	state->next_code = CODE_FIRST;
	state->old_code = -1; // previous code
	state->code_width = 9;
	state->code_mask = (1UL << state->code_width) - 1;
	state->must_reset = false;
}

void lzwd_init(struct lzwd_state *state) {
	for (size_t i=0 ; i < 256 ; ++i) {
		state->tree[i] = lzw_make_node(i, 0, 0);
	}
	lzwd_reset(state);
	state->readptr = 0;
	state->bitres = 0;
	state->bitres_len = 0;
}

ssize_t lzw_decompress(struct lzwd_state *state, uint8_t *src, size_t slen, uint8_t *dest, size_t dlen) {
	uint32_t code = 0;
	size_t wptr = 0;

	uint32_t bitres = state->bitres;
	uint32_t bitres_len = state->bitres_len;

	while (state->readptr < slen) {
		// Fill bit-reservoir.
		while (bitres_len < state->code_width) {
			bitres = bitres + (src[state->readptr++] << bitres_len);
			bitres_len += 8;
		}

		state->bitres = bitres;
		state->bitres_len = bitres_len;

		code = bitres & state->code_mask;
		bitres >>= state->code_width;
		bitres_len -= state->code_width;

		if (code == CODE_CLEAR) {
			if (state->next_code != CODE_FIRST)
				lzwd_reset(state);
			continue;
		} else if (code == CODE_EOF) {
			break;
		} else if (state->must_reset) {
			// ERROR: Ran out of space in string table
			return -3;
		}

		if (code <= state->next_code) {
			bool known_code = code < state->next_code;
			uint16_t tcode = known_code ? code : (uint16_t)state->old_code;
			size_t prefix_len = lzw_node_prefix_len(state->tree[tcode]);
			uint8_t ch;

			// Assert invalid state.
			assert(!(!known_code && state->old_code == -1));

			// Check if room in output buffer, else return early.
			if (wptr + prefix_len + 2 > dlen) {
				return wptr;
			}

			// Write out prefix to destination
			for (size_t i=0 ; i < prefix_len + 1 ; ++i) {
				ch = lzw_node_data(state->tree[tcode]);
				dest[wptr + prefix_len - i] = ch;
				tcode = lzw_node_parent(state->tree[tcode]);
			}
			wptr += prefix_len + 1;

			// Add the first character of the prefix as a new code with old_code as the parent.
			if (state->old_code != -1) {
				if (!known_code) {
					dest[wptr++] = ch; // Special case for new codes.
					assert(code == state->next_code);
				}

				state->tree[state->next_code] = lzw_make_node(ch, state->old_code, 1 + lzw_node_prefix_len(state->tree[state->old_code]));

				if (state->next_code >= state->code_mask) {
					if (state->code_width == LZW_MAX_CODE_WIDTH) {
						// Out of bits in code, next code MUST be a reset!
						state->must_reset = true;
						state->old_code = code;
						continue;
					}
					state->code_mask <<= 1;
					++state->code_mask;
					++state->code_width;
				}
				state->next_code++;
			}
			state->old_code = code;
		} else {
			// Desynchronized, probably corrupt input.
			return -2;
		}
	}
	return wptr;
}
