// parser_core.c - Parser state and core utilities
// Verbatim from original parser.c

#include "parser_internal.h"

Ast *err_ret(Parser *p, ParseErrorCode code, int row, int col, const char *fmt, ...) {
    if (p && p->err) {
        p->err->code = code;
        p->err->row = row;
        p->err->col = col;
        if (fmt && *fmt) {
            va_list ap;
            va_start(ap, fmt);
            vsnprintf(p->err->message, sizeof p->err->message, fmt, ap);
            va_end(ap);
        } else {
            p->err->message[0] = '\0';
        }
    }
    return NULL;
}

void next(Parser *p) {
    p->look = lex_next(&p->lx);
}
int peek_ch(Parser *p, char ch) {
    return p->look.kind == TOK_SYM && p->look.text[0] == ch;
}
int is_cmd(const Token *t, const char *name) {
    return t->kind == TOK_IDENT && strcmp(t->text, name) == 0;
}
