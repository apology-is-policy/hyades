#pragma once
// Toggle global render mode for ASCII vs Unicode glyphs.
// 0 = ASCII (default), nonzero = Unicode.
void set_unicode_mode(int enabled);
int get_unicode_mode(void);

int get_math_cursive_mode(void);
void set_math_cursive_mode(int enabled);

// Get/set the current linebreaker mode: "greedy", "knuth", or "raggedright"
const char *get_linebreaker_mode(void);
void set_linebreaker_mode(const char *mode);
