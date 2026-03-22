// compose.c - Main compositor entry point
//
// Public API: compose_text, compose_text_with_map

#include "compositor_internal.h"
#include "layout/layout.h"
#include <stdio.h>
#include "math/parser/parser.h"
#include "math/renderer/render_opts.h"
#include "utils/utf8.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// External declarations
extern Box render_ast(const Ast *node);
extern Ast *parse_math(const char *input, ParseError *err);
extern void ast_free(Ast *node);
extern int get_unicode_mode(void);
extern void set_unicode_mode(int mode);
extern CompOptions default_options(void);

// Map context note helper
static void mc_note_one(MapCtx *mc, int in_idx, int kind) {
    mc_emit(mc, in_idx, kind);
    mc_advance_col(mc, 1);
}

// Character kind constants
#define CK_SPACE 0
#define CK_MATH_GENERIC 1
#define CK_RULE 2

// Metadata emission helper
static void meta_emit(U8Buf *meta, uint8_t m) {
    if (meta) u8_push(meta, m);
}

static void meta_emit_n(U8Buf *meta, uint8_t m, int count) {
    if (meta) u8_push_n(meta, m, count);
}

// ============================================================================
// Document Command Stripping
// ============================================================================

static char *strip_doc_commands(const char *input) {
    size_t len = strlen(input);
    char *out = malloc(len + 1);
    if (!out) return NULL;

    char *op = out;
    const char *p = input;

    while (*p) {
        if (*p == '\\' && isalpha((unsigned char)p[1])) {
            const char *cmd_start = p + 1;
            const char *cmd_end = cmd_start;
            while (isalpha((unsigned char)*cmd_end)) cmd_end++;

            size_t cmd_len = cmd_end - cmd_start;
            char cmd_name[32];
            if (cmd_len > 0 && cmd_len < sizeof(cmd_name)) {
                memcpy(cmd_name, cmd_start, cmd_len);
                cmd_name[cmd_len] = '\0';

                bool is_doc_cmd =
                    (strcmp(cmd_name, "setunicode") == 0 || strcmp(cmd_name, "setwidth") == 0 ||
                     strcmp(cmd_name, "sethyphenate") == 0 ||
                     strcmp(cmd_name, "sethyphenminleft") == 0 ||
                     strcmp(cmd_name, "sethyphenminright") == 0 ||
                     strcmp(cmd_name, "setmathitalic") == 0 ||
                     strcmp(cmd_name, "linebreaker") == 0 ||
                     strcmp(cmd_name, "setlinepenalty") == 0 ||
                     strcmp(cmd_name, "sethyphenpenalty") == 0 ||
                     strcmp(cmd_name, "setconsechyphenpenalty") == 0 ||
                     strcmp(cmd_name, "settolerance") == 0 ||
                     strcmp(cmd_name, "setshortthreshold") == 0 ||
                     strcmp(cmd_name, "setlinkthreshold") == 0 ||
                     strcmp(cmd_name, "setspreaddistance") == 0 ||
                     strcmp(cmd_name, "setneighbordivisor") == 0 ||
                     strcmp(cmd_name, "setspreaddivisor") == 0 ||
                     strcmp(cmd_name, "setminscore") == 0 || strcmp(cmd_name, "setmode") == 0 ||
                     strcmp(cmd_name, "setsym") == 0);

                if (is_doc_cmd) {
                    p = cmd_end;
                    while (*p == ' ' || *p == '\t') p++;
                    while (*p == '{') {
                        int depth = 1;
                        p++;
                        while (*p && depth > 0) {
                            if (*p == '{')
                                depth++;
                            else if (*p == '}')
                                depth--;
                            p++;
                        }
                        while (*p == ' ' || *p == '\t') p++;
                    }
                    continue;
                }
            }
        }
        *op++ = *p++;
    }
    *op = '\0';
    return out;
}

// ============================================================================
// Internal Implementation
// ============================================================================

// Forward declaration for paragraph rendering with metadata
void render_paragraph_with_map_meta(const SegBuf *seg, int width, int unicode_on,
                                    const CompOptions *opt, Str *out, MapCtx *mc, U8Buf *meta);

static char *compose_text_internal(const char *input, const CompOptions *opt_in, MapCtx *mc,
                                   U8Buf *meta, ParseError *err) {
    char *cleaned_input = strip_doc_commands(input);
    if (!cleaned_input) {
        if (err) {
            err->code = PARSE_ERR_OOM;
            snprintf(err->message, sizeof(err->message), "Out of memory");
        }
        return NULL;
    }

    CompOptions opt = opt_in ? *opt_in : default_options();
    if (opt.width < 10) opt.width = 10;
    // Ensure sane hyphenation defaults (prevents hangs with zeroed CompOptions)
    if (opt.hyphenate) {
        if (opt.hyphen_min_left < 2) opt.hyphen_min_left = 2;
        if (opt.hyphen_min_right < 3) opt.hyphen_min_right = 3;
    }

    if (mc) {
        mc->row = 0;
        mc->col = 0;
    }
    if (err) {
        err->code = PARSE_OK;
        err->row = err->col = 0;
        err->message[0] = '\0';
    }

    int unicode_on = get_unicode_mode();
    Str out;
    str_init(&out);

    BlockList bl = split_blocks_display_math_with_pos(cleaned_input);

    for (int bi = 0; bi < bl.n; bi++) {
        if (!bl.is_math[bi]) {
            // Text block - process paragraphs
            const char *p = bl.blocks[bi];
            int base_off = bl.start[bi];
            int para_count = 0;

            while (*p) {
                if (++para_count > 100000) break;
                // Find paragraph end
                const char *q = p;
                while (*q) {
                    if (*q == '\n' && q[1] == '\n') break;
                    q++;
                }

                size_t span = (size_t)(q - p);
                char *chunk = malloc(span + 1);
                memcpy(chunk, p, span);
                chunk[span] = 0;

                // Check if all whitespace
                bool allws = true;
                for (size_t k = 0; k < span; k++) {
                    if (!isspace((unsigned char)chunk[k])) {
                        allws = false;
                        break;
                    }
                }

                if (!allws) {
                    // Replace newlines with spaces
                    for (size_t k = 0; k < span; k++)
                        if (chunk[k] == '\n') chunk[k] = ' ';

                    // Skip leading spaces (from replaced newlines at start)
                    char *trimmed = chunk;
                    int trim_off = 0;
                    while (*trimmed == ' ') {
                        trimmed++;
                        trim_off++;
                    }

                    SegBuf seg = parse_text_to_segments_pos(
                        input, trimmed, base_off + (int)(p - bl.blocks[bi]) + trim_off, err);
                    render_paragraph_with_map_meta(&seg, opt.width, unicode_on, &opt, &out, mc,
                                                   meta);
                    unicode_on = get_unicode_mode();
                    seg_free(&seg);
                }
                free(chunk);

                p = q;
                int blanks = 0;
                while (*p == '\n') {
                    if (p[1] == '\n') {
                        blanks++;
                        p++;
                    } else {
                        p++;
                        break;
                    }
                }
                if (blanks > 0) {
                    str_putc(&out, '\n');
                    meta_emit(meta, CELL_META_NONE);
                    mc_newline(mc);
                }
            }
        } else {
            // Display math block
            set_unicode_mode(unicode_on);

            Ast *root = parse_math(bl.blocks[bi], err);
            if (!root) {
                str_putc(&out, '\n');
                meta_emit(meta, CELL_META_NONE);
                mc_newline(mc);
                if (bi + 1 < bl.n && !bl.is_math[bi + 1]) {
                    str_putc(&out, '\n');
                    meta_emit(meta, CELL_META_NONE);
                    mc_newline(mc);
                }
                continue;
            }

            Box b = render_ast(root);
            ast_free(root);

            // Skip if box allocation failed
            if (!b.cells) {
                str_putc(&out, '\n');
                meta_emit(meta, CELL_META_NONE);
                mc_newline(mc);
                continue;
            }

            // Tag-aware display math layout:
            // If tag_width > 0, center equation (left part) and right-justify tag
            int tw = b.tag_width;
            int eq_w = b.w - tw; // width of equation part (including gap before tag)
            int pad;
            if (tw > 0 && eq_w > 0 && b.w < opt.width) {
                // Center equation portion, right-justify tag
                pad = (opt.width - eq_w) / 2;
                if (pad < 0) pad = 0;
            } else {
                pad = (b.w < opt.width) ? (opt.width - b.w) / 2 : 0;
            }

            for (int y = 0; y < b.h; y++) {
                if (tw > 0 && eq_w > 0 && b.w < opt.width) {
                    // Emit equation part centered
                    for (int i = 0; i < pad; i++) {
                        str_putc(&out, ' ');
                        meta_emit(meta, CELL_META_NONE);
                        mc_note_one(mc, -1, CK_SPACE);
                    }

                    // Check for rule-like row (equation portion only)
                    int has_dash = 0, rule_like = 1;
                    for (int x = 0; x < eq_w; x++) {
                        uint32_t cp = b.cells[y * b.w + x];
                        if (cp == U'-') {
                            has_dash = 1;
                            continue;
                        }
                        if (cp != U' ') {
                            rule_like = 0;
                            break;
                        }
                    }
                    (void)has_dash;

                    for (int x = 0; x < eq_w; x++) {
                        uint32_t cp = b.cells[y * b.w + x];
                        char enc[8];
                        size_t n;
                        if (get_unicode_mode() && rule_like && cp == U'-')
                            n = utf8_encode(0x2500, enc);
                        else
                            n = encode_cell_utf8(cp, enc);
                        for (size_t k = 0; k < n; k++) str_putc(&out, enc[k]);
                        uint8_t cell_meta = CELL_META_NONE;
                        if (b.meta) cell_meta = b.meta[y * b.w + x];
                        meta_emit(meta, cell_meta);
                        mc_note_one(mc, -1,
                                    (rule_like && cp == U'-')
                                        ? CK_RULE
                                        : (cp == U' ' ? CK_SPACE : CK_MATH_GENERIC));
                    }

                    // Fill gap between equation end and tag start
                    int tag_start = opt.width - tw;
                    int cur_col = pad + eq_w;
                    int fill = tag_start - cur_col;
                    if (fill < 0) fill = 0;
                    for (int i = 0; i < fill; i++) {
                        str_putc(&out, ' ');
                        meta_emit(meta, CELL_META_NONE);
                        mc_note_one(mc, -1, CK_SPACE);
                    }

                    // Emit tag cells (right-justified)
                    for (int x = eq_w; x < b.w; x++) {
                        uint32_t cp = b.cells[y * b.w + x];
                        char enc[8];
                        size_t n = encode_cell_utf8(cp, enc);
                        for (size_t k = 0; k < n; k++) str_putc(&out, enc[k]);
                        uint8_t cell_meta = CELL_META_NONE;
                        if (b.meta) cell_meta = b.meta[y * b.w + x];
                        meta_emit(meta, cell_meta);
                        mc_note_one(mc, -1, cp == U' ' ? CK_SPACE : CK_MATH_GENERIC);
                    }
                } else {
                    // Normal centering (no tag)
                    for (int i = 0; i < pad; i++) {
                        str_putc(&out, ' ');
                        meta_emit(meta, CELL_META_NONE);
                        mc_note_one(mc, -1, CK_SPACE);
                    }

                    // Check for rule-like row
                    int has_dash = 0, rule_like = 1;
                    for (int x = 0; x < b.w; x++) {
                        uint32_t cp = b.cells[y * b.w + x];
                        if (cp == U'-') {
                            has_dash = 1;
                            continue;
                        }
                        if (cp != U' ') {
                            rule_like = 0;
                            break;
                        }
                    }
                    (void)has_dash;

                    for (int x = 0; x < b.w; x++) {
                        uint32_t cp = b.cells[y * b.w + x];
                        char enc[8];
                        size_t n;
                        if (get_unicode_mode() && rule_like && cp == U'-')
                            n = utf8_encode(0x2500, enc);
                        else
                            n = encode_cell_utf8(cp, enc);
                        for (size_t k = 0; k < n; k++) str_putc(&out, enc[k]);
                        uint8_t cell_meta = CELL_META_NONE;
                        if (b.meta) cell_meta = b.meta[y * b.w + x];
                        meta_emit(meta, cell_meta);
                        mc_note_one(mc, -1,
                                    (rule_like && cp == U'-')
                                        ? CK_RULE
                                        : (cp == U' ' ? CK_SPACE : CK_MATH_GENERIC));
                    }
                }
                str_putc(&out, '\n');
                meta_emit(meta, CELL_META_NONE); // newline has no metadata
                mc_newline(mc);
            }
            box_free(&b);

            if (bi + 1 < bl.n && !bl.is_math[bi + 1]) {
                str_putc(&out, '\n');
                meta_emit(meta, CELL_META_NONE);
                mc_newline(mc);
            }
        }
    }

    blocks_free(&bl);
    free(cleaned_input);
    return str_detach(&out);
}

// ============================================================================
// Public API
// ============================================================================

char *compose_text(const char *input, const CompOptions *opt_in, ParseError *err) {
    return compose_text_internal(input, opt_in, NULL, NULL, err);
}

char *compose_text_with_map(const char *input, const CompOptions *opt_in, MapCtx *mc,
                            ParseError *err) {
    return compose_text_internal(input, opt_in, mc, NULL, err);
}

char *compose_text_with_meta(const char *input, const CompOptions *opt, uint8_t **out_meta,
                             int *out_meta_len, ParseError *err) {
    U8Buf meta;
    u8_init(&meta);

    char *result = compose_text_internal(input, opt, NULL, &meta, err);

    // Save length before detach (detach resets n to 0)
    int len = meta.n;

    if (out_meta) {
        *out_meta = u8_detach(&meta);
    } else {
        u8_free(&meta);
    }
    if (out_meta_len) {
        *out_meta_len = len;
    }

    return result;
}
