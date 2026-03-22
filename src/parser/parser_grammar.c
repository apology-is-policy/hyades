// parser_grammar.c - Main grammar: primary, factor, term, expr
// Verbatim from original parser.c

#include "parser_internal.h"

Ast *parse_primary(Parser *p) {
    if (p->look.kind == TOK_IDENT) {
        // Commands/macros
        if (is_cmd(&p->look, "\\frac")) {
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
        // Sized parens \big/\Big/\bigg/\Bigg
        if (is_cmd(&p->look, "\\big") || is_cmd(&p->look, "\\Big") || is_cmd(&p->look, "\\bigg") ||
            is_cmd(&p->look, "\\Bigg")) {
            int sz = macro_size(p->look.text);
            next(p); // consume the size macro
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
                return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected \\rvert");
            }
            next(p);
            return ast_paren(e, PAREN_VBAR, 1);
        }
        if (is_cmd(&p->look, "\\Vert")) { // same token opens/closes
            next(p);                      // open
            Ast *e = parse_expr(p);
            if (!e) return NULL;
            if (!tok_is_close_dvbar(&p->look)) {
                return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected closing \\Vert");
            }
            next(p);
            return ast_paren(e, PAREN_DVBAR, 1);
        }
        if (is_cmd(&p->look, "\\lfloor")) {
            next(p);
            Ast *e = parse_expr(p);
            if (!e) return NULL;
            if (!tok_is_close_floor(&p->look)) {
                return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected \\rfloor");
            }
            next(p);
            return ast_paren(e, PAREN_FLOOR, 1);
        }
        if (is_cmd(&p->look, "\\lceil")) {
            next(p);
            Ast *e = parse_expr(p);
            if (!e) return NULL;
            if (!tok_is_close_ceil(&p->look)) {
                return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected \\rceil");
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
            int sz = size_from_height(mbox.h);
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
                return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected '{' after matrix command");
            }

            int R = 0, C = 0;
            Ast *M = parse_matrix_core(p, &R, &C);
            if (!M) return NULL;

            if (!do_wrap) return M;

            // Auto-size wrappers by measuring matrix height
            Box mbox = render_ast(M);
            int sz = size_from_height(mbox.h);
            box_free(&mbox);

            return ast_paren(M, wrap, sz);
        }
        // ---- math alphabets with one argument ----
        if (is_cmd(&p->look, "\\mathbf") || is_cmd(&p->look, "\\mathbb") ||
            is_cmd(&p->look, "\\mathcal") || is_cmd(&p->look, "\\mathit") ||
            is_cmd(&p->look, "\\mathrm")) {
            StyleKind sk = is_cmd(&p->look, "\\mathbf")    ? STYLE_BOLD
                           : is_cmd(&p->look, "\\mathbb")  ? STYLE_BLACKBOARD
                           : is_cmd(&p->look, "\\mathcal") ? STYLE_SCRIPT
                           : is_cmd(&p->look, "\\mathit")  ? STYLE_ITALIC
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
        if (is_cmd(&p->look, "\\fn")) {
            next(p); // consume \fn

            // Expect {argument}
            if (!peek_ch(p, '{')) {
                return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "\\fn expects {argument}");
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
                return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "unclosed \\fn{...}");
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
                return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "\\text expects {argument}");
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
        // auto-sized delimiters: \left ... \right
        if (is_cmd(&p->look, "\\left")) {
            Ast *e = parse_left_right(p);
            if (!e) return NULL;
            return e;
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
        bool found_match = false;

        // Skip the opening |
        Token probe = lex_next(&saved_lx);

        while (probe.kind != TOK_EOF) {
            if (tok_is_sym_bar(&probe)) {
                depth--;
                if (depth == 0) {
                    found_match = true;
                    break;
                }
            }
            // Check for tokens that would make this NOT an absolute value
            // These include angle brackets, \right, closing delimiters without openers
            if (strcmp(probe.text, "\\rangle") == 0 || strcmp(probe.text, "\\right") == 0 ||
                strcmp(probe.text, "\\rbrace") == 0 || strcmp(probe.text, "\\}") == 0 ||
                (probe.kind == TOK_SYM && probe.text[0] == ')') ||
                (probe.kind == TOK_SYM && probe.text[0] == ']')) {
                // Hit an incompatible closer - this | is not starting absolute value
                break;
            }
            // Track nested |
            if (tok_is_sym_bar(&probe)) {
                depth++;
            }
            probe = lex_next(&saved_lx);
        }

        if (found_match) {
            // Parse as absolute value
            next(p); // consume opening '|'
            Ast *e = parse_expr(p);
            if (!e) return NULL;
            if (!tok_is_sym_bar(&p->look)) {
                return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected closing '|'");
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
            else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
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
                else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
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
            else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
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
                else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
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
        if (is_cmd(&p->look, "\\right") || tok_is_close_round(&p->look) ||
            tok_is_close_square(&p->look) || tok_is_close_curly(&p->look) ||
            tok_is_close_vbar(&p->look) || tok_is_close_dvbar(&p->look) ||
            tok_is_close_floor(&p->look) || tok_is_close_ceil(&p->look) ||
            tok_is_sym_bar(&p->look)) {
            break;
        }

        char opch = 0;

        // explicit '*'
        if (p->look.kind == TOK_SYM && p->look.text[0] == '*') {
            opch = '*';
            next(p);
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
        else if (is_cmd(&p->look, "\\bmod")) {
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
            // Handle < > too
            if (ch == '<' || ch == '>') {
                next(p);
                Ast *rhs = parse_term(p);
                if (!rhs) return NULL;
                lhs = ast_binop(ch, lhs, rhs);
                continue;
            }
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

        break;
    }
    return lhs;
}
