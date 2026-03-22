// list_macro.h - List macro system for Hyades
// Expands \list{...} syntax with Markdown-style list items

#ifndef LIST_MACRO_H
#define LIST_MACRO_H

#include <stdbool.h>

// ============================================================================
// List Macro
// ============================================================================
//
// The list macro transforms Markdown-style lists:
//
//   \list{
//     - First item
//     - Second item
//       - Nested item
//         - Deeper nested
//     - Third item
//   }
//
// With optional parameters for customization:
//
//   \list[indent=2, point1:-, point2:-, point3:-]{...}
//
// The macro parses the content to detect:
// - Leading whitespace on each line
// - Position of the `-` character to determine nesting level
// - Item content after the `-`
//
// ============================================================================

// Check if input starts with \list
bool is_list_macro(const char *input);

// Expand \list[options]{content} macro
// Returns newly allocated Hyades script string, caller must free
// Returns NULL on error
char *list_macro_expand(const char *input, int *end_pos, char *error_msg, int error_size);

#endif // LIST_MACRO_H
