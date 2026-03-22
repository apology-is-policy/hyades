#include "render_opts.h"
#include <string.h>

static int g_unicode_mode_flag = 0;
static int g_math_cursive_mode = 0;
static const char *g_linebreaker_mode = "greedy"; // Default: "greedy", "knuth", or "raggedright"

void set_unicode_mode(int enabled) {
    g_unicode_mode_flag = enabled ? 1 : 0;
}
int get_unicode_mode(void) {
    return g_unicode_mode_flag;
}

int get_math_cursive_mode(void) {
    return g_math_cursive_mode;
}
void set_math_cursive_mode(int enabled) {
    g_math_cursive_mode = enabled ? 1 : 0;
}

const char *get_linebreaker_mode(void) {
    return g_linebreaker_mode;
}
void set_linebreaker_mode(const char *mode) {
    // Use static strings to avoid dangling pointers when caller frees the mode string
    if (mode && strcmp(mode, "greedy") == 0) {
        g_linebreaker_mode = "greedy";
    } else if (mode && strcmp(mode, "knuth") == 0) {
        g_linebreaker_mode = "knuth";
    } else if (mode && strcmp(mode, "raggedright") == 0) {
        g_linebreaker_mode = "raggedright";
    }
}
