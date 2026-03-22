// math_styles.c - Style stack and text transformations for math rendering
// Verbatim from original render.c

#include "math_internal.h"

// ============================================================================
// Math Italic Transformation
// ============================================================================

// ---- Math italic for Latin (Unicode). h is special: ℎ (U+210E)
uint32_t latin_to_math_italic(uint32_t c) {
    if (c >= 'a' && c <= 'z') {
        static const uint32_t m[] = {U'𝑎', U'𝑏', U'𝑐', U'𝑑', U'𝑒', U'𝑓', U'𝑔', U'ℎ', U'𝑖',
                                     U'𝑗', U'𝑘', U'𝑙', U'𝑚', U'𝑛', U'𝑜', U'𝑝', U'𝑞', U'𝑟',
                                     U'𝑠', U'𝑡', U'𝑢', U'𝑣', U'𝑤', U'𝑥', U'𝑦', U'𝑧'};
        return m[c - 'a'];
    }
    if (c >= 'A' && c <= 'Z') {
        static const uint32_t m[] = {U'𝐴', U'𝐵', U'𝐶', U'𝐷', U'𝐸', U'𝐹', U'𝐺', U'𝐻', U'𝐼',
                                     U'𝐽', U'𝐾', U'𝐿', U'𝑀', U'𝑁', U'𝑂', U'𝑃', U'𝑄', U'𝑅',
                                     U'𝑆', U'𝑇', U'𝑈', U'𝑉', U'𝑊', U'𝑋', U'𝑌', U'𝑍'};
        return m[c - 'A'];
    }
    return c;
}

// ============================================================================
// Upright Operator Detection
// ============================================================================

// ---- Common operator names to keep upright (no cursive):
int is_upright_operator_word(const char *s) {
    // Trig
    static const char *const ops[] = {
        "sin",
        "cos",
        "tan",
        "cot",
        "sec",
        "csc",
        "arcsin",
        "arccos",
        "arctan",
        "arccot",
        "arcsec",
        "arccsc",
        "sinh",
        "cosh",
        "tanh",
        "coth",
        "sech",
        "csch",

        // Logs / exp
        "log",
        "ln",
        "exp",

        // Limits / extrema
        "lim",
        "limsup",
        "liminf",
        "sup",
        "inf",
        "max",
        "min",

        // Algebra / linear
        "det",
        "dim",
        "deg",
        "rank",
        "tr",
        "trace",
        "span",
        "ker",
        "im",
        "Im",
        "Re",

        // Number theory / prob
        "gcd",
        "lcm",
        "Pr",
        "mod",
        "bmod",
        "pmod",

        // Category / misc (common in papers)
        "Hom",
        "End",
        "Aut",
        "id",
        "arg",
        "argmin",
        "argmax",

        // Sets / measure-ish
        "sgn",
        "sign",
        "supp",
        "card",
        "Var",
        "Cov"

        // More upright operators (Phase 1)
        "mod",
        "gcd",
        "lcm", // Already there
        "argmax",
        "argmin", // Already there
        "Pr",
        "Var",
        "Cov", // Already there

        // NEW additions:
        "diag",
        "Diag", // Diagonal matrix
        "grad",
        "curl",
        "div", // Vector calculus
        "erf",
        "erfc", // Error functions
        "sgn",
        "sign", // Already there
        "softmax",
        "relu",      // ML functions
        "attention", // ML
    };
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); ++i)
        if (strcmp(s, ops[i]) == 0) return 1;
    return 0;
}

// ============================================================================
// Greek Letter Mapping
// ============================================================================

const char *map_greek(const char *s) {
    // Lowercase
    if (!strcmp(s, "\\alpha")) return get_symbol(SYM_ALPHA);
    if (!strcmp(s, "\\beta")) return get_symbol(SYM_BETA);
    if (!strcmp(s, "\\gamma")) return get_symbol(SYM_GAMMA);
    if (!strcmp(s, "\\delta")) return get_symbol(SYM_DELTA);
    if (!strcmp(s, "\\epsilon")) return get_symbol(SYM_EPSILON);
    if (!strcmp(s, "\\varepsilon")) return get_symbol(SYM_VAREPSILON);
    if (!strcmp(s, "\\zeta")) return get_symbol(SYM_ZETA);
    if (!strcmp(s, "\\eta")) return get_symbol(SYM_ETA);
    if (!strcmp(s, "\\theta")) return get_symbol(SYM_THETA);
    if (!strcmp(s, "\\vartheta")) return get_symbol(SYM_VARTHETA);
    if (!strcmp(s, "\\iota")) return get_symbol(SYM_IOTA);
    if (!strcmp(s, "\\kappa")) return get_symbol(SYM_KAPPA);
    if (!strcmp(s, "\\lambda")) return get_symbol(SYM_LAMBDA);
    if (!strcmp(s, "\\mu")) return get_symbol(SYM_MU);
    if (!strcmp(s, "\\nu")) return get_symbol(SYM_NU);
    if (!strcmp(s, "\\xi")) return get_symbol(SYM_XI);
    if (!strcmp(s, "\\pi")) return get_symbol(SYM_PI);
    if (!strcmp(s, "\\varpi")) return get_symbol(SYM_VARPI);
    if (!strcmp(s, "\\rho")) return get_symbol(SYM_RHO);
    if (!strcmp(s, "\\varrho")) return get_symbol(SYM_VARRHO);
    if (!strcmp(s, "\\sigma")) return get_symbol(SYM_SIGMA);
    if (!strcmp(s, "\\varsigma")) return get_symbol(SYM_VARSIGMA);
    if (!strcmp(s, "\\tau")) return get_symbol(SYM_TAU);
    if (!strcmp(s, "\\upsilon")) return get_symbol(SYM_UPSILON);
    if (!strcmp(s, "\\phi")) return get_symbol(SYM_PHI);       // (straight phi)
    if (!strcmp(s, "\\varphi")) return get_symbol(SYM_VARPHI); // (curly phi)
    if (!strcmp(s, "\\chi")) return get_symbol(SYM_CHI);
    if (!strcmp(s, "\\psi")) return get_symbol(SYM_PSI);
    if (!strcmp(s, "\\omega")) return get_symbol(SYM_OMEGA);

    // Uppercase
    if (!strcmp(s, "\\Gamma")) return get_symbol(SYM_GAMMA_UPPER);
    if (!strcmp(s, "\\Delta")) return get_symbol(SYM_DELTA_UPPER);
    if (!strcmp(s, "\\Theta")) return get_symbol(SYM_THETA_UPPER);
    if (!strcmp(s, "\\Lambda")) return get_symbol(SYM_LAMBDA_UPPER);
    if (!strcmp(s, "\\Xi")) return get_symbol(SYM_XI_UPPER);
    if (!strcmp(s, "\\Pi")) return get_symbol(SYM_PI_UPPER);
    if (!strcmp(s, "\\Sigma")) return get_symbol(SYM_SIGMA_UPPER);
    if (!strcmp(s, "\\Upsilon")) return get_symbol(SYM_UPSILON_UPPER);
    if (!strcmp(s, "\\Phi")) return get_symbol(SYM_PHI_UPPER);
    if (!strcmp(s, "\\Psi")) return get_symbol(SYM_PSI_UPPER);
    if (!strcmp(s, "\\Omega")) return get_symbol(SYM_OMEGA_UPPER);

    return NULL; // not Greek
}

// ============================================================================
// Style Stack
// ============================================================================

typedef struct {
    StyleKind kind;
} StyleFrame;
static StyleFrame g_style_stack[16];
static int g_style_sp = 0;

void push_style(StyleKind k) {
    if (g_style_sp < (int)(sizeof(g_style_stack) / sizeof(g_style_stack[0])))
        g_style_stack[g_style_sp++].kind = k;
}

void pop_style(void) {
    if (g_style_sp > 0) --g_style_sp;
}

StyleKind current_style(void) {
    return (g_style_sp > 0) ? g_style_stack[g_style_sp - 1].kind : STYLE_NORMAL;
}

// ============================================================================
// Text Style Transformations
// ============================================================================

// -- Bold Latin (U+1D400..), Bold digits (U+1D7CE..), Bold Greek (U+1D6A8..)
uint32_t to_bold(uint32_t c) {
    if (c >= 'A' && c <= 'Z') return 0x1D400 + (c - 'A'); // 𝐀..
    if (c >= 'a' && c <= 'z') return 0x1D41A + (c - 'a'); // 𝐚..
    if (c >= '0' && c <= '9') return 0x1D7CE + (c - '0'); // 𝟎..
    // Bold Greek uppercase: Α(U+0391)..Ω(U+03A9) → 𝚨(U+1D6A8)..
    if (c >= 0x0391 && c <= 0x03A9) return 0x1D6A8 + (c - 0x0391);
    // Bold Greek lowercase: α(U+03B1)..ω(U+03C9) → 𝛂(U+1D6C2)..
    if (c >= 0x03B1 && c <= 0x03C9) return 0x1D6C2 + (c - 0x03B1);
    // Bold operator symbols
    if (c == 0x2207) return 0x1D6C1; // ∇ → 𝛁 (bold nabla)
    if (c == 0x2202) return 0x1D6DB; // ∂ → 𝛛 (bold partial)
    return c;
}

// -- Double-struck (Blackboard Bold). Prefer BMP letters if they exist.
uint32_t to_blackboard(uint32_t c) {
    // Uppercase (best coverage)
    switch (c) {
    case 'C': return U'ℂ';
    case 'H': return U'ℍ';
    case 'N': return U'ℕ';
    case 'P': return U'ℙ';
    case 'Q': return U'ℚ';
    case 'R': return U'ℝ';
    case 'Z': return U'ℤ';
    default: break;
    }
    if (c >= 'A' && c <= 'Z') return 0x1D538 + (c - 'A'); // 𝔸.. (note: many fonts OK)
    if (c >= 'a' && c <= 'z') return 0x1D552 + (c - 'a'); // 𝕒.. (font support varies)
    if (c >= '0' && c <= '9') return 0x1D7D8 + (c - '0'); // 𝟘..
    return c;
}

// -- Script/Calligraphic capitals.
// Unicode has special BMP codepoints for some letters (ℬ, ℰ, ℱ, ℋ, ℐ, ℒ, ℳ, ℛ).
uint32_t to_script(uint32_t c) {
    // By TeX convention \mathcal applies to A–Z; leave lowercase unchanged.
    switch (c) {
    case 'B': return U'ℬ';
    case 'E': return U'ℰ';
    case 'F': return U'ℱ';
    case 'H': return U'ℋ';
    case 'I': return U'ℐ';
    case 'L': return U'ℒ';
    case 'M': return U'ℳ';
    case 'R': return U'ℛ';
    default: break;
    }
    if (c >= 'A' && c <= 'Z')
        return 0x1D49C + (c - 'A'); // 𝒜.. (some holes are filled by BMP above)
    // \mathscr lowercase: a-z → U+1D4B6..
    if (c >= 'a' && c <= 'z') return 0x1D4B6 + (c - 'a');
    return c;
}

// -- Fraktur: \mathfrak  A-Z → U+1D504.., a-z → U+1D51E..
// Some BMP exceptions: C=ℭ, H=ℌ, I=ℑ, R=ℜ, Z=ℨ
uint32_t to_fraktur(uint32_t c) {
    switch (c) {
    case 'C': return 0x212D; // ℭ
    case 'H': return 0x210C; // ℌ
    case 'I': return 0x2111; // ℑ
    case 'R': return 0x211C; // ℜ
    case 'Z': return 0x2128; // ℨ
    default: break;
    }
    if (c >= 'A' && c <= 'Z') return 0x1D504 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 0x1D51E + (c - 'a');
    return c;
}

// -- Sans-serif: \mathsf  A-Z → U+1D5A0.., a-z → U+1D5BA.., 0-9 → U+1D7E2..
uint32_t to_sans(uint32_t c) {
    if (c >= 'A' && c <= 'Z') return 0x1D5A0 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 0x1D5BA + (c - 'a');
    if (c >= '0' && c <= '9') return 0x1D7E2 + (c - '0');
    return c;
}

// ============================================================================
// AST Analysis Helpers
// ============================================================================

// Check if a node is an upright function operator (sin, cos, log, etc.)
// These get a space after them when followed by a non-parenthesized argument
int is_upright_function_node(const Ast *node) {
    if (!node) return 0;

    // Direct symbol that's an upright operator word
    if (node->kind == AST_SYMBOL) {
        const char *s = node->sym.text;
        // Check if it's a backslash command that maps to upright
        if (s[0] == '\\') {
            return is_upright_operator_word(s + 1); // skip backslash
        }
        return is_upright_operator_word(s);
    }

    // Explicit function marker from \fn{...}
    if (node->kind == AST_FUNCTION) return 1;

    // Symbol with superscript: check the base (e.g., \sin^2)
    if (node->kind == AST_SUP) {
        return is_upright_function_node(node->sup.base);
    }

    // Symbol with subscript: check the base (e.g., \log_2)
    if (node->kind == AST_SUB) {
        return is_upright_function_node(node->sup.base);
    }

    // Symbol with both scripts: check the base
    if (node->kind == AST_SUPSUB) {
        return is_upright_function_node(node->supsub.base);
    }

    return 0;
}

// Check if a node needs space around it in implicit multiplication contexts
// This includes:
// - Large operators (sums, products, integrals, etc.)
// - Fractions
// - Roots
// - Tall parentheses (from \left...\right)
// - Upright function operators (sin, cos, log, etc.) - need space BEFORE them
// Note: Regular parentheses do NOT trigger spacing - they rely on explicit \, or the RHS
// Note: \text{...} manages its own spacing so doesn't trigger implicit spacing
int needs_space_in_implicit_mult(const Ast *node, int check_as_lhs) {
    if (!node) return 0;
    switch (node->kind) {
    // Large operators always need spacing
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
    case AST_FRACTION:
    case AST_SQRT:
    case AST_LIMFUNC: // lim, max, min, etc.
        return 1;
    case AST_SYMBOL:
        // Upright function operators (\log, \sin, etc.) need space before (as RHS)
        if (!check_as_lhs && is_upright_function_node(node)) {
            return 1;
        }
        return 0;
    case AST_PAREN:
        // Only tall parentheses (size > 1) need spacing
        // Regular parentheses don't force spacing - let the RHS determine it
        // This avoids double-spacing with explicit \, or \text{ ... }
        return node->paren.size > 1;
    case AST_FUNCTION:
        // \fn{...} - upright function names need space before (as RHS)
        return !check_as_lhs;
    case AST_GROUP:
        // Check inside group
        return needs_space_in_implicit_mult(node->group.child, check_as_lhs);
    case AST_SUP:
    case AST_SUB:
    case AST_SUPSUB:
        // Check the base for constructs that need spacing
        if (node->kind == AST_SUP)
            return needs_space_in_implicit_mult(node->sup.base, check_as_lhs);
        if (node->kind == AST_SUB)
            return needs_space_in_implicit_mult(node->sup.base, check_as_lhs);
        if (node->kind == AST_SUPSUB)
            return needs_space_in_implicit_mult(node->supsub.base, check_as_lhs);
        return 0;
    default: return 0;
    }
}

int is_function_base(const Ast *node) {
    if (!node) return 0;

    // Explicit function marker
    if (node->kind == AST_FUNCTION) return 1; // NEW

    // Text node (from \text{...} or \fn{...})
    if (node->kind == AST_TEXT) return 1; // NEW

    // Direct symbol
    if (node->kind == AST_SYMBOL) return 1;

    // Symbol with superscript: check the base
    if (node->kind == AST_SUP) {
        return is_function_base(node->sup.base);
    }

    // Symbol with subscript: check the base
    if (node->kind == AST_SUB) {
        return is_function_base(node->sup.base);
    }

    // Symbol with both scripts: check the base
    if (node->kind == AST_SUPSUB) {
        return is_function_base(node->supsub.base);
    }

    return 0;
}

// Get the rightmost element of an implicit multiplication chain
// For (a * b) * c, returns c; for a * (b * c), returns c
const Ast *rightmost_element(const Ast *node) {
    if (!node) return NULL;
    // For implicit mult binop, recurse into RHS
    if (node->kind == AST_BINOP && node->bin.op == '\0') {
        return rightmost_element(node->bin.rhs);
    }
    // For groups, look inside
    if (node->kind == AST_GROUP) {
        return rightmost_element(node->group.child);
    }
    return node;
}

// Get the leftmost element of an implicit multiplication chain
const Ast *leftmost_element(const Ast *node) {
    if (!node) return NULL;
    // For implicit mult binop, recurse into LHS
    if (node->kind == AST_BINOP && node->bin.op == '\0') {
        return leftmost_element(node->bin.lhs);
    }
    // For groups, look inside
    if (node->kind == AST_GROUP) {
        return leftmost_element(node->group.child);
    }
    return node;
}
