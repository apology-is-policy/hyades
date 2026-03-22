// figlet.h - ASCII Art Text Renderer
#pragma once

#include <stddef.h>

// Font styles available
typedef enum {
    FIGLET_STANDARD = 0, // Classic FIGlet font
    FIGLET_BANNER,       // Bold block letters
    FIGLET_SMALL,        // Compact font
    FIGLET_TINY,
    FIGLET_COUNT
} FigletFont;

// Render text in ASCII art
// Returns newly allocated string containing the ASCII art
// NULL on error (unsupported characters, OOM, etc.)
//
// Supported characters: A-Z, a-z, 0-9, space, and basic punctuation
char *figlet_render(const char *text, FigletFont font);

// Get height of a font (number of lines)
int figlet_font_height(FigletFont font);

// Check if a character is supported
int figlet_is_supported(int c, FigletFont font);