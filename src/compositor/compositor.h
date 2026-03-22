// compositor.h — compositor + mapping API (C23)
#include "utils/error.h"

#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "math/parser/parser.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------------------------------------
// Existing options (unchanged)
// ------------------------------------------------------------

// Line breaking algorithm selection
typedef enum {
    LINEBREAK_GREEDY = 0, // Fast greedy algorithm (default)
    LINEBREAK_KNUTH_PLASS // Optimal Knuth-Plass algorithm
} LineBreakAlgo;

// Text alignment modes (for paragraph justification)
typedef enum {
    TEXT_ALIGN_JUSTIFIED = 0, // Full justification (default)
    TEXT_ALIGN_RAGGED_RIGHT,  // Left-aligned, ragged right edge
    TEXT_ALIGN_CENTERED       // Centered text
} TextAlignment;

typedef struct {
    int width;            // target column width (monospace), can be modified by \setwidth
    int document_width;   // original document width (for detecting document-level content)
    bool measuring_mode;  // true when measuring content (skip alignment/centering)
    bool hyphenate;       // enable hyphenation on overflow
    int hyphen_min_left;  // minimum chars kept on the left of a hyphen
    int hyphen_min_right; // minimum chars kept on the right of a hyphen
    // Vertical spacing (implicit, non-stacking)
    int parskip;         // space after prose paragraphs (default 1)
    int math_above_skip; // space before display math (default 1)
    int math_below_skip; // space after display math (default 1)

    LineBreakAlgo linebreaker; // line breaking algorithm
    TextAlignment alignment;   // text alignment/justification mode

    // Knuth-Plass parameters (only used when linebreaker == LINEBREAK_KNUTH_PLASS)
    double kp_line_penalty;   // base cost per line (TeX: \linepenalty, default 10)
    double kp_hyphen_penalty; // cost for hyphenating (TeX: \hyphenpenalty, default 50)
    double
        kp_consec_hyphen_penalty; // extra cost for consecutive hyphens (TeX: \doublehyphendemerits, default 3000)
    double kp_tolerance; // max stretch per gap before infinite badness (default 2.5)

    // Greedy linebreaker parameters (only used when linebreaker == LINEBREAK_GREEDY)
    int gp_short_threshold; // min word len for relaxed scoring (default 3, allows short+long pairs)
    int gp_link_threshold;  // max token width for symmetric spacing (default 2)
    int gp_spread_distance; // gaps within this distance get penalized (default 2)
    int gp_neighbor_divisor; // penalty for immediate neighbors: 0=disable, else divide (default 0)
    int gp_spread_divisor;   // divisor for gaps at distance 2..spread_dist (default 2)
    int gp_min_score;        // minimum score to receive extra space (default 0 = no minimum)
} CompOptions;

// A 0-based (row, col) coordinate in a monospace grid.
// row grows downward; col grows rightward.
typedef struct {
    int row;
    int col;
} Coord;

// Kinds for colorization of the *output* characters.
// This is deliberately conservative for now; we can expand later.
// You can treat these as "categories" for syntax highlighting in your UI.
typedef enum {
    CK_TEXT = 0,     // normal text
    CK_SPACE,        // ASCII space ' ' (including justification spaces)
    CK_NBSP,         // non-breaking space (ties)
    CK_DASH,         // hyphen/em/en dashes, minus, rule chars on text rows
    CK_PUNCT,        // quotes, commas, periods (when rendered as ASCII)
    CK_RULE,         // box/overline/underline rule cells
    CK_MATH_GENERIC, // any math box cell (default; fine-grain to come)
    // Reserved for future fine-grained math kinds:
    CK_MATH_BIGOP,   // Σ, Π, ∫, ⋃, ⋂ … (future renderer tagging)
    CK_MATH_PAREN,   // scalable delimiters ( … ), [ … ], { … }, | … |
    CK_MATH_BRACKET, // matrix brackets, floor/ceil
    CK_MATH_SMALLOP, // +, −, ⋅, ×, … small operators
    CK_MATH_GREEK,   // αβγ… and Greek capitals
    CK_OTHER
} CharKind;

#define CellKind CharKind

typedef struct {
    int row;
    int col;
    int in; // input byte index; -1 if synthetic
    unsigned char kind;
} MapEntry;

typedef struct {
    MapEntry *v;
    int n, cap;
    int row, col; // live cursor
} MapCtx;

char *compose_text_with_map(const char *input, const CompOptions *opt_in, MapCtx *mc,
                            ParseError *err);
// Back-compat convenience compose: returns newly allocated string.
// NOTE: This remains supported and unchanged in behavior.
char *compose_text(const char *input, const CompOptions *opt, ParseError *err);

// Compose text with cell metadata output.
// If out_meta is non-NULL, returns a parallel array of cell metadata flags (one per codepoint).
// The caller is responsible for freeing both the returned string and *out_meta.
// The metadata array has one entry per output codepoint (not per byte).
char *compose_text_with_meta(const char *input, const CompOptions *opt, uint8_t **out_meta,
                             int *out_meta_len, ParseError *err);

// Baseline tracking for composed content
// Call compositor_reset_baseline() before compose_text*, then compositor_get_baseline() after
void compositor_reset_baseline(void);
int compositor_get_baseline(void);

// Map (input row,col) → (output row,col,kind) using an existing MapCtx
int map_input_coord_to_output_mc(const char *input, int in_row, int in_col, const MapCtx *mc,
                                 int *out_row, int *out_col, int *out_kind);

// Utility: translate (row,col) in input to byte offset in `input`
// (1 column per UTF-8 codepoint — same as compositor counting).
int input_coord_to_byte_index(const char *input, int y, int x);

// Default options (unchanged)
static inline CompOptions default_options(void) {
    return (CompOptions){.width = 72,             // classic terminal width
                         .document_width = 72,    // same as width by default
                         .measuring_mode = false, // normal rendering by default
                         .hyphenate = true,
                         .hyphen_min_left = 2,              // TeX-style conservative defaults
                         .hyphen_min_right = 3,             // helps avoid ugly short endings
                         .parskip = 1,                      // 1 blank line between paragraphs
                         .math_above_skip = 1,              // 1 blank line before display math
                         .math_below_skip = 1,              // 1 blank line after display math
                         .linebreaker = LINEBREAK_GREEDY,   // fast default
                         .alignment = TEXT_ALIGN_JUSTIFIED, // full justification by default
                         // Knuth-Plass defaults
                         .kp_line_penalty = 10.0,
                         .kp_hyphen_penalty = 50.0,
                         .kp_consec_hyphen_penalty = 3000.0,
                         .kp_tolerance = 2.5,
                         // Greedy defaults
                         .gp_short_threshold = 3,
                         .gp_link_threshold = 2,
                         .gp_spread_distance = 2,
                         .gp_neighbor_divisor = 0,
                         .gp_spread_divisor = 2,
                         .gp_min_score = 0};
}

// ComposeResult holds everything you asked for.
//
// Memory ownership: all pointers are heap-allocated by the compositor;
// the caller frees them with free_compose_result().
typedef struct {
    // 1) Typeset text (exactly what compose_text() would return)
    char *out_text;      // NUL-terminated
    size_t out_text_len; // strlen(out_text)

    // 2) Input → Output coordinate map.
    //
    // One Coord per input byte offset (0..in_len-1). For multi-byte UTF-8
    // sequences in the input, *each* byte is given the same output coord
    // (so caret movement by byte or by codepoint both remain stable).
    //
    // IMPORTANT: For contractions like "---"→"—" or "..."→"…", we map
    // ALL involved input bytes to the *same* single output coordinate,
    // so the output caret stays pinned to the rendered glyph regardless
    // of which input byte the caret sits on.
    Coord *in2out;     // array length = in_len_bytes
    size_t in2out_len; // equals input byte length

    // 3) Output character kind map for colorization.
    //
    // One CharKind per output character (including '\n'). This is aligned
    // with the out_text *byte stream* in the simple "one kind per byte"
    // way. Since we only ever emit 1-byte ASCII or 1-column Unicode
    // glyphs in this compositor, this is sufficient for highlighting.
    //
    // For '\n' we set CK_SPACE (you can treat it specially in your UI).
    unsigned char *out_kind; // array length = out_text_len
    size_t out_kind_len;     // equals out_text_len

    // Geometry helpers for the output buffer
    int out_rows;     // number of lines in out_text
    int out_cols_max; // maximum line width (columns)

    // Reserved for future: Output → Input reverse index (optional).
    // For now you can compute it on demand if you need it.
} ComposeResult;

// Compose into a rich result with mapping & kinds.
// Returns true on success; false on OOM or invalid args.
//
// This function preserves *all existing features & behavior* of compose_text()
// and is layout-identical to it. You can safely replace a compose_text()
// call with compose_all()+use res.out_text.
//
// Notes on current mapping fidelity:
// - Text: full-fidelity mapping including smart quotes/dashes/ellipsis,
//   NBSP ties (~ and U+00A0), hyphenation (“foo-” at line end), and
//   justification spaces. All input bytes map to a concrete out coord
//   (no (-1,-1) holes). For contractions (--- → —) all three input
//   bytes map to the dash cell as requested.
// - Math: the entire inline math box ($…$) is currently tagged as
//   CK_MATH_GENERIC in out_kind and all input bytes inside $…$ map
//   to the *nearest baseline row* within the emitted math box,
//   distributing columns left-to-right across the box width.
//   (Fine-grained superscript/subscript/operator-level mapping
//   will be added in a follow-up patch to the parser/renderer
//   by threading source spans through the AST.)
bool compose_all(const char *input, const CompOptions *opt, ComposeResult *out_res);

// Free all memory owned by a ComposeResult and zero it out.
void free_compose_result(ComposeResult *res);

#ifdef __cplusplus
}
#endif

#endif // COMPOSITOR_H