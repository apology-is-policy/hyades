// paragraph.c - Paragraph rendering and segment parsing for compositor

#include "compositor_internal.h"
#include "document/document.h" // For verbatim_store_get
#include <stdio.h>
#include "layout/layout.h"
#include "math/ast.h"
#include "math/parser/parser.h"
#include "math/renderer/render_opts.h"
#include "utils/utf8.h"
#include "utils/util.h"
#include "utils/warnings.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// External declarations
extern Box render_ast(const Ast *node);
extern Ast *parse_math(const char *input, ParseError *err);
extern void ast_free(Ast *node);
extern int get_unicode_mode(void);
extern void set_unicode_mode(int mode);

// Baseline tracking for compose_text_with_baseline API
static int g_last_baseline = 0;
static bool g_baseline_set = false;

void compositor_reset_baseline(void) {
    g_last_baseline = 0;
    g_baseline_set = false;
}

int compositor_get_baseline(void) {
    return g_baseline_set ? g_last_baseline : 0;
}

// Create a preformatted Box from text (for verbatim content with newlines)
static Box make_verbatim_box(const char *text) {
    if (!text || !*text) {
        Box empty = make_box(1, 1, 0);
        empty.cells[0] = (uint32_t)' ';
        return empty;
    }

    // Count lines and max width
    int lines = 0;
    int max_width = 0;
    int current_width = 0;

    size_t pos = 0;
    size_t len = strlen(text);

    while (pos < len) {
        uint32_t cp = utf8_next(text, len, &pos);
        if (cp == '\n') {
            lines++;
            if (current_width > max_width) max_width = current_width;
            current_width = 0;
        } else {
            current_width++;
        }
    }

    if (current_width > 0 || lines == 0) {
        lines++;
        if (current_width > max_width) max_width = current_width;
    }

    if (max_width == 0) max_width = 1;

    // Create box
    Box box = make_box(max_width, lines, 0);

    // Fill with spaces first
    for (int i = 0; i < box.w * box.h; i++) {
        box.cells[i] = (uint32_t)' ';
    }

    // Fill with content
    int row = 0, col = 0;
    pos = 0;

    while (pos < len) {
        uint32_t cp = utf8_next(text, len, &pos);
        if (cp == '\n') {
            row++;
            col = 0;
        } else if (col < box.w && row < box.h) {
            box.cells[row * box.w + col] = cp;
            col++;
        }
    }

    return box;
}

// Character kinds
#define CK_TEXT 0
#define CK_SPACE 1
#define CK_NBSP 2
#define CK_MATH_GENERIC 3
#define CK_RULE 4

// ============================================================================
// Map Context Helpers
// ============================================================================

static void mc_note_one(MapCtx *mc, int in_idx, int kind) {
    mc_emit(mc, in_idx, kind);
    mc_advance_col(mc, 1);
}

static void mc_note_repeat(MapCtx *mc, int count, int kind) {
    for (int i = 0; i < count; i++) mc_note_one(mc, -1, kind);
}

// ============================================================================
// Token Emission
// ============================================================================

static void emit_spaces(Str *out, int n) {
    for (int i = 0; i < n; i++) str_putc(out, ' ');
}

static void emit_spaces_meta(Str *out, U8Buf *meta, int n) {
    for (int i = 0; i < n; i++) {
        str_putc(out, ' ');
        if (meta) u8_push(meta, CELL_META_NONE);
    }
}

static void emit_token_row_map(Str *out, const LT *tok, int rel_row, int unicode, MapCtx *mc) {
    if (tok->kind == LT_TEXT) {
        if (rel_row == 0) {
            const unsigned char *p = (const unsigned char *)tok->text;
            int col = 0;
            while (*p) {
                unsigned c = *p;
                int adv = ((c & 0x80) == 0x00)   ? 1
                          : ((c & 0xE0) == 0xC0) ? 2
                          : ((c & 0xF0) == 0xE0) ? 3
                          : ((c & 0xF8) == 0xF0) ? 4
                                                 : 1;
                for (int k = 0; k < adv; k++) str_putc(out, (char)p[k]);
                int in_idx = (tok->col_in && col < tok->width) ? tok->col_in[col] : -1;
                mc_note_one(mc, in_idx, CK_TEXT);
                p += adv;
                col++;
            }
        } else {
            emit_spaces(out, tok->width);
            mc_note_repeat(mc, tok->width, CK_SPACE);
        }
        return;
    }

    if (tok->kind == LT_NBSP) {
        str_putc(out, ' ');
        mc_note_one(mc, tok->nb_in, CK_NBSP);
        return;
    }

    // Math token
    int top = -tok->baseline;
    int bottom = tok->box.h - 1 - tok->baseline;
    if (rel_row < top || rel_row > bottom) {
        emit_spaces(out, tok->box.w);
        mc_note_repeat(mc, tok->box.w, CK_SPACE);
        return;
    }

    int box_y = tok->baseline + rel_row;
    int rule_like = 1;
    for (int x = 0; x < tok->box.w; x++) {
        uint32_t cp = tok->box.cells[box_y * tok->box.w + x];
        if (cp == U'-') continue;
        if (cp != U' ') {
            rule_like = 0;
            break;
        }
    }

    for (int x = 0; x < tok->box.w; x++) {
        uint32_t cp = tok->box.cells[box_y * tok->box.w + x];
        char enc[8]; // 8 bytes for base + combining mark
        size_t n;
        if (unicode && rule_like && cp == U'-')
            n = utf8_encode(0x2500, enc);
        else
            n = encode_cell_utf8(cp, enc);
        for (size_t k = 0; k < n; k++) str_putc(out, enc[k]);
        mc_note_one(mc, -1,
                    (rule_like && cp == U'-') ? CK_RULE
                                              : (cp == U' ' ? CK_SPACE : CK_MATH_GENERIC));
    }
}

// Token emission with metadata tracking
static void emit_token_row_map_meta(Str *out, const LT *tok, int rel_row, int unicode, MapCtx *mc,
                                    U8Buf *meta) {
    if (tok->kind == LT_TEXT) {
        if (rel_row == 0) {
            const unsigned char *p = (const unsigned char *)tok->text;
            int col = 0;
            while (*p) {
                unsigned c = *p;
                int adv = ((c & 0x80) == 0x00)   ? 1
                          : ((c & 0xE0) == 0xC0) ? 2
                          : ((c & 0xF0) == 0xE0) ? 3
                          : ((c & 0xF8) == 0xF0) ? 4
                                                 : 1;
                for (int k = 0; k < adv; k++) str_putc(out, (char)p[k]);
                if (meta) u8_push(meta, CELL_META_NONE); // text has no special metadata
                int in_idx = (tok->col_in && col < tok->width) ? tok->col_in[col] : -1;
                mc_note_one(mc, in_idx, CK_TEXT);
                p += adv;
                col++;
            }
        } else {
            emit_spaces_meta(out, meta, tok->width);
            mc_note_repeat(mc, tok->width, CK_SPACE);
        }
        return;
    }

    if (tok->kind == LT_NBSP) {
        str_putc(out, ' ');
        if (meta) u8_push(meta, CELL_META_NONE);
        mc_note_one(mc, tok->nb_in, CK_NBSP);
        return;
    }

    // Math token - emit cells with metadata
    int top = -tok->baseline;
    int bottom = tok->box.h - 1 - tok->baseline;
    if (rel_row < top || rel_row > bottom) {
        emit_spaces_meta(out, meta, tok->box.w);
        mc_note_repeat(mc, tok->box.w, CK_SPACE);
        return;
    }

    int box_y = tok->baseline + rel_row;
    int rule_like = 1;
    for (int x = 0; x < tok->box.w; x++) {
        uint32_t cp = tok->box.cells[box_y * tok->box.w + x];
        if (cp == U'-') continue;
        if (cp != U' ') {
            rule_like = 0;
            break;
        }
    }

    for (int x = 0; x < tok->box.w; x++) {
        uint32_t cp = tok->box.cells[box_y * tok->box.w + x];
        char enc[8];
        size_t n;
        if (unicode && rule_like && cp == U'-')
            n = utf8_encode(0x2500, enc);
        else
            n = encode_cell_utf8(cp, enc);
        for (size_t k = 0; k < n; k++) str_putc(out, enc[k]);

        // Emit cell metadata from the box
        uint8_t cell_meta = CELL_META_NONE;
        if (tok->box.meta) {
            cell_meta = tok->box.meta[box_y * tok->box.w + x];
        }
        if (meta) u8_push(meta, cell_meta);

        mc_note_one(mc, -1,
                    (rule_like && cp == U'-') ? CK_RULE
                                              : (cp == U' ' ? CK_SPACE : CK_MATH_GENERIC));
    }
}

// ============================================================================
// Line Emission
// ============================================================================

static void emit_lt_lines_with_map(LTBuf *lt, int W, const CompOptions *opt, int unicode_on,
                                   Str *out, MapCtx *mc) {
    if (lt->n == 0) {
        str_putc(out, '\n');
        mc_newline(mc);
        return;
    }

    LineBuf lines = {0};
    make_lines_dispatch(lt, W, opt, &lines);

    for (int L = 0; L < lines.n; L++) {
        Line ln = lines.v[L];
        bool is_last_line = (L == lines.n - 1);

        int token_count = ln.end - ln.start;
        int boundaries = token_count > 0 ? token_count - 1 : 0;

        int *gaps = boundaries ? malloc(boundaries * sizeof(int)) : NULL;
        unsigned char *is_stretchable = boundaries ? calloc(boundaries, 1) : NULL;

        for (int b = 0; b < boundaries; b++) {
            const LT *A = &lt->v[ln.start + b];
            const LT *B = &lt->v[ln.start + b + 1];

            if (A->kind == LT_NBSP || B->kind == LT_NBSP || B->is_continuation) {
                gaps[b] = 0;
                is_stretchable[b] = 0;
            } else if (A->kind == LT_TEXT && B->kind == LT_TEXT) {
                gaps[b] = 1;
                is_stretchable[b] = 1;
            } else {
                gaps[b] = 0; // TeX-like: no auto spacing around inline math
                is_stretchable[b] = 0;
            }
        }

        // Calculate stretch
        int base_width = 0;
        for (int t = ln.start; t < ln.end; t++) base_width += lt->v[t].width;
        for (int b = 0; b < boundaries; b++) base_width += gaps[b];

        int slack = W - base_width;
        int n_stretchable = 0;
        for (int b = 0; b < boundaries; b++)
            if (is_stretchable[b]) n_stretchable++;

        // Distribute slack for justification (skip for ragged alignment)
        // Two refinements for monospace text:
        // 1. Prefer placing extra spaces between longer words (less noticeable)
        // 2. Keep spaces symmetric around short elements (dashes, 1-2 char words)
        bool should_justify = opt->alignment == TEXT_ALIGN_JUSTIFIED && !is_last_line;
        if (should_justify && slack > 0 && n_stretchable > 0) {
            int per_gap = slack / n_stretchable;
            int extra = slack % n_stretchable;

            // First pass: give base amount to all stretchable gaps
            for (int b = 0; b < boundaries; b++) {
                if (is_stretchable[b]) {
                    gaps[b] += per_gap;
                }
            }

            // Second pass: distribute extra spaces with two goals:
            // 1. Hide spaces between longer words (short+long pairs like "The counters" are OK)
            // 2. Spread spaces apart (penalize gaps near already-used ones)
            if (extra > 0) {
                // Calculate base scores for hiding quality
                // If min >= threshold, use max (allows short+long pairs); otherwise use min
                int *base_scores = malloc(boundaries * sizeof(int));
                int *scores = malloc(boundaries * sizeof(int));
                int *linked = malloc(boundaries * sizeof(int)); // -1 or index of linked partner
                int short_thresh = opt->gp_short_threshold;
                int link_thresh = opt->gp_link_threshold;
                int spread_dist = opt->gp_spread_distance;
                int neighbor_div = opt->gp_neighbor_divisor;
                int spread_div = opt->gp_spread_divisor;
                int min_score = opt->gp_min_score;
                for (int b = 0; b < boundaries; b++) {
                    base_scores[b] = 0;
                    scores[b] = 0;
                    linked[b] = -1;
                    if (is_stretchable[b]) {
                        int left_w = lt->v[ln.start + b].width;
                        int right_w = lt->v[ln.start + b + 1].width;
                        int min_w = (left_w < right_w) ? left_w : right_w;
                        int max_w = (left_w > right_w) ? left_w : right_w;
                        // Short+long pairs (min >= threshold) can hide spaces well
                        base_scores[b] = (min_w >= short_thresh) ? max_w : min_w;
                        scores[b] = base_scores[b];
                    }
                }

                // Identify linked pairs: gaps on either side of short tokens
                for (int t = 1; t < token_count - 1; t++) {
                    int tok_w = lt->v[ln.start + t].width;
                    if (tok_w <= link_thresh) {
                        int left_gap = t - 1;
                        int right_gap = t;
                        if (is_stretchable[left_gap] && is_stretchable[right_gap]) {
                            linked[left_gap] = right_gap;
                            linked[right_gap] = left_gap;
                        }
                    }
                }

                // Track which gaps have been used for distance-based spreading
                unsigned char *used = calloc(boundaries, 1);

                // Distribute extra spaces to highest-scored gaps first
                // Linked pairs consume 2 spaces at once (or 0 if not enough)
                int _justify_guard = 0;
                while (extra > 0) {
                    if (++_justify_guard > 1000000) break;
                    int best = -1;
                    int best_score = -1;
                    bool use_linked = true; // Whether to honor linked pair constraint
                    for (int b = 0; b < boundaries; b++) {
                        if (is_stretchable[b] && scores[b] > best_score && scores[b] >= min_score) {
                            // Check if this is a linked pair
                            if (linked[b] >= 0) {
                                // Only consider the lower-indexed one to avoid double-counting
                                if (b < linked[b] && extra >= 2) {
                                    best = b;
                                    best_score = scores[b];
                                }
                            } else {
                                best = b;
                                best_score = scores[b];
                            }
                        }
                    }
                    // Fallback: if no gap found but extra > 0, break linked pair constraint
                    // This happens when all remaining gaps are in linked pairs but extra < 2
                    if (best < 0 && extra > 0) {
                        use_linked = false; // Don't honor linked pairs in fallback
                        for (int b = 0; b < boundaries; b++) {
                            if (is_stretchable[b] && scores[b] > best_score &&
                                scores[b] >= min_score) {
                                best = b;
                                best_score = scores[b];
                            }
                        }
                    }
                    if (best < 0) break;

                    if (use_linked && linked[best] >= 0) {
                        // Add to both gaps in the pair
                        gaps[best]++;
                        gaps[linked[best]]++;
                        scores[best] = -1; // Mark as used
                        scores[linked[best]] = -1;
                        used[best] = 1;
                        used[linked[best]] = 1;
                        extra -= 2;
                    } else {
                        gaps[best]++;
                        scores[best] = -1; // Mark as used
                        used[best] = 1;
                        extra--;
                    }

                    // Apply distance-based penalty to spread spaces apart
                    // Closer gaps get stronger penalty (controlled by spread_dist)
                    if (spread_dist > 0) {
                        for (int b = 0; b < boundaries; b++) {
                            if (scores[b] > 0 && !used[b]) {
                                // Find distance to nearest used gap
                                int nearest_dist = boundaries;
                                for (int u = 0; u < boundaries; u++) {
                                    if (used[u]) {
                                        int d = (b > u) ? (b - u) : (u - b);
                                        if (d < nearest_dist) nearest_dist = d;
                                    }
                                }
                                // Penalize based on distance: closer = more penalty
                                // dist 1: neighbor_div (0=disable, else divide)
                                // dist 2..spread_dist: divide by spread_div
                                // dist > spread_dist: no change
                                if (nearest_dist == 1) {
                                    if (neighbor_div == 0) {
                                        scores[b] = 0;
                                    } else {
                                        scores[b] = base_scores[b] / neighbor_div;
                                    }
                                } else if (nearest_dist <= spread_dist) {
                                    scores[b] = base_scores[b] / spread_div;
                                }
                            }
                        }
                    }
                }

                free(base_scores);
                free(scores);
                free(linked);
                free(used);
            }
        }

        // Emit rows
        int asc = ln.baseline_ascent;
        int desc = ln.baseline_descent;
        int total_rows = asc + 1 + desc;

        for (int row = 0; row < total_rows; row++) {
            int rel_row = row - asc;

            for (int t = ln.start; t < ln.end; t++) {
                emit_token_row_map(out, &lt->v[t], rel_row, unicode_on, mc);
                int b = t - ln.start;
                if (b < boundaries) {
                    emit_spaces(out, gaps[b]);
                    mc_note_repeat(mc, gaps[b], CK_SPACE);
                }
            }
            str_putc(out, '\n');
            mc_newline(mc);
        }

        free(gaps);
        free(is_stretchable);
    }

    lines_free(&lines);
}

// Line emission with metadata tracking
static void emit_lt_lines_with_map_meta(LTBuf *lt, int W, const CompOptions *opt, int unicode_on,
                                        Str *out, MapCtx *mc, U8Buf *meta) {
    if (lt->n == 0) {
        str_putc(out, '\n');
        if (meta) u8_push(meta, CELL_META_NONE);
        mc_newline(mc);
        return;
    }

    LineBuf lines = {0};
    make_lines_dispatch(lt, W, opt, &lines);

    // Capture first line's baseline for the compositor API
    if (lines.n > 0 && !g_baseline_set) {
        g_last_baseline = lines.v[0].baseline_ascent;
        g_baseline_set = true;
    }

    for (int L = 0; L < lines.n; L++) {
        Line ln = lines.v[L];
        bool is_last_line = (L == lines.n - 1);

        int token_count = ln.end - ln.start;
        int boundaries = token_count > 0 ? token_count - 1 : 0;

        int *gaps = boundaries ? malloc(boundaries * sizeof(int)) : NULL;
        unsigned char *is_stretchable = boundaries ? calloc(boundaries, 1) : NULL;

        for (int b = 0; b < boundaries; b++) {
            const LT *A = &lt->v[ln.start + b];
            const LT *B = &lt->v[ln.start + b + 1];

            if (A->kind == LT_NBSP || B->kind == LT_NBSP || B->is_continuation) {
                gaps[b] = 0;
                is_stretchable[b] = 0;
            } else if (A->kind == LT_TEXT && B->kind == LT_TEXT) {
                gaps[b] = 1;
                is_stretchable[b] = 1;
            } else {
                gaps[b] = 0;
                is_stretchable[b] = 0;
            }
        }

        int base_width = 0;
        for (int t = ln.start; t < ln.end; t++) base_width += lt->v[t].width;
        for (int b = 0; b < boundaries; b++) base_width += gaps[b];

        int slack = W - base_width;
        int n_stretchable = 0;
        for (int b = 0; b < boundaries; b++)
            if (is_stretchable[b]) n_stretchable++;

        // Distribute slack for justification (skip for ragged alignment)
        // Two refinements for monospace text:
        // 1. Prefer placing extra spaces between longer words (less noticeable)
        // 2. Keep spaces symmetric around short elements (dashes, 1-2 char words)
        bool should_justify = opt->alignment == TEXT_ALIGN_JUSTIFIED && !is_last_line;
        if (should_justify && slack > 0 && n_stretchable > 0) {
            int per_gap = slack / n_stretchable;
            int extra = slack % n_stretchable;

            // First pass: give base amount to all stretchable gaps
            for (int b = 0; b < boundaries; b++) {
                if (is_stretchable[b]) {
                    gaps[b] += per_gap;
                }
            }

            // Second pass: distribute extra spaces with two goals:
            // 1. Hide spaces between longer words (short+long pairs like "The counters" are OK)
            // 2. Spread spaces apart (penalize gaps near already-used ones)
            if (extra > 0) {
                // Calculate base scores for hiding quality
                // If min >= threshold, use max (allows short+long pairs); otherwise use min
                int *base_scores = malloc(boundaries * sizeof(int));
                int *scores = malloc(boundaries * sizeof(int));
                int *linked = malloc(boundaries * sizeof(int)); // -1 or index of linked partner
                int short_thresh = opt->gp_short_threshold;
                int link_thresh = opt->gp_link_threshold;
                int spread_dist = opt->gp_spread_distance;
                int neighbor_div = opt->gp_neighbor_divisor;
                int spread_div = opt->gp_spread_divisor;
                int min_score = opt->gp_min_score;
                for (int b = 0; b < boundaries; b++) {
                    base_scores[b] = 0;
                    scores[b] = 0;
                    linked[b] = -1;
                    if (is_stretchable[b]) {
                        int left_w = lt->v[ln.start + b].width;
                        int right_w = lt->v[ln.start + b + 1].width;
                        int min_w = (left_w < right_w) ? left_w : right_w;
                        int max_w = (left_w > right_w) ? left_w : right_w;
                        // Short+long pairs (min >= threshold) can hide spaces well
                        base_scores[b] = (min_w >= short_thresh) ? max_w : min_w;
                        scores[b] = base_scores[b];
                    }
                }

                // Identify linked pairs: gaps on either side of short tokens
                for (int t = 1; t < token_count - 1; t++) {
                    int tok_w = lt->v[ln.start + t].width;
                    if (tok_w <= link_thresh) {
                        int left_gap = t - 1;
                        int right_gap = t;
                        if (is_stretchable[left_gap] && is_stretchable[right_gap]) {
                            linked[left_gap] = right_gap;
                            linked[right_gap] = left_gap;
                        }
                    }
                }

                // Track which gaps have been used for distance-based spreading
                unsigned char *used = calloc(boundaries, 1);

                // Distribute extra spaces to highest-scored gaps first
                // Linked pairs consume 2 spaces at once (or 0 if not enough)
                int _justify_guard = 0;
                while (extra > 0) {
                    if (++_justify_guard > 1000000) break;
                    int best = -1;
                    int best_score = -1;
                    bool use_linked = true; // Whether to honor linked pair constraint
                    for (int b = 0; b < boundaries; b++) {
                        if (is_stretchable[b] && scores[b] > best_score && scores[b] >= min_score) {
                            // Check if this is a linked pair
                            if (linked[b] >= 0) {
                                // Only consider the lower-indexed one to avoid double-counting
                                if (b < linked[b] && extra >= 2) {
                                    best = b;
                                    best_score = scores[b];
                                }
                            } else {
                                best = b;
                                best_score = scores[b];
                            }
                        }
                    }
                    // Fallback: if no gap found but extra > 0, break linked pair constraint
                    // This happens when all remaining gaps are in linked pairs but extra < 2
                    if (best < 0 && extra > 0) {
                        use_linked = false; // Don't honor linked pairs in fallback
                        for (int b = 0; b < boundaries; b++) {
                            if (is_stretchable[b] && scores[b] > best_score &&
                                scores[b] >= min_score) {
                                best = b;
                                best_score = scores[b];
                            }
                        }
                    }
                    if (best < 0) break;

                    if (use_linked && linked[best] >= 0) {
                        // Add to both gaps in the pair
                        gaps[best]++;
                        gaps[linked[best]]++;
                        scores[best] = -1; // Mark as used
                        scores[linked[best]] = -1;
                        used[best] = 1;
                        used[linked[best]] = 1;
                        extra -= 2;
                    } else {
                        gaps[best]++;
                        scores[best] = -1; // Mark as used
                        used[best] = 1;
                        extra--;
                    }

                    // Apply distance-based penalty to spread spaces apart
                    // Closer gaps get stronger penalty (controlled by spread_dist)
                    if (spread_dist > 0) {
                        for (int b = 0; b < boundaries; b++) {
                            if (scores[b] > 0 && !used[b]) {
                                // Find distance to nearest used gap
                                int nearest_dist = boundaries;
                                for (int u = 0; u < boundaries; u++) {
                                    if (used[u]) {
                                        int d = (b > u) ? (b - u) : (u - b);
                                        if (d < nearest_dist) nearest_dist = d;
                                    }
                                }
                                // Penalize based on distance: closer = more penalty
                                // dist 1: neighbor_div (0=disable, else divide)
                                // dist 2..spread_dist: divide by spread_div
                                // dist > spread_dist: no change
                                if (nearest_dist == 1) {
                                    if (neighbor_div == 0) {
                                        scores[b] = 0;
                                    } else {
                                        scores[b] = base_scores[b] / neighbor_div;
                                    }
                                } else if (nearest_dist <= spread_dist) {
                                    scores[b] = base_scores[b] / spread_div;
                                }
                            }
                        }
                    }
                }

                free(base_scores);
                free(scores);
                free(linked);
                free(used);
            }
        }

        int asc = ln.baseline_ascent;
        int desc = ln.baseline_descent;
        int total_rows = asc + 1 + desc;

        for (int row = 0; row < total_rows; row++) {
            int rel_row = row - asc;

            for (int t = ln.start; t < ln.end; t++) {
                emit_token_row_map_meta(out, &lt->v[t], rel_row, unicode_on, mc, meta);
                int b = t - ln.start;
                if (b < boundaries) {
                    emit_spaces_meta(out, meta, gaps[b]);
                    mc_note_repeat(mc, gaps[b], CK_SPACE);
                }
            }
            str_putc(out, '\n');
            if (meta) u8_push(meta, CELL_META_NONE);
            mc_newline(mc);
        }

        free(gaps);
        free(is_stretchable);
    }

    lines_free(&lines);
}

// ============================================================================
// NBSP Coalescing
// ============================================================================

// Merge TEXT~NBSP~TEXT into a single TEXT; preserve mapping:
// insert a ' ' between parts and set its in_idx to NBSP's source index.
static void coalesce_nbsp_text_runs(LTBuf *lt) {
    LT *v = lt->v;
    int n = lt->n;
    LTBuf out = {0};

    for (int k = 0; k < n;) {
        if (v[k].kind == LT_TEXT) {
            Str acc;
            str_init(&acc);
            I32Buf map;
            i32_init(&map);

            // take first
            str_puts(&acc, v[k].text);
            for (int c = 0; c < v[k].width; c++) i32_push(&map, v[k].col_in ? v[k].col_in[c] : -1);
            free(v[k].text);
            free(v[k].col_in);

            int t = k + 1;
            while (t + 1 < n && v[t].kind == LT_NBSP && v[t + 1].kind == LT_TEXT) {
                // insert a single space, mapped to NBSP source
                str_putc(&acc, ' ');
                i32_push(&map, v[t].nb_in);

                // append next TEXT
                str_puts(&acc, v[t + 1].text);
                for (int c = 0; c < v[t + 1].width; c++)
                    i32_push(&map, v[t + 1].col_in ? v[t + 1].col_in[c] : -1);

                free(v[t + 1].text);
                free(v[t + 1].col_in);
                t += 2; // consumed NBSP and TEXT
            }

            char *merged = str_detach(&acc);
            int *marr = i32_detach(&map);
            lt_push(&out, (LT){.kind = LT_TEXT,
                               .text = merged,
                               .width = u8_cols_str(merged),
                               .col_in = marr,
                               .is_continuation = v[k].is_continuation});
            k = t;
            continue;
        }

        // Pass-through non-TEXT (NBSP/MATH)
        lt_push(&out, v[k]);
        k++;
    }

    free(lt->v);
    *lt = out;
}

// ============================================================================
// Paragraph Rendering
// ============================================================================

void render_paragraph_with_map(const SegBuf *seg, int width, int unicode_on, const CompOptions *opt,
                               Str *out, MapCtx *mc) {
    CompOptions local_opt = *opt;
    int W = width;

    LTBuf lt = {0};
    bool prev_text_ended_with_space = false;
    bool prev_was_math = false;
    bool prev_verbatim_no_space = false;
    bool prev_seg_no_trailing_space = false;

    for (int i = 0; i < seg->n; i++) {
        const Segment *s = &seg->v[i];

        if (s->kind == SEG_DIRECTIVE) {
            if (s->unicode_switch_valid) {
                set_unicode_mode(s->unicode_on);
                unicode_on = s->unicode_on;
            }
            if (s->width_set) W = s->width_value;
            if (s->hyphenate_set) {
                local_opt.hyphenate = s->hyphenate_on;
                if (local_opt.hyphenate) {
                    if (local_opt.hyphen_min_left < 2) local_opt.hyphen_min_left = 2;
                    if (local_opt.hyphen_min_right < 3) local_opt.hyphen_min_right = 3;
                }
            }
            if (s->hyminL_set) local_opt.hyphen_min_left = s->hyminL_value;
            if (s->hyminR_set) local_opt.hyphen_min_right = s->hyminR_value;
            if (s->linebreaker_set) local_opt.linebreaker = s->linebreaker_value;
            if (s->alignment_set) local_opt.alignment = s->alignment_value;
            continue;
        }

        if (s->kind == SEG_DISPLAY_MATH) {
            if (lt.n > 0) {
                emit_lt_lines_with_map(&lt, W, &local_opt, unicode_on, out, mc);
                lt_free(&lt);
                lt = (LTBuf){0};
            }

            int tw = s->box.tag_width;
            int eq_w = s->box.w - tw;
            int pad;
            if (tw > 0 && eq_w > 0 && s->box.w < W) {
                pad = (W - eq_w) / 2;
                if (pad < 0) pad = 0;
            } else {
                pad = (s->box.w < W) ? (W - s->box.w) / 2 : 0;
            }
            for (int y = 0; y < s->box.h; y++) {
                if (tw > 0 && eq_w > 0 && s->box.w < W) {
                    emit_spaces(out, pad);
                    mc_note_repeat(mc, pad, CK_SPACE);
                    for (int x = 0; x < eq_w; x++) {
                        uint32_t cp = s->box.cells[y * s->box.w + x];
                        char enc[8];
                        size_t n = encode_cell_utf8(cp, enc);
                        for (size_t k = 0; k < n; k++) str_putc(out, enc[k]);
                        mc_note_one(mc, -1, cp == U' ' ? CK_SPACE : CK_MATH_GENERIC);
                    }
                    int tag_start = W - tw;
                    int fill = tag_start - (pad + eq_w);
                    if (fill < 0) fill = 0;
                    emit_spaces(out, fill);
                    mc_note_repeat(mc, fill, CK_SPACE);
                    for (int x = eq_w; x < s->box.w; x++) {
                        uint32_t cp = s->box.cells[y * s->box.w + x];
                        char enc[8];
                        size_t n = encode_cell_utf8(cp, enc);
                        for (size_t k = 0; k < n; k++) str_putc(out, enc[k]);
                        mc_note_one(mc, -1, cp == U' ' ? CK_SPACE : CK_MATH_GENERIC);
                    }
                } else {
                    emit_spaces(out, pad);
                    mc_note_repeat(mc, pad, CK_SPACE);
                    for (int x = 0; x < s->box.w; x++) {
                        uint32_t cp = s->box.cells[y * s->box.w + x];
                        char enc[8];
                        size_t n = encode_cell_utf8(cp, enc);
                        for (size_t k = 0; k < n; k++) str_putc(out, enc[k]);
                        mc_note_one(mc, -1, cp == U' ' ? CK_SPACE : CK_MATH_GENERIC);
                    }
                }
                str_putc(out, '\n');
                mc_newline(mc);
            }
            continue;
        }

        if (s->kind == SEG_TEXT) {
            Str transformed;
            I32Buf col2in;

            if (s->verbatim || !unicode_on)
                passthrough_with_map(s->text, &transformed, &col2in);
            else
                smart_typography_with_map(s->text, &transformed, &col2in);

            // Adjust offsets
            for (int j = 0; j < col2in.n; j++) {
                if (col2in.v[j] >= 0) col2in.v[j] += s->src_lo;
            }

            // TeX-like spacing: if text starts with space and follows math, preserve it
            // Use LT_NBSP to avoid creating a gap with adjacent tokens
            bool starts_with_space = (transformed.n > 0 && transformed.s[0] == ' ');
            if (prev_was_math && starts_with_space) {
                lt_push(&lt, (LT){.kind = LT_NBSP, .width = 1});
            }

            // Verbatim segments cause text splits that don't exist in source.
            // Suppress spurious inter-word gaps at these boundaries:
            // - prev ended without space + current is verbatim (text→verb join)
            // - prev was verbatim without trailing space + current starts without space (verb→text join)
            bool no_leading_space = transformed.n > 0 && transformed.s[0] != ' ';
            bool needs_join = no_leading_space && ((s->verbatim && prev_seg_no_trailing_space) ||
                                                   prev_verbatim_no_space);
            int lt_before = lt.n;
            explode_text_into_tokens_mapped(transformed.s, col2in.v, col2in.n, &lt);
            if (needs_join && lt.n > lt_before) {
                lt.v[lt_before].is_continuation = true;
            }

            bool no_trailing_space = transformed.n > 0 && transformed.s[transformed.n - 1] != ' ';
            prev_verbatim_no_space = s->verbatim && no_trailing_space;
            prev_seg_no_trailing_space = no_trailing_space;

            // Track if this text ends with space (for following math)
            prev_text_ended_with_space =
                (transformed.n > 0 && transformed.s[transformed.n - 1] == ' ');

            str_free(&transformed);
            i32_free(&col2in);
            prev_was_math = false;
            continue;
        }

        if (s->kind == SEG_INLINE_MATH || s->kind == SEG_INLINE_BOX) {
            // TeX-like spacing: if previous text ended with space, preserve it
            // Use LT_NBSP to avoid creating a gap with adjacent tokens
            if (prev_text_ended_with_space) {
                lt_push(&lt, (LT){.kind = LT_NBSP, .width = 1});
            }

            // Copy the box (including metadata)
            Box box_copy = make_box(s->box.w, s->box.h, s->box.baseline);
            if (box_copy.cells && s->box.cells) {
                memcpy(box_copy.cells, s->box.cells, s->box.w * s->box.h * sizeof(uint32_t));
            }
            // Also copy metadata if present
            if (s->box.meta) {
                box_ensure_meta(&box_copy);
                if (box_copy.meta) {
                    memcpy(box_copy.meta, s->box.meta, s->box.w * s->box.h * sizeof(uint8_t));
                }
            }
            // Copy style if present
            if (s->box.style) {
                box_ensure_style(&box_copy);
                if (box_copy.style) {
                    memcpy(box_copy.style, s->box.style, s->box.w * s->box.h * sizeof(uint16_t));
                }
            }
            lt_push(
                &lt,
                (LT){.kind = LT_MATH, .box = box_copy, .width = s->box.w, .baseline = s->baseline});

            prev_text_ended_with_space = false;
            prev_was_math = true;
        }
    }

    if (lt.n > 0) {
        coalesce_nbsp_text_runs(&lt);
        emit_lt_lines_with_map(&lt, W, &local_opt, unicode_on, out, mc);
        lt_free(&lt);
    } else {
        str_putc(out, '\n');
        mc_newline(mc);
    }
}

// Paragraph rendering with metadata tracking
void render_paragraph_with_map_meta(const SegBuf *seg, int width, int unicode_on,
                                    const CompOptions *opt, Str *out, MapCtx *mc, U8Buf *meta) {
    CompOptions local_opt = *opt;
    int W = width;

    LTBuf lt = {0};
    bool prev_text_ended_with_space = false;
    bool prev_was_math = false;
    bool prev_verbatim_no_space = false;
    bool prev_seg_no_trailing_space = false;

    for (int i = 0; i < seg->n; i++) {
        const Segment *s = &seg->v[i];

        if (s->kind == SEG_DIRECTIVE) {
            if (s->unicode_switch_valid) {
                set_unicode_mode(s->unicode_on);
                unicode_on = s->unicode_on;
            }
            if (s->width_set) W = s->width_value;
            if (s->hyphenate_set) {
                local_opt.hyphenate = s->hyphenate_on;
                if (local_opt.hyphenate) {
                    if (local_opt.hyphen_min_left < 2) local_opt.hyphen_min_left = 2;
                    if (local_opt.hyphen_min_right < 3) local_opt.hyphen_min_right = 3;
                }
            }
            if (s->hyminL_set) local_opt.hyphen_min_left = s->hyminL_value;
            if (s->hyminR_set) local_opt.hyphen_min_right = s->hyminR_value;
            if (s->linebreaker_set) local_opt.linebreaker = s->linebreaker_value;
            if (s->alignment_set) local_opt.alignment = s->alignment_value;
            continue;
        }

        if (s->kind == SEG_DISPLAY_MATH) {
            if (lt.n > 0) {
                emit_lt_lines_with_map_meta(&lt, W, &local_opt, unicode_on, out, mc, meta);
                lt_free(&lt);
                lt = (LTBuf){0};
            }

            int tw = s->box.tag_width;
            int eq_w = s->box.w - tw;
            int pad;
            if (tw > 0 && eq_w > 0 && s->box.w < W) {
                pad = (W - eq_w) / 2;
                if (pad < 0) pad = 0;
            } else {
                pad = (s->box.w < W) ? (W - s->box.w) / 2 : 0;
            }
            for (int y = 0; y < s->box.h; y++) {
                if (tw > 0 && eq_w > 0 && s->box.w < W) {
                    emit_spaces_meta(out, meta, pad);
                    mc_note_repeat(mc, pad, CK_SPACE);
                    for (int x = 0; x < eq_w; x++) {
                        uint32_t cp = s->box.cells[y * s->box.w + x];
                        char enc[8];
                        size_t n = encode_cell_utf8(cp, enc);
                        for (size_t k = 0; k < n; k++) str_putc(out, enc[k]);
                        uint8_t cell_meta = CELL_META_NONE;
                        if (s->box.meta) cell_meta = s->box.meta[y * s->box.w + x];
                        if (meta) u8_push(meta, cell_meta);
                        mc_note_one(mc, -1, cp == U' ' ? CK_SPACE : CK_MATH_GENERIC);
                    }
                    int tag_start = W - tw;
                    int fill = tag_start - (pad + eq_w);
                    if (fill < 0) fill = 0;
                    emit_spaces_meta(out, meta, fill);
                    mc_note_repeat(mc, fill, CK_SPACE);
                    for (int x = eq_w; x < s->box.w; x++) {
                        uint32_t cp = s->box.cells[y * s->box.w + x];
                        char enc[8];
                        size_t n = encode_cell_utf8(cp, enc);
                        for (size_t k = 0; k < n; k++) str_putc(out, enc[k]);
                        uint8_t cell_meta = CELL_META_NONE;
                        if (s->box.meta) cell_meta = s->box.meta[y * s->box.w + x];
                        if (meta) u8_push(meta, cell_meta);
                        mc_note_one(mc, -1, cp == U' ' ? CK_SPACE : CK_MATH_GENERIC);
                    }
                } else {
                    emit_spaces_meta(out, meta, pad);
                    mc_note_repeat(mc, pad, CK_SPACE);
                    for (int x = 0; x < s->box.w; x++) {
                        uint32_t cp = s->box.cells[y * s->box.w + x];
                        char enc[8];
                        size_t n = encode_cell_utf8(cp, enc);
                        for (size_t k = 0; k < n; k++) str_putc(out, enc[k]);
                        uint8_t cell_meta = CELL_META_NONE;
                        if (s->box.meta) cell_meta = s->box.meta[y * s->box.w + x];
                        if (meta) u8_push(meta, cell_meta);
                        mc_note_one(mc, -1, cp == U' ' ? CK_SPACE : CK_MATH_GENERIC);
                    }
                }
                str_putc(out, '\n');
                if (meta) u8_push(meta, CELL_META_NONE);
                mc_newline(mc);
            }
            continue;
        }

        if (s->kind == SEG_TEXT) {
            Str transformed;
            I32Buf col2in;

            if (s->verbatim || !unicode_on)
                passthrough_with_map(s->text, &transformed, &col2in);
            else
                smart_typography_with_map(s->text, &transformed, &col2in);

            for (int j = 0; j < col2in.n; j++) {
                if (col2in.v[j] >= 0) col2in.v[j] += s->src_lo;
            }

            bool starts_with_space = (transformed.n > 0 && transformed.s[0] == ' ');
            if (prev_was_math && starts_with_space) {
                lt_push(&lt, (LT){.kind = LT_NBSP, .width = 1});
            }

            bool no_leading_space = transformed.n > 0 && transformed.s[0] != ' ';
            bool needs_join = no_leading_space && ((s->verbatim && prev_seg_no_trailing_space) ||
                                                   prev_verbatim_no_space);
            int lt_before = lt.n;
            explode_text_into_tokens_mapped(transformed.s, col2in.v, col2in.n, &lt);
            if (needs_join && lt.n > lt_before) {
                lt.v[lt_before].is_continuation = true;
            }

            bool no_trailing_space = transformed.n > 0 && transformed.s[transformed.n - 1] != ' ';
            prev_verbatim_no_space = s->verbatim && no_trailing_space;
            prev_seg_no_trailing_space = no_trailing_space;

            prev_text_ended_with_space =
                (transformed.n > 0 && transformed.s[transformed.n - 1] == ' ');

            str_free(&transformed);
            i32_free(&col2in);
            prev_was_math = false;
            continue;
        }

        if (s->kind == SEG_INLINE_MATH || s->kind == SEG_INLINE_BOX) {
            if (prev_text_ended_with_space) {
                lt_push(&lt, (LT){.kind = LT_NBSP, .width = 1});
            }

            // Copy the box (including metadata)
            Box box_copy = make_box(s->box.w, s->box.h, s->box.baseline);
            if (box_copy.cells && s->box.cells) {
                memcpy(box_copy.cells, s->box.cells, s->box.w * s->box.h * sizeof(uint32_t));
            }
            if (s->box.meta) {
                box_ensure_meta(&box_copy);
                if (box_copy.meta) {
                    memcpy(box_copy.meta, s->box.meta, s->box.w * s->box.h * sizeof(uint8_t));
                }
            }
            // Copy style if present
            if (s->box.style) {
                box_ensure_style(&box_copy);
                if (box_copy.style) {
                    memcpy(box_copy.style, s->box.style, s->box.w * s->box.h * sizeof(uint16_t));
                }
            }
            lt_push(
                &lt,
                (LT){.kind = LT_MATH, .box = box_copy, .width = s->box.w, .baseline = s->baseline});

            prev_text_ended_with_space = false;
            prev_was_math = true;
        }
    }

    if (lt.n > 0) {
        coalesce_nbsp_text_runs(&lt);
        emit_lt_lines_with_map_meta(&lt, W, &local_opt, unicode_on, out, mc, meta);
        lt_free(&lt);
    } else {
        str_putc(out, '\n');
        if (meta) u8_push(meta, CELL_META_NONE);
        mc_newline(mc);
    }
}

// Emit styled content that may contain inline $...$ math.
// Text parts are styled with convert_text_style(); math parts are parsed and rendered as boxes.
static void emit_styled_content_with_math(SegBuf *sb, const char *content, char style,
                                          ParseError *err, int src_lo, int src_hi) {
    const char *scan = content;
    while (*scan) {
        // Find next '$' (not "$$" which is display math)
        const char *dollar = scan;
        while (*dollar && *dollar != '$') dollar++;
        if (!*dollar) {
            // No more math — style remaining text
            if (scan < dollar) {
                char *styled = convert_text_style(scan, style);
                if (styled && *styled) {
                    if (sb->n > 0 && sb->v[sb->n - 1].kind == SEG_TEXT &&
                        !sb->v[sb->n - 1].verbatim) {
                        Segment *prev = &sb->v[sb->n - 1];
                        size_t prev_len = strlen(prev->text);
                        size_t st_len = strlen(styled);
                        char *merged = malloc(prev_len + st_len + 1);
                        memcpy(merged, prev->text, prev_len);
                        memcpy(merged + prev_len, styled, st_len + 1);
                        free(prev->text);
                        prev->text = merged;
                        prev->src_hi = src_hi;
                    } else {
                        Segment s = {0};
                        s.kind = SEG_TEXT;
                        s.text = strdup(styled);
                        s.src_lo = src_lo;
                        s.src_hi = src_hi;
                        seg_push(sb, s);
                    }
                }
                free(styled);
            }
            break;
        }
        // Emit text before '$'
        if (dollar > scan) {
            char *part = strndup(scan, dollar - scan);
            char *styled = convert_text_style(part, style);
            if (styled && *styled) {
                if (sb->n > 0 && sb->v[sb->n - 1].kind == SEG_TEXT && !sb->v[sb->n - 1].verbatim) {
                    Segment *prev = &sb->v[sb->n - 1];
                    size_t prev_len = strlen(prev->text);
                    size_t st_len = strlen(styled);
                    char *merged = malloc(prev_len + st_len + 1);
                    memcpy(merged, prev->text, prev_len);
                    memcpy(merged + prev_len, styled, st_len + 1);
                    free(prev->text);
                    prev->text = merged;
                    prev->src_hi = src_hi;
                } else {
                    Segment s = {0};
                    s.kind = SEG_TEXT;
                    s.text = strdup(styled);
                    s.src_lo = src_lo;
                    s.src_hi = src_hi;
                    seg_push(sb, s);
                }
            }
            free(part);
            free(styled);
        }
        // Find closing '$'
        const char *close = strchr(dollar + 1, '$');
        if (!close) {
            // Unmatched '$' — treat rest as styled text
            char *styled = convert_text_style(dollar, style);
            if (styled && *styled) {
                if (sb->n > 0 && sb->v[sb->n - 1].kind == SEG_TEXT && !sb->v[sb->n - 1].verbatim) {
                    Segment *prev = &sb->v[sb->n - 1];
                    size_t prev_len = strlen(prev->text);
                    size_t st_len = strlen(styled);
                    char *merged = malloc(prev_len + st_len + 1);
                    memcpy(merged, prev->text, prev_len);
                    memcpy(merged + prev_len, styled, st_len + 1);
                    free(prev->text);
                    prev->text = merged;
                    prev->src_hi = src_hi;
                } else {
                    Segment s = {0};
                    s.kind = SEG_TEXT;
                    s.text = strdup(styled);
                    s.src_lo = src_lo;
                    s.src_hi = src_hi;
                    seg_push(sb, s);
                }
            }
            free(styled);
            break;
        }
        // Parse math between $...$
        size_t math_len = close - (dollar + 1);
        char *math_str = strndup(dollar + 1, math_len);
        Ast *ast = parse_math(math_str, err);
        if (ast) {
            Box box = render_ast(ast);
            ast_free(ast);
            Segment s = {0};
            s.kind = SEG_INLINE_MATH;
            s.box = box;
            s.baseline = box_baseline(&box);
            s.src_lo = src_lo;
            s.src_hi = src_hi;
            seg_push(sb, s);
        }
        free(math_str);
        scan = close + 1;
    }
}

// ============================================================================
// Segment Parsing
// ============================================================================

SegBuf parse_text_to_segments_pos(const char *full_input, const char *txt, int base_off,
                                  ParseError *err) {
    SegBuf sb = {0};
    const char *p = txt;

    while (*p) {
        // Check for directives
        if (*p == '%') {
            if (strncmp(p, "%unicode=", 9) == 0) {
                const char *v = p + 9;
                int b = -1;
                if (strncmp(v, "true", 4) == 0)
                    b = 1;
                else if (strncmp(v, "false", 5) == 0)
                    b = 0;
                if (b != -1) {
                    Segment s = {0};
                    s.kind = SEG_DIRECTIVE;
                    s.unicode_switch_valid = true;
                    s.unicode_on = b;
                    s.src_lo = base_off + (int)(p - txt);
                    s.src_hi = s.src_lo + 9 + (b ? 4 : 5);
                    seg_push(&sb, s);
                    p += 9 + (b ? 4 : 5);
                    if (*p == ' ') p++;
                    continue;
                }
            }
            if (strncmp(p, "%width=", 7) == 0) {
                const char *q = p + 7;
                if (isdigit((unsigned char)*q)) {
                    char *end;
                    long val = strtol(q, &end, 10);
                    if (end != q && val > 0) {
                        Segment s = {0};
                        s.kind = SEG_DIRECTIVE;
                        s.width_set = true;
                        s.width_value = (int)val;
                        s.src_lo = base_off + (int)(p - txt);
                        s.src_hi = base_off + (int)(end - txt);
                        seg_push(&sb, s);
                        p = end;
                        if (*p == ' ') p++;
                        continue;
                    }
                }
            }
            // More directives would go here...
        }

        // Check for inline math $...$ (but not ${name} variable access)
        if (*p == '$' && p[1] != '$' && p[1] != '{') {
            const char *end = strchr(p + 1, '$');
            if (end) {
                size_t len = end - (p + 1);
                char *math_str = malloc(len + 1);
                memcpy(math_str, p + 1, len);
                math_str[len] = 0;

                Ast *ast = parse_math(math_str, err);
                if (ast) {
                    Box box = render_ast(ast);
                    ast_free(ast);

                    Segment s = {0};
                    s.kind = SEG_INLINE_MATH;
                    s.box = box;
                    s.baseline = box_baseline(&box);
                    s.src_lo = base_off + (int)(p - txt);
                    s.src_hi = base_off + (int)(end - txt) + 1;
                    seg_push(&sb, s);
                }
                free(math_str);
                p = end + 1;
                continue;
            }
        }

        // Handle \{ and \} for literal braces (before general command parsing)
        if (*p == '\\' && (p[1] == '{' || p[1] == '}')) {
            char brace_char = p[1];
            p += 2; // Skip \{ or \}

            // Merge with previous SEG_TEXT if exists
            // Note: This is safe even for \{\} because the segment text "{}" won't be
            // filtered - filtering only happens when collecting regular text, not here.
            if (sb.n > 0 && sb.v[sb.n - 1].kind == SEG_TEXT && !sb.v[sb.n - 1].verbatim) {
                Segment *prev = &sb.v[sb.n - 1];
                size_t prev_len = strlen(prev->text);
                char *merged = malloc(prev_len + 2);
                memcpy(merged, prev->text, prev_len);
                merged[prev_len] = brace_char;
                merged[prev_len + 1] = '\0';
                free(prev->text);
                prev->text = merged;
            } else {
                char *text = malloc(2);
                text[0] = brace_char;
                text[1] = '\0';
                Segment s = {0};
                s.kind = SEG_TEXT;
                s.text = text;
                s.src_lo = base_off + (int)(p - 2 - txt);
                s.src_hi = base_off + (int)(p - txt);
                seg_push(&sb, s);
            }
            continue;
        }

        // Handle verbatim placeholders: @@VERB_N@@
        if (strncmp(p, "@@VERB_", 7) == 0) {
            const char *num_start = p + 7;
            char *num_end;
            long index = strtol(num_start, &num_end, 10);
            if (num_end > num_start && strncmp(num_end, "@@", 2) == 0) {
                const char *content = verbatim_store_get((int)index);
                if (content) {
                    // Check if content contains newlines (multiline verbatim)
                    bool has_newline = (strchr(content, '\n') != NULL);

                    if (has_newline) {
                        // Multiline verbatim: create preformatted inline box
                        Segment s = {0};
                        s.kind = SEG_INLINE_BOX;
                        s.box = make_verbatim_box(content);
                        s.baseline = 0;
                        s.src_lo = base_off + (int)(p - txt);
                        s.src_hi = base_off + (int)(num_end + 2 - txt);
                        seg_push(&sb, s);
                    } else {
                        // Single-line verbatim: use SEG_TEXT with verbatim flag
                        size_t content_len = strlen(content);
                        char *text = malloc(content_len + 1);
                        if (text) {
                            memcpy(text, content, content_len + 1);

                            // Don't merge with previous SEG_TEXT - verbatim needs
                            // its own segment to skip smart typography
                            Segment s = {0};
                            s.kind = SEG_TEXT;
                            s.text = text;
                            s.verbatim = true;
                            s.src_lo = base_off + (int)(p - txt);
                            s.src_hi = base_off + (int)(num_end + 2 - txt);
                            seg_push(&sb, s);
                        }
                    }
                } else {
                    // Content not found - output error placeholder
                    const char *err_text = "[VERB?]";
                    size_t err_len = strlen(err_text);
                    char *text = malloc(err_len + 1);
                    if (text) {
                        memcpy(text, err_text, err_len + 1);
                        if (sb.n > 0 && sb.v[sb.n - 1].kind == SEG_TEXT &&
                            !sb.v[sb.n - 1].verbatim) {
                            Segment *prev = &sb.v[sb.n - 1];
                            size_t prev_len = strlen(prev->text);
                            char *merged = malloc(prev_len + err_len + 1);
                            memcpy(merged, prev->text, prev_len);
                            memcpy(merged + prev_len, text, err_len + 1);
                            free(prev->text);
                            free(text);
                            prev->text = merged;
                        } else {
                            Segment s = {0};
                            s.kind = SEG_TEXT;
                            s.text = text;
                            seg_push(&sb, s);
                        }
                    }
                }
                p = num_end + 2; // Skip past @@
                continue;
            }
        }

        // Check for inline commands
        if (*p == '\\' && isalpha((unsigned char)p[1])) {
            int cmd_pos = (int)(p - txt);
            InlineCommand *cmd = parse_inline_command(txt, &cmd_pos);

            if (cmd && cmd->name) {
                // Handle text style commands
                // \texttt — passthrough (already monospace in terminal)
                if (strcmp(cmd->name, "texttt") == 0) {
                    if (cmd->n_args >= 1) {
                        char *content = cmd->args[0];
                        if (content && *content) {
                            if (sb.n > 0 && sb.v[sb.n - 1].kind == SEG_TEXT &&
                                !sb.v[sb.n - 1].verbatim) {
                                Segment *prev = &sb.v[sb.n - 1];
                                size_t prev_len = strlen(prev->text);
                                size_t cont_len = strlen(content);
                                char *merged = malloc(prev_len + cont_len + 1);
                                memcpy(merged, prev->text, prev_len);
                                memcpy(merged + prev_len, content, cont_len + 1);
                                free(prev->text);
                                prev->text = merged;
                            } else {
                                Segment s = {.kind = SEG_TEXT, .text = strdup(content)};
                                seg_push(&sb, s);
                            }
                        }
                    }
                    inline_command_free(cmd);
                    free(cmd);
                    p = txt + cmd_pos;
                    continue;
                }
                if (strcmp(cmd->name, "textbf") == 0 || strcmp(cmd->name, "textit") == 0 ||
                    strcmp(cmd->name, "emph") == 0) {
                    if (cmd->n_args >= 1) {
                        char style = (cmd->name[4] == 'b') ? 'b' : 'i';
                        // Pre-process text symbol commands before styling
                        char *content = cmd->args[0];
                        char *processed = NULL;

                        // Simple expansion of \textXXX commands and LaTeX spacing
                        if (strchr(content, '\\') != NULL) {
                            size_t len = strlen(content);
                            char *buf = malloc(len + 1);
                            char *dst = buf;
                            const char *src = content;
                            while (*src) {
                                if (strncmp(src, "\\textbackslash", 14) == 0 &&
                                    !isalpha((unsigned char)src[14])) {
                                    *dst++ = '\\';
                                    src += 14;
                                } else if (strncmp(src, "\\textdollar", 11) == 0 &&
                                           !isalpha((unsigned char)src[11])) {
                                    *dst++ = '$';
                                    src += 11;
                                } else if (strncmp(src, "\\textpercent", 12) == 0 &&
                                           !isalpha((unsigned char)src[12])) {
                                    *dst++ = '%';
                                    src += 12;
                                } else if (strncmp(src, "\\textbar", 8) == 0 &&
                                           !isalpha((unsigned char)src[8])) {
                                    *dst++ = '|';
                                    src += 8;
                                } else if (strncmp(src, "\\textless", 9) == 0 &&
                                           !isalpha((unsigned char)src[9])) {
                                    *dst++ = '<';
                                    src += 9;
                                } else if (strncmp(src, "\\textgreater", 12) == 0 &&
                                           !isalpha((unsigned char)src[12])) {
                                    *dst++ = '>';
                                    src += 12;
                                } else if (strncmp(src, "\\textampersand", 14) == 0 &&
                                           !isalpha((unsigned char)src[14])) {
                                    *dst++ = '&';
                                    src += 14;
                                } else if (strncmp(src, "\\texthash", 9) == 0 &&
                                           !isalpha((unsigned char)src[9])) {
                                    *dst++ = '#';
                                    src += 9;
                                } else if (strncmp(src, "\\textunderscore", 15) == 0 &&
                                           !isalpha((unsigned char)src[15])) {
                                    *dst++ = '_';
                                    src += 15;
                                } else if (strncmp(src, "\\textcaret", 10) == 0 &&
                                           !isalpha((unsigned char)src[10])) {
                                    *dst++ = '^';
                                    src += 10;
                                } else if (strncmp(src, "\\texttilde", 10) == 0 &&
                                           !isalpha((unsigned char)src[10])) {
                                    *dst++ = '~';
                                    src += 10;
                                } else if (src[0] == '\\' && src[1] == ' ') {
                                    // \  (backslash-space) → inter-word space
                                    *dst++ = ' ';
                                    src += 2;
                                } else if (src[0] == '\\' && src[1] == ',') {
                                    // \, → thin space (emit regular space)
                                    *dst++ = ' ';
                                    src += 2;
                                } else if (src[0] == '\\' && src[1] == '@') {
                                    // \@ → LaTeX spacing hint, consume silently
                                    src += 2;
                                } else {
                                    *dst++ = *src++;
                                }
                            }
                            *dst = '\0';
                            processed = buf;
                            content = processed;
                        }

                        // Emit styled content, handling inline $...$ math
                        int slo = base_off + (int)(p - txt);
                        int shi = base_off + cmd_pos;
                        // Check for nested \textbf/\textit/\emph inside content
                        if (content[0] == '\\') {
                            int inner_pos = 0;
                            InlineCommand *inner = parse_inline_command(content, &inner_pos);
                            if (inner && inner->n_args >= 1 &&
                                (strcmp(inner->name, "textbf") == 0 ||
                                 strcmp(inner->name, "textit") == 0 ||
                                 strcmp(inner->name, "emph") == 0)) {
                                char inner_style = (strcmp(inner->name, "textbf") == 0) ? 'b' : 'i';
                                char combined = style;
                                if ((style == 'b' && inner_style == 'i') ||
                                    (style == 'i' && inner_style == 'b'))
                                    combined = 'B'; // bold-italic
                                else
                                    combined = inner_style; // same nesting: inner wins

                                emit_styled_content_with_math(&sb, inner->args[0], combined, err,
                                                              slo, shi);
                                // Handle trailing text after the inner command
                                const char *after = content + inner_pos;
                                if (*after) {
                                    emit_styled_content_with_math(&sb, after, style, err, slo, shi);
                                }
                                inline_command_free(inner);
                                free(inner);
                            } else {
                                if (inner) {
                                    inline_command_free(inner);
                                    free(inner);
                                }
                                emit_styled_content_with_math(&sb, content, style, err, slo, shi);
                            }
                        } else {
                            emit_styled_content_with_math(&sb, content, style, err, slo, shi);
                        }
                        free(processed);
                    }
                    inline_command_free(cmd);
                    free(cmd);
                    p = txt + cmd_pos;
                    continue;
                }

                // Handle text symbol commands (produce literal characters)
                // These are merged with adjacent text to avoid word-break gaps
                const char *symbol_char = NULL;
                if (strcmp(cmd->name, "textbackslash") == 0)
                    symbol_char = "\\";
                else if (strcmp(cmd->name, "textdollar") == 0)
                    symbol_char = "$";
                else if (strcmp(cmd->name, "textpercent") == 0)
                    symbol_char = "%";
                else if (strcmp(cmd->name, "textbar") == 0)
                    symbol_char = "|";
                else if (strcmp(cmd->name, "textless") == 0)
                    symbol_char = "<";
                else if (strcmp(cmd->name, "textgreater") == 0)
                    symbol_char = ">";
                else if (strcmp(cmd->name, "textampersand") == 0)
                    symbol_char = "&";
                else if (strcmp(cmd->name, "texthash") == 0)
                    symbol_char = "#";
                else if (strcmp(cmd->name, "textunderscore") == 0)
                    symbol_char = "_";
                else if (strcmp(cmd->name, "textcaret") == 0)
                    symbol_char = "^";
                else if (strcmp(cmd->name, "texttilde") == 0)
                    symbol_char = "~";

                if (symbol_char) {
                    // Collect the symbol plus any immediately following non-space text
                    // to avoid word-break gaps.
                    p = txt + cmd_pos;
                    const char *text_start = p;
                    while (*p && *p != '$' && *p != '%' && *p != '\\' && *p != ' ' && *p != '\n' &&
                           *p != '\t')
                        p++;

                    size_t sym_len = strlen(symbol_char);
                    size_t raw_len = p - text_start;

                    // Filter out empty groups {} from the collected text
                    char *filtered = malloc(raw_len + 1);
                    char *dst = filtered;
                    const char *src = text_start;
                    while (src < p) {
                        if (src[0] == '{' && src[1] == '}') {
                            src += 2;
                            continue;
                        }
                        *dst++ = *src++;
                    }
                    *dst = '\0';
                    size_t text_len = dst - filtered;

                    // Check if previous segment is SEG_TEXT - merge with it to avoid gaps
                    if (sb.n > 0 && sb.v[sb.n - 1].kind == SEG_TEXT && !sb.v[sb.n - 1].verbatim) {
                        Segment *prev = &sb.v[sb.n - 1];
                        size_t prev_len = strlen(prev->text);
                        char *merged = malloc(prev_len + sym_len + text_len + 1);
                        memcpy(merged, prev->text, prev_len);
                        memcpy(merged + prev_len, symbol_char, sym_len);
                        memcpy(merged + prev_len + sym_len, filtered, text_len);
                        merged[prev_len + sym_len + text_len] = '\0';
                        free(prev->text);
                        prev->text = merged;
                        prev->src_hi = base_off + (int)(p - txt);
                        free(filtered);
                    } else {
                        char *merged = malloc(sym_len + text_len + 1);
                        memcpy(merged, symbol_char, sym_len);
                        memcpy(merged + sym_len, filtered, text_len);
                        merged[sym_len + text_len] = '\0';
                        free(filtered);

                        Segment s = {0};
                        s.kind = SEG_TEXT;
                        s.text = merged;
                        s.src_lo = base_off + (int)(txt - txt); // start of segment
                        s.src_hi = base_off + (int)(p - txt);
                        seg_push(&sb, s);
                    }
                    inline_command_free(cmd);
                    free(cmd);
                    continue;
                }

                // Handle output commands
                if (is_output_command(cmd->name)) {
                    Box box = render_command_box(cmd, NULL);
                    if (box.w > 0 && box.h > 0) {
                        Segment s = {0};
                        s.kind = SEG_INLINE_BOX;
                        s.box = box;
                        s.baseline = box_baseline(&box);
                        s.src_lo = base_off + (int)(p - txt);
                        s.src_hi = base_off + cmd_pos;
                        seg_push(&sb, s);
                    }
                    inline_command_free(cmd);
                    free(cmd);
                    p = txt + cmd_pos;
                    continue;
                }

                // Unknown command — render as visible literal text
                // e.g., \unknowncmd → shows "\unknowncmd" in output
                hyades_add_warning("unknown command \\%s rendered as literal text", cmd->name);
                {
                    char literal[128];
                    snprintf(literal, sizeof(literal), "\\%s", cmd->name);
                    size_t lit_len = strlen(literal);

                    if (sb.n > 0 && sb.v[sb.n - 1].kind == SEG_TEXT && !sb.v[sb.n - 1].verbatim) {
                        Segment *prev = &sb.v[sb.n - 1];
                        size_t prev_len = strlen(prev->text);
                        char *merged = malloc(prev_len + lit_len + 1);
                        memcpy(merged, prev->text, prev_len);
                        memcpy(merged + prev_len, literal, lit_len + 1);
                        free(prev->text);
                        prev->text = merged;
                        prev->src_hi = base_off + cmd_pos;
                    } else {
                        Segment s = {0};
                        s.kind = SEG_TEXT;
                        s.text = strdup(literal);
                        s.src_lo = base_off + (int)(p - txt);
                        s.src_hi = base_off + cmd_pos;
                        seg_push(&sb, s);
                    }
                }
                inline_command_free(cmd);
                free(cmd);
                p = txt + cmd_pos;
            }
        }

        // Handle \@ (LaTeX spacing hint) — silently consume
        if (*p == '\\' && p[1] == '@') {
            p += 2;
            continue;
        }

        // Regular text - collect until special char, filtering out empty groups {}
        // Also stop at @@VERB_ for verbatim placeholders
        // Note: ${name} is variable access and should be included in text (not math mode)
        const char *start = p;
        while (*p && *p != '%' && *p != '\\' &&
               !(p[0] == '@' && p[1] == '@' && strncmp(p, "@@VERB_", 7) == 0) &&
               !(*p == '$' && p[1] != '{')) // Stop at $ unless it's ${
            p++;

        if (p > start) {
            // Filter out empty groups {} from the collected text
            size_t raw_len = p - start;
            char *filtered = malloc(raw_len + 1);
            char *dst = filtered;
            const char *src = start;
            while (src < p) {
                // Skip empty groups {}
                if (src[0] == '{' && src[1] == '}') {
                    src += 2;
                    continue;
                }
                *dst++ = *src++;
            }
            *dst = '\0';
            size_t len = dst - filtered;

            if (len > 0) {
                // Merge with previous SEG_TEXT to avoid word-break gaps
                if (sb.n > 0 && sb.v[sb.n - 1].kind == SEG_TEXT && !sb.v[sb.n - 1].verbatim) {
                    Segment *prev = &sb.v[sb.n - 1];
                    size_t prev_len = strlen(prev->text);
                    char *merged = malloc(prev_len + len + 1);
                    memcpy(merged, prev->text, prev_len);
                    memcpy(merged + prev_len, filtered, len);
                    merged[prev_len + len] = '\0';
                    free(prev->text);
                    prev->text = merged;
                    prev->src_hi = base_off + (int)(p - txt);
                    free(filtered);
                } else {
                    Segment s = {0};
                    s.kind = SEG_TEXT;
                    s.text = filtered;
                    s.src_lo = base_off + (int)(start - txt);
                    s.src_hi = base_off + (int)(p - txt);
                    seg_push(&sb, s);
                }
            } else {
                free(filtered);
            }
        } else if (*p) {
            // Skip unrecognized special char
            p++;
        }
    }

    return sb;
}
