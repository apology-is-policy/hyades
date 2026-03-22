// math_internal.h - Internal shared types and declarations for math renderer
//
// This header is shared by all files in the math/ module.

#ifndef MATH_INTERNAL_H
#define MATH_INTERNAL_H

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "math/ast.h"
#include "render_opts.h"
#include "symbols.h"
#include "utils/utf8.h"
#include "utils/util.h"

// ============================================================================
// Style Management (math_styles.c)
// ============================================================================

// Style stack for \mathbf, \mathbb, \mathcal
void push_style(StyleKind k);
void pop_style(void);
StyleKind current_style(void);

// Text transformation functions
uint32_t latin_to_math_italic(uint32_t c);
uint32_t to_bold(uint32_t c);
uint32_t to_blackboard(uint32_t c);
uint32_t to_script(uint32_t c);
uint32_t to_fraktur(uint32_t c);
uint32_t to_sans(uint32_t c);

// Operator detection
int is_upright_operator_word(const char *s);
const char *map_greek(const char *s);

// AST analysis helpers
int is_upright_function_node(const Ast *node);
int needs_space_in_implicit_mult(const Ast *node, int check_as_lhs);
int is_function_base(const Ast *node);
const Ast *rightmost_element(const Ast *node);
const Ast *leftmost_element(const Ast *node);

// ============================================================================
// Box Utilities (math_boxes.c)
// ============================================================================

// Low-level box operations
void put(Box *b, int x, int y, uint32_t cp);
void put_cell(Box *dst, int dx, int dy, const Box *src, int sx, int sy); // copies cell + metadata
void put_str(Box *b, int x, int y, const char *s);
int str_cols(const char *s);

// Box combination
Box hcat(const Box *A, const Box *B, int gap);
Box hcat_overlap(const Box *A, const Box *B, int overlap);
Box hcat_shifted(const Box *A, const Box *B, int gap, int shift_up);
Box hcat_on_left_axis(const Box *A, const Box *B, int gap);
Box vstack_centered(const Box *A, const Box *B, bool rule);

// Text box creation
Box text_box(const char *s);
Box text_box_from_utf32(const uint32_t *cps, int n);
Box space_box(int n);

// ============================================================================
// Symbol Mapping (math_symbols.c)
// ============================================================================

const char *map_op_char(int unicode, char op);
const char *map_rel_str(int unicode, const char *op);
const char *map_symbol_token(const char *s);
Box symbol_box(const char *s);
int is_known_math_symbol(const char *s);

// ============================================================================
// Big Operators and Delimiters (math_delimiters.c)
// ============================================================================

// Big operators (sum, product, integral, etc.)
Box big_sigma(int size);
Box big_prod(int size);
Box big_int(int size);
Box big_oint(int size);
Box big_iint(int size);
Box big_iiint(int size);
Box big_oiint(int size);
Box big_coprod(int size);
Box big_cup(int size);
Box big_cap(int size);

// Tall delimiters
int ensure_odd_min3(int s);
Box tall_vbar(int size);
Box tall_dvbar(int size);
Box tall_floor_left(int size);
Box tall_floor_right(int size);
Box tall_ceil_left(int size);
Box tall_ceil_right(int size);
Box tall_angle_left(int size);
Box tall_angle_right(int size);

// ============================================================================
// Accents (math_accents.c)
// ============================================================================

uint32_t get_combining_accent(AccentKind ak);
uint32_t get_standalone_accent(AccentKind ak, int unicode_mode);
int is_single_char_box(const Box *b, uint32_t *out_char);
Box render_accent(AccentKind ak, const Box *content);
Box render_brace(bool is_over, const Box *content, const Box *label);

// ============================================================================
// Subscripts/Superscripts (math_scripts.c)
// ============================================================================

Box attach_sup(const Box *base, const Box *sup);
Box attach_sub(const Box *base, const Box *sub);
Box attach_supsub(const Box *base, const Box *sup, const Box *sub);

// ============================================================================
// Math Constructs (math_constructs.c)
// ============================================================================

int rhs_starts_with_paren(const Ast *node);
Box render_bin(const BinOp *b);
Box render_bin_rel(const BinRel *b);
Box render_frac(const Fraction *f);
int count_trailing_spaces(const Box *b);
Box render_lim_op(const LimOp *op, Box (*glyph)(int), int shift_per_size);
Box render_limfunc(const LimFunc *op);
Box render_paren(const Ast *n);
Box render_sqrt(const SqrtOp *sq);
Box render_matrix(const Ast *node);
Box render_embed(const Embed *e);
Box render_overset(const Overset *o);
Box render_boxed(const Ast *child);
Box render_xarrow(const XArrow *xa);
Box render_substack(const Substack *ss);
Box render_tag(const Ast *child);

// ============================================================================
// Main Render (math_render.c)
// ============================================================================

Box render_internal(const Ast *node);
// render_ast is declared in render.h (public API)

#endif // MATH_INTERNAL_H
