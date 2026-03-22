// math_render.c - Main AST render dispatcher
// Verbatim from original render.c

#include "compositor/compositor.h"
#include "diagnostics/diagnostics.h"
#include "document/document.h"
#include "layout/layout.h"
#include "macro/system/aligned_macro.h"
#include "math_internal.h"

// ---- AST kind name for diagnostics ----
static const char *ast_kind_name(AstKind k) {
    switch (k) {
    case AST_SYMBOL: return "symbol";
    case AST_BINOP: return "binop";
    case AST_BINREL: return "binrel";
    case AST_FRACTION: return "frac";
    case AST_SUP: return "sup";
    case AST_SUB: return "sub";
    case AST_SUPSUB: return "supsub";
    case AST_GROUP: return "group";
    case AST_PAREN: return "paren";
    case AST_SQRT: return "sqrt";
    case AST_SUM: return "sum";
    case AST_PROD: return "prod";
    case AST_INT: return "int";
    case AST_IINT: return "iint";
    case AST_IIINT: return "iiint";
    case AST_OINT: return "oint";
    case AST_OIINT: return "oiint";
    case AST_BIGCUP: return "bigcup";
    case AST_BIGCAP: return "bigcap";
    case AST_COPROD: return "coprod";
    case AST_FORALL: return "forall";
    case AST_EXISTS: return "exists";
    case AST_LIMFUNC: return "limfunc";
    case AST_MATRIX: return "matrix";
    case AST_FUNCTION: return "function";
    case AST_TEXT: return "text";
    case AST_STYLE: return "style";
    case AST_ACCENT: return "accent";
    case AST_OVERBRACE: return "overbrace";
    case AST_UNDERBRACE: return "underbrace";
    case AST_EMBED: return "embed";
    case AST_OVERSET: return "overset";
    case AST_BOXED: return "boxed";
    case AST_PHANTOM: return "phantom";
    case AST_SMASH: return "smash";
    case AST_XARROW: return "xarrow";
    case AST_SUBSTACK: return "substack";
    case AST_TAG: return "tag";
    default: return "?";
    }
}

// Track nesting depth for diagnostics
static int g_math_diag_depth = 0;

// ---- Master render ----
Box render_internal(const Ast *node) {
    // Empty group {} or NULL node: return zero-width box
    // This is important for tensor notation like R^\rho{}_{\sigma\mu\nu}
    // where the empty group serves to separate superscript from subscript
    if (!node) return make_box(0, 1, 0);

    // Log entry for interesting nodes (skip simple groups/symbols to reduce noise)
    bool should_log =
        diag_is_enabled(DIAG_MATH) && node->kind != AST_GROUP && node->kind != AST_SYMBOL;

    if (should_log) {
        const char *detail = "";
        char detail_buf[64] = "";
        switch (node->kind) {
        case AST_BINOP:
            if (node->bin.op)
                snprintf(detail_buf, sizeof(detail_buf), " op='%c'", node->bin.op);
            else
                snprintf(detail_buf, sizeof(detail_buf), " (implicit mult)");
            detail = detail_buf;
            break;
        case AST_BINREL:
            snprintf(detail_buf, sizeof(detail_buf), " op='%s'", node->binrel.op);
            detail = detail_buf;
            break;
        case AST_LIMFUNC:
            snprintf(detail_buf, sizeof(detail_buf), " name='%s'", node->limfunc.name);
            detail = detail_buf;
            break;
        case AST_MATRIX:
            snprintf(detail_buf, sizeof(detail_buf), " %dx%d", node->matrix.rows,
                     node->matrix.cols);
            detail = detail_buf;
            break;
        default: break;
        }
        diag_log(DIAG_MATH, g_math_diag_depth, "render %s%s", ast_kind_name(node->kind), detail);
        g_math_diag_depth++;
    }

    Box result;

    switch (node->kind) {
    case AST_SYMBOL: result = symbol_box(node->sym.text); break;
    case AST_GROUP: result = render_internal(node->group.child); break;
    case AST_PAREN: result = render_paren(node); break;
    case AST_BINOP: result = render_bin(&node->bin); break;
    case AST_BINREL: result = render_bin_rel(&node->binrel); break;
    case AST_FRACTION: result = render_frac(&node->frac); break;
    case AST_SUP: {
        Box base = render_internal(node->sup.base);
        Box sup = render_internal(node->sup.sup);
        result = attach_sup(&base, &sup);
        box_free(&base);
        box_free(&sup);
        break;
    }
    case AST_SUB: {
        Box base = render_internal(node->sup.base);
        Box sub = render_internal(node->sup.sup);
        result = attach_sub(&base, &sub);
        box_free(&base);
        box_free(&sub);
        break;
    }
    case AST_SUPSUB: {
        Box base = render_internal(node->supsub.base);
        Box sup = render_internal(node->supsub.sup);
        Box sub = render_internal(node->supsub.sub);
        result = attach_supsub(&base, &sup, &sub);
        box_free(&base);
        box_free(&sup);
        box_free(&sub);
        break;
    }
    case AST_SUM: result = render_lim_op(&node->lim, big_sigma, 1); break;
    case AST_PROD: result = render_lim_op(&node->lim, big_prod, 1); break;
    case AST_INT: result = render_lim_op(&node->lim, big_int, 1); break;
    case AST_IINT: result = render_lim_op(&node->lim, big_iint, 1); break;
    case AST_IIINT: result = render_lim_op(&node->lim, big_iiint, 1); break;
    case AST_OINT: result = render_lim_op(&node->lim, big_oint, 1); break;
    case AST_OIINT: result = render_lim_op(&node->lim, big_oiint, 1); break;
    case AST_BIGCUP: result = render_lim_op(&node->lim, big_cup, 1); break;
    case AST_BIGCAP: result = render_lim_op(&node->lim, big_cap, 1); break;
    case AST_COPROD: result = render_lim_op(&node->lim, big_coprod, 1); break;
    case AST_FORALL: result = render_lim_op(&node->lim, big_prod /*placeholder*/, 1); break;
    case AST_EXISTS: result = render_lim_op(&node->lim, big_cup /*placeholder*/, 1); break;
    case AST_LIMFUNC: result = render_limfunc(&node->limfunc); break;
    case AST_SQRT: result = render_sqrt(&node->sqrt); break;
    case AST_MATRIX: result = render_matrix(node); break;
    case AST_FUNCTION: result = render_internal(node->func.child); break;
    case AST_TEXT: {
        StyleKind st = current_style();
        const char *s = node->text.text;
        if (st != STYLE_NORMAL && get_unicode_mode()) {
            uint32_t buf[512];
            int n = 0;
            size_t len = strlen(s), p = 0;
            while (p < len && n < 512) {
                uint32_t cp = utf8_next(s, len, &p);
                if (st == STYLE_BOLD)
                    cp = to_bold(cp);
                else if (st == STYLE_BLACKBOARD)
                    cp = to_blackboard(cp);
                else if (st == STYLE_SCRIPT)
                    cp = to_script(cp);
                else if (st == STYLE_ITALIC)
                    cp = latin_to_math_italic(cp);
                else if (st == STYLE_FRAKTUR)
                    cp = to_fraktur(cp);
                else if (st == STYLE_SANS)
                    cp = to_sans(cp);
                // STYLE_ROMAN: leave as-is
                buf[n++] = cp;
            }
            result = text_box_from_utf32(buf, n);
        } else {
            result = text_box(s);
        }
        break;
    }
    case AST_STYLE: {
        push_style(node->style.kind);
        result = render_internal(node->style.child);
        pop_style();
        break;
    }
    case AST_ACCENT: {
        Box content = render_internal(node->accent.child);
        result = render_accent(node->accent.akind, &content);
        box_free(&content);
        break;
    }
    case AST_OVERBRACE:
    case AST_UNDERBRACE: {
        Box content = render_internal(node->brace.content);
        Box label = node->brace.label ? render_internal(node->brace.label) : make_box(0, 0, 0);
        result = render_brace(node->kind == AST_OVERBRACE, &content, &label);
        box_free(&content);
        box_free(&label);
        break;
    }
    case AST_EMBED: result = render_embed(&node->embed); break;
    case AST_OVERSET: result = render_overset(&node->overset); break;
    case AST_BOXED: result = render_boxed(node->boxed.child); break;
    case AST_PHANTOM: {
        Box content = render_internal(node->phantom.child);
        result = make_box(content.w, content.h, content.baseline);
        box_free(&content);
        break;
    }
    case AST_SMASH: {
        Box content = render_internal(node->smash.child);
        // Render content but collapse to baseline row only
        result = make_box(content.w, 1, 0);
        if (content.baseline >= 0 && content.baseline < content.h) {
            for (int c = 0; c < content.w; c++) {
                result.cells[c] = content.cells[content.baseline * content.w + c];
            }
        }
        box_free(&content);
        break;
    }
    case AST_XARROW: result = render_xarrow(&node->xarrow); break;
    case AST_SUBSTACK: result = render_substack(&node->substack); break;
    case AST_TAG: result = render_tag(node->tag.child); break;
    default: result = text_box("<?>"); break;
    }

    // Log result dimensions for interesting nodes
    if (should_log) {
        g_math_diag_depth--;
        diag_result(DIAG_MATH, g_math_diag_depth, "=> %dx%d baseline=%d", result.w, result.h,
                    result.baseline);
    }

    return result;
}

Box render_ast(const Ast *node) {
    g_math_diag_depth = 0; // Reset depth at entry
    return render_internal(node);
}

// -------------- FACTORIES & FREE --------------
Ast *node_new(AstKind k) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = k;
    return n;
}
Ast *ast_symbol(const char *txt) {
    Ast *n = node_new(AST_SYMBOL);
    snprintf(n->sym.text, sizeof(n->sym.text), "%s", txt);
    return n;
}
Ast *ast_binop(char op, Ast *lhs, Ast *rhs) {
    Ast *n = node_new(AST_BINOP);
    n->bin.op = op;
    n->bin.lhs = lhs;
    n->bin.rhs = rhs;
    return n;
}
Ast *ast_binop_str(const char *op, Ast *lhs, Ast *rhs) {
    Ast *n = node_new(AST_BINREL);
    strncpy(n->binrel.op, op, 15); // Copy up to 15 chars (op is char[16])
    n->binrel.op[15] = '\0';       // Ensure null termination
    n->binrel.lhs = lhs;
    n->binrel.rhs = rhs;
    return n;
}
Ast *ast_fraction(Ast *numer, Ast *denom) {
    Ast *n = node_new(AST_FRACTION);
    n->frac.numer = numer;
    n->frac.denom = denom;
    return n;
}
Ast *ast_sup(Ast *base, Ast *sup) {
    Ast *n = node_new(AST_SUP);
    n->sup.base = base;
    n->sup.sup = sup;
    return n;
}
Ast *ast_sub(Ast *base, Ast *sub) {
    Ast *n = node_new(AST_SUB);
    n->sup.base = base;
    n->sup.sup = sub;
    return n;
}
Ast *ast_supsub(Ast *base, Ast *sup, Ast *sub) {
    Ast *n = node_new(AST_SUPSUB);
    n->supsub.base = base;
    n->supsub.sup = sup;
    n->supsub.sub = sub;
    return n;
}
Ast *ast_group(Ast *child) {
    Ast *n = node_new(AST_GROUP);
    n->group.child = child;
    return n;
}
Ast *ast_paren(Ast *child, ParenType t, int size) {
    Ast *n = node_new(AST_PAREN);
    n->paren.pchild = child;
    n->paren.ptype = t;
    n->paren.right_ptype = t;
    n->paren.size = size;
    return n;
}
Ast *ast_paren_asym(Ast *child, ParenType left, ParenType right, int size) {
    Ast *n = node_new(AST_PAREN);
    n->paren.pchild = child;
    n->paren.ptype = left;
    n->paren.right_ptype = right;
    n->paren.size = size;
    return n;
}
Ast *ast_sqrt(Ast *index, Ast *rad) {
    Ast *n = node_new(AST_SQRT);
    n->sqrt.index = index;
    n->sqrt.rad = rad;
    return n;
}

Ast *ast_sum(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_SUM);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_prod(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_PROD);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_int(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_INT);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_iint(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_IINT);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_iiint(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_IIINT);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_oint(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_OINT);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_oiint(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_OIINT);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_bigcup(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_BIGCUP);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_bigcap(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_BIGCAP);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_coprod(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_COPROD);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_forall(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_FORALL);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}
Ast *ast_exists(Ast *l, Ast *u, Ast *b, int s) {
    Ast *n = node_new(AST_EXISTS);
    n->lim.lower = l;
    n->lim.upper = u;
    n->lim.body = b;
    n->lim.size = s;
    return n;
}

Ast *ast_function(Ast *child) {
    Ast *n = node_new(AST_FUNCTION);
    n->func.child = child;
    return n;
}

Ast *ast_text(const char *str) {
    Ast *n = node_new(AST_TEXT);
    snprintf(n->text.text, sizeof(n->text.text), "%s", str);
    return n;
}

Ast *ast_style(StyleKind k, Ast *child) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = AST_STYLE;
    n->style.kind = k;
    n->style.child = child;
    return n;
}

Ast *ast_accent(AccentKind k, Ast *child) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = AST_ACCENT;
    n->accent.akind = k;
    n->accent.child = child;
    return n;
}

Ast *ast_overbrace(Ast *content, Ast *label) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = AST_OVERBRACE;
    n->brace.content = content;
    n->brace.label = label;
    return n;
}
Ast *ast_underbrace(Ast *content, Ast *label) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = AST_UNDERBRACE;
    n->brace.content = content;
    n->brace.label = label;
    return n;
}

Ast *ast_embed(EmbedKind k, const char *content) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = AST_EMBED;
    n->embed.ekind = k;
    n->embed.content = strdup(content);
    return n;
}

Ast *ast_overset(Ast *base, Ast *annotation, bool is_over) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = AST_OVERSET;
    n->overset.base = base;
    n->overset.annotation = annotation;
    n->overset.is_over = is_over;
    return n;
}

Ast *ast_boxed(Ast *child) {
    Ast *n = node_new(AST_BOXED);
    n->boxed.child = child;
    return n;
}

Ast *ast_phantom(Ast *child) {
    Ast *n = node_new(AST_PHANTOM);
    n->phantom.child = child;
    return n;
}

Ast *ast_smash(Ast *child) {
    Ast *n = node_new(AST_SMASH);
    n->smash.child = child;
    return n;
}

Ast *ast_xarrow(Ast *label, bool is_right) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = AST_XARROW;
    n->xarrow.label = label;
    n->xarrow.is_right = is_right;
    return n;
}

Ast *ast_substack(int num_rows, Ast **rows) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = AST_SUBSTACK;
    n->substack.num_rows = num_rows;
    n->substack.rows = rows;
    return n;
}

Ast *ast_tag(Ast *child) {
    Ast *n = node_new(AST_TAG);
    n->tag.child = child;
    return n;
}

Ast *ast_limfunc(const char *name, Ast *lower, Ast *upper, Ast *body) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = AST_LIMFUNC;
    snprintf(n->limfunc.name, sizeof(n->limfunc.name), "%s", name);
    n->limfunc.lower = lower;
    n->limfunc.upper = upper;
    n->limfunc.body = body;
    return n;
}

Ast *ast_matrix(int rows, int cols, Ast **cells, int cell_padding) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    n->kind = AST_MATRIX;
    n->matrix.rows = rows;
    n->matrix.cols = cols;
    n->matrix.cells = cells; // ownership transferred
    n->matrix.cell_padding = cell_padding;
    return n;
}

void ast_free(Ast *node) {
    if (!node) return;
    switch (node->kind) {
    case AST_SYMBOL: break;
    case AST_GROUP: ast_free(node->group.child); break;
    case AST_PAREN: ast_free(node->paren.pchild); break;
    case AST_BINOP:
        ast_free(node->bin.lhs);
        ast_free(node->bin.rhs);
        break;
    case AST_BINREL:
        ast_free(node->binrel.lhs);
        ast_free(node->binrel.rhs);
        break;
    case AST_FRACTION:
        ast_free(node->frac.numer);
        ast_free(node->frac.denom);
        break;
    case AST_SUP:
    case AST_SUB:
        ast_free(node->sup.base);
        ast_free(node->sup.sup);
        break;
    case AST_SUPSUB:
        ast_free(node->supsub.base);
        ast_free(node->supsub.sup);
        ast_free(node->supsub.sub);
        break;
    case AST_SUM:
    case AST_PROD:
    case AST_INT:
    case AST_IINT:
    case AST_IIINT:
    case AST_OINT:
    case AST_OIINT:
    case AST_BIGCUP:
    case AST_BIGCAP:
    case AST_COPROD:
    case AST_FORALL:
    case AST_EXISTS:
        ast_free(node->lim.lower);
        ast_free(node->lim.upper);
        ast_free(node->lim.body);
        break;
    case AST_LIMFUNC:
        ast_free(node->limfunc.lower);
        ast_free(node->limfunc.upper);
        ast_free(node->limfunc.body);
        break;
    case AST_SQRT:
        ast_free(node->sqrt.index);
        ast_free(node->sqrt.rad);
        break;
    case AST_MATRIX: {
        int n = node->matrix.rows * node->matrix.cols;
        for (int i = 0; i < n; i++) ast_free(node->matrix.cells[i]);
        free(node->matrix.cells);
        break;
    }
    case AST_FUNCTION: ast_free(node->func.child); break;
    case AST_TEXT:
        // No children to free - just the text string
        break;
    case AST_STYLE: ast_free(node->style.child); break;
    case AST_ACCENT: ast_free(node->accent.child); break;
    case AST_OVERBRACE:
    case AST_UNDERBRACE:
        ast_free(node->brace.content);
        ast_free(node->brace.label);
        break;
    case AST_EMBED: free(node->embed.content); break;
    case AST_OVERSET:
        ast_free(node->overset.base);
        ast_free(node->overset.annotation);
        break;
    case AST_BOXED: ast_free(node->boxed.child); break;
    case AST_PHANTOM: ast_free(node->phantom.child); break;
    case AST_SMASH: ast_free(node->smash.child); break;
    case AST_XARROW: ast_free(node->xarrow.label); break;
    case AST_SUBSTACK:
        for (int i = 0; i < node->substack.num_rows; i++) ast_free(node->substack.rows[i]);
        free(node->substack.rows);
        break;
    case AST_TAG: ast_free(node->tag.child); break;
    }
    free(node);
}

// ---- Embed Rendering (delegates to document/layout system) ----
Box render_embed(const Embed *e) {
    char error_msg[256] = "";
    int end_pos = 0;
    char *expanded = NULL;

    // Build the full macro call string
    char *macro_input = NULL;
    if (e->ekind == EMBED_CASES) {
        // Build "\cases{content}"
        size_t len = 7 + strlen(e->content) + 2; // \cases{ + content + }
        macro_input = malloc(len);
        snprintf(macro_input, len, "\\cases{%s}", e->content);
        expanded = cases_macro_expand(macro_input, &end_pos, error_msg, sizeof(error_msg));
    } else if (e->ekind == EMBED_ALIGNED) {
        // Build "\aligned{content}"
        size_t len = 9 + strlen(e->content) + 2; // \aligned{ + content + }
        macro_input = malloc(len);
        snprintf(macro_input, len, "\\aligned{%s}", e->content);
        expanded = aligned_macro_expand(macro_input, &end_pos, error_msg, sizeof(error_msg));
    }
    free(macro_input);

    if (!expanded) {
        // Expansion failed - return error box
        char err_text[300];
        snprintf(err_text, sizeof(err_text), "[embed error: %s]", error_msg);
        return text_box(err_text);
    }

    // Parse the expanded code into a BoxLayout
    // Use -3 (intrinsic) to get natural content width, not a fixed width
    ParseError parse_err = {0};
    BoxLayout *layout = parse_document_as_vbox(expanded, -3, &parse_err);
    free(expanded);

    if (!layout) {
        char err_text[300];
        snprintf(err_text, sizeof(err_text), "[parse error: %s]", parse_err.message);
        return text_box(err_text);
    }

    // Render the BoxLayout to a Box
    CompOptions opt = default_options();
    Box *rendered = box_layout_render(layout, &opt, &parse_err);
    box_layout_free(layout);

    if (!rendered) {
        char err_text[300];
        snprintf(err_text, sizeof(err_text), "[render error: %s]", parse_err.message);
        return text_box(err_text);
    }

    // Scan for intertext markers (U+E000 PUA) BEFORE width calculation
    // so intertext rows don't inflate the equation content width
    bool has_intertext = false;
    bool *intertext_rows = calloc(rendered->h, sizeof(bool));
    for (int y = 0; y < rendered->h; y++) {
        for (int x = 0; x < rendered->w; x++) {
            uint32_t cell = rendered->cells[y * rendered->w + x];
            if (cell == 0xE000) {
                has_intertext = true;
                intertext_rows[y] = true;
                // Remove marker by shifting row content left
                for (int sx = x; sx < rendered->w - 1; sx++) {
                    rendered->cells[y * rendered->w + sx] =
                        rendered->cells[y * rendered->w + sx + 1];
                }
                rendered->cells[y * rendered->w + rendered->w - 1] = ' ';
                break;
            }
            if (cell != ' ' && cell != 0) break; // non-marker content
        }
    }

    // Find actual content widths: equation rows and all rows (including intertext)
    // Equation width determines centering; full width determines Box size
    int eq_content_width = 0;
    int full_content_width = 0;
    for (int y = 0; y < rendered->h; y++) {
        for (int x = rendered->w - 1; x >= 0; x--) {
            uint32_t cell = rendered->cells[y * rendered->w + x];
            if (cell != 0 && cell != ' ') {
                if (x + 1 > full_content_width) full_content_width = x + 1;
                if (!intertext_rows[y] && x + 1 > eq_content_width) eq_content_width = x + 1;
                break;
            }
        }
    }
    int content_width = full_content_width > 0 ? full_content_width : rendered->w;

    // Set baseline: when intertext rows are present, anchor to last equation row
    // so that \tag appears next to the last equation, not an intertext row.
    int embed_baseline;
    if (has_intertext) {
        int last_eq_row = rendered->h - 1;
        while (last_eq_row > 0 && intertext_rows[last_eq_row]) last_eq_row--;
        embed_baseline = last_eq_row;
    } else {
        embed_baseline = rendered->h / 2;
    }
    Box result = make_box(content_width, rendered->h, embed_baseline);

    for (int y = 0; y < result.h; y++) {
        for (int x = 0; x < result.w; x++) {
            result.cells[y * result.w + x] = rendered->cells[y * rendered->w + x];
        }
    }
    if (rendered->meta) {
        box_ensure_meta(&result);
        if (result.meta) {
            for (int y = 0; y < result.h; y++) {
                for (int x = 0; x < result.w; x++) {
                    result.meta[y * result.w + x] = rendered->meta[y * rendered->w + x];
                }
            }
        }
    }

    // Set row_flags for intertext rows
    if (has_intertext) {
        result.row_flags = calloc(result.h, 1);
        for (int y = 0; y < result.h; y++) {
            if (intertext_rows[y]) {
                result.row_flags[y] = BOX_ROW_FLAG_FULL_LEFT;
            }
        }
    }
    free(intertext_rows);

    box_free(rendered);
    free(rendered);

    return result;
}