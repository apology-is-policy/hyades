// math_constructs.c - Complex math constructs
// Verbatim from original render.c

#include "math_internal.h"

Box render_internal(const Ast *node);

// Check if RHS starts with visible parentheses (looking through groups and binops)
int rhs_starts_with_paren(const Ast *node) {
    if (!node) return 0;
    if (node->kind == AST_PAREN) return 1;
    // Look through groups to see if contents start with paren
    if (node->kind == AST_GROUP) {
        return rhs_starts_with_paren(node->group.child);
    }
    // Look through binop LHS to see if expression starts with paren
    // (e.g., \gcd body might be "(a,b) = ..." which starts with paren)
    if (node->kind == AST_BINOP) {
        return rhs_starts_with_paren(node->bin.lhs);
    }
    return 0;
}

// Binary op
Box render_bin(const BinOp *b) {
    Box L = render_internal(b->lhs);
    Box R = render_internal(b->rhs);

    // Implicit multiplication: no glyph, determine gap based on context
    if (b->op == '\0') {
        int gap = 0;

        // Check if RHS starts with visible parentheses
        // Note: AST_GROUP alone does NOT suppress space (braces are just grouping)
        // Only actual AST_PAREN suppresses space
        int rhs_is_paren = rhs_starts_with_paren(b->rhs);

        // Upright function operators (sin, cos, log, etc.) get a space
        // ONLY when followed by a non-parenthesized argument
        // e.g., "\sin x" → "sin x", "\sin{x}" → "sin x", but "\sin(x)" → "sin(x)"
        // Use rightmost_element to handle chained functions: "\sin \cos x" → "sin cos x"
        if (is_upright_function_node(rightmost_element(b->lhs)) && !rhs_is_paren) {
            gap = 1;
        }

        // Check if spacing is needed based on what's adjacent
        // - LHS ending with paren/fraction/etc. needs space after
        // - RHS starting with function/fraction/etc. needs space before
        const Ast *lhs_right = rightmost_element(b->lhs);
        const Ast *rhs_left = leftmost_element(b->rhs);
        if (needs_space_in_implicit_mult(lhs_right, 1) || // check LHS as LHS
            needs_space_in_implicit_mult(rhs_left, 0)) {  // check RHS as RHS
            gap = 1;
        }

        Box OUT = hcat(&L, &R, gap);
        box_free(&L);
        box_free(&R);
        return OUT;
    }

    const char *opstr = map_op_char(get_unicode_mode(), b->op);
    Box OP = symbol_box(opstr);

    // Punctuation (comma, semicolon): no space before, space after
    // This matches TeX behavior: "a, b, c" not "a , b , c"
    if (b->op == ',' || b->op == ';' || b->op == ':') {
        Box LR = hcat(&L, &OP, 0); // no space before comma
        int right_gap = (R.w > 0) ? 1 : 0;
        Box LRO = hcat(&LR, &R, right_gap); // space after comma
        box_free(&L);
        box_free(&R);
        box_free(&OP);
        box_free(&LR);
        return LRO;
    }

    // Regular binary operators: space on both sides
    int left_gap = (L.w > 0) ? 1 : 0;
    int right_gap = (R.w > 0) ? 1 : 0;
    Box LR = hcat(&L, &OP, left_gap);
    Box LRO = hcat(&LR, &R, right_gap);

    box_free(&L);
    box_free(&R);
    box_free(&OP);
    box_free(&LR);
    return LRO;
}

// Binary op
Box render_bin_rel(const BinRel *b) {
    Box L = render_internal(b->lhs);
    Box R = render_internal(b->rhs);

    const char *opstr = map_rel_str(get_unicode_mode(), b->op);
    Box OP = symbol_box(opstr);

    int left_gap = (L.w > 0) ? 1 : 0;
    int right_gap = (R.w > 0) ? 1 : 0;
    Box LR = hcat(&L, &OP, left_gap);
    Box LRO = hcat(&LR, &R, right_gap);

    box_free(&L);
    box_free(&R);
    box_free(&OP);
    box_free(&LR);
    return LRO;
}

// Fraction
Box render_frac(const Fraction *f) {
    Box N = render_internal(f->numer);
    Box D = render_internal(f->denom);

    // padding around numerator/denominator
    Box Npad = make_box(N.w + 2, N.h, N.baseline);
    if (Npad.cells && N.cells) {
        for (int y = 0; y < N.h; y++)
            for (int x = 0; x < N.w; x++) put(&Npad, x + 1, y, N.cells[y * N.w + x]);
    }
    Box Dpad = make_box(D.w + 2, D.h, D.baseline);
    if (Dpad.cells && D.cells) {
        for (int y = 0; y < D.h; y++)
            for (int x = 0; x < D.w; x++) put(&Dpad, x + 1, y, D.cells[y * D.w + x]);
    }

    // Stack with a rule
    Box stacked = vstack_centered(&Npad, &Dpad, true);
    stacked.baseline = Npad.h;

    box_free(&N);
    box_free(&D);
    box_free(&Npad);
    box_free(&Dpad);
    return stacked;
}

// Count trailing space columns in a box (for gap adjustment)
int count_trailing_spaces(const Box *b) {
    if (!b || !b->cells) return 0;
    int trailing = 0;
    for (int x = b->w - 1; x >= 0; x--) {
        int all_spaces = 1;
        for (int y = 0; y < b->h; y++) {
            if (b->cells[y * b->w + x] != U' ') {
                all_spaces = 0;
                break;
            }
        }
        if (all_spaces) {
            trailing++;
        } else {
            break;
        }
    }
    return trailing;
}

Box render_lim_op(const LimOp *op, Box (*glyph)(int), int shift_per_size) {
    Box G = glyph(op->size);
    Box left = G;
    if (op->upper) {
        Box U = render_internal(op->upper);
        Box top = vstack_centered(&U, &left, false);
        top.baseline = left.baseline + U.h;
        box_free(&left);
        box_free(&U);
        left = top;
    }
    if (op->lower) {
        Box L = render_internal(op->lower);
        Box bot = vstack_centered(&left, &L, false);
        bot.baseline = left.baseline;
        box_free(&left);
        box_free(&L);
        left = bot;
    }
    Box body = render_internal(op->body);

    // Calculate gap to achieve consistent 2-space distance from symbol to body
    // Account for trailing spaces in the left box
    int trailing = count_trailing_spaces(&left);
    int desired_gap = 2;
    int actual_gap = desired_gap - trailing;
    if (actual_gap < 0) actual_gap = 0;

    Box out = hcat_on_left_axis(&left, &body, actual_gap);
    box_free(&left);
    box_free(&body);
    return out;
}

// Render limit-style function operators (lim, max, min, sup, inf, etc.)
// Like render_lim_op but uses upright text instead of a big symbol
Box render_limfunc(const LimFunc *op) {
    // Create the operator name box (upright text)
    Box name = text_box(op->name);

    Box left = name;

    // Stack upper limit above (if present)
    if (op->upper) {
        Box U = render_internal(op->upper);
        Box top = vstack_centered(&U, &left, false);
        top.baseline = left.baseline + U.h;
        box_free(&left);
        box_free(&U);
        left = top;
    }

    // Stack lower limit below (if present)
    if (op->lower) {
        Box L = render_internal(op->lower);
        Box bot = vstack_centered(&left, &L, false);
        bot.baseline = left.baseline;
        box_free(&left);
        box_free(&L);
        left = bot;
    }

    // Render body and concatenate (if body is present)
    if (op->body) {
        Box body = render_internal(op->body);
        int gap = rhs_starts_with_paren(op->body) ? 0 : 1;
        Box out = hcat_on_left_axis(&left, &body, gap);
        box_free(&left);
        box_free(&body);
        return out;
    }
    return left;
}

// -------- Scalable parentheses rendering (ASCII vs Unicode) --------
// Procedural builder: constructs a delimiter of any odd size >= 3
// using top/ext/mid/bot piece SymbolIDs.
static Box build_tall_delim(ParenType pt, int sz, bool is_right, int uni) {
    // Special types with dedicated helpers
    switch (pt) {
    case PAREN_VBAR: return tall_vbar(sz);
    case PAREN_DVBAR: return tall_dvbar(sz);
    case PAREN_FLOOR: return is_right ? tall_floor_right(sz) : tall_floor_left(sz);
    case PAREN_CEIL: return is_right ? tall_ceil_right(sz) : tall_ceil_left(sz);
    case PAREN_ANGLE: return is_right ? tall_angle_right(sz) : tall_angle_left(sz);
    case PAREN_NONE: return make_box(0, sz, sz / 2);
    default: break;
    }

    // Select piece SymbolIDs based on type, side, and mode
    SymbolID top_id, ext_id, bot_id, mid_id = SYM_INVALID;
    bool has_mid = (pt == PAREN_CURLY);

    if (pt == PAREN_ROUND) {
        if (uni) {
            top_id = is_right ? SYM_PAREN_UNI_R_TOP : SYM_PAREN_UNI_L_TOP;
            ext_id = is_right ? SYM_PAREN_UNI_R_EXT : SYM_PAREN_UNI_L_EXT;
            bot_id = is_right ? SYM_PAREN_UNI_R_BOT : SYM_PAREN_UNI_L_BOT;
        } else {
            top_id = is_right ? SYM_PAREN_ASCII_R_TOP : SYM_PAREN_ASCII_L_TOP;
            ext_id = is_right ? SYM_PAREN_ASCII_R_EXT : SYM_PAREN_ASCII_L_EXT;
            bot_id = is_right ? SYM_PAREN_ASCII_R_BOT : SYM_PAREN_ASCII_L_BOT;
        }
    } else if (pt == PAREN_SQUARE) {
        if (uni) {
            top_id = is_right ? SYM_BRACKET_UNI_R_TOP : SYM_BRACKET_UNI_L_TOP;
            ext_id = is_right ? SYM_BRACKET_UNI_R_EXT : SYM_BRACKET_UNI_L_EXT;
            bot_id = is_right ? SYM_BRACKET_UNI_R_BOT : SYM_BRACKET_UNI_L_BOT;
        } else {
            top_id = is_right ? SYM_BRACKET_ASCII_R_TOP : SYM_BRACKET_ASCII_L_TOP;
            ext_id = is_right ? SYM_BRACKET_ASCII_R_EXT : SYM_BRACKET_ASCII_L_EXT;
            bot_id = is_right ? SYM_BRACKET_ASCII_R_BOT : SYM_BRACKET_ASCII_L_BOT;
        }
    } else { // PAREN_CURLY
        if (uni) {
            top_id = is_right ? SYM_BRACE_UNI_R_TOP : SYM_BRACE_UNI_L_TOP;
            ext_id = is_right ? SYM_BRACE_UNI_R_EXT : SYM_BRACE_UNI_L_EXT;
            mid_id = is_right ? SYM_BRACE_UNI_R_MID : SYM_BRACE_UNI_L_MID;
            bot_id = is_right ? SYM_BRACE_UNI_R_BOT : SYM_BRACE_UNI_L_BOT;
        } else {
            top_id = is_right ? SYM_BRACE_ASCII_R_TOP : SYM_BRACE_ASCII_L_TOP;
            ext_id = is_right ? SYM_BRACE_ASCII_R_EXT : SYM_BRACE_ASCII_L_EXT;
            mid_id = is_right ? SYM_BRACE_ASCII_R_MID : SYM_BRACE_ASCII_L_MID;
            bot_id = is_right ? SYM_BRACE_ASCII_R_BOT : SYM_BRACE_ASCII_L_BOT;
        }
    }

    const char *top_str = get_symbol(top_id);
    const char *ext_str = get_symbol(ext_id);
    const char *bot_str = get_symbol(bot_id);
    const char *mid_str = has_mid ? get_symbol(mid_id) : NULL;

    // Calculate width from widest piece
    int w = str_cols(top_str);
    int ew = str_cols(ext_str);
    int bw = str_cols(bot_str);
    if (ew > w) w = ew;
    if (bw > w) w = bw;
    if (mid_str) {
        int mw = str_cols(mid_str);
        if (mw > w) w = mw;
    }

    Box b = make_box(w, sz, sz / 2);

    if (!has_mid) {
        // ROUND / SQUARE: top + ext×(sz-2) + bot
        put_str(&b, 0, 0, top_str);
        for (int y = 1; y < sz - 1; y++) put_str(&b, 0, y, ext_str);
        put_str(&b, 0, sz - 1, bot_str);
    } else {
        // CURLY: top + ext/knuckle + mid + ext/knuckle + bot
        int mid_row = sz / 2;
        put_str(&b, 0, 0, top_str);
        put_str(&b, 0, mid_row, mid_str);
        put_str(&b, 0, sz - 1, bot_str);

        if (!uni) {
            // ASCII knuckle approach:
            // Row before mid = bot_str (knuckle down: \ for left, / for right)
            // Row after mid = top_str (knuckle up: / for left, \ for right)
            if (mid_row > 1) put_str(&b, 0, mid_row - 1, bot_str);
            if (mid_row + 1 < sz - 1) put_str(&b, 0, mid_row + 1, top_str);
            // Fill remaining with ext
            for (int y = 1; y < mid_row - 1; y++) put_str(&b, 0, y, ext_str);
            for (int y = mid_row + 2; y < sz - 1; y++) put_str(&b, 0, y, ext_str);
        } else {
            // Unicode: uniform ⎪ extension above and below middle
            for (int y = 1; y < mid_row; y++) put_str(&b, 0, y, ext_str);
            for (int y = mid_row + 1; y < sz - 1; y++) put_str(&b, 0, y, ext_str);
        }
    }

    return b;
}

Box render_paren(const Ast *n) {
    Box inner = render_internal(n->paren.pchild);
    const int sz = n->paren.size;
    ParenType left_type = n->paren.ptype;
    ParenType right_type = n->paren.right_ptype;

    // Handle PAREN_NONE (invisible delimiter from \left. or \right.)
    if (left_type == PAREN_NONE && right_type == PAREN_NONE) {
        return inner; // \left. ... \right. — no delimiters
    }

    // Determine which symbols to use based on size, type, and mode
    int uni = get_unicode_mode();

    if (left_type == PAREN_NONE || right_type == PAREN_NONE) {
        // Asymmetric delimiter: one side invisible
        ParenType vis = (left_type != PAREN_NONE) ? left_type : right_type;
        bool vis_is_left = (left_type != PAREN_NONE);

        if (sz == 1) {
            // Size 1: build just the visible delimiter
            const char *ds = NULL;
            switch (vis) {
            case PAREN_ROUND: ds = vis_is_left ? "(" : ")"; break;
            case PAREN_SQUARE: ds = vis_is_left ? "[" : "]"; break;
            case PAREN_CURLY: ds = vis_is_left ? "{" : "}"; break;
            case PAREN_VBAR: ds = "|"; break;
            case PAREN_DVBAR: ds = uni ? "║" : "||"; break;
            case PAREN_FLOOR: ds = get_symbol(vis_is_left ? SYM_LFLOOR : SYM_RFLOOR); break;
            case PAREN_CEIL: ds = get_symbol(vis_is_left ? SYM_LCEIL : SYM_RCEIL); break;
            case PAREN_ANGLE: ds = get_symbol(vis_is_left ? SYM_LANGLE : SYM_RANGLE); break;
            default: ds = vis_is_left ? "(" : ")"; break;
            }
            Box d = text_box(ds);
            Box out;
            if (vis_is_left) {
                out = hcat(&d, &inner, 0);
            } else {
                out = hcat(&inner, &d, 0);
            }
            out.baseline = inner.baseline;
            box_free(&d);
            box_free(&inner);
            return out;
        }

        // Tall asymmetric delimiter
        Box d = build_tall_delim(vis, sz, !vis_is_left, uni);
        int pad = (sz >= 3) ? 1 : 0;
        Box out;
        if (vis_is_left) {
            out = hcat(&d, &inner, pad);
        } else {
            out = hcat(&inner, &d, pad);
        }
        out.baseline = (sz > 1) ? sz / 2 : inner.baseline;
        // Scripts attach flush with edges of tall delimiters (like LaTeX eval bars)
        if (sz > 1) {
            out.sub_shift = out.h - out.baseline - 1;
            out.sup_shift = 0;
        }
        box_free(&d);
        box_free(&inner);
        return out;
    }

    // ---- size == 1: inline glyphs (1-row delimiters) ----
    if (sz == 1) {
        const char *L = NULL, *R = NULL;

        switch (n->paren.ptype) {
        case PAREN_ROUND:
            L = "(";
            R = ")";
            break;
        case PAREN_SQUARE:
            L = "[";
            R = "]";
            break;
        case PAREN_CURLY:
            L = "{";
            R = "}";
            break;

        case PAREN_VBAR: {
            // Single-line: use plain | (visually distinct, won't form junctions)
            L = "|";
            R = "|";
            Box l = text_box(L);
            Box r = text_box(R);
            Box temp = hcat(&l, &inner, 0);
            Box out = hcat(&temp, &r, 0);
            out.baseline = temp.baseline;
            box_free(&inner);
            box_free(&l);
            box_free(&r);
            box_free(&temp);
            return out;
        }

        case PAREN_DVBAR: {
            // Create boxes with NO_CONNECT metadata so they don't form table junctions
            L = uni ? "║" : "||";
            R = uni ? "║" : "||";
            Box l = text_box(L);
            Box r = text_box(R);
            // Mark all cells as NO_CONNECT
            for (int y = 0; y < l.h; y++) {
                for (int x = 0; x < l.w; x++) {
                    box_set_meta(&l, x, y, CELL_META_NO_CONNECT);
                    box_set_meta(&r, x, y, CELL_META_NO_CONNECT);
                }
            }
            Box temp = hcat(&l, &inner, 0);
            Box out = hcat(&temp, &r, 0);
            out.baseline = temp.baseline;
            box_free(&inner);
            box_free(&l);
            box_free(&r);
            box_free(&temp);
            return out;
        }

        case PAREN_FLOOR:
            // Unicode corners vs ASCII 2-col corners
            L = get_symbol(SYM_LFLOOR);
            R = get_symbol(SYM_RFLOOR);
            break;

        case PAREN_CEIL:
            if (!uni) {
                // custom 2-row ASCII inline ceil: top underscores + vertical bars below
                Box l = make_box(1, 2, /*baseline*/ 1);
                put(&l, 0, 0, U'_'); // top underscore
                put(&l, 0, 1, U'|'); // vertical on baseline row

                Box r = make_box(1, 2, /*baseline*/ 1);
                put(&r, 0, 0, U'_'); // top underscore
                put(&r, 0, 1, U'|'); // vertical on baseline row

                // Raise inner by 1 row automatically via baseline alignment
                Box temp = hcat(&l, &inner, 0);
                Box out = hcat(&temp, &r, 0);
                out.baseline = temp.baseline;

                box_free(&inner);
                box_free(&l);
                box_free(&r);
                box_free(&temp);
                return out;
            } else {
                L = get_symbol(SYM_LCEIL);
                R = get_symbol(SYM_RCEIL);
            }
            break;

        case PAREN_ANGLE:
            L = get_symbol(SYM_LANGLE);
            R = get_symbol(SYM_RANGLE);
            break;

        default:
            // fallback
            L = "(";
            R = ")";
            break;
        }

        Box l = text_box(L);
        Box r = text_box(R);
        Box temp = hcat(&l, &inner, 0); // No padding for size=1 (keep tight)
        Box out = hcat(&temp, &r, 0);
        out.baseline = temp.baseline;
        box_free(&inner);
        box_free(&l);
        box_free(&r);
        box_free(&temp);
        return out;
    }

    // ---- size > 1: tall delimiters (all types via build_tall_delim) ----
    Box Lb = build_tall_delim(left_type, sz, false, uni);
    Box Rb = build_tall_delim(right_type, sz, true, uni);
    int pad = (sz >= 3) ? 1 : 0;
    Box temp = hcat(&Lb, &inner, pad);
    Box out = hcat(&temp, &Rb, pad);
    out.baseline = temp.baseline;
    out.sub_shift = out.h - out.baseline - 1;
    out.sup_shift = 0;
    box_free(&inner);
    box_free(&Lb);
    box_free(&Rb);
    box_free(&temp);
    return out;
}

// ---------- Overset / Underset ----------
Box render_overset(const Overset *o) {
    Box base = render_internal(o->base);
    Box ann = render_internal(o->annotation);

    if (o->is_over) {
        // Annotation above base, centered
        Box result = vstack_centered(&ann, &base, false);
        result.baseline = ann.h + base.baseline;
        box_free(&base);
        box_free(&ann);
        return result;
    } else {
        // Annotation below base, centered
        Box result = vstack_centered(&base, &ann, false);
        result.baseline = base.baseline;
        box_free(&base);
        box_free(&ann);
        return result;
    }
}

// ---------- Boxed ----------
Box render_boxed(const Ast *child) {
    Box content = render_internal(child);
    int uni = get_unicode_mode();

    int w = content.w + 2; // 1-char border each side
    int h = content.h + 2; // 1-row border top+bottom
    Box result = make_box(w, h, content.baseline + 1);

    // Draw frame
    uint32_t tl = uni ? U'┌' : U'+';
    uint32_t tr = uni ? U'┐' : U'+';
    uint32_t bl = uni ? U'└' : U'+';
    uint32_t br = uni ? U'┘' : U'+';
    uint32_t hz = uni ? U'─' : U'-';
    uint32_t vt = uni ? U'│' : U'|';

    put(&result, 0, 0, tl);
    put(&result, w - 1, 0, tr);
    put(&result, 0, h - 1, bl);
    put(&result, w - 1, h - 1, br);

    for (int x = 1; x < w - 1; x++) {
        put(&result, x, 0, hz);
        put(&result, x, h - 1, hz);
    }
    for (int y = 1; y < h - 1; y++) {
        put(&result, 0, y, vt);
        put(&result, w - 1, y, vt);
    }

    // Place content inside
    for (int y = 0; y < content.h; y++)
        for (int x = 0; x < content.w; x++)
            put(&result, x + 1, y + 1, content.cells[y * content.w + x]);

    // Mark frame cells as NO_CONNECT to prevent junction fixup interference
    box_ensure_meta(&result);
    for (int x = 0; x < w; x++) {
        box_set_meta(&result, x, 0, CELL_META_NO_CONNECT);
        box_set_meta(&result, x, h - 1, CELL_META_NO_CONNECT);
    }
    for (int y = 0; y < h; y++) {
        box_set_meta(&result, 0, y, CELL_META_NO_CONNECT);
        box_set_meta(&result, w - 1, y, CELL_META_NO_CONNECT);
    }

    box_free(&content);
    return result;
}

// ---------- Extensible arrows ----------
Box render_xarrow(const XArrow *xa) {
    Box label = render_internal(xa->label);
    int uni = get_unicode_mode();

    // Arrow width = max(label.w + 2, 3) for padding
    int arrow_w = label.w + 2;
    if (arrow_w < 3) arrow_w = 3;

    // Build 2-row box: label on top, arrow below
    Box result = make_box(arrow_w, 2, 1); // baseline on arrow row

    // Center label on top row
    int lx = (arrow_w - label.w) / 2;
    for (int y = 0; y < label.h && y < 1; y++)
        for (int x = 0; x < label.w; x++) put(&result, lx + x, 0, label.cells[y * label.w + x]);

    // Draw arrow on bottom row
    if (xa->is_right) {
        for (int x = 0; x < arrow_w - 1; x++) put(&result, x, 1, uni ? U'─' : U'-');
        put(&result, arrow_w - 1, 1, uni ? U'→' : U'>');
    } else {
        put(&result, 0, 1, uni ? U'←' : U'<');
        for (int x = 1; x < arrow_w; x++) put(&result, x, 1, uni ? U'─' : U'-');
    }

    box_free(&label);
    return result;
}

// ---------- Substack ----------
Box render_substack(const Substack *ss) {
    if (ss->num_rows == 0) return make_box(0, 1, 0);

    // Render all rows
    Box *rows = calloc(ss->num_rows, sizeof(Box));
    int max_w = 0;
    for (int i = 0; i < ss->num_rows; i++) {
        rows[i] = render_internal(ss->rows[i]);
        if (rows[i].w > max_w) max_w = rows[i].w;
    }

    // Total height = sum of row heights
    int total_h = 0;
    for (int i = 0; i < ss->num_rows; i++) total_h += rows[i].h;

    Box result = make_box(max_w, total_h, total_h / 2);

    // Stack rows vertically, centered
    int y_cursor = 0;
    for (int i = 0; i < ss->num_rows; i++) {
        int ox = (max_w - rows[i].w) / 2;
        for (int y = 0; y < rows[i].h; y++)
            for (int x = 0; x < rows[i].w; x++)
                put(&result, ox + x, y_cursor + y, rows[i].cells[y * rows[i].w + x]);
        y_cursor += rows[i].h;
    }

    for (int i = 0; i < ss->num_rows; i++) box_free(&rows[i]);
    free(rows);
    return result;
}

// ---------- Tag ----------
Box render_tag(const Ast *child) {
    Box content = render_internal(child);

    // Render as "(content)"
    Box lp = text_box("(");
    Box rp = text_box(")");
    Box temp = hcat(&lp, &content, 0);
    Box result = hcat(&temp, &rp, 0);
    result.baseline = temp.baseline;
    result.tag_width = result.w; // Mark entire tag box for right-justification

    box_free(&content);
    box_free(&lp);
    box_free(&rp);
    box_free(&temp);
    return result;
}

// ---------- Square root (ASCII & Unicode) ----------
Box render_sqrt(const SqrtOp *sq) {
    Box rad = render_internal(sq->rad);
    int uni = get_unicode_mode();

    // Optional index
    Box idx = (Box){0};
    int have_idx = (sq->index != NULL);
    if (have_idx) idx = render_internal(sq->index);

    // Reserve 1 row above for the overline
    const int extra_above = 1;
    const int rad_top_in_out = extra_above;
    const int bar_y = rad_top_in_out - 1; // row 0

    // Beak
    const char *cap = get_symbol(SYM_SQRT_HOOK);
    //const int is_single_line = (rad.h == 1);
    const int is_single_line = (rad.baseline == 0);

    // ASCII single-line: add one space of left/right padding to avoid "Vc" ambiguity
    int pad_left = (!uni && is_single_line) ? 1 : 0;
    int pad_right = (!uni && is_single_line) ? 1 : 0;

    // For multi-line, compute leg height; for single-line we won't use it.
    int leg = imax(2, rad.baseline + 1);

    // Where body/bar start:
    //  - single-line: immediately after the beak
    //  - multi-line : one column right of the last diagonal cell (i.e., at x = leg)
    int body_x = is_single_line ? 1 : leg;

    // NOTE the + pad_left + pad_right
    Box out = make_box(body_x + pad_left + rad.w + pad_right, extra_above + rad.h,
                       extra_above + rad.baseline);

    // Blit radicand at (body_x, extra_above)
    for (int y = 0; y < rad.h; ++y)
        for (int x = 0; x < rad.w; ++x)
            put(&out, body_x + pad_left + x, rad_top_in_out + y, rad.cells[y * rad.w + x]);

    // Beak on baseline
    int base_y = out.baseline;
    put_str(&out, 0, base_y, cap);

    // Diagonal leg (multi-line only): from (1, base_y-1) up to the cell just left of the bar.
    if (!is_single_line) {
        uint32_t slash = uni ? U'╱' : U'/';

        // Draw strictly below the bar row
        for (int i = 1; i < leg - 1; ++i) {
            int x = i;
            int y = base_y - i;
            if (x <= body_x - 2 && y >= 0) put(&out, x, y, slash);
        }
        // Put the tip one row BELOW the bar so it doesn't intrude into the overline
        put(&out, body_x - 1, bar_y + 1, slash);
    }

    // Continuous underscore bar over the entire radicand width
    // Continuous overline across pad_left + rad + pad_right
    int bar_start = body_x;
    int bar_end = body_x + pad_left + rad.w + pad_right; // exclusive
    uint32_t over = uni ? U'⎽' /* U+203E '‾' if you used that */ : U'_';

    for (int x = bar_start; x < bar_end; ++x) put(&out, x, bar_y, over);

    // ----- Optional index (order) tucked into the crook of the radical -----
    if (have_idx) {
        // Place the index near the beak (√), not at the bar.
        // For single-line sqrts: baseline=1, so baseline-1=0=bar_y (same as before).
        // For tall sqrts: baseline is further down, placing index in the crook.
        int idx_target_y = out.baseline - 1; // row just above the beak

        int extra_height = 0;
        if (idx.h > idx_target_y + 1) {
            extra_height = idx.h - (idx_target_y + 1);
        }

        Box with_idx = make_box(out.w + idx.w, out.h + extra_height, out.baseline + extra_height);

        // Place index so its bottom row is at the target row
        int idx_bottom_y = extra_height + idx_target_y;
        int idx_y = idx_bottom_y - (idx.h - 1);
        if (idx_y < 0) idx_y = 0;

        // Paint index at far-left
        for (int y = 0; y < idx.h; ++y)
            for (int x = 0; x < idx.w; ++x) put(&with_idx, x, idx_y + y, idx.cells[y * idx.w + x]);

        // Now blit the sqrt block, shifted right by idx.w
        int out_off_x = idx.w;
        int out_off_y = extra_height;
        for (int y = 0; y < out.h; ++y)
            for (int x = 0; x < out.w; ++x)
                put(&with_idx, out_off_x + x, out_off_y + y, out.cells[y * out.w + x]);

        box_free(&out);
        out = with_idx;
    }

    if (have_idx) box_free(&idx);
    box_free(&rad);
    return out;
}

Box render_matrix(const Ast *node) {
    int R = node->matrix.rows, C = node->matrix.cols;
    // Render all cells first
    Box *cells = (Box *)calloc((size_t)R * C, sizeof(Box));
    for (int i = 0; i < R * C; i++) cells[i] = render_internal(node->matrix.cells[i]);

    // Per-column widths; per-row above/below maxima to align baselines row-wise
    int *colw = (int *)calloc(C, sizeof(int));
    int *row_above = (int *)calloc(R, sizeof(int));
    int *row_below = (int *)calloc(R, sizeof(int));

    for (int r = 0; r < R; r++) {
        int above = 0, below = 0;
        for (int c = 0; c < C; c++) {
            Box *b = &cells[r * C + c];
            if (b->w > colw[c]) colw[c] = b->w;
            if (b->baseline > above) above = b->baseline;
            int under = b->h - b->baseline - 1;
            if (under > below) below = under;
        }
        row_above[r] = above;
        row_below[r] = below;
    }

    // Cell padding and gaps
    const int pad_x =
        node->matrix.cell_padding; // use per-matrix padding (2 for matrices, 1 for binomials)
    const int pad_y_top = 0, pad_y_bot = 0;
    const int col_gap = 2; // gap between columns
    const int row_gap = 1; // gap between rows

    // Compute total width/height
    int total_w = 0;
    for (int c = 0; c < C; c++) total_w += (pad_x + colw[c] + pad_x);
    total_w += col_gap * (C ? (C - 1) : 0);

    int total_h = 0;
    for (int r = 0; r < R; r++) {
        int hrow = row_above[r] + 1 + row_below[r] + pad_y_top + pad_y_bot;
        total_h += hrow;
    }
    total_h += row_gap * (R ? (R - 1) : 0);

    // Baseline: center the whole matrix block
    int baseline = total_h / 2;

    Box out = make_box(total_w, total_h, baseline);

    // Blit cells row by row, baseline-align within each row to row_above[r]
    int y_cursor = 0;
    for (int r = 0; r < R; r++) {
        int hrow = row_above[r] + 1 + row_below[r] + pad_y_top + pad_y_bot;
        int x_cursor = 0;

        for (int c = 0; c < C; c++) {
            Box *b = &cells[r * C + c];
            int cell_w = pad_x + colw[c] + pad_x;

            // position: leave pad_x on the left, baseline align to row_above[r]
            int oy = y_cursor + pad_y_top + (row_above[r] - b->baseline);
            int ox = x_cursor + pad_x + (colw[c] - b->w) / 2; // center in the column

            for (int y = 0; y < b->h; y++)
                for (int x = 0; x < b->w; x++) put(&out, ox + x, oy + y, b->cells[y * b->w + x]);

            x_cursor += cell_w;
            if (c + 1 < C) x_cursor += col_gap;
        }

        y_cursor += hrow;
        if (r + 1 < R) y_cursor += row_gap;
    }

    // Cleanup
    for (int i = 0; i < R * C; i++) box_free(&cells[i]);
    free(cells);
    free(colw);
    free(row_above);
    free(row_below);
    return out;
}
