// math_delimiters.c - Big operators and tall delimiters
// Verbatim from original render.c

#include "math_internal.h"

// ----------------- Σ glyphs (ASCII & Unicode multi-line) -----------------
Box big_sigma(int size) {
    if (size < 1) size = 1;
    if (size > 3) size = 3;

    // Map size to symbol IDs
    SymbolID *line_syms;
    int lines;

    if (size == 1) {
        lines = 3;
        static SymbolID syms[] = {SYM_SIGMA1_L0, SYM_SIGMA1_L1, SYM_SIGMA1_L2};
        line_syms = syms;
    } else if (size == 2) {
        lines = 5;
        static SymbolID syms[] = {SYM_SIGMA2_L0, SYM_SIGMA2_L1, SYM_SIGMA2_L2, SYM_SIGMA2_L3,
                                  SYM_SIGMA2_L4};
        line_syms = syms;
    } else { // size == 3
        lines = 7;
        static SymbolID syms[] = {SYM_SIGMA3_L0, SYM_SIGMA3_L1, SYM_SIGMA3_L2, SYM_SIGMA3_L3,
                                  SYM_SIGMA3_L4, SYM_SIGMA3_L5, SYM_SIGMA3_L6};
        line_syms = syms;
    }

    // Calculate width
    int w = 0;
    for (int i = 0; i < lines; i++) {
        int len = str_cols(get_symbol(line_syms[i]));
        if (len > w) w = len;
    }

    Box b = make_box(w, lines, 0);
    for (int y = 0; y < lines; y++) {
        put_str(&b, 0, y, get_symbol(line_syms[y]));
    }
    b.baseline = lines / 2;
    return b;
}

Box big_prod(int size) {
    if (size < 1) size = 1;
    if (size > 3) size = 3;

    SymbolID *line_syms;
    int lines;

    if (size == 1) {
        lines = 3;
        static SymbolID syms[] = {SYM_PROD1_L0, SYM_PROD1_L1, SYM_PROD1_L2};
        line_syms = syms;
    } else if (size == 2) {
        lines = 5;
        static SymbolID syms[] = {SYM_PROD2_L0, SYM_PROD2_L1, SYM_PROD2_L2, SYM_PROD2_L3,
                                  SYM_PROD2_L4};
        line_syms = syms;
    } else {
        lines = 7;
        static SymbolID syms[] = {SYM_PROD3_L0, SYM_PROD3_L1, SYM_PROD3_L2, SYM_PROD3_L3,
                                  SYM_PROD3_L4, SYM_PROD3_L5, SYM_PROD3_L6};
        line_syms = syms;
    }

    int w = 0;
    for (int i = 0; i < lines; i++) {
        int len = str_cols(get_symbol(line_syms[i]));
        if (len > w) w = len;
    }

    Box b = make_box(w, lines, 0);
    for (int y = 0; y < lines; y++) {
        put_str(&b, 0, y, get_symbol(line_syms[y]));
    }
    b.baseline = lines / 2;
    return b;
}

Box big_int(int size) {
    if (size < 1) size = 1;
    if (size > 3) size = 3;

    SymbolID *line_syms;
    int lines;

    if (size == 1) {
        lines = 3;
        static SymbolID syms[] = {SYM_INT1_L0, SYM_INT1_L1, SYM_INT1_L2};
        line_syms = syms;
    } else if (size == 2) {
        lines = 5;
        static SymbolID syms[] = {SYM_INT2_L0, SYM_INT2_L1, SYM_INT2_L2, SYM_INT2_L3, SYM_INT2_L4};
        line_syms = syms;
    } else {
        lines = 7;
        static SymbolID syms[] = {SYM_INT3_L0, SYM_INT3_L1, SYM_INT3_L2, SYM_INT3_L3,
                                  SYM_INT3_L4, SYM_INT3_L5, SYM_INT3_L6};
        line_syms = syms;
    }

    int w = 0;
    for (int i = 0; i < lines; i++) {
        int len = str_cols(get_symbol(line_syms[i]));
        if (len > w) w = len;
    }

    Box b = make_box(w, lines, 0);
    for (int y = 0; y < lines; y++) {
        put_str(&b, 0, y, get_symbol(line_syms[y]));
    }
    b.baseline = lines / 2;
    return b;
}

// Contour integral (∮) - integral with a circle
Box big_oint(int size) {
    if (size < 1) size = 1;
    if (size > 3) size = 3;

    SymbolID *line_syms;
    int lines;

    if (size == 1) {
        lines = 3;
        static SymbolID syms[] = {SYM_OINT1_L0, SYM_OINT1_L1, SYM_OINT1_L2};
        line_syms = syms;
    } else if (size == 2) {
        lines = 5;
        static SymbolID syms[] = {SYM_OINT2_L0, SYM_OINT2_L1, SYM_OINT2_L2, SYM_OINT2_L3,
                                  SYM_OINT2_L4};
        line_syms = syms;
    } else {
        lines = 7;
        static SymbolID syms[] = {SYM_OINT3_L0, SYM_OINT3_L1, SYM_OINT3_L2, SYM_OINT3_L3,
                                  SYM_OINT3_L4, SYM_OINT3_L5, SYM_OINT3_L6};
        line_syms = syms;
    }

    int w = 0;
    for (int i = 0; i < lines; i++) {
        int len = str_cols(get_symbol(line_syms[i]));
        if (len > w) w = len;
    }

    Box b = make_box(w, lines, 0);
    for (int y = 0; y < lines; y++) {
        put_str(&b, 0, y, get_symbol(line_syms[y]));
    }
    b.baseline = lines / 2;
    return b;
}

// Double integral (∬) - two integrals side by side, ultra-tight
Box big_iint(int size) {
    Box i1 = big_int(size);
    Box i2 = big_int(size);
    // Size 1: maximum overlap (no space), sizes 2-3: leave 1 space between
    // The integral symbols have padding, so we overlap more to get tighter
    int overlap;
    if (size == 1) {
        overlap = i1.w - 1; // Maximum overlap, leaving just 1 col for each integral
    } else {
        overlap = i1.w - 2; // Leave 1 space between
    }
    if (overlap < 0) overlap = 0;
    Box result = hcat_overlap(&i1, &i2, overlap);
    box_free(&i1);
    box_free(&i2);
    return result;
}

// Triple integral (∭) - three integrals side by side, ultra-tight
Box big_iiint(int size) {
    Box i1 = big_int(size);
    Box i2 = big_int(size);
    Box i3 = big_int(size);
    int overlap;
    if (size == 1) {
        overlap = i1.w - 1;
    } else {
        overlap = i1.w - 2;
    }
    if (overlap < 0) overlap = 0;
    Box temp = hcat_overlap(&i1, &i2, overlap);
    Box result = hcat_overlap(&temp, &i3, overlap);
    box_free(&i1);
    box_free(&i2);
    box_free(&i3);
    box_free(&temp);
    return result;
}

// Surface integral (∯) - two contour integrals side by side, ultra-tight
// Each integral has its own circle
Box big_oiint(int size) {
    Box i1 = big_oint(size);
    Box i2 = big_oint(size);
    int overlap;
    if (size == 1) {
        overlap = i1.w - 1;
    } else {
        overlap = i1.w - 2;
    }
    if (overlap < 0) overlap = 0;
    Box result = hcat_overlap(&i1, &i2, overlap);
    box_free(&i1);
    box_free(&i2);
    return result;
}

// Coproduct (∐) - like product but upside down
Box big_coprod(int size) {
    if (size < 1) size = 1;
    if (size > 3) size = 3;

    SymbolID *line_syms;
    int lines;

    if (size == 1) {
        lines = 3;
        static SymbolID syms[] = {SYM_COPROD1_L0, SYM_COPROD1_L1, SYM_COPROD1_L2};
        line_syms = syms;
    } else if (size == 2) {
        lines = 5;
        static SymbolID syms[] = {SYM_COPROD2_L0, SYM_COPROD2_L1, SYM_COPROD2_L2, SYM_COPROD2_L3,
                                  SYM_COPROD2_L4};
        line_syms = syms;
    } else {
        lines = 7;
        static SymbolID syms[] = {SYM_COPROD3_L0, SYM_COPROD3_L1, SYM_COPROD3_L2, SYM_COPROD3_L3,
                                  SYM_COPROD3_L4, SYM_COPROD3_L5, SYM_COPROD3_L6};
        line_syms = syms;
    }

    int w = 0;
    for (int i = 0; i < lines; i++) {
        int len = str_cols(get_symbol(line_syms[i]));
        if (len > w) w = len;
    }

    Box b = make_box(w, lines, 0);
    for (int y = 0; y < lines; y++) {
        put_str(&b, 0, y, get_symbol(line_syms[y]));
    }
    b.baseline = lines / 2;
    return b;
}

Box big_cup(int size) { // union
    if (size < 1) size = 1;
    if (size > 3) size = 3;

    SymbolID *line_syms;
    int lines;

    if (size == 1) {
        lines = 3;
        static SymbolID syms[] = {SYM_CUP1_L0, SYM_CUP1_L1, SYM_CUP1_L2};
        line_syms = syms;
    } else if (size == 2) {
        lines = 5;
        static SymbolID syms[] = {SYM_CUP2_L0, SYM_CUP2_L1, SYM_CUP2_L2, SYM_CUP2_L3, SYM_CUP2_L4};
        line_syms = syms;
    } else {
        lines = 7;
        static SymbolID syms[] = {SYM_CUP3_L0, SYM_CUP3_L1, SYM_CUP3_L2, SYM_CUP3_L3,
                                  SYM_CUP3_L4, SYM_CUP3_L5, SYM_CUP3_L6};
        line_syms = syms;
    }

    int w = 0;
    for (int i = 0; i < lines; i++) {
        int len = str_cols(get_symbol(line_syms[i]));
        if (len > w) w = len;
    }

    Box b = make_box(w, lines, 0);
    for (int y = 0; y < lines; y++) {
        put_str(&b, 0, y, get_symbol(line_syms[y]));
    }
    b.baseline = lines / 2;
    return b;
}
Box big_cap(int size) { // intersection
    if (size < 1) size = 1;
    if (size > 3) size = 3;

    SymbolID *line_syms;
    int lines;

    if (size == 1) {
        lines = 3;
        static SymbolID syms[] = {SYM_CAP1_L0, SYM_CAP1_L1, SYM_CAP1_L2};
        line_syms = syms;
    } else if (size == 2) {
        lines = 5;
        static SymbolID syms[] = {SYM_CAP2_L0, SYM_CAP2_L1, SYM_CAP2_L2, SYM_CAP2_L3, SYM_CAP2_L4};
        line_syms = syms;
    } else {
        lines = 7;
        static SymbolID syms[] = {SYM_CAP3_L0, SYM_CAP3_L1, SYM_CAP3_L2, SYM_CAP3_L3,
                                  SYM_CAP3_L4, SYM_CAP3_L5, SYM_CAP3_L6};
        line_syms = syms;
    }

    int w = 0;
    for (int i = 0; i < lines; i++) {
        int len = str_cols(get_symbol(line_syms[i]));
        if (len > w) w = len;
    }

    Box b = make_box(w, lines, 0);
    for (int y = 0; y < lines; y++) {
        put_str(&b, 0, y, get_symbol(line_syms[y]));
    }
    b.baseline = lines / 2;
    return b;
}

// ---------- Tall delimiter helpers (sizes 3/5/7) ----------

int ensure_odd_min3(int s) {
    if (s < 3) return 3;
    if (s % 2 == 0) return s + 1;
    return s;
}

// | ... | (tall version, size >= 3)
// Uses box-drawing │ for vertical connection, marked NO_CONNECT to prevent table junctions
Box tall_vbar(int size) {
    int uni = get_unicode_mode();
    int lines = ensure_odd_min3(size); // always >= 3

    const char *col = get_symbol(SYM_VERT_SINGLE); // │ box-drawing
    int w = str_cols(col);
    Box b = make_box(w, lines, lines / 2);
    for (int y = 0; y < lines; ++y) {
        put_str(&b, 0, y, col);
        // Mark as non-connecting (content delimiter, not table border)
        for (int x = 0; x < w; ++x) box_set_meta(&b, x, y, CELL_META_NO_CONNECT);
    }
    return b;
}

// || ... ||
// Mark all cells as NO_CONNECT so they don't participate in junction fixup
Box tall_dvbar(int size) {
    int uni = get_unicode_mode();
    int lines = ensure_odd_min3(size);
    const char *col = get_symbol(SYM_VERT_DOUBLE); // ASCII width=2
    int w = str_cols(col);
    Box b = make_box(w, lines, lines / 2);
    for (int y = 0; y < lines; ++y) {
        put_str(&b, 0, y, col);
        // Mark all cells as non-connecting
        for (int x = 0; x < w; ++x) box_set_meta(&b, x, y, CELL_META_NO_CONNECT);
    }
    return b;
}

// ⌊ ... ⌋  (floor)
// Unicode: left uses │ with bottom └; right uses │ with bottom ┘ (width=1).
// ASCII: width=2 for both sides, mirrored, to keep alignment.
Box tall_floor_left(int size) {
    int uni = get_unicode_mode();
    int lines = ensure_odd_min3(size);

    if (uni) {
        Box b = make_box(1, lines, lines / 2);
        for (int y = 0; y < lines - 1; ++y) put(&b, 0, y, U'│');
        put(&b, 0, lines - 1, U'└');
        return b;
    } else {
        // width=2: top..lines-2 = "| ", bottom = "|_"
        Box b = make_box(2, lines, lines / 2);
        for (int y = 0; y < lines - 1; ++y) {
            put(&b, 0, y, '|');
            put(&b, 1, y, ' ');
        }
        put(&b, 0, lines - 1, '|');
        put(&b, 1, lines - 1, '_');
        return b;
    }
}

Box tall_floor_right(int size) {
    int uni = get_unicode_mode();
    int lines = ensure_odd_min3(size);

    if (uni) {
        Box b = make_box(1, lines, lines / 2);
        for (int y = 0; y < lines - 1; ++y) put(&b, 0, y, U'│');
        put(&b, 0, lines - 1, U'┘');
        return b;
    } else {
        // width=2: top..lines-2 = " |", bottom = "_|"
        Box b = make_box(2, lines, lines / 2);
        for (int y = 0; y < lines - 1; ++y) {
            put(&b, 0, y, ' ');
            put(&b, 1, y, '|');
        }
        put(&b, 0, lines - 1, '_');
        put(&b, 1, lines - 1, '|');
        return b;
    }
}

// ⌈ ... ⌉  (ceil)
// Unicode: left top ┌, then │; right top ┐, then │ (width=1).
// ASCII: width=2 for both sides, mirrored, to keep alignment.
Box tall_ceil_left(int size) {
    int uni = get_unicode_mode();
    int lines = ensure_odd_min3(size);

    if (uni) {
        Box b = make_box(1, lines, lines / 2);
        put(&b, 0, 0, U'┌');
        for (int y = 1; y < lines; ++y) put(&b, 0, y, U'│');
        return b;
    } else {
        // width=2: top = "_|", rows 1.. = "| "
        Box b = make_box(2, lines, lines / 2);
        put(&b, 0, 0, '|');
        put(&b, 1, 0, '-');
        for (int y = 1; y < lines; ++y) {
            put(&b, 0, y, '|');
            put(&b, 1, y, ' ');
        }
        return b;
    }
}

Box tall_ceil_right(int size) {
    int uni = get_unicode_mode();
    int lines = ensure_odd_min3(size);

    if (uni) {
        Box b = make_box(1, lines, lines / 2);
        put(&b, 0, 0, U'┐');
        for (int y = 1; y < lines; ++y) put(&b, 0, y, U'│');
        return b;
    } else {
        // width=2: top = "|_", rows 1.. = " |"
        Box b = make_box(2, lines, lines / 2);
        put(&b, 0, 0, '-');
        put(&b, 1, 0, '|');
        for (int y = 1; y < lines; ++y) {
            put(&b, 0, y, ' ');
            put(&b, 1, y, '|');
        }
        return b;
    }
}

// ⟨ ... ⟩  (angle brackets)
// Tall angle brackets: render using diagonal lines that grow with size.
// Unicode: Use box-drawing diagonals ╲ (U+2572) and ╱ (U+2571)
// ASCII: Use / and \ characters
//
// Left angle pattern (3 lines):     Right angle pattern:
//   ╲                                 ╱
//    ╳ (or ⟨ at center)              ╳ (or ⟩ at center)
//   ╱                                 ╲
//
// For larger sizes, diagonals extend with appropriate width.
Box tall_angle_left(int size) {
    int uni = get_unicode_mode();
    int lines = ensure_odd_min3(size);
    int mid = lines / 2;

    // Width needed: half the height (to form a proper angle)
    int w = (lines / 2) + 1;

    Box b = make_box(w, lines, mid);

    // Draw top half: diagonals going from right to left as we go down
    for (int y = 0; y < mid; y++) {
        int x = w - 1 - y; // Start from right, move left
        if (x >= 0 && x < w) {
            put(&b, x, y, uni ? U'╱' : '/');
        }
    }

    // Draw center point (tip of angle)
    put(&b, 0, mid, uni ? U'⟨' : '<');

    // Draw bottom half: diagonals going from left to right as we go down
    for (int y = mid + 1; y < lines; y++) {
        int x = y - mid; // Start from left, move right
        if (x >= 0 && x < w) {
            put(&b, x, y, uni ? U'╲' : '\\');
        }
    }

    return b;
}

Box tall_angle_right(int size) {
    int uni = get_unicode_mode();
    int lines = ensure_odd_min3(size);
    int mid = lines / 2;

    // Width needed: half the height (to form a proper angle)
    int w = (lines / 2) + 1;

    Box b = make_box(w, lines, mid);

    // Draw top half: diagonals going from left to right as we go down
    for (int y = 0; y < mid; y++) {
        int x = y; // Start from left, move right
        if (x >= 0 && x < w) {
            put(&b, x, y, uni ? U'╲' : '\\');
        }
    }

    // Draw center point (tip of angle)
    put(&b, w - 1, mid, uni ? U'⟩' : '>');

    // Draw bottom half: diagonals going from right to left as we go down
    for (int y = mid + 1; y < lines; y++) {
        int x = w - 1 - (y - mid); // Start from right, move left
        if (x >= 0 && x < w) {
            put(&b, x, y, uni ? U'╱' : '/');
        }
    }

    return b;
}
