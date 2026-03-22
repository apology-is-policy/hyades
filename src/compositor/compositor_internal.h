// compositor_internal.h - Internal types and declarations for compositor
//
// Shared between compositor module files. Not part of public API.

#ifndef COMPOSITOR_INTERNAL_H
#define COMPOSITOR_INTERNAL_H

#include "compositor.h"
#include "math/ast.h"
#include "utils/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// ============================================================================
// String Buffer
// ============================================================================

typedef struct {
    char *s;
    size_t n, cap;
} Str;

void str_init(Str *b);
void str_reserve(Str *b, size_t add);
void str_putc(Str *b, char c);
void str_puts(Str *b, const char *s);
char *str_detach(Str *b);
void str_free(Str *b);

// ============================================================================
// Integer Buffer
// ============================================================================

typedef struct {
    int *v;
    int n, cap;
} I32Buf;

void i32_init(I32Buf *b);
void i32_push(I32Buf *b, int x);
int *i32_detach(I32Buf *b);
void i32_free(I32Buf *b);

// ============================================================================
// Uint8 Buffer (for cell metadata)
// ============================================================================

typedef struct {
    uint8_t *v;
    int n, cap;
} U8Buf;

void u8_init(U8Buf *b);
void u8_push(U8Buf *b, uint8_t x);
void u8_push_n(U8Buf *b, uint8_t x, int count);
uint8_t *u8_detach(U8Buf *b);
void u8_free(U8Buf *b);

// ============================================================================
// Text Styling (textstyle.c)
// ============================================================================

uint32_t char_to_bold(uint32_t c);
uint32_t char_to_italic(uint32_t c);
uint32_t styled_to_ascii(uint32_t c);
bool is_hyphenatable_letter(uint32_t c);
char *convert_text_style(const char *text, char style);
char *normalize_for_hyphenation(const char *utf8_word, int **offsets, int *num_codepoints);

// ============================================================================
// Hyphenation (hyphenate.c)
// ============================================================================

int hyphen_points(const char *w, int *pos, int maxpos, int Lmin, int Rmin);
int hyphen_best_fit(const char *w, int avail, int Lmin, int Rmin);
int hyphen_best_fit_unicode(const char *w, int avail, int Lmin, int Rmin, int *cut_cols);

// ============================================================================
// Segments (segments.c)
// ============================================================================

typedef enum { SEG_TEXT, SEG_INLINE_MATH, SEG_DISPLAY_MATH, SEG_DIRECTIVE, SEG_INLINE_BOX } SegKind;

typedef struct {
    SegKind kind;
    char *text;
    Box box;
    int baseline;
    int src_lo, src_hi;

    // Directive flags
    bool unicode_switch_valid;
    int unicode_on;
    bool width_set;
    int width_value;
    bool hyphenate_set;
    int hyphenate_on;
    bool hyminL_set;
    int hyminL_value;
    bool hyminR_set;
    int hyminR_value;
    bool linebreaker_set;
    int linebreaker_value;
    bool alignment_set;
    int alignment_value;
    bool verbatim;
} Segment;

typedef struct {
    Segment *v;
    int n, cap;
} SegBuf;

void seg_push(SegBuf *sb, Segment s);
void seg_free(SegBuf *sb);

// Block splitting for display math
typedef struct {
    char **blocks;
    bool *is_math;
    int *start;
    int n;
} BlockList;

BlockList split_blocks_display_math_with_pos(const char *input);
void blocks_free(BlockList *bl);

// ============================================================================
// Line Tokens (linebreak.c)
// ============================================================================

typedef enum { LT_TEXT, LT_MATH, LT_NBSP } LineTokKind;

typedef struct {
    LineTokKind kind;
    char *text;
    int width;
    int *col_in;
    bool is_continuation;
    int nb_in;
    Box box;
    int baseline;
} LT;

typedef struct {
    LT *v;
    int n, cap;
} LTBuf;

void lt_push(LTBuf *lb, LT z);
void lt_insert(LTBuf *lb, int pos, LT z);
void lt_replace_with(LTBuf *lb, int pos, LT z);
void lt_free(LTBuf *lb);

typedef struct {
    int start, end;
    int baseline_ascent, baseline_descent;
    int baseline_width;
    int gaps;
} Line;

typedef struct {
    Line *v;
    int n, cap;
} LineBuf;

void line_push(LineBuf *lb, Line L);
void lines_free(LineBuf *lb);

void make_lines_dispatch(LTBuf *lt, int W, const CompOptions *opt, LineBuf *out);

// ============================================================================
// Tokenization (tokenize.c)
// ============================================================================

void explode_text_into_tokens_mapped(const char *out_str, const int *col2in, int col2in_n,
                                     LTBuf *lb);
void flush_word_into_lb_mapped(Str *word, I32Buf *w_map, LTBuf *lb);

// ============================================================================
// Smart Typography (typography.c)
// ============================================================================

void smart_typography_with_map(const char *in, Str *out, I32Buf *col2in);
void passthrough_with_map(const char *in, Str *out, I32Buf *col2in);

// ============================================================================
// Paragraph Rendering (paragraph.c)
// ============================================================================

void render_paragraph_with_map(const SegBuf *seg, int width, int unicode_on, const CompOptions *opt,
                               Str *out, MapCtx *mc);
SegBuf parse_text_to_segments_pos(const char *full_input, const char *txt, int base_off,
                                  ParseError *err);

// ============================================================================
// Inline Commands (commands.c)
// ============================================================================

typedef struct {
    char *name;
    char **args;
    int n_args;
} InlineCommand;

void inline_command_free(InlineCommand *cmd);
InlineCommand *parse_inline_command(const char *input, int *pos);
bool is_output_command(const char *name);
Box render_command_box(InlineCommand *cmd, const CompOptions *opt);
Box string_to_box(const char *str);

// ============================================================================
// Mapping (mapping.c)
// ============================================================================

void mc_newline(MapCtx *mc);
void mc_emit(MapCtx *mc, int input_byte_idx, int kind);
void mc_advance_col(MapCtx *mc, int cols);

// ============================================================================
// Utilities
// ============================================================================

int u8_cols_str(const char *s);
int heuristic_baseline(const Box *b);
int box_baseline(const Box *b);
char *strip_tex_comments(const char *in);

#endif // COMPOSITOR_INTERNAL_H
