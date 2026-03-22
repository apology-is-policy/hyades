#pragma once
#include <stddef.h>
#include <stdint.h>

// Decode next codepoint from a UTF-8 string.
// *p points inside [s, s+len); advances *p by the number of bytes consumed.
// Returns U+FFFD on invalid sequences.
uint32_t utf8_next(const char *s, size_t len, size_t *p);

// Encode one codepoint to UTF-8 into buf[0..3]. Returns number of bytes written (1..4).
size_t utf8_encode(uint32_t cp, char out[4]);

// Count display "columns" for a UTF-8 string under our 1-col-per-codepoint model.
// ANSI escape sequences (ESC[...m) are treated as zero-width.
size_t utf8_display_width(const char *s);

// Skip an ANSI CSI escape sequence at position p in string s of length len.
// If s[p] starts an ANSI escape (ESC[...final_byte), advances p past it and returns 1.
// Otherwise returns 0 and does not modify p.
int utf8_skip_ansi_escape(const char *s, size_t len, size_t *p);