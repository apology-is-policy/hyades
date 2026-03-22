// parser_tokens.c - Token classification helpers
// Verbatim from original parser.c

#include "parser_internal.h"

int tok_is_sym_bar(const Token *t) {
    return t->kind == TOK_SYM && t->text[0] == '|';
}

// open/close for single bar: accept raw '|' and \lvert/\rvert/\vert
int tok_is_open_vbar(const Token *t) {
    return tok_is_sym_bar(t) || (t->kind == TOK_IDENT && (strcmp(t->text, "\\lvert") == 0 ||
                                                          strcmp(t->text, "\\vert") == 0));
}
int tok_is_close_vbar(const Token *t) {
    return tok_is_sym_bar(t) || (t->kind == TOK_IDENT && (strcmp(t->text, "\\rvert") == 0 ||
                                                          strcmp(t->text, "\\vert") == 0));
}

// double bar: \Vert for both open and close, \lVert/\rVert for explicit sides
int tok_is_open_dvbar(const Token *t) {
    return t->kind == TOK_IDENT &&
           (strcmp(t->text, "\\Vert") == 0 || strcmp(t->text, "\\lVert") == 0);
}
int tok_is_close_dvbar(const Token *t) {
    return t->kind == TOK_IDENT &&
           (strcmp(t->text, "\\Vert") == 0 || strcmp(t->text, "\\rVert") == 0);
}

// floors/ceils
int tok_is_open_floor(const Token *t) {
    return t->kind == TOK_IDENT && strcmp(t->text, "\\lfloor") == 0;
}
int tok_is_close_floor(const Token *t) {
    return t->kind == TOK_IDENT && strcmp(t->text, "\\rfloor") == 0;
}

int tok_is_open_ceil(const Token *t) {
    return t->kind == TOK_IDENT && strcmp(t->text, "\\lceil") == 0;
}
int tok_is_close_ceil(const Token *t) {
    return t->kind == TOK_IDENT && strcmp(t->text, "\\rceil") == 0;
}

// angle brackets
int tok_is_open_angle(const Token *t) {
    return t->kind == TOK_IDENT && strcmp(t->text, "\\langle") == 0;
}
int tok_is_close_angle(const Token *t) {
    return t->kind == TOK_IDENT && strcmp(t->text, "\\rangle") == 0;
}

int tok_is_rowbreak(const Token *t) {
    // Accept TeX \\ and also literal ';' as a row break
    if (t->kind == TOK_IDENT && strcmp(t->text, "\\\\") == 0) return 1;
    if (t->kind == TOK_SYM && t->text[0] == ';') return 1;
    return 0;
}

int macro_size(const char *s) {
    if (strcmp(s, "\\bigg") == 0 || strcmp(s, "\\Bigg") == 0 || strcmp(s, "\\biggl") == 0 ||
        strcmp(s, "\\Biggl") == 0 || strcmp(s, "\\biggr") == 0 || strcmp(s, "\\Biggr") == 0)
        return 7;
    if (strcmp(s, "\\Big") == 0 || strcmp(s, "\\Bigl") == 0 || strcmp(s, "\\Bigr") == 0) return 5;
    if (strcmp(s, "\\big") == 0 || strcmp(s, "\\bigl") == 0 || strcmp(s, "\\bigr") == 0) return 3;
    return 1;
}
int is_size_macro_tok(const Token *t) {
    return t->kind == TOK_IDENT &&
           (strcmp(t->text, "\\big") == 0 || strcmp(t->text, "\\Big") == 0 ||
            strcmp(t->text, "\\bigg") == 0 || strcmp(t->text, "\\Bigg") == 0);
}
int is_left_size_macro(const Token *t) {
    return t->kind == TOK_IDENT &&
           (strcmp(t->text, "\\bigl") == 0 || strcmp(t->text, "\\Bigl") == 0 ||
            strcmp(t->text, "\\biggl") == 0 || strcmp(t->text, "\\Biggl") == 0);
}
int is_right_size_macro(const Token *t) {
    return t->kind == TOK_IDENT &&
           (strcmp(t->text, "\\bigr") == 0 || strcmp(t->text, "\\Bigr") == 0 ||
            strcmp(t->text, "\\biggr") == 0 || strcmp(t->text, "\\Biggr") == 0);
}

// tolerant delimiter recognizers (ignore token kind)
int tok_is_open_round(const Token *t) {
    const char *s = t->text;
    return s && (s[0] == '(' || strcmp(s, "\\(") == 0);
}
int tok_is_open_square(const Token *t) {
    const char *s = t->text;
    return s && (s[0] == '[' || strcmp(s, "\\[") == 0);
}
int tok_is_open_curly(const Token *t) {
    const char *s = t->text;
    return s && (strcmp(s, "\\lbrace") == 0 || strcmp(s, "\\{") == 0 || s[0] == '{');
}
int tok_is_close_round(const Token *t) {
    const char *s = t->text;
    return s && (s[0] == ')' || strcmp(s, "\\)") == 0);
}
int tok_is_close_square(const Token *t) {
    const char *s = t->text;
    return s && (s[0] == ']' || strcmp(s, "\\]") == 0);
}
int tok_is_close_curly(const Token *t) {
    const char *s = t->text;
    return s && (strcmp(s, "\\rbrace") == 0 || strcmp(s, "\\}") == 0 || s[0] == '}');
}

int tok_is_relation(const Token *t) {
    if (t->kind != TOK_IDENT) return 0;

    const char *cmd = t->text;
    return strcmp(cmd, "\\leq") == 0 || strcmp(cmd, "\\geq") == 0 || strcmp(cmd, "\\neq") == 0 ||
           strcmp(cmd, "\\approx") == 0 || strcmp(cmd, "\\equiv") == 0 ||
           strcmp(cmd, "\\ll") == 0 || strcmp(cmd, "\\gg") == 0 || strcmp(cmd, "\\sim") == 0 ||
           strcmp(cmd, "\\simeq") == 0 || strcmp(cmd, "\\cong") == 0 ||
           strcmp(cmd, "\\propto") == 0 || strcmp(cmd, "\\mid") == 0 ||
           strcmp(cmd, "\\nmid") == 0 || strcmp(cmd, "\\subset") == 0 ||
           strcmp(cmd, "\\supset") == 0 || strcmp(cmd, "\\subseteq") == 0 ||
           strcmp(cmd, "\\supseteq") == 0 || strcmp(cmd, "\\in") == 0 ||
           strcmp(cmd, "\\notin") == 0 || strcmp(cmd, "\\ni") == 0 ||
           strcmp(cmd, "\\implies") == 0 || strcmp(cmd, "\\Rightarrow") == 0 ||
           strcmp(cmd, "\\iff") == 0 || strcmp(cmd, "\\Leftrightarrow") == 0 ||
           strcmp(cmd, "\\to") == 0 || strcmp(cmd, "\\rightarrow") == 0 ||
           strcmp(cmd, "\\leftarrow") == 0 || strcmp(cmd, "\\gets") == 0 ||
           strcmp(cmd, "\\leftrightarrow") == 0 || strcmp(cmd, "\\Leftarrow") == 0 ||
           strcmp(cmd, "\\mapsto") == 0 || strcmp(cmd, "\\prec") == 0 ||
           strcmp(cmd, "\\succ") == 0 || strcmp(cmd, "\\preceq") == 0 ||
           strcmp(cmd, "\\succeq") == 0 || strcmp(cmd, "\\nleq") == 0 ||
           strcmp(cmd, "\\ngeq") == 0 || strcmp(cmd, "\\nsubset") == 0 ||
           strcmp(cmd, "\\nequiv") == 0 || strcmp(cmd, "\\therefore") == 0 ||
           strcmp(cmd, "\\because") == 0 || strcmp(cmd, "\\coloneqq") == 0 ||
           strcmp(cmd, "\\Coloneqq") == 0 || strcmp(cmd, "\\eqqcolon") == 0;
}

int tok_is_close_for_pt(const Token *t, ParenType pt) {
    switch (pt) {
    case PAREN_ROUND: return tok_is_close_round(t);
    case PAREN_SQUARE: return tok_is_close_square(t);
    case PAREN_CURLY: return tok_is_close_curly(t);
    case PAREN_VBAR: return tok_is_close_vbar(t);
    case PAREN_DVBAR: return tok_is_close_dvbar(t);
    case PAREN_FLOOR: return tok_is_close_floor(t);
    case PAREN_CEIL: return tok_is_close_ceil(t);
    case PAREN_ANGLE: return tok_is_close_angle(t);
    default: return 0;
    }
}
int starts_primary_tok(const Token *t) {
    if (t->kind == TOK_IDENT) {
        if (is_cmd(t, "\\\\")) return 0;
        if (tok_is_rowbreak(t)) return 0; // defensive
        // Do NOT start a new primary on closers or \right
        if (is_cmd(t, "\\right") || is_cmd(t, "\\middle")) return 0;
        if (tok_is_close_round(t) || tok_is_close_square(t) || tok_is_close_curly(t) ||
            tok_is_close_vbar(t) || tok_is_close_dvbar(t) || tok_is_close_floor(t) ||
            tok_is_close_ceil(t))
            return 0;
        // Note: \langle/\rangle are NOT excluded here because they're used standalone
        // in bra-ket notation (e.g., |ψ⟩). They only act as delimiters in \left..\right.
        if (tok_is_relation(t)) return 0;
        // Do NOT start a new primary on additive operators (handled in parse_expr)
        if (is_cmd(t, "\\pm") || is_cmd(t, "\\mp")) return 0;
        // Atom-type overrides are handled at parse_expr level (like operators)
        if (is_cmd(t, "\\mathbin") || is_cmd(t, "\\mathrel")) return 0;
        return 1; // other control words / identifiers can start a primary
    }
    if (t->kind == TOK_SYM) {
        char c = t->text[0];
        if (c == '{' || c == '(' || c == '[') return 1;
        // Raw '|' CAN start a primary - parse_primary has lookahead logic
        // to handle it as either absolute value or a plain symbol
        if (c == '|') return 1;
        // do NOT start on closers
        if (c == '}' || c == ')' || c == ']' || c == ',' || c == ';' || c == ':') return 0;
        // Multi-byte UTF-8 symbols (e.g., °) can start a primary
        if ((unsigned char)c >= 0x80) return 1;
    }
    return 0;
}
int peek_is_matching_close(Parser *p) {
    Lexer tmp = p->lx;          // copy by value
    Token nxt = lex_next(&tmp); // peek next token
    return tok_is_close_for_pt(&nxt, p->bound_pt);
}
int at_sized_boundary(Parser *p) {
    if (p->in_sized <= 0) return 0;
    if (tok_is_close_for_pt(&p->look, p->bound_pt)) return 1;
    if (is_size_macro_tok(&p->look)) {
        if (macro_size(p->look.text) == p->bound_size && peek_is_matching_close(p)) return 1;
    }
    return 0;
}
