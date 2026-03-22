// parser_grammar.c - Main grammar: primary, factor, term, expr
// Verbatim from original parser.c

#include "parser_internal.h"

// From math_symbols.c — used for unknown command detection
extern int is_known_math_symbol(const char *s);

Ast *parse_primary(Parser *p) {
    if (p->look.kind == TOK_IDENT) {
        // Commands/macros
        if (is_cmd(&p->look, "\\frac") || is_cmd(&p->look, "\\dfrac") ||
            is_cmd(&p->look, "\\tfrac")) {
            Ast *x = parse_fraction(p);
            if (!x) return NULL;
            return x;
        }
        if (is_cmd(&p->look, "\\sum") || is_cmd(&p->look, "\\Sum") || is_cmd(&p->look, "\\SUM")) {
            Ast *x = parse_limop(p, AST_SUM, 0);
            if (!x) return NULL;
            return x;
        }
        // product
        if (is_cmd(&p->look, "\\prod") || is_cmd(&p->look, "\\Prod") ||
            is_cmd(&p->look, "\\PROD")) {
            Ast *x = parse_limop(p, AST_PROD, 0);
            if (!x) return NULL;
            return x;
        }
        // integral
        if (is_cmd(&p->look, "\\int") || is_cmd(&p->look, "\\Int") || is_cmd(&p->look, "\\INT")) {
            Ast *x = parse_limop(p, AST_INT, 0);
            if (!x) return NULL;
            return x;
        }
        // double integral
        if (is_cmd(&p->look, "\\iint") || is_cmd(&p->look, "\\Iint") ||
            is_cmd(&p->look, "\\IINT")) {
            Ast *x = parse_limop(p, AST_IINT, 0);
            if (!x) return NULL;
            return x;
        }
        // triple integral
        if (is_cmd(&p->look, "\\iiint") || is_cmd(&p->look, "\\Iiint") ||
            is_cmd(&p->look, "\\IIINT")) {
            Ast *x = parse_limop(p, AST_IIINT, 0);
            if (!x) return NULL;
            return x;
        }
        // contour integral
        if (is_cmd(&p->look, "\\oint") || is_cmd(&p->look, "\\Oint") ||
            is_cmd(&p->look, "\\OINT")) {
            Ast *x = parse_limop(p, AST_OINT, 0);
            if (!x) return NULL;
            return x;
        }
        // surface integral (double contour)
        if (is_cmd(&p->look, "\\oiint") || is_cmd(&p->look, "\\Oiint") ||
            is_cmd(&p->look, "\\OIINT")) {
            Ast *x = parse_limop(p, AST_OIINT, 0);
            if (!x) return NULL;
            return x;
        }
        // union / intersection (bigcup/bigcap families)
        if (is_cmd(&p->look, "\\bigcup") || is_cmd(&p->look, "\\Bigcup") ||
            is_cmd(&p->look, "\\biggcup") || is_cmd(&p->look, "\\Biggcup")) {
            Ast *x = parse_limop(p, AST_BIGCUP, 0);
            if (!x) return NULL;
            return x;
        }
        if (is_cmd(&p->look, "\\bigcap") || is_cmd(&p->look, "\\Bigcap") ||
            is_cmd(&p->look, "\\biggcap") || is_cmd(&p->look, "\\Biggcap")) {
            Ast *x = parse_limop(p, AST_BIGCAP, 0);
            if (!x) return NULL;
            return x;
        }
        // coproduct
        if (is_cmd(&p->look, "\\coprod") || is_cmd(&p->look, "\\Coprod") ||
            is_cmd(&p->look, "\\COPROD")) {
            Ast *x = parse_limop(p, AST_COPROD, 0);
            if (!x) return NULL;
            return x;
        }
        // Limit-style function operators: lim, max, min, sup, inf, etc.
        // These render with limits below/above like \sum, but with text instead of a symbol
        if (is_cmd(&p->look, "\\lim")) {
            return parse_limfunc(p, "lim");
        }
        if (is_cmd(&p->look, "\\limsup")) {
            return parse_limfunc(p, "lim sup");
        }
        if (is_cmd(&p->look, "\\liminf")) {
            return parse_limfunc(p, "lim inf");
        }
        if (is_cmd(&p->look, "\\max")) {
            return parse_limfunc(p, "max");
        }
        if (is_cmd(&p->look, "\\min")) {
            return parse_limfunc(p, "min");
        }
        if (is_cmd(&p->look, "\\sup")) {
            return parse_limfunc(p, "sup");
        }
        if (is_cmd(&p->look, "\\inf")) {
            return parse_limfunc(p, "inf");
        }
        if (is_cmd(&p->look, "\\arg")) {
            return parse_limfunc(p, "arg");
        }
        if (is_cmd(&p->look, "\\argmax")) {
            return parse_limfunc(p, "argmax"); // CS convention: single word
        }
        if (is_cmd(&p->look, "\\argmin")) {
            return parse_limfunc(p, "argmin"); // CS convention: single word
        }
        if (is_cmd(&p->look, "\\gcd")) {
            return parse_limfunc(p, "gcd");
        }
        if (is_cmd(&p->look, "\\lcm")) {
            return parse_limfunc(p, "lcm");
        }
        if (is_cmd(&p->look, "\\det")) {
            return parse_limfunc(p, "det");
        }
        if (is_cmd(&p->look, "\\Pr")) {
            return parse_limfunc(p, "Pr");
        }
        if (is_cmd(&p->look, "\\sqrt")) {
            Ast *x = parse_sqrt(p);
            if (!x) return NULL;
            return x;
        }
        // Standalone sized delimiters: \bigl/\bigr/\Bigl/\Bigr/\biggl/\biggr/\Biggl/\Biggr
        if (is_left_size_macro(&p->look) || is_right_size_macro(&p->look)) {
            int is_right = is_right_size_macro(&p->look);
            int sz = macro_size(p->look.text);
            next(p); // consume the size macro
            // Determine delimiter type from next token
            ParenType pt = PAREN_ROUND;
            if (tok_is_open_round(&p->look) || tok_is_close_round(&p->look))
                pt = PAREN_ROUND;
            else if (tok_is_open_square(&p->look) || tok_is_close_square(&p->look))
                pt = PAREN_SQUARE;
            else if (tok_is_open_curly(&p->look) || tok_is_close_curly(&p->look))
                pt = PAREN_CURLY;
            else if (tok_is_open_vbar(&p->look) || tok_is_close_vbar(&p->look))
                pt = PAREN_VBAR;
            else if (tok_is_open_dvbar(&p->look) || tok_is_close_dvbar(&p->look))
                pt = PAREN_DVBAR;
            else if (tok_is_open_floor(&p->look) || tok_is_close_floor(&p->look))
                pt = PAREN_FLOOR;
            else if (tok_is_open_ceil(&p->look) || tok_is_close_ceil(&p->look))
                pt = PAREN_CEIL;
            else if (tok_is_open_angle(&p->look) || tok_is_close_angle(&p->look))
                pt = PAREN_ANGLE;
            else {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX,
                                    "sized delimiter requires a delimiter character");
            }
            next(p); // consume the delimiter
            if (is_right)
                return ast_paren_asym(ast_symbol(""), PAREN_NONE, pt, sz);
            else
                return ast_paren_asym(ast_symbol(""), pt, PAREN_NONE, sz);
        }
        // Sized parens \big/\Big/\bigg/\Bigg
        if (is_cmd(&p->look, "\\big") || is_cmd(&p->look, "\\Big") || is_cmd(&p->look, "\\bigg") ||
            is_cmd(&p->look, "\\Bigg")) {
            int sz = macro_size(p->look.text);
            next(p); // consume the size macro
            if (p->look.kind == TOK_EOF) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX,
                                    "\\big requires a delimiter — e.g. \\big(, \\big[, \\big\\{");
            }
            Ast *x = parse_paren_sized(p, sz);
            if (!x) return NULL;
            return x;
        }
        // Size=1 curly via control words/symbols: \lbrace ... \rbrace  or  \{ ... \}
        if (is_cmd(&p->look, "\\lbrace") || is_cmd(&p->look, "\\{")) {
            Ast *x = parse_paren_basic(p, PAREN_CURLY);
            if (!x) return NULL;
            return x;
        }
        // Size=1 paired bars/floors/ceils
        if (is_cmd(&p->look, "\\lvert")) {
            next(p); // consume \lvert
            Ast *e = parse_expr(p);
            if (!e) return NULL;
            if (!tok_is_close_vbar(&p->look)) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected \\rvert");
            }
            next(p);
            return ast_paren(e, PAREN_VBAR, 1);
        }
        if (is_cmd(&p->look, "\\Vert")) { // same token opens/closes
            next(p);                      // open
            Ast *e = parse_expr(p);
            if (!e) return NULL;
            if (!tok_is_close_dvbar(&p->look)) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected closing \\Vert");
            }
            next(p);
            return ast_paren(e, PAREN_DVBAR, 1);
        }
        if (is_cmd(&p->look, "\\lVert")) {
            next(p);
            Ast *e = parse_expr(p);
            if (!e) return NULL;
            if (!tok_is_close_dvbar(&p->look)) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected \\rVert");
            }
            next(p);
            return ast_paren(e, PAREN_DVBAR, 1);
        }
        if (is_cmd(&p->look, "\\lfloor")) {
            next(p);
            Ast *e = parse_expr(p);
            if (!e) return NULL;
            if (!tok_is_close_floor(&p->look)) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected \\rfloor");
            }
            next(p);
            return ast_paren(e, PAREN_FLOOR, 1);
        }
        if (is_cmd(&p->look, "\\lceil")) {
            next(p);
            Ast *e = parse_expr(p);
            if (!e) return NULL;
            if (!tok_is_close_ceil(&p->look)) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected \\rceil");
            }
            next(p);
            return ast_paren(e, PAREN_CEIL, 1);
        }
        // --- binomial coefficient \binom{n}{k} → pmatrix with 2 rows, 1 col ---
        if (is_cmd(&p->look, "\\binom") || is_cmd(&p->look, "\\tbinom") ||
            is_cmd(&p->look, "\\dbinom")) {
            next(p); // consume \binom

            // Parse first argument (numerator)
            Ast *numer = NULL;
            if (peek_ch(p, '{'))
                numer = parse_group(p);
            else
                numer = parse_primary(p);
            if (!numer) return NULL;

            // Parse second argument (denominator)
            Ast *denom = NULL;
            if (peek_ch(p, '{'))
                denom = parse_group(p);
            else
                denom = parse_primary(p);
            if (!denom) return NULL;

            // Create a 2-row, 1-column matrix with minimal padding (0)
            // The paren rendering adds its own padding, so we don't need matrix padding
            Ast **cells = malloc(2 * sizeof(Ast *));
            cells[0] = numer;
            cells[1] = denom;
            Ast *matrix = ast_matrix(2, 1, cells, 0); // padding=0 for binomials

            // Auto-size parentheses based on matrix height
            Box mbox = render_ast(matrix);
            int sz = size_from_height(mbox.h, mbox.baseline);
            box_free(&mbox);

            return ast_paren(matrix, PAREN_ROUND, sz);
        }
        // --- matrices ---
        if (is_cmd(&p->look, "\\matrix") || is_cmd(&p->look, "\\pmatrix") ||
            is_cmd(&p->look, "\\bmatrix") || is_cmd(&p->look, "\\Bmatrix") ||
            is_cmd(&p->look, "\\vmatrix") || is_cmd(&p->look, "\\Vmatrix")) {
            // Remember which wrapper (if any)
            ParenType wrap = PAREN_ROUND;
            int do_wrap = 0;
            if (is_cmd(&p->look, "\\pmatrix")) {
                wrap = PAREN_ROUND;
                do_wrap = 1;
            } else if (is_cmd(&p->look, "\\bmatrix")) {
                wrap = PAREN_SQUARE;
                do_wrap = 1;
            } else if (is_cmd(&p->look, "\\Bmatrix")) {
                wrap = PAREN_CURLY;
                do_wrap = 1;
            } else if (is_cmd(&p->look, "\\vmatrix")) {
                wrap = PAREN_VBAR;
                do_wrap = 1;
            } else if (is_cmd(&p->look, "\\Vmatrix")) {
                wrap = PAREN_DVBAR;
                do_wrap = 1;
            }

            next(p); // consume command
            if (!peek_ch(p, '{')) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected '{' after matrix command");
            }

            int R = 0, C = 0;
            Ast *M = parse_matrix_core(p, &R, &C);
            if (!M) return NULL;

            if (!do_wrap) return M;

            // Auto-size wrappers by measuring matrix height
            Box mbox = render_ast(M);
            int sz = size_from_height(mbox.h, mbox.baseline);
            box_free(&mbox);

            return ast_paren(M, wrap, sz);
        }
        // ---- embedded document constructs: \cases, \aligned ----
        if (is_cmd(&p->look, "\\cases") || is_cmd(&p->look, "\\aligned")) {
            EmbedKind ek = is_cmd(&p->look, "\\cases") ? EMBED_CASES : EMBED_ALIGNED;
            next(p); // consume command

            if (!peek_ch(p, '{')) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX,
                                    ek == EMBED_CASES ? "expected '{' after \\cases"
                                                      : "expected '{' after \\aligned");
            }

            // At this point, p->look = '{' token, and p->lx.pos is right AFTER '{'
            // So p->lx.pos already points to the content we want to read
            int content_start = p->lx.pos;

            next(p); // consume '{' - this advances lexer

            // Scan from saved position to find matching closing brace
            int brace_depth = 1;
            int pos = content_start;
            while (brace_depth > 0 && p->lx.src[pos] != '\0') {
                char c = p->lx.src[pos];
                if (c == '{')
                    brace_depth++;
                else if (c == '}')
                    brace_depth--;
                if (brace_depth > 0) pos++;
            }

            // Extract content (everything between { and })
            int content_len = pos - content_start;
            char *content = malloc(content_len + 1);
            memcpy(content, p->lx.src + content_start, content_len);
            content[content_len] = '\0';

            // Sync lexer position past the closing brace
            p->lx.pos = pos + 1; // skip past '}'
            next(p);             // re-sync lexer

            Ast *result = ast_embed(ek, content);
            free(content);
            return result;
        }
        // ---- math alphabets with one argument ----
        if (is_cmd(&p->look, "\\mathbf") || is_cmd(&p->look, "\\mathbb") ||
            is_cmd(&p->look, "\\mathcal") || is_cmd(&p->look, "\\mathit") ||
            is_cmd(&p->look, "\\mathrm") || is_cmd(&p->look, "\\mathfrak") ||
            is_cmd(&p->look, "\\mathscr") || is_cmd(&p->look, "\\mathsf") ||
            is_cmd(&p->look, "\\boldsymbol")) {
            StyleKind sk =
                is_cmd(&p->look, "\\mathbf") || is_cmd(&p->look, "\\boldsymbol") ? STYLE_BOLD
                : is_cmd(&p->look, "\\mathbb")                                   ? STYLE_BLACKBOARD
                : is_cmd(&p->look, "\\mathcal") || is_cmd(&p->look, "\\mathscr") ? STYLE_SCRIPT
                : is_cmd(&p->look, "\\mathit")                                   ? STYLE_ITALIC
                : is_cmd(&p->look, "\\mathfrak")                                 ? STYLE_FRAKTUR
                : is_cmd(&p->look, "\\mathsf")                                   ? STYLE_SANS
                                                                                 : STYLE_ROMAN;
            next(p); // consume the control word

            // \mathit and \mathrm: raw-read content to preserve spaces (like \text)
            if ((sk == STYLE_ITALIC || sk == STYLE_ROMAN) && peek_ch(p, '{')) {
                int content_pos = p->lx.pos;
                next(p); // consume '{'
                char text_buf[256];
                int text_len = 0;
                int brace_depth = 1;
                int pos = content_pos;
                while (brace_depth > 0 && p->lx.src[pos] != '\0' && text_len < 255) {
                    char c = p->lx.src[pos];
                    if (c == '{') {
                        brace_depth++;
                        text_buf[text_len++] = c;
                    } else if (c == '}') {
                        brace_depth--;
                        if (brace_depth > 0) text_buf[text_len++] = c;
                    } else {
                        text_buf[text_len++] = c;
                    }
                    pos++;
                }
                text_buf[text_len] = '\0';
                p->lx.pos = pos;
                next(p); // re-sync lexer
                return ast_style(sk, ast_text(text_buf));
            }

            // Other styles: parse as math group
            Ast *arg = NULL;
            if (peek_ch(p, '{'))
                arg = parse_group(p);
            else
                arg = parse_primary(p);
            if (!arg) return NULL;

            return ast_style(sk, arg);
        }
        // ---- overset / underset / stackrel ----
        if (is_cmd(&p->look, "\\overset") || is_cmd(&p->look, "\\underset") ||
            is_cmd(&p->look, "\\stackrel")) {
            bool is_over = !is_cmd(&p->look, "\\underset");
            next(p); // consume command

            // Parse first argument (annotation for overset/stackrel, bottom for underset)
            Ast *first = NULL;
            if (peek_ch(p, '{'))
                first = parse_group(p);
            else
                first = parse_primary(p);
            if (!first) return NULL;

            // Parse second argument (base)
            Ast *second = NULL;
            if (peek_ch(p, '{'))
                second = parse_group(p);
            else
                second = parse_primary(p);
            if (!second) return NULL;

            return ast_overset(second, first, is_over);
        }
        // ---- boxed ----
        if (is_cmd(&p->look, "\\boxed")) {
            next(p); // consume command
            Ast *arg = NULL;
            if (peek_ch(p, '{'))
                arg = parse_group(p);
            else
                arg = parse_primary(p);
            if (!arg) return NULL;
            return ast_boxed(arg);
        }
        // ---- phantom ----
        if (is_cmd(&p->look, "\\phantom")) {
            next(p); // consume command
            Ast *arg = NULL;
            if (peek_ch(p, '{'))
                arg = parse_group(p);
            else
                arg = parse_primary(p);
            if (!arg) return NULL;
            return ast_phantom(arg);
        }
        // ---- smash ----
        if (is_cmd(&p->look, "\\smash")) {
            next(p); // consume command
            Ast *arg = NULL;
            if (peek_ch(p, '{'))
                arg = parse_group(p);
            else
                arg = parse_primary(p);
            if (!arg) return NULL;
            return ast_smash(arg);
        }
        // ---- extensible arrows ----
        if (is_cmd(&p->look, "\\xrightarrow") || is_cmd(&p->look, "\\xleftarrow")) {
            bool is_right = is_cmd(&p->look, "\\xrightarrow");
            next(p); // consume command
            Ast *label = NULL;
            if (peek_ch(p, '{'))
                label = parse_group(p);
            else
                label = parse_primary(p);
            if (!label) return NULL;
            return ast_xarrow(label, is_right);
        }
        // ---- substack ----
        if (is_cmd(&p->look, "\\substack")) {
            next(p); // consume command
            if (!peek_ch(p, '{')) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected '{' after \\substack");
            }
            next(p); // consume '{'

            // Parse rows separated by \\ until '}'
            int cap = 8;
            int n = 0;
            Ast **rows = malloc(cap * sizeof(Ast *));

            while (!peek_ch(p, '}') && p->look.kind != TOK_EOF) {
                // Skip row breaks between rows
                if (tok_is_rowbreak(&p->look)) {
                    next(p);
                    continue;
                }
                Ast *row = parse_expr(p);
                if (!row) {
                    free(rows);
                    return NULL;
                }
                if (n >= cap) {
                    cap *= 2;
                    rows = realloc(rows, cap * sizeof(Ast *));
                }
                rows[n++] = row;
            }

            if (!peek_ch(p, '}')) {
                for (int i = 0; i < n; i++) ast_free(rows[i]);
                free(rows);
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected '}' to close \\substack");
            }
            next(p); // consume '}'
            return ast_substack(n, rows);
        }
        // ---- mathord: force ordinary spacing ----
        if (is_cmd(&p->look, "\\mathord")) {
            next(p); // consume \mathord
            if (!peek_ch(p, '{')) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "\\mathord expects {argument}");
            }
            // Read raw content (like \text) so operators aren't parsed as binops
            int content_pos = p->lx.pos;
            next(p); // consume '{'
            char text_buf[64];
            int text_len = 0;
            int brace_depth = 1;
            int pos = content_pos;
            while (brace_depth > 0 && p->lx.src[pos] != '\0' && text_len < 63) {
                char c = p->lx.src[pos];
                if (c == '{') {
                    brace_depth++;
                    text_buf[text_len++] = c;
                } else if (c == '}') {
                    brace_depth--;
                    if (brace_depth > 0) text_buf[text_len++] = c;
                } else {
                    text_buf[text_len++] = c;
                }
                pos++;
            }
            text_buf[text_len] = '\0';
            p->lx.pos = pos;
            next(p); // re-sync lexer
            // Trim leading/trailing whitespace
            char *start = text_buf;
            while (*start == ' ') start++;
            char *end = start + strlen(start);
            while (end > start && *(end - 1) == ' ') end--;
            *end = '\0';
            return ast_symbol(start);
        }
        // ---- tag ----
        if (is_cmd(&p->look, "\\tag")) {
            next(p); // consume command
            Ast *arg = NULL;
            if (peek_ch(p, '{'))
                arg = parse_group(p);
            else
                arg = parse_primary(p);
            if (!arg) return NULL;
            return ast_tag(arg);
        }
        // ---- overbrace / underbrace ----
        if (is_cmd(&p->look, "\\overbrace") || is_cmd(&p->look, "\\underbrace")) {
            bool is_over = is_cmd(&p->look, "\\overbrace");
            next(p); // consume command

            // Parse content: {group} or single primary
            Ast *content = NULL;
            if (peek_ch(p, '{'))
                content = parse_group(p);
            else
                content = parse_primary(p);
            if (!content) return NULL;

            // Eagerly consume optional label: ^{} or _{} for either brace type
            // (LaTeX accepts both; canonical placement is above for overbrace, below for underbrace)
            Ast *label = NULL;
            if (peek_ch(p, '^') || peek_ch(p, '_')) {
                next(p); // consume ^ or _
                if (peek_ch(p, '{'))
                    label = parse_group(p);
                else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
                    label = ast_symbol(p->look.text);
                    next(p);
                } else
                    label = parse_primary(p);
            }

            return is_over ? ast_overbrace(content, label) : ast_underbrace(content, label);
        }
        // ---- accents with one argument ----
        if (is_cmd(&p->look, "\\hat") || is_cmd(&p->look, "\\widehat") ||
            is_cmd(&p->look, "\\bar") || is_cmd(&p->look, "\\overline") ||
            is_cmd(&p->look, "\\tilde") || is_cmd(&p->look, "\\widetilde") ||
            is_cmd(&p->look, "\\dot") || is_cmd(&p->look, "\\ddot") || is_cmd(&p->look, "\\vec") ||
            is_cmd(&p->look, "\\acute") || is_cmd(&p->look, "\\grave") ||
            is_cmd(&p->look, "\\breve") || is_cmd(&p->look, "\\check") ||
            is_cmd(&p->look, "\\underline") || is_cmd(&p->look, "\\overrightarrow") ||
            is_cmd(&p->look, "\\overleftarrow")) {

            AccentKind ak;
            if (is_cmd(&p->look, "\\hat") || is_cmd(&p->look, "\\widehat"))
                ak = ACCENT_HAT;
            else if (is_cmd(&p->look, "\\bar") || is_cmd(&p->look, "\\overline"))
                ak = ACCENT_BAR;
            else if (is_cmd(&p->look, "\\tilde") || is_cmd(&p->look, "\\widetilde"))
                ak = ACCENT_TILDE;
            else if (is_cmd(&p->look, "\\dot"))
                ak = ACCENT_DOT;
            else if (is_cmd(&p->look, "\\ddot"))
                ak = ACCENT_DDOT;
            else if (is_cmd(&p->look, "\\vec"))
                ak = ACCENT_VEC;
            else if (is_cmd(&p->look, "\\acute"))
                ak = ACCENT_ACUTE;
            else if (is_cmd(&p->look, "\\grave"))
                ak = ACCENT_GRAVE;
            else if (is_cmd(&p->look, "\\breve"))
                ak = ACCENT_BREVE;
            else if (is_cmd(&p->look, "\\check"))
                ak = ACCENT_CHECK;
            else if (is_cmd(&p->look, "\\underline"))
                ak = ACCENT_UNDERLINE;
            else if (is_cmd(&p->look, "\\overrightarrow"))
                ak = ACCENT_OVERRIGHTARROW;
            else if (is_cmd(&p->look, "\\overleftarrow"))
                ak = ACCENT_OVERLEFTARROW;
            else // shouldn't reach here
                ak = ACCENT_CHECK;

            next(p); // consume the accent command

            // Argument: accept {group} or single primary
            Ast *arg = NULL;
            if (peek_ch(p, '{'))
                arg = parse_group(p);
            else
                arg = parse_primary(p);
            if (!arg) return NULL;

            return ast_accent(ak, arg);
        }
        if (is_cmd(&p->look, "\\fn") || is_cmd(&p->look, "\\operatorname")) {
            next(p); // consume \fn

            // Expect {argument}
            if (!peek_ch(p, '{')) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "\\fn expects {argument}");
            }
            next(p); // consume '{'

            // Collect function name as text (like \text does)
            char fn_buf[256];
            int fn_len = 0;

            while (!peek_ch(p, '}') && p->look.kind != TOK_EOF && fn_len < 255) {
                const char *tok = p->look.text;
                while (*tok && fn_len < 255) {
                    fn_buf[fn_len++] = *tok++;
                }
                next(p);
            }
            fn_buf[fn_len] = '\0';

            if (!peek_ch(p, '}')) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "unclosed \\fn{...}");
            }
            next(p); // consume '}'

            // Create a FUNCTION node wrapping a TEXT node
            // This marks it as a function operator for spacing purposes
            Ast *text_node = ast_text(fn_buf);
            return ast_function(text_node);
        }
        // \pmod{n} → " (mod n)" - parenthetical modulus with preceding space
        if (is_cmd(&p->look, "\\pmod")) {
            next(p); // consume \pmod

            // Parse argument
            Ast *arg = NULL;
            if (peek_ch(p, '{'))
                arg = parse_group(p);
            else
                arg = parse_primary(p);
            if (!arg) return NULL;

            // Build: " " + "(" + "mod" + " " + arg + ")"
            // The preceding space is TeX convention for \pmod
            Ast *space = ast_symbol("\\,"); // thin space before
            Ast *lparen = ast_symbol("(");
            Ast *mod_text = ast_text("mod ");
            Ast *rparen = ast_symbol(")");

            // Concatenate: thin_space + ( + "mod " + arg + )
            Ast *t0 = ast_binop('\0', space, lparen);
            Ast *t1 = ast_binop('\0', t0, mod_text);
            Ast *t2 = ast_binop('\0', t1, arg);
            Ast *result = ast_binop('\0', t2, rparen);

            return result;
        }
        if (is_cmd(&p->look, "\\text")) {
            next(p); // consume \text - now p->look = '{'

            // Expect opening brace
            if (!peek_ch(p, '{')) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "\\text expects {argument}");
            }

            // At this point, p->look = '{' token, and p->lx.pos is right AFTER '{'
            // So p->lx.pos already points to the content we want to read
            int content_pos = p->lx.pos; // position right after '{'

            next(p); // consume '{' - this advances p->lx.pos past whitespace to next token

            // Read raw characters from saved position until matching '}'
            // This preserves whitespace unlike normal tokenization
            char text_buf[256];
            int text_len = 0;
            int brace_depth = 1;
            int pos = content_pos; // start right after '{'

            while (brace_depth > 0 && p->lx.src[pos] != '\0' && text_len < 255) {
                char c = p->lx.src[pos];

                if (c == '{') {
                    brace_depth++;
                    text_buf[text_len++] = c;
                    pos++;
                } else if (c == '}') {
                    brace_depth--;
                    if (brace_depth > 0) {
                        text_buf[text_len++] = c;
                    }
                    pos++;
                } else {
                    text_buf[text_len++] = c;
                    pos++;
                }
            }

            text_buf[text_len] = '\0';

            // Sync the lexer position to after the closing '}'
            p->lx.pos = pos;

            // Re-sync the lexer by getting the next token
            next(p);

            return ast_text(text_buf);
        }
        // \not prefix — negated relations
        if (is_cmd(&p->look, "\\not")) {
            next(p); // consume \not
            const char *neg = NULL;
            if (peek_ch(p, '=') || is_cmd(&p->look, "\\eq"))
                neg = "\\neq";
            else if (is_cmd(&p->look, "\\in"))
                neg = "\\notin";
            else if (is_cmd(&p->look, "\\leq") || is_cmd(&p->look, "\\le"))
                neg = "\\nleq";
            else if (is_cmd(&p->look, "\\geq") || is_cmd(&p->look, "\\ge"))
                neg = "\\ngeq";
            else if (is_cmd(&p->look, "\\subset"))
                neg = "\\nsubset";
            else if (is_cmd(&p->look, "\\equiv"))
                neg = "\\nequiv";
            else if (is_cmd(&p->look, "\\exists"))
                neg = "\\nexists";
            if (neg) {
                next(p); // consume the negated token
                return ast_symbol(neg);
            }
            // Unknown \not X — just emit the next symbol as-is (best effort)
            return ast_symbol("\\neg");
        }
        // No-op style/tag commands: Hyades has no math size concept, so these are transparent
        if (is_cmd(&p->look, "\\displaystyle") || is_cmd(&p->look, "\\textstyle") ||
            is_cmd(&p->look, "\\scriptstyle") || is_cmd(&p->look, "\\scriptscriptstyle") ||
            is_cmd(&p->look, "\\notag") || is_cmd(&p->look, "\\nonumber")) {
            next(p);
            return parse_primary(p);
        }
        // auto-sized delimiters: \left ... \right
        if (is_cmd(&p->look, "\\left")) {
            Ast *e = parse_left_right(p);
            if (!e) return NULL;
            return e;
        }

        // Check for unknown \commands — anything starting with '\' that wasn't
        // handled above and isn't recognized by the symbol system
        if (p->look.text[0] == '\\' && p->look.text[1] != '\0') {
            if (!is_known_math_symbol(p->look.text)) {
                return err_ret_here(p, PARSE_ERR_UNKNOWN_COMMAND, "unknown command: %s",
                                    p->look.text);
            }
        }

        // Fallthrough: bare identifier / number / control symbol → SYMBOL
        Ast *id = ast_symbol(p->look.text);
        next(p);
        return id;
    }

    // Groups
    if (peek_ch(p, '{')) return parse_group(p);
    // Basic parens
    if (peek_ch(p, '(') || is_cmd(&p->look, "\\(")) {
        Ast *e = parse_paren_basic(p, PAREN_ROUND);
        if (!e) return NULL;
        return e;
    }
    if (peek_ch(p, '[') || is_cmd(&p->look, "\\[")) {
        Ast *e = parse_paren_basic(p, PAREN_SQUARE);
        if (!e) return NULL;
        return e;
    }

    // Absolute value: |x| (bare vertical bars as TOK_SYM)
    // Only parse as absolute value if we can find a matching closing |
    // before hitting incompatible delimiters like \rangle, \right, etc.
    if (tok_is_sym_bar(&p->look)) {
        // Lookahead: scan to see if there's a matching '|' before an incompatible token
        Lexer saved_lx = p->lx; // save lexer state
        int depth = 1;
        int paren_depth = 0;   // Track (...) depth
        int bracket_depth = 0; // Track [...] depth
        int brace_depth = 0;   // Track {...} depth
        bool found_match = false;

        // Skip the opening |
        Token probe = lex_next(&saved_lx);

        while (probe.kind != TOK_EOF) {
            // Track nested delimiters
            if (probe.kind == TOK_SYM && probe.text[0] == '(') paren_depth++;
            if (probe.kind == TOK_SYM && probe.text[0] == ')') paren_depth--;
            if (probe.kind == TOK_SYM && probe.text[0] == '[') bracket_depth++;
            if (probe.kind == TOK_SYM && probe.text[0] == ']') bracket_depth--;
            if (probe.kind == TOK_SYM && probe.text[0] == '{') brace_depth++;
            if (tok_is_close_curly(&probe)) brace_depth--;

            // Check for matching |
            if (tok_is_sym_bar(&probe) && paren_depth == 0 && bracket_depth == 0 &&
                brace_depth == 0) {
                depth--;
                if (depth == 0) {
                    found_match = true;
                    break;
                }
            }
            // Check for tokens that would make this NOT an absolute value
            // Only break on unmatched closers (when depth is negative)
            if (strcmp(probe.text, "\\rangle") == 0 || strcmp(probe.text, "\\right") == 0) {
                // These always break - they're for \left...\right pairs
                break;
            }
            if ((probe.kind == TOK_SYM && probe.text[0] == ')' && paren_depth < 0) ||
                (probe.kind == TOK_SYM && probe.text[0] == ']' && bracket_depth < 0) ||
                (tok_is_close_curly(&probe) && brace_depth < 0)) {
                // Unmatched closer - this | is not starting absolute value
                break;
            }
            // Track nested |
            if (tok_is_sym_bar(&probe) && paren_depth == 0 && bracket_depth == 0 &&
                brace_depth == 0) {
                depth++;
            }
            probe = lex_next(&saved_lx);
        }

        if (found_match) {
            // Parse as absolute value
            next(p);        // consume opening '|'
            p->in_absval++; // Mark that we're inside absolute value
            Ast *e = parse_expr(p);
            p->in_absval--;
            if (!e) return NULL;
            if (!tok_is_sym_bar(&p->look)) {
                return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected closing '|'");
            }
            next(p); // consume closing '|'
            return ast_paren(e, PAREN_VBAR, 1);
        } else {
            // Treat | as a regular symbol (e.g., separator in bra-ket)
            Ast *id = ast_symbol(p->look.text);
            next(p);
            return id;
        }
    }

    // Fallback: single symbol
    Ast *id = ast_symbol(p->look.text);
    next(p);
    return id;
}

Ast *parse_factor(Parser *p) {
    Ast *base = parse_primary(p);
    if (!base) return NULL;
    // Parse super/subscripts, detecting combined patterns
    // Also handle primes: f' renders as f′ (prime appended, not superscript)
    for (;;) {
        // Handle prime(s): ' becomes appended prime symbol(s)
        if (p->look.kind == TOK_SYM && p->look.text[0] == '\'') {
            // Count consecutive primes
            int prime_count = 0;
            while (p->look.kind == TOK_SYM && p->look.text[0] == '\'') {
                prime_count++;
                next(p);
            }
            // Create the appropriate prime symbol to append (not superscript!)
            // This gives us f′ instead of f with ′ floating above
            Ast *prime_ast;
            if (prime_count == 1) {
                prime_ast = ast_symbol("\\prime");
            } else if (prime_count == 2) {
                prime_ast = ast_symbol("\\dprime");
            } else if (prime_count == 3) {
                prime_ast = ast_symbol("\\tprime");
            } else {
                // For 4+ primes, concatenate multiple prime symbols
                Ast *seq = ast_symbol("\\prime");
                for (int i = 1; i < prime_count; i++) {
                    Ast *next_prime = ast_symbol("\\prime");
                    seq = ast_binop('\0', seq, next_prime);
                }
                prime_ast = seq;
            }
            // Append directly (implicit multiplication = concatenation)
            base = ast_binop('\0', base, prime_ast);
            continue;
        }
        // Handle factorial: n! becomes n followed by "!"
        if (p->look.kind == TOK_SYM && p->look.text[0] == '!') {
            next(p); // consume '!'
            Ast *factorial = ast_symbol("!");
            base = ast_binop('\0', base, factorial);
            continue;
        }
        if (peek_ch(p, '^')) {
            next(p);
            Ast *sup = NULL;
            if (peek_ch(p, '{'))
                sup = parse_group(p);
            else if (p->look.kind == TOK_SYM) {
                sup = ast_symbol(p->look.text);
                next(p);
            } else
                sup = parse_primary(p);
            if (!sup) return NULL;

            // Check if subscript follows immediately
            if (peek_ch(p, '_')) {
                next(p);
                Ast *sub = NULL;
                if (peek_ch(p, '{'))
                    sub = parse_group(p);
                else if (p->look.kind == TOK_SYM) {
                    sub = ast_symbol(p->look.text);
                    next(p);
                } else
                    sub = parse_primary(p);
                if (!sub) {
                    ast_free(sup);
                    return NULL;
                }

                // Create combined node
                base = ast_supsub(base, sup, sub);
                continue;
            } else {
                base = ast_sup(base, sup);
            }
        } else if (peek_ch(p, '_')) {
            next(p);
            Ast *sub = NULL;
            if (peek_ch(p, '{'))
                sub = parse_group(p);
            else if (p->look.kind == TOK_SYM) {
                sub = ast_symbol(p->look.text);
                next(p);
            } else
                sub = parse_primary(p);
            if (!sub) return NULL;

            // Check if superscript follows immediately
            if (peek_ch(p, '^')) {
                next(p);
                Ast *sup = NULL;
                if (peek_ch(p, '{'))
                    sup = parse_group(p);
                else if (p->look.kind == TOK_SYM) {
                    sup = ast_symbol(p->look.text);
                    next(p);
                } else
                    sup = parse_primary(p);
                if (!sup) {
                    ast_free(sub);
                    return NULL;
                }

                // Create combined node
                base = ast_supsub(base, sup, sub);
                continue;
            } else {
                base = ast_sub(base, sub);
            }
        } else
            break;
    }
    return base;
}

Ast *parse_term(Parser *p) {
    Ast *lhs = parse_factor(p);
    if (!lhs) return NULL;
    for (;;) {
        // If we’re inside a sized paren and the next tokens close it, stop the term here.
        if (at_sized_boundary(p)) break;
        // Hard boundary: do not read across \right or a closing delimiter.
        // For raw '|', only break if we're inside absolute value parsing (in_absval > 0).
        // Outside absolute value, '|' can be part of expressions like bra-ket (⟨φ|ψ⟩).
        if (is_cmd(&p->look, "\\right") || is_cmd(&p->look, "\\middle") ||
            tok_is_close_round(&p->look) || tok_is_close_square(&p->look) ||
            tok_is_close_curly(&p->look) || tok_is_close_dvbar(&p->look) ||
            tok_is_close_floor(&p->look) || tok_is_close_ceil(&p->look)) {
            break;
        }
        // For vbar: only break on \rvert or \vert commands, or raw '|' when inside absolute value
        if (tok_is_close_vbar(&p->look)) {
            if (p->in_absval > 0) break; // inside |...| - stop at closing |
            // Outside absolute value: only break on explicit \rvert/\vert, not raw |
            if (p->look.kind == TOK_IDENT) break; // \rvert or \vert
            // Raw '|' outside absolute value: continue parsing (will be handled as symbol)
        }

        char opch = 0;

        // explicit '*' or '/'
        if (p->look.kind == TOK_SYM && p->look.text[0] == '*') {
            opch = '*';
            next(p);
        } else if (p->look.kind == TOK_SYM && p->look.text[0] == '/') {
            opch = 'S';
            next(p); // 'S' for slash (distinct from '/' used by \div)
        }
        // operator macros
        else if (is_cmd(&p->look, "\\cdot")) {
            opch = '.';
            next(p);
        } // dot
        else if (is_cmd(&p->look, "\\times")) {
            opch = 'x';
            next(p);
        } // ×
        else if (is_cmd(&p->look, "\\div")) {
            opch = '/';
            next(p);
        } // ÷
        else if (is_cmd(&p->look, "\\ast")) {
            opch = 'a';
            next(p);
        } // ∗ asterisk
        else if (is_cmd(&p->look, "\\star")) {
            opch = 's';
            next(p);
        } // ⋆ star
        else if (is_cmd(&p->look, "\\circ")) {
            opch = 'o';
            next(p);
        } // ∘
        else if (is_cmd(&p->look, "\\bullet")) {
            opch = 'b';
            next(p);
        } // •
        else if (is_cmd(&p->look, "\\cup")) {
            opch = 'U';
            next(p);
        } // ∪
        else if (is_cmd(&p->look, "\\cap")) {
            opch = 'n';
            next(p);
        } // ∩
        else if (is_cmd(&p->look, "\\setminus")) {
            opch = '\\';
            next(p);
        } // ∖
        else if (is_cmd(&p->look, "\\land") || is_cmd(&p->look, "\\wedge")) {
            opch = '&';
            next(p);
        } // ∧
        else if (is_cmd(&p->look, "\\lor") || is_cmd(&p->look, "\\vee")) {
            opch = '|';
            next(p);
        } // ∨
        else if (is_cmd(&p->look, "\\oplus")) {
            opch = 'P';
            next(p);
        } // ⊕
        else if (is_cmd(&p->look, "\\ominus")) {
            opch = 'M';
            next(p);
        } // ⊖
        else if (is_cmd(&p->look, "\\otimes")) {
            opch = 'X';
            next(p);
        } // ⊗
        else if (is_cmd(&p->look, "\\odot")) {
            opch = 'O';
            next(p);
        } // ⊙
        else if (is_cmd(&p->look, "\\dagger") || is_cmd(&p->look, "\\dag")) {
            opch = 'd';
            next(p);
        } // †
        else if (is_cmd(&p->look, "\\ddagger") || is_cmd(&p->look, "\\ddag")) {
            opch = 'D';
            next(p);
        } // ‡
        else if (is_cmd(&p->look, "\\bmod") || is_cmd(&p->look, "\\mod")) {
            opch = 'm';
            next(p);
        } // mod (binary operator)
        // implicit multiplication (juxtaposition)
        else if (starts_primary_tok(&p->look)) {
            opch = '\0'; // implicit multiplication: no printed operator
        } else
            break;

        Ast *rhs = parse_factor(p);
        if (!rhs) return NULL;
        lhs = ast_binop(opch, lhs, rhs);
    }
    return lhs;
}

Ast *parse_expr(Parser *p) {
    Ast *lhs = parse_term(p);
    if (!lhs) return NULL;

    while (1) {
        // Character-based operators
        if (p->look.kind == TOK_SYM) {
            char ch = p->look.text[0];
            if (ch == '+' || ch == '-' || ch == '=' || ch == ',') {
                next(p);
                Ast *rhs = parse_term(p);
                if (!rhs) return NULL;
                lhs = ast_binop(ch, lhs, rhs);
                continue;
            }
            if (ch == '<' || ch == '>') {
                next(p);
                Ast *rhs = parse_term(p);
                if (!rhs) return NULL;
                lhs = ast_binop(ch, lhs, rhs);
                continue;
            }
            if (ch == ':') {
                next(p); // consume ':'
                if (p->look.kind == TOK_SYM && p->look.text[0] == '=') {
                    next(p); // consume '=' → combined := relation
                    Ast *rhs = parse_term(p);
                    if (!rhs) return NULL;
                    lhs = ast_binop_str(":=", lhs, rhs);
                } else {
                    Ast *rhs = parse_term(p);
                    if (!rhs) return NULL;
                    lhs = ast_binop(':', lhs, rhs); // punctuation spacing
                }
                continue;
            }
            // Note: raw '|' is NOT treated as a binary operator here.
            // In bra-ket notation like ⟨φ|ψ⟩, the | is just a symbol without spacing.
            // Use \mid for relation "divides" with proper spacing (a \mid b).
            // The | is handled in parse_primary as either absolute value or plain symbol.
        }

        // Plus-minus and minus-plus operators
        if (is_cmd(&p->look, "\\pm")) {
            next(p);
            Ast *rhs = parse_term(p);
            if (!rhs) return NULL;
            lhs = ast_binop('p', lhs, rhs); // 'p' for \pm
            continue;
        }
        if (is_cmd(&p->look, "\\mp")) {
            next(p);
            Ast *rhs = parse_term(p);
            if (!rhs) return NULL;
            lhs = ast_binop('q', lhs, rhs); // 'q' for \mp
            continue;
        }

        // Command-based relations
        if (p->look.kind == TOK_IDENT && tok_is_relation(&p->look)) {
            char op_str[32];
            strncpy(op_str, p->look.text, 31);
            op_str[31] = '\0';
            next(p);
            Ast *rhs = parse_term(p);
            if (!rhs) return NULL;
            lhs = ast_binop_str(op_str, lhs, rhs); // New function for string ops
            continue;
        }

        // \mathbin{X} / \mathrel{X}: explicit atom-type overrides
        // Both produce relation-style spacing (gap=1 on both sides)
        if (is_cmd(&p->look, "\\mathbin") || is_cmd(&p->look, "\\mathrel")) {
            next(p);                     // consume command
            if (!peek_ch(p, '{')) break; // no argument, give up
            // Read raw content
            int content_pos = p->lx.pos;
            next(p); // consume '{'
            char op_buf[32];
            int op_len = 0;
            int brace_depth = 1;
            int pos = content_pos;
            while (brace_depth > 0 && p->lx.src[pos] != '\0' && op_len < 31) {
                char c = p->lx.src[pos];
                if (c == '{') {
                    brace_depth++;
                    op_buf[op_len++] = c;
                } else if (c == '}') {
                    brace_depth--;
                    if (brace_depth > 0) op_buf[op_len++] = c;
                } else {
                    op_buf[op_len++] = c;
                }
                pos++;
            }
            op_buf[op_len] = '\0';
            p->lx.pos = pos;
            next(p); // re-sync lexer
            // Trim whitespace
            char *s = op_buf;
            while (*s == ' ') s++;
            char *e = s + strlen(s);
            while (e > s && *(e - 1) == ' ') e--;
            *e = '\0';

            Ast *rhs = parse_term(p);
            if (!rhs) return NULL;
            lhs = ast_binop_str(s, lhs, rhs);
            continue;
        }

        break;
    }
    return lhs;
}
