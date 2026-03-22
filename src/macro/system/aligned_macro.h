// aligned_macro.h - Aligned equations macro for Hyades
// Expands \aligned{...} and \cases{...} into measure/layout code

#ifndef ALIGNED_MACRO_H
#define ALIGNED_MACRO_H

#include <stdbool.h>

// ============================================================================
// Aligned Macro
// ============================================================================
//
// The aligned macro transforms LaTeX-style aligned equations:
//
//   \aligned{
//       f(x) &= x^2 + 1 \\
//            &= y
//   }
//
// Into measure/recall layout code:
//
//   \measure<_a00,_w00,_h00>{$f(x)$}
//   \measure<_a01,_w01,_h01>{$= x^2 + 1$}
//   \measure<_a10,_w10,_h10>{$$}
//   \measure<_a11,_w11,_h11>{$= y$}
//   \let<_mw0>{\if{\gt{\valueof<_w00>,\valueof<_w10>}}{\valueof<_w00>}{\valueof<_w10>}}
//   \let<_mw1>{\if{\gt{\valueof<_w01>,\valueof<_w11>}}{\valueof<_w01>}{\valueof<_w11>}}
//   \begin{vbox}
//   \child{\begin{hbox}
//     \child[\valueof<_mw0>][right]{\recall<_a00>}
//     \child[\valueof<_mw1>][left]{\recall<_a01>}
//   \end{hbox}}
//   ...
//   \end{vbox}
//
// ============================================================================

// Flags for aligned expansion
typedef enum {
    ALIGNED_NONE = 0,
    ALIGNED_BRACE_LEFT = 1,  // Add stretchy { on left (cases)
    ALIGNED_BRACE_RIGHT = 2, // Add stretchy } on right (rcases)
    ALIGNED_NO_DOLLAR = 4,   // Don't wrap cells in $...$ (for math-mode embedding)
} AlignedFlags;

// ============================================================================
// Expansion API
// ============================================================================

// Expand \aligned{...} macro
// Returns newly allocated Hyades script string, caller must free
// Returns NULL on error
char *aligned_macro_expand(const char *input, int *end_pos, char *error_msg, int error_size);

// Expand \cases{...} macro (aligned with left brace)
// Returns newly allocated Hyades script string, caller must free
// Returns NULL on error
char *cases_macro_expand(const char *input, int *end_pos, char *error_msg, int error_size);

// Raw versions (no $...$ wrapping) for use inside math mode
char *aligned_macro_expand_raw(const char *input, int *end_pos, char *error_msg, int error_size);
char *cases_macro_expand_raw(const char *input, int *end_pos, char *error_msg, int error_size);

// Check if input starts with \aligned
bool is_aligned_macro(const char *input);

// Check if input starts with \cases
bool is_cases_macro(const char *input);

#endif // ALIGNED_MACRO_H
