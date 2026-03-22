// linebreak.c - Line breaking algorithms for compositor
// This is the ORIGINAL code from compositor.c, copied verbatim

#include "compositor_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Line Token Buffer Operations
// ============================================================================

void lt_push(LTBuf *lb, LT tok) {
    if (lb->n >= lb->cap) {
        lb->cap = lb->cap ? lb->cap * 2 : 64;
        lb->v = realloc(lb->v, lb->cap * sizeof(LT));
    }
    lb->v[lb->n++] = tok;
}

void lt_free(LTBuf *lb) {
    for (int i = 0; i < lb->n; i++) {
        if (lb->v[i].text) free(lb->v[i].text);
        if (lb->v[i].col_in) free(lb->v[i].col_in);
        if (lb->v[i].kind == LT_MATH) box_free(&lb->v[i].box);
    }
    free(lb->v);
    lb->v = NULL;
    lb->n = lb->cap = 0;
}

void lt_replace_with(LTBuf *lt, int idx, LT new_tok) {
    if (idx < 0 || idx >= lt->n) return;
    if (lt->v[idx].text) free(lt->v[idx].text);
    if (lt->v[idx].col_in) free(lt->v[idx].col_in);
    if (lt->v[idx].kind == LT_MATH) box_free(&lt->v[idx].box);
    lt->v[idx] = new_tok;
}

void lt_insert(LTBuf *lt, int idx, LT tok) {
    if (idx < 0) idx = 0;
    if (idx > lt->n) idx = lt->n;
    if (lt->n >= lt->cap) {
        lt->cap = lt->cap ? lt->cap * 2 : 64;
        lt->v = realloc(lt->v, lt->cap * sizeof(LT));
    }
    for (int i = lt->n; i > idx; i--) lt->v[i] = lt->v[i - 1];
    lt->v[idx] = tok;
    lt->n++;
}

// ============================================================================
// Line Buffer Operations
// ============================================================================

void line_push(LineBuf *lb, Line ln) {
    if (lb->n >= lb->cap) {
        lb->cap = lb->cap ? lb->cap * 2 : 16;
        lb->v = realloc(lb->v, lb->cap * sizeof(Line));
    }
    lb->v[lb->n++] = ln;
}

void lines_free(LineBuf *lb) {
    free(lb->v);
    lb->v = NULL;
    lb->n = lb->cap = 0;
}

// ============================================================================
// Text Tokenization
// ============================================================================

void flush_word_into_lb_mapped(Str *word, I32Buf *w_map, LTBuf *lb) {
    if (word->n > 0) {
        char *w = str_detach(word);
        int *m = i32_detach(w_map);
        lt_push(lb, (LT){.kind = LT_TEXT, .text = w, .width = u8_cols_str(w), .col_in = m});
        str_init(word);
        i32_init(w_map);
    }
}

void explode_text_into_tokens_mapped(const char *out_str, const int *col2in, int col2in_n,
                                     LTBuf *lb) {
    const unsigned char *p = (const unsigned char *)out_str;
    Str word;
    str_init(&word);
    I32Buf w_map;
    i32_init(&w_map);
    int col = 0;
    int tok_iters = 0;

    while (*p) {
        if (++tok_iters > 1000000) break;
        // NBSP: tilde or UTF-8 0xC2 0xA0
        if (*p == '~' || (*p == 0xC2 && p[1] == 0xA0)) {
            flush_word_into_lb_mapped(&word, &w_map, lb);
            int nb_in = (col < col2in_n) ? col2in[col] : -1;
            lt_push(lb, (LT){.kind = LT_NBSP, .nb_in = nb_in, .width = 1});
            if (*p == '~') {
                p += 1;
                col += 1;
            } else {
                p += 2;
                col += 1;
            }
            continue;
        }

        // Space: flush word
        if (*p == ' ') {
            flush_word_into_lb_mapped(&word, &w_map, lb);
            while (*p == ' ') {
                p++;
                col++;
            }
            continue;
        }

        // Regular character
        int adv = (((*p) & 0x80) == 0x00)   ? 1
                  : (((*p) & 0xE0) == 0xC0) ? 2
                  : (((*p) & 0xF0) == 0xE0) ? 3
                  : (((*p) & 0xF8) == 0xF0) ? 4
                                            : 1;

        for (int k = 0; k < adv; k++) str_putc(&word, (char)p[k]);
        i32_push(&w_map, (col < col2in_n) ? col2in[col] : -1);
        p += adv;
        col += 1;
    }
    flush_word_into_lb_mapped(&word, &w_map, lb);
}

// ============================================================================
// Knuth-Plass Optimal Line Breaking
// ============================================================================

#define KP_INF 1e18

typedef struct {
    int tok_idx;
    bool is_hyphen;
    int hyph_byte;
    int hyph_col;
} KPBreak;

typedef struct {
    double cost;
    int prev;
    int consec_hyph;
} KPNode;

typedef struct {
    KPBreak *v;
    int n, cap;
} KPBreakBuf;

static void kpb_push(KPBreakBuf *b, KPBreak bp) {
    if (b->n >= b->cap) {
        b->cap = b->cap ? b->cap * 2 : 128;
        b->v = realloc(b->v, b->cap * sizeof(KPBreak));
    }
    b->v[b->n++] = bp;
}

// Calculate badness: how bad is this line's spacing?
// TeX formula: badness = 100 * |r|^3, capped at 10000
// r = adjustment ratio = slack / stretchable_gaps
static double kp_badness(int natural_width, int target_width, int stretchable_gaps,
                         bool is_last_line, const CompOptions *opt) {
    int slack = target_width - natural_width;

    if (slack < 0) {
        // Overfull line - very high badness
        return 10000.0;
    }

    // Last line: don't penalize being short (ragged right is fine)
    if (is_last_line) {
        return (slack <= target_width / 2) ? 0.0 : 50.0;
    }

    if (stretchable_gaps == 0) {
        // Can't stretch - only OK if perfect fit
        return (slack == 0) ? 0.0 : KP_INF;
    }

    // Adjustment ratio: extra spaces needed per gap
    double r = (double)slack / stretchable_gaps;

    // If ratio exceeds tolerance, line is too loose
    if (r > opt->kp_tolerance) {
        return KP_INF;
    }

    // TeX badness: 100 * r^3
    double bad = 100.0 * r * r * r;
    return (bad > 10000.0) ? 10000.0 : bad;
}

// Calculate demerits for a line break
// TeX formula: d = (linepenalty + badness)^2 + penalties
static double kp_demerits(double badness, bool is_hyphen, int prev_consec_hyph,
                          const CompOptions *opt) {
    if (badness >= KP_INF) return KP_INF;

    double d = opt->kp_line_penalty + badness;
    d = d * d; // Square the sum

    if (is_hyphen) {
        d += opt->kp_hyphen_penalty * opt->kp_hyphen_penalty;
        if (prev_consec_hyph >= 1) {
            d += opt->kp_consec_hyphen_penalty;
        }
    }

    return d;
}

// Measure line from breakpoint 'from' to breakpoint 'to'
// Returns natural width and count of stretchable gaps
static void kp_measure_line(const LTBuf *lt, const KPBreakBuf *bps, int from_bp, int to_bp,
                            int *out_width, int *out_gaps) {
    const KPBreak *from = &bps->v[from_bp];
    const KPBreak *to = &bps->v[to_bp];

    int width = 0;
    int gaps = 0;

    // Determine token range
    int first_tok, last_tok;
    bool start_partial = false;
    bool end_partial = false;
    int start_skip_cols = 0; // Columns to skip at start (left part of prev hyphenation)
    int end_cols = 0;        // Columns to include at end (left part + hyphen)

    if (from->is_hyphen) {
        // Line starts with right fragment of hyphenated word
        first_tok = from->tok_idx;
        start_partial = true;
        start_skip_cols = from->hyph_col; // Skip this many columns
    } else {
        first_tok = from->tok_idx + 1;
    }

    if (to->is_hyphen) {
        // Line ends with left fragment + hyphen
        last_tok = to->tok_idx;
        end_partial = true;
        end_cols = to->hyph_col + 1; // +1 for hyphen
    } else {
        last_tok = to->tok_idx;
    }

    // Handle empty range
    if (first_tok > last_tok || last_tok < 0) {
        *out_width = 0;
        *out_gaps = 0;
        return;
    }

    // Accumulate widths
    bool is_first = true;
    for (int i = first_tok; i <= last_tok && i < lt->n; i++) {
        // Inter-token gap (not before first token on line)
        if (!is_first) {
            const LT *A = &lt->v[i - 1];
            const LT *B = &lt->v[i];
            if (!(A->kind == LT_NBSP || B->kind == LT_NBSP)) {
                width += 1;
                if (A->kind == LT_TEXT && B->kind == LT_TEXT) {
                    gaps++;
                }
            }
        }

        // Token width (possibly partial)
        int tok_width = lt->v[i].width;
        if (i == first_tok && start_partial) {
            tok_width = tok_width - start_skip_cols; // Right part only
        }
        if (i == last_tok && end_partial) {
            if (i == first_tok && start_partial) {
                // Both start and end partial on same token - complex case
                tok_width = end_cols - start_skip_cols;
            } else {
                tok_width = end_cols;
            }
        }

        width += tok_width;
        is_first = false;
    }

    *out_width = width;
    *out_gaps = gaps;
}

// Enumerate all feasible breakpoints
static void kp_enum_breakpoints(const LTBuf *lt, const CompOptions *opt, KPBreakBuf *bps) {
    // Start sentinel
    kpb_push(bps, (KPBreak){.tok_idx = -1, .is_hyphen = false});

    for (int i = 0; i < lt->n; i++) {
        // Hyphenation points within this token (if TEXT)
        if (lt->v[i].kind == LT_TEXT && opt->hyphenate) {
            const char *word = lt->v[i].text;
            int *offsets = NULL;
            int num_cp = 0;
            char *ascii = normalize_for_hyphenation(word, &offsets, &num_cp);

            if (ascii && num_cp >= opt->hyphen_min_left + opt->hyphen_min_right) {
                int pts[64];
                int npts =
                    hyphen_points(ascii, pts, 64, opt->hyphen_min_left, opt->hyphen_min_right);

                for (int j = 0; j < npts; j++) {
                    kpb_push(bps, (KPBreak){.tok_idx = i,
                                            .is_hyphen = true,
                                            .hyph_byte = offsets[pts[j]],
                                            .hyph_col = pts[j]});
                }
            }
            free(ascii);
            free(offsets);
        }

        // Natural break after this token
        kpb_push(bps, (KPBreak){.tok_idx = i, .is_hyphen = false});
    }
}

// Apply hyphenation split to token buffer
static void kp_apply_split(LTBuf *lt, int tok_idx, int byte_pos, int col_pos) {
    if (tok_idx < 0 || tok_idx >= lt->n) return;
    LT *tok = &lt->v[tok_idx];
    if (tok->kind != LT_TEXT) return;

    const char *w = tok->text;
    size_t wn = strlen(w);

    // Left part with hyphen
    char *L = malloc(byte_pos + 2);
    memcpy(L, w, byte_pos);
    L[byte_pos] = '-';
    L[byte_pos + 1] = '\0';

    // Right part
    size_t rn = wn - byte_pos;
    char *R = malloc(rn + 1);
    memcpy(R, w + byte_pos, rn);
    R[rn] = '\0';

    // Split mapping arrays
    int *col_in = tok->col_in;
    int wcols = tok->width;
    int *Lmap = NULL, *Rmap = NULL;

    if (wcols > 0 && col_in) {
        Lmap = malloc((col_pos + 1) * sizeof(int));
        Rmap = malloc((wcols - col_pos) * sizeof(int));
        for (int t = 0; t < col_pos; t++) Lmap[t] = col_in[t];
        Lmap[col_pos] = (col_pos > 0) ? col_in[col_pos - 1] : -1;
        for (int t = 0; t < wcols - col_pos; t++) Rmap[t] = col_in[col_pos + t];
    }

    LT left = {.kind = LT_TEXT,
               .text = L,
               .width = u8_cols_str(L),
               .col_in = Lmap,
               .is_continuation = false};
    LT right = {.kind = LT_TEXT,
                .text = R,
                .width = u8_cols_str(R),
                .col_in = Rmap,
                .is_continuation = true};

    lt_replace_with(lt, tok_idx, left);
    lt_insert(lt, tok_idx + 1, right);
}

// Forward declaration for fallback
static void make_lines(LTBuf *lt, int W, const CompOptions *opt, LineBuf *out);

// Main Knuth-Plass line breaker
static void make_lines_kp(LTBuf *lt, int W, const CompOptions *opt, LineBuf *out) {
    if (lt->n == 0) return;

    // Step 1: Enumerate all breakpoints
    KPBreakBuf bps = {0};
    kp_enum_breakpoints(lt, opt, &bps);

    if (bps.n < 2) {
        free(bps.v);
        int width = 0, gaps = 0;
        for (int i = 0; i < lt->n; i++) {
            if (i > 0) {
                width++;
                gaps++;
            }
            width += lt->v[i].width;
        }
        line_push(out, (Line){.start = 0, .end = lt->n, .baseline_width = width, .gaps = gaps});
        return;
    }

    // Step 2: DP to find optimal break sequence
    int nbp = bps.n;
    KPNode *nodes = calloc(nbp, sizeof(KPNode));
    for (int i = 0; i < nbp; i++) {
        nodes[i].cost = KP_INF;
        nodes[i].prev = -1;
    }
    nodes[0].cost = 0;

    long kp_iters = 0;
    for (int j = 1; j < nbp; j++) {
        bool is_last = (j == nbp - 1);

        for (int i = 0; i < j; i++) {
            if (++kp_iters > 1000000) goto kp_dp_done;
            if (nodes[i].cost >= KP_INF) continue;

            int line_width, line_gaps;
            kp_measure_line(lt, &bps, i, j, &line_width, &line_gaps);

            double bad = kp_badness(line_width, W, line_gaps, is_last, opt);
            if (bad >= KP_INF) continue;

            double dem = kp_demerits(bad, bps.v[j].is_hyphen, nodes[i].consec_hyph, opt);
            double total = nodes[i].cost + dem;

            if (total < nodes[j].cost) {
                nodes[j].cost = total;
                nodes[j].prev = i;
                nodes[j].consec_hyph = bps.v[j].is_hyphen ? (nodes[i].consec_hyph + 1) : 0;
            }
        }
    }
    kp_dp_done:

    // Step 3: Check if valid path exists
    if (nodes[nbp - 1].cost >= KP_INF) {
        // No valid path - fall back to greedy
        free(nodes);
        free(bps.v);
        make_lines(lt, W, opt, out);
        return;
    }

    // Step 4: Backtrack to get break sequence
    int path[2048];
    int path_len = 0;
    for (int bp = nbp - 1; bp >= 0 && path_len < 2048;) {
        path[path_len++] = bp;
        int prev = nodes[bp].prev;
        if (prev < 0) break;
        bp = prev;
    }

    // Reverse to get start-to-end order
    for (int i = 0; i < path_len / 2; i++) {
        int tmp = path[i];
        path[i] = path[path_len - 1 - i];
        path[path_len - 1 - i] = tmp;
    }

    // Step 5: Apply hyphenation splits
    // We need to apply splits and then build lines from the modified token buffer

    typedef struct {
        int tok_idx, byte_pos, col_pos;
    } HyphSplit;
    HyphSplit splits[1024];
    int nsplits = 0;

    for (int p = 1; p < path_len && nsplits < 1024; p++) {
        KPBreak *bp = &bps.v[path[p]];
        if (bp->is_hyphen) {
            splits[nsplits++] = (HyphSplit){bp->tok_idx, bp->hyph_byte, bp->hyph_col};
        }
    }

    // Sort splits by tok_idx descending, then by col_pos descending
    // This way we apply from end to start, and indices remain valid
    for (int i = 0; i < nsplits - 1; i++) {
        for (int j = i + 1; j < nsplits; j++) {
            bool swap = false;
            if (splits[j].tok_idx > splits[i].tok_idx) {
                swap = true;
            } else if (splits[j].tok_idx == splits[i].tok_idx &&
                       splits[j].col_pos > splits[i].col_pos) {
                swap = true;
            }
            if (swap) {
                HyphSplit tmp = splits[i];
                splits[i] = splits[j];
                splits[j] = tmp;
            }
        }
    }

    // Apply splits from end to start
    // After each split, token indices >= split point increase by 1
    for (int s = 0; s < nsplits; s++) {
        kp_apply_split(lt, splits[s].tok_idx, splits[s].byte_pos, splits[s].col_pos);

        // Adjust indices for remaining splits (those with lower tok_idx are unaffected)
        // But same tok_idx with lower col_pos need their tok_idx increased
        // Actually, since we sorted descending, remaining splits have lower indices, so no adjustment needed
        // UNLESS they're on the same token - but we sorted by col_pos descending too, so the
        // higher col_pos splits on the same token are already done
    }

    // Step 6: Build lines using simple greedy on the now-split token buffer
    // The DP determined WHERE to break; now we just scan and break at those points
    // Since tokens are now split, greedy will naturally break at the right places

    free(nodes);
    free(bps.v);

    // Simple greedy line builder (tokens are already optimally split)
    int i = 0, n = lt->n;
    int kp_greedy_outer = 0;
    while (i < n) {
        if (++kp_greedy_outer > 1000000) break;
        int j = i;
        int width = 0, gaps = 0;
        int asc = 0, desc = 0;

        while (j < n) {
            int add = lt->v[j].width;
            int gap_add = 0;

            if (j > i) {
                const LT *A = &lt->v[j - 1];
                const LT *B = &lt->v[j];
                // Only add gap between TEXT-TEXT boundaries (TeX-like: no auto spacing around math)
                if (!(A->kind == LT_NBSP || B->kind == LT_NBSP) && !B->is_continuation &&
                    A->kind == LT_TEXT && B->kind == LT_TEXT) {
                    gap_add = 1;
                }
            }

            int trial = width + gap_add + add;
            if (trial > W && j > i) break;

            width = trial;

            // Count stretchable gaps (for justification later)
            if (j > i) {
                const LT *A = &lt->v[j - 1];
                const LT *B = &lt->v[j];
                if (!(A->kind == LT_NBSP || B->kind == LT_NBSP) && !B->is_continuation &&
                    A->kind == LT_TEXT && B->kind == LT_TEXT) {
                    gaps++;
                }
            }

            // Track math baseline
            if (lt->v[j].kind == LT_MATH) {
                if (lt->v[j].baseline > asc) asc = lt->v[j].baseline;
                int d = lt->v[j].box.h - 1 - lt->v[j].baseline;
                if (d > desc) desc = d;
            }

            j++;
        }

        if (j == i) j = i + 1; // Safety: always advance

        line_push(out, (Line){.start = i,
                              .end = j,
                              .baseline_ascent = asc,
                              .baseline_descent = desc,
                              .baseline_width = width,
                              .gaps = gaps});
        i = j;
    }
}

// ============================================================================
// Greedy Line Breaking (with hyphenation)
// ============================================================================

static void make_lines(LTBuf *lt, int W, const CompOptions *opt, LineBuf *out) {
    int i = 0, n = lt->n;
    int max_lines = n + 1000;
    int line_count = 0;
    while (i < n) {
        if (++line_count > max_lines) break;
        int j = i;
        int width = 0, gaps = 0;
        int asc = 0, desc = 0;
        int inner_iters = 0;

        while (j < n) {
            if (++inner_iters > 5000) {
                j = n;
                break;
            }
            int add = lt->v[j].width;

            bool has_boundary = (j > i);
            int gap_add = 0;

            if (has_boundary) {
                const LT *A = &lt->v[j - 1];
                const LT *B = &lt->v[j];

                if (A->kind == LT_NBSP || B->kind == LT_NBSP) {
                    gap_add = 0; // NBSP/tie consumes its own 1-col token; no extra boundary gap
                } else if (A->kind == LT_TEXT && B->kind == LT_TEXT && !B->is_continuation) {
                    gap_add = 1; // stretchable boundary between words
                } else {
                    gap_add = 0; // TeX-like: no auto spacing around inline math
                }
            }

            int trial = width + gap_add + add;

            if (trial > W) {
                if (lt->v[j].kind == LT_TEXT && opt->hyphenate) {
                    const char *w = lt->v[j].text;
                    int avail = W - (width + gap_add); // columns left for the prefix+'-'
                    int cut_cols = 0;
                    int cut = hyphen_best_fit_unicode(w, avail, opt->hyphen_min_left,
                                                      opt->hyphen_min_right, &cut_cols);
                    if (cut > 0) {
                        size_t wn = strlen(w);

                        char *L = (char *)malloc((size_t)cut + 1 + 1);
                        memcpy(L, w, (size_t)cut);
                        L[cut] = '-';
                        L[cut + 1] = 0;

                        size_t rn = wn - (size_t)cut;
                        char *R = (char *)malloc(rn + 1);
                        memcpy(R, w + cut, rn);
                        R[rn] = 0;

                        // Split mapping arrays accordingly (using column counts, not bytes):
                        int *col_in = lt->v[j].col_in;
                        int wcols = lt->v[j].width;
                        int *Lmap = NULL, *Rmap = NULL;
                        if (wcols > 0 && col_in) {
                            Lmap = (int *)malloc((size_t)(cut_cols + 1) * sizeof(int));
                            Rmap = (int *)malloc((size_t)(wcols - cut_cols) * sizeof(int));
                            // left part gets first 'cut_cols' cols + hyphen (map hyphen to last left column's source if exists)
                            for (int t = 0; t < cut_cols; t++) Lmap[t] = col_in[t];
                            Lmap[cut_cols] = (cut_cols > 0) ? col_in[cut_cols - 1] : -1;
                            // right part gets remaining
                            for (int t = 0; t < wcols - cut_cols; t++)
                                Rmap[t] = col_in[cut_cols + t];
                        }

                        LT left = {.kind = LT_TEXT,
                                   .text = L,
                                   .width = u8_cols_str(L),
                                   .col_in = Lmap,
                                   .is_continuation = false};
                        LT right = {.kind = LT_TEXT,
                                    .text = R,
                                    .width = u8_cols_str(R),
                                    .col_in = Rmap,
                                    .is_continuation = true};

                        lt_replace_with(lt, j, left);
                        lt_insert(lt, j + 1, right);
                        n = lt->n;

                        add = lt->v[j].width;
                        trial = width + gap_add + add;
                        if (trial > W) break; // safety
                    } else {
                        if (i == j) {
                            // Overflow: token wider than line, force it onto this line
                            width = trial;
                            // Track math baseline for overflowed tokens too
                            if (lt->v[j].kind == LT_MATH) {
                                int bl = lt->v[j].baseline;
                                int h = lt->v[j].box.h;
                                if (bl > asc) asc = bl;
                                int d = (h - 1 - bl);
                                if (d > desc) desc = d;
                            }
                            j++;
                        }
                        break;
                    }
                } else {
                    if (i == j) {
                        // Overflow: token wider than line, force it onto this line
                        width = trial;
                        // Track math baseline for overflowed tokens too
                        if (lt->v[j].kind == LT_MATH) {
                            int bl = lt->v[j].baseline;
                            int h = lt->v[j].box.h;
                            if (bl > asc) asc = bl;
                            int d = (h - 1 - bl);
                            if (d > desc) desc = d;
                        }
                        j++;
                    }
                    break;
                }
            }

            width = trial;

            if (has_boundary) {
                const LT *A = &lt->v[j - 1];
                const LT *B = &lt->v[j];
                if (!(A->kind == LT_NBSP || B->kind == LT_NBSP) &&
                    (A->kind == LT_TEXT && B->kind == LT_TEXT) && !B->is_continuation) {
                    gaps++;
                }
            }

            if (lt->v[j].kind == LT_MATH) {
                int bl = lt->v[j].baseline;
                int h = lt->v[j].box.h;
                if (bl > asc) asc = bl;
                int d = (h - 1 - bl);
                if (d > desc) desc = d;
            }
            j++;
        }

        if (j == i) {
            j = i + 1;
        } // safety

        line_push(out, (Line){.start = i,
                              .end = j,
                              .baseline_ascent = asc,
                              .baseline_descent = desc,
                              .baseline_width = width,
                              .gaps = gaps});
        i = j;
    }
}

// ============================================================================
// Dispatch
// ============================================================================

void make_lines_dispatch(LTBuf *lt, int W, const CompOptions *opt, LineBuf *out) {
    if (opt->linebreaker == LINEBREAK_KNUTH_PLASS) {
        make_lines_kp(lt, W, opt, out);
    } else {
        make_lines(lt, W, opt, out);
    }
}
