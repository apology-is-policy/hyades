#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render_opts.h"
#include "symbols.h"

// ============================================================================
// Symbol Records - Single Source of Truth
// ============================================================================
//
// Each record contains ALL metadata for a symbol:
//   - id: SymbolID enum value
//   - name: String name for \setsym{NAME}{value} commands
//   - category: Display category for template organization
//   - ascii: ASCII-mode rendering
//   - unicode: Unicode-mode rendering
//
// This is the ONLY place where symbol data is defined.
// Adding a new symbol = adding ONE line here!
//
// ============================================================================

typedef struct {
    SymbolID id;
    char *ascii_override;   // NULL = use default
    char *unicode_override; // NULL = use default
} SymbolOverride;

#define MAX_OVERRIDES 512

static SymbolOverride g_overrides[MAX_OVERRIDES];
static int g_override_count = 0;

// Find override for a symbol ID (returns NULL if not found)
static SymbolOverride *find_override(SymbolID id) {
    for (int i = 0; i < g_override_count; i++) {
        if (g_overrides[i].id == id) {
            return &g_overrides[i];
        }
    }
    return NULL;
}

static const SymbolRecord g_symbol_records[] = {

    // ========== Greek Letters (Lowercase) ==========
    {SYM_ALPHA, "SYM_ALPHA", "Greek Letters (Lowercase)", "alpha", "α"},
    {SYM_BETA, "SYM_BETA", "Greek Letters (Lowercase)", "beta", "β"},
    {SYM_GAMMA, "SYM_GAMMA", "Greek Letters (Lowercase)", "gamma", "γ"},
    {SYM_DELTA, "SYM_DELTA", "Greek Letters (Lowercase)", "delta", "δ"},
    {SYM_EPSILON, "SYM_EPSILON", "Greek Letters (Lowercase)", "epsilon", "ε"},
    {SYM_VAREPSILON, "SYM_VAREPSILON", "Greek Letters (Lowercase)", "varepsilon", "ϵ"},
    {SYM_ZETA, "SYM_ZETA", "Greek Letters (Lowercase)", "zeta", "ζ"},
    {SYM_ETA, "SYM_ETA", "Greek Letters (Lowercase)", "eta", "η"},
    {SYM_THETA, "SYM_THETA", "Greek Letters (Lowercase)", "theta", "θ"},
    {SYM_VARTHETA, "SYM_VARTHETA", "Greek Letters (Lowercase)", "vartheta", "ϑ"},
    {SYM_IOTA, "SYM_IOTA", "Greek Letters (Lowercase)", "iota", "ι"},
    {SYM_KAPPA, "SYM_KAPPA", "Greek Letters (Lowercase)", "kappa", "κ"},
    {SYM_LAMBDA, "SYM_LAMBDA", "Greek Letters (Lowercase)", "lambda", "λ"},
    {SYM_MU, "SYM_MU", "Greek Letters (Lowercase)", "mu", "μ"},
    {SYM_NU, "SYM_NU", "Greek Letters (Lowercase)", "nu", "ν"},
    {SYM_XI, "SYM_XI", "Greek Letters (Lowercase)", "xi", "ξ"},
    {SYM_OMICRON, "SYM_OMICRON", "Greek Letters (Lowercase)", "omicron", "o"},
    {SYM_PI, "SYM_PI", "Greek Letters (Lowercase)", "pi", "π"},
    {SYM_VARPI, "SYM_VARPI", "Greek Letters (Lowercase)", "varpi", "ϖ"},
    {SYM_RHO, "SYM_RHO", "Greek Letters (Lowercase)", "rho", "ρ"},
    {SYM_VARRHO, "SYM_VARRHO", "Greek Letters (Lowercase)", "varrho", "ϱ"},
    {SYM_SIGMA, "SYM_SIGMA", "Greek Letters (Lowercase)", "sigma", "σ"},
    {SYM_VARSIGMA, "SYM_VARSIGMA", "Greek Letters (Lowercase)", "varsigma", "ς"},
    {SYM_TAU, "SYM_TAU", "Greek Letters (Lowercase)", "tau", "τ"},
    {SYM_UPSILON, "SYM_UPSILON", "Greek Letters (Lowercase)", "upsilon", "υ"},
    {SYM_PHI, "SYM_PHI", "Greek Letters (Lowercase)", "phi", "φ"},
    {SYM_VARPHI, "SYM_VARPHI", "Greek Letters (Lowercase)", "varphi", "ϕ"},
    {SYM_CHI, "SYM_CHI", "Greek Letters (Lowercase)", "chi", "χ"},
    {SYM_PSI, "SYM_PSI", "Greek Letters (Lowercase)", "psi", "ψ"},
    {SYM_OMEGA, "SYM_OMEGA", "Greek Letters (Lowercase)", "omega", "ω"},

    // ========== Greek Letters (Uppercase) ==========
    {SYM_ALPHA_UPPER, "SYM_ALPHA_UPPER", "Greek Letters (Uppercase)", "Alpha", "Α"},
    {SYM_BETA_UPPER, "SYM_BETA_UPPER", "Greek Letters (Uppercase)", "Beta", "Β"},
    {SYM_GAMMA_UPPER, "SYM_GAMMA_UPPER", "Greek Letters (Uppercase)", "Gamma", "Γ"},
    {SYM_DELTA_UPPER, "SYM_DELTA_UPPER", "Greek Letters (Uppercase)", "Delta", "Δ"},
    {SYM_THETA_UPPER, "SYM_THETA_UPPER", "Greek Letters (Uppercase)", "Theta", "Θ"},
    {SYM_LAMBDA_UPPER, "SYM_LAMBDA_UPPER", "Greek Letters (Uppercase)", "Lambda", "Λ"},
    {SYM_XI_UPPER, "SYM_XI_UPPER", "Greek Letters (Uppercase)", "Xi", "Ξ"},
    {SYM_PI_UPPER, "SYM_PI_UPPER", "Greek Letters (Uppercase)", "Pi", "Π"},
    {SYM_SIGMA_UPPER, "SYM_SIGMA_UPPER", "Greek Letters (Uppercase)", "Sigma", "Σ"},
    {SYM_UPSILON_UPPER, "SYM_UPSILON_UPPER", "Greek Letters (Uppercase)", "Upsilon", "Υ"},
    {SYM_PHI_UPPER, "SYM_PHI_UPPER", "Greek Letters (Uppercase)", "Phi", "Φ"},
    {SYM_PSI_UPPER, "SYM_PSI_UPPER", "Greek Letters (Uppercase)", "Psi", "Ψ"},
    {SYM_OMEGA_UPPER, "SYM_OMEGA_UPPER", "Greek Letters (Uppercase)", "Omega", "Ω"},

    // ========== Binary Operators ==========
    {SYM_PLUS, "SYM_PLUS", "Binary Operators", "+", "+"},
    {SYM_MINUS, "SYM_MINUS", "Binary Operators", "-", "−"},
    {SYM_TIMES, "SYM_TIMES", "Binary Operators", "x", "×"},
    {SYM_CDOT, "SYM_CDOT", "Binary Operators", "*", "⋅"},
    {SYM_DIV, "SYM_DIV", "Binary Operators", "/", "÷"},
    {SYM_PM, "SYM_PM", "Binary Operators", "+/-", "±"},
    {SYM_MP, "SYM_MP", "Binary Operators", "-/+", "∓"},

    // ========== Relational Operators ==========
    {SYM_EQ, "SYM_EQ", "Relational Operators", "=", "="},
    {SYM_NEQ, "SYM_NEQ", "Relational Operators", "!=", "≠"},
    {SYM_LT, "SYM_LT", "Relational Operators", "<", "<"},
    {SYM_GT, "SYM_GT", "Relational Operators", ">", ">"},
    {SYM_LEQ, "SYM_LEQ", "Relational Operators", "<=", "≤"},
    {SYM_GEQ, "SYM_GEQ", "Relational Operators", ">=", "≥"},
    {SYM_EQUIV, "SYM_EQUIV", "Relational Operators", "===", "≡"},
    {SYM_APPROX, "SYM_APPROX", "Relational Operators", "~=", "≈"},
    {SYM_LL, "SYM_LL", "Relational Operators", "<<", "≪"},
    {SYM_GG, "SYM_GG", "Relational Operators", ">>", "≫"},
    {SYM_SIM, "SYM_SIM", "Relational Operators", "~", "∼"},
    {SYM_SIMEQ, "SYM_SIMEQ", "Relational Operators", "~=", "≃"},
    {SYM_CONG, "SYM_CONG", "Relational Operators", "~=", "≅"},
    {SYM_PROPTO, "SYM_PROPTO", "Relational Operators", "oc", "∝"},
    {SYM_MID, "SYM_MID", "Relational Operators", "|", "|"},
    {SYM_NMID, "SYM_NMID", "Relational Operators", "|/", "∤"},

    // ========== Set Theory ==========
    {SYM_IN, "SYM_IN", "Set Theory", "in", "∈"},
    {SYM_NOTIN, "SYM_NOTIN", "Set Theory", "!in", "∉"},
    {SYM_NI, "SYM_NI", "Set Theory", "ni", "∋"},
    {SYM_SUBSET, "SYM_SUBSET", "Set Theory", "subset", "⊂"},
    {SYM_SUBSETEQ, "SYM_SUBSETEQ", "Set Theory", "subseteq", "⊆"},
    {SYM_SUPSET, "SYM_SUPSET", "Set Theory", "supset", "⊃"},
    {SYM_SUPSETEQ, "SYM_SUPSETEQ", "Set Theory", "supseteq", "⊇"},
    {SYM_CUP, "SYM_CUP", "Set Theory", "U", "∪"},
    {SYM_CAP, "SYM_CAP", "Set Theory", "n", "∩"},
    {SYM_EMPTYSET, "SYM_EMPTYSET", "Set Theory", "O", "∅"},
    {SYM_SETMINUS, "SYM_SETMINUS", "Set Theory", "\\", "∖"},
    {SYM_VARNOTHING, "SYM_VARNOTHING", "Set Theory", "O", "∅"},

    // ========== Logic ==========
    {SYM_FORALL, "SYM_FORALL", "Logic", "A", "∀"},
    {SYM_EXISTS, "SYM_EXISTS", "Logic", "E", "∃"},
    {SYM_NEXISTS, "SYM_NEXISTS", "Logic", "!E", "∄"},
    {SYM_NEG, "SYM_NEG", "Logic", "~", "¬"},
    {SYM_LAND, "SYM_LAND", "Logic", "/\\", "∧"},
    {SYM_LOR, "SYM_LOR", "Logic", "\\/", "∨"},
    {SYM_IMPLIES, "SYM_IMPLIES", "Logic", "=>", "⇒"},
    {SYM_IFF, "SYM_IFF", "Logic", "<=>", "⇔"},
    {SYM_BOT, "SYM_BOT", "Logic", "_|_", "⊥"},
    {SYM_VDASH, "SYM_VDASH", "Logic", "|-", "⊢"},
    {SYM_DASHV, "SYM_DASHV", "Logic", "-|", "⊣"},
    {SYM_MODELS, "SYM_MODELS", "Logic", "|=", "⊨"},

    // ========== Hebrew Letters ==========
    {SYM_ALEPH, "SYM_ALEPH", "Hebrew Letters", "N", "ℵ"},
    {SYM_BETH, "SYM_BETH", "Hebrew Letters", "B", "ℶ"},
    {SYM_GIMEL, "SYM_GIMEL", "Hebrew Letters", "G", "ℷ"},
    {SYM_DALETH, "SYM_DALETH", "Hebrew Letters", "D", "ℸ"},

    // ========== Arrows ==========
    {SYM_RIGHTARROW, "SYM_RIGHTARROW", "Arrows", "->", "→"},
    {SYM_LEFTARROW, "SYM_LEFTARROW", "Arrows", "<-", "←"},
    {SYM_LEFTRIGHTARROW, "SYM_LEFTRIGHTARROW", "Arrows", "<->", "↔"},
    {SYM_RIGHTARROW_DOUBLE, "SYM_RIGHTARROW_DOUBLE", "Arrows", "=>", "⇒"},
    {SYM_LEFTARROW_DOUBLE, "SYM_LEFTARROW_DOUBLE", "Arrows", "<=", "⇐"},
    {SYM_LEFTRIGHTARROW_DOUBLE, "SYM_LEFTRIGHTARROW_DOUBLE", "Arrows", "<=>", "⇔"},
    {SYM_MAPSTO, "SYM_MAPSTO", "Arrows", "|->", "↦"},
    {SYM_LONGMAPSTO, "SYM_LONGMAPSTO", "Arrows", "|-->", "⟼"},
    {SYM_LONGRIGHTARROW, "SYM_LONGRIGHTARROW", "Arrows", "-->", "⟶"},
    {SYM_LONGLEFTARROW, "SYM_LONGLEFTARROW", "Arrows", "<--", "⟵"},
    {SYM_HOOKRIGHTARROW, "SYM_HOOKRIGHTARROW", "Arrows", "`->", "↪"},
    {SYM_HOOKLEFTARROW, "SYM_HOOKLEFTARROW", "Arrows", "<-'", "↩"},
    {SYM_UPARROW, "SYM_UPARROW", "Arrows", "^", "↑"},
    {SYM_DOWNARROW, "SYM_DOWNARROW", "Arrows", "v", "↓"},
    {SYM_UPDOWNARROW, "SYM_UPDOWNARROW", "Arrows", "^v", "↕"},
    {SYM_UPARROW_DOUBLE, "SYM_UPARROW_DOUBLE", "Arrows", "^^", "⇑"},
    {SYM_DOWNARROW_DOUBLE, "SYM_DOWNARROW_DOUBLE", "Arrows", "vv", "⇓"},
    {SYM_NEARROW, "SYM_NEARROW", "Arrows", "/^", "↗"},
    {SYM_SEARROW, "SYM_SEARROW", "Arrows", "\\v", "↘"},
    {SYM_NWARROW, "SYM_NWARROW", "Arrows", "^\\", "↖"},
    {SYM_SWARROW, "SYM_SWARROW", "Arrows", "/v", "↙"},

    // ========== Miscellaneous Symbols ==========
    {SYM_INFTY, "SYM_INFTY", "Miscellaneous Symbols", "inf", "∞"},
    {SYM_PARTIAL, "SYM_PARTIAL", "Miscellaneous Symbols", "d", "∂"},
    {SYM_NABLA, "SYM_NABLA", "Miscellaneous Symbols", "V", "∇"},
    {SYM_HBAR, "SYM_HBAR", "Miscellaneous Symbols", "h", "ℏ"},
    {SYM_PRIME, "SYM_PRIME", "Miscellaneous Symbols", "'", "′"},
    {SYM_DPRIME, "SYM_DPRIME", "Miscellaneous Symbols", "''", "″"},
    {SYM_TPRIME, "SYM_TPRIME", "Miscellaneous Symbols", "'''", "‴"},
    {SYM_ANGLE, "SYM_ANGLE", "Miscellaneous Symbols", "/_", "∠"},
    {SYM_PERP, "SYM_PERP", "Miscellaneous Symbols", "_|_", "⊥"},
    {SYM_TOP, "SYM_TOP", "Miscellaneous Symbols", "T", "⊤"},
    {SYM_PARALLEL, "SYM_PARALLEL", "Miscellaneous Symbols", "||", "∥"},
    {SYM_BULLET, "SYM_BULLET", "Miscellaneous Symbols", "*", "•"},
    {SYM_CIRC, "SYM_CIRC", "Miscellaneous Symbols", "o", "∘"},
    {SYM_STAR, "SYM_STAR", "Miscellaneous Symbols", "*", "⋆"},
    {SYM_DAG, "SYM_DAG", "Miscellaneous Symbols", "+", "†"},
    {SYM_DDAG, "SYM_DDAG", "Miscellaneous Symbols", "++", "‡"},
    {SYM_ELL, "SYM_ELL", "Miscellaneous Symbols", "l", "ℓ"},

    // ========== Circled Operators ==========
    {SYM_OPLUS, "SYM_OPLUS", "Circled Operators", "(+)", "⊕"},
    {SYM_OMINUS, "SYM_OMINUS", "Circled Operators", "(-)", "⊖"},
    {SYM_OTIMES, "SYM_OTIMES", "Circled Operators", "(x)", "⊗"},
    {SYM_ODOT, "SYM_ODOT", "Circled Operators", "(.)", "⊙"},

    // ========== Dots ==========
    {SYM_LDOTS, "SYM_LDOTS", "Dots", "...", "…"},
    {SYM_CDOTS, "SYM_CDOTS", "Dots", "...", "⋯"},
    {SYM_VDOTS, "SYM_VDOTS", "Dots", ":", "⋮"},
    {SYM_DDOTS, "SYM_DDOTS", "Dots", ".", "⋱"},

    // ========== Delimiters ==========
    {SYM_LANGLE, "SYM_LANGLE", "Delimiters", "<", "⟨"},
    {SYM_RANGLE, "SYM_RANGLE", "Delimiters", ">", "⟩"},

    // ========== Geometry ==========
    {SYM_TRIANGLE, "SYM_TRIANGLE", "Geometry", "/\\", "△"},
    {SYM_SQUARE, "SYM_SQUARE", "Geometry", "[]", "□"},

    // ========== Structural Symbols ==========
    {SYM_FRAC_BAR, "SYM_FRAC_BAR", "Structural Symbols", "-", "─"},
    {SYM_SQRT_VINCULUM, "SYM_SQRT_VINCULUM", "Structural Symbols", "_", "‾"},
    {SYM_SQRT_HOOK, "SYM_SQRT_HOOK", "Structural Symbols", "V", "√"},

    // ========== Big Operators - Summation (Σ) ==========
    {SYM_SIGMA1_L0, "SYM_SIGMA1_L0", "Big Operators - Summation (Σ)", "---", "───"},
    {SYM_SIGMA1_L1, "SYM_SIGMA1_L1", "Big Operators - Summation (Σ)", "\\", "╲  "},
    {SYM_SIGMA1_L2, "SYM_SIGMA1_L2", "Big Operators - Summation (Σ)", "/__", "╱⎽⎽"},
    {SYM_SIGMA2_L0, "SYM_SIGMA2_L0", "Big Operators - Summation (Σ)", "____", "─────"},
    {SYM_SIGMA2_L1, "SYM_SIGMA2_L1", "Big Operators - Summation (Σ)", "\\", "╲    "},
    {SYM_SIGMA2_L2, "SYM_SIGMA2_L2", "Big Operators - Summation (Σ)", " \\", " ╲   "},
    {SYM_SIGMA2_L3, "SYM_SIGMA2_L3", "Big Operators - Summation (Σ)", " /", " ╱   "},
    {SYM_SIGMA2_L4, "SYM_SIGMA2_L4", "Big Operators - Summation (Σ)", "/___", "╱⎽⎽⎽⎽"},
    {SYM_SIGMA3_L0, "SYM_SIGMA3_L0", "Big Operators - Summation (Σ)", "______", "──────"},
    {SYM_SIGMA3_L1, "SYM_SIGMA3_L1", "Big Operators - Summation (Σ)", "\\", "╲     "},
    {SYM_SIGMA3_L2, "SYM_SIGMA3_L2", "Big Operators - Summation (Σ)", " \\", " ╲    "},
    {SYM_SIGMA3_L3, "SYM_SIGMA3_L3", "Big Operators - Summation (Σ)", "  \\", "  ╲   "},
    {SYM_SIGMA3_L4, "SYM_SIGMA3_L4", "Big Operators - Summation (Σ)", "  /", "  ╱   "},
    {SYM_SIGMA3_L5, "SYM_SIGMA3_L5", "Big Operators - Summation (Σ)", " /", " ╱    "},
    {SYM_SIGMA3_L6, "SYM_SIGMA3_L6", "Big Operators - Summation (Σ)", "/_____",
     "╱⎽⎽⎽⎽⎽"}, // For Julia, it's better to use _ whereas correctly we should use ⎽ .

    // ========== Big Operators - Product (Π) ==========
    {SYM_PROD1_L0, "SYM_PROD1_L0", "Big Operators - Product (Π)", "___", "┬─┬"},
    {SYM_PROD1_L1, "SYM_PROD1_L1", "Big Operators - Product (Π)", "| |", "│ │"},
    {SYM_PROD1_L2, "SYM_PROD1_L2", "Big Operators - Product (Π)", "| |", "┴ ┴"},
    {SYM_PROD2_L0, "SYM_PROD2_L0", "Big Operators - Product (Π)", "_____", "┬───┬"},
    {SYM_PROD2_L1, "SYM_PROD2_L1", "Big Operators - Product (Π)", "|   |", "│   │"},
    {SYM_PROD2_L2, "SYM_PROD2_L2", "Big Operators - Product (Π)", "|   |", "│   │"},
    {SYM_PROD2_L3, "SYM_PROD2_L3", "Big Operators - Product (Π)", "|   |", "│   │"},
    {SYM_PROD2_L4, "SYM_PROD2_L4", "Big Operators - Product (Π)", "|   |", "┴   ┴"},
    {SYM_PROD3_L0, "SYM_PROD3_L0", "Big Operators - Product (Π)", "_______", "┬─────┬"},
    {SYM_PROD3_L1, "SYM_PROD3_L1", "Big Operators - Product (Π)", "|     |", "│     │"},
    {SYM_PROD3_L2, "SYM_PROD3_L2", "Big Operators - Product (Π)", "|     |", "│     │"},
    {SYM_PROD3_L3, "SYM_PROD3_L3", "Big Operators - Product (Π)", "|     |", "│     │"},
    {SYM_PROD3_L4, "SYM_PROD3_L4", "Big Operators - Product (Π)", "|     |", "│     │"},
    {SYM_PROD3_L5, "SYM_PROD3_L5", "Big Operators - Product (Π)", "|     |", "│     │"},
    {SYM_PROD3_L6, "SYM_PROD3_L6", "Big Operators - Product (Π)", "|_   _|", "┴─   ─┴"},

    // ========== Big Operators - Integral (∫) ==========
    {SYM_INT1_L0, "SYM_INT1_L0", "Big Operators - Integral (∫)", " / ", " ⌠ "},
    {SYM_INT1_L1, "SYM_INT1_L1", "Big Operators - Integral (∫)", " | ", " ⎮ "},
    {SYM_INT1_L2, "SYM_INT1_L2", "Big Operators - Integral (∫)", " / ", " ⌡ "},
    {SYM_INT2_L0, "SYM_INT2_L0", "Big Operators - Integral (∫)", "  /  ", "  ⌠  "},
    {SYM_INT2_L1, "SYM_INT2_L1", "Big Operators - Integral (∫)", "  |  ", "  ⎮  "},
    {SYM_INT2_L2, "SYM_INT2_L2", "Big Operators - Integral (∫)", "  |  ", "  ⎮  "},
    {SYM_INT2_L3, "SYM_INT2_L3", "Big Operators - Integral (∫)", "  |  ", "  ⎮  "},
    {SYM_INT2_L4, "SYM_INT2_L4", "Big Operators - Integral (∫)", "  /  ", "  ⌡  "},
    {SYM_INT3_L0, "SYM_INT3_L0", "Big Operators - Integral (∫)", "   /   ", "   ⌠   "},
    {SYM_INT3_L1, "SYM_INT3_L1", "Big Operators - Integral (∫)", "   |   ", "   ⎮   "},
    {SYM_INT3_L2, "SYM_INT3_L2", "Big Operators - Integral (∫)", "   |   ", "   ⎮   "},
    {SYM_INT3_L3, "SYM_INT3_L3", "Big Operators - Integral (∫)", "   |   ", "   ⎮   "},
    {SYM_INT3_L4, "SYM_INT3_L4", "Big Operators - Integral (∫)", "   |   ", "   ⎮   "},
    {SYM_INT3_L5, "SYM_INT3_L5", "Big Operators - Integral (∫)", "   |   ", "   ⎮   "},
    {SYM_INT3_L6, "SYM_INT3_L6", "Big Operators - Integral (∫)", "   /   ", "   ⌡   "},

    // ========== Big Operators - Contour Integral (∮) ==========
    // The contour integral has a circle in the middle of the integral sign
    {SYM_OINT1_L0, "SYM_OINT1_L0", "Big Operators - Contour Integral (∮)", " ⌠ ", " ⌠ "},
    {SYM_OINT1_L1, "SYM_OINT1_L1", "Big Operators - Contour Integral (∮)", " ○ ", " ⌽ "},
    {SYM_OINT1_L2, "SYM_OINT1_L2", "Big Operators - Contour Integral (∮)", " ⌡ ", " ⌡ "},
    {SYM_OINT2_L0, "SYM_OINT2_L0", "Big Operators - Contour Integral (∮)", "  ⌠  ", "  ⌠  "},
    {SYM_OINT2_L1, "SYM_OINT2_L1", "Big Operators - Contour Integral (∮)", "  ⎮  ", "  ⎮  "},
    {SYM_OINT2_L2, "SYM_OINT2_L2", "Big Operators - Contour Integral (∮)", "  ○  ", "  ⌽  "},
    {SYM_OINT2_L3, "SYM_OINT2_L3", "Big Operators - Contour Integral (∮)", "  ⎮  ", "  ⎮  "},
    {SYM_OINT2_L4, "SYM_OINT2_L4", "Big Operators - Contour Integral (∮)", "  ⌡  ", "  ⌡  "},
    {SYM_OINT3_L0, "SYM_OINT3_L0", "Big Operators - Contour Integral (∮)", "   ⌠   ", "   ⌠   "},
    {SYM_OINT3_L1, "SYM_OINT3_L1", "Big Operators - Contour Integral (∮)", "   ⎮   ", "   ⎮   "},
    {SYM_OINT3_L2, "SYM_OINT3_L2", "Big Operators - Contour Integral (∮)", "   ⎮   ", "   ⎮   "},
    {SYM_OINT3_L3, "SYM_OINT3_L3", "Big Operators - Contour Integral (∮)", "   ○   ", "   ⌽   "},
    {SYM_OINT3_L4, "SYM_OINT3_L4", "Big Operators - Contour Integral (∮)", "   ⎮   ", "   ⎮   "},
    {SYM_OINT3_L5, "SYM_OINT3_L5", "Big Operators - Contour Integral (∮)", "   ⎮   ", "   ⎮   "},
    {SYM_OINT3_L6, "SYM_OINT3_L6", "Big Operators - Contour Integral (∮)", "   ⌡   ", "   ⌡   "},

    // ========== Big Operators - Coproduct (∐) ==========
    // Like product but upside down (open top, bar at bottom)
    {SYM_COPROD1_L0, "SYM_COPROD1_L0", "Big Operators - Coproduct (∐)", "| |", "│ │"},
    {SYM_COPROD1_L1, "SYM_COPROD1_L1", "Big Operators - Coproduct (∐)", "| |", "│ │"},
    {SYM_COPROD1_L2, "SYM_COPROD1_L2", "Big Operators - Coproduct (∐)", "---", "┴─┴"},
    {SYM_COPROD2_L0, "SYM_COPROD2_L0", "Big Operators - Coproduct (∐)", "|   |", "│   │"},
    {SYM_COPROD2_L1, "SYM_COPROD2_L1", "Big Operators - Coproduct (∐)", "|   |", "│   │"},
    {SYM_COPROD2_L2, "SYM_COPROD2_L2", "Big Operators - Coproduct (∐)", "|   |", "│   │"},
    {SYM_COPROD2_L3, "SYM_COPROD2_L3", "Big Operators - Coproduct (∐)", "|   |", "│   │"},
    {SYM_COPROD2_L4, "SYM_COPROD2_L4", "Big Operators - Coproduct (∐)", "-----", "┴───┴"},
    {SYM_COPROD3_L0, "SYM_COPROD3_L0", "Big Operators - Coproduct (∐)", "|     |", "│     │"},
    {SYM_COPROD3_L1, "SYM_COPROD3_L1", "Big Operators - Coproduct (∐)", "|     |", "│     │"},
    {SYM_COPROD3_L2, "SYM_COPROD3_L2", "Big Operators - Coproduct (∐)", "|     |", "│     │"},
    {SYM_COPROD3_L3, "SYM_COPROD3_L3", "Big Operators - Coproduct (∐)", "|     |", "│     │"},
    {SYM_COPROD3_L4, "SYM_COPROD3_L4", "Big Operators - Coproduct (∐)", "|     |", "│     │"},
    {SYM_COPROD3_L5, "SYM_COPROD3_L5", "Big Operators - Coproduct (∐)", "|     |", "│     │"},
    {SYM_COPROD3_L6, "SYM_COPROD3_L6", "Big Operators - Coproduct (∐)", "-------", "┴─────┴"},

    // ========== Big Operators - Union (⋃) ==========
    {SYM_CUP1_L0, "SYM_CUP1_L0", "Big Operators - Union (⋃)", "|   |", "│ │"},
    {SYM_CUP1_L1, "SYM_CUP1_L1", "Big Operators - Union (⋃)", "|   |", "│ │"},
    {SYM_CUP1_L2, "SYM_CUP1_L2", "Big Operators - Union (⋃)", " \\_/", "╰─╯"},
    {SYM_CUP2_L0, "SYM_CUP2_L0", "Big Operators - Union (⋃)", "|    |", "│   │"},
    {SYM_CUP2_L1, "SYM_CUP2_L1", "Big Operators - Union (⋃)", "|    |", "│   │"},
    {SYM_CUP2_L2, "SYM_CUP2_L2", "Big Operators - Union (⋃)", "|    |", "│   │"},
    {SYM_CUP2_L3, "SYM_CUP2_L3", "Big Operators - Union (⋃)", " \\  /", "│   │"},
    {SYM_CUP2_L4, "SYM_CUP2_L4", "Big Operators - Union (⋃)", "  \\/", "╰───╯"},
    {SYM_CUP3_L0, "SYM_CUP3_L0", "Big Operators - Union (⋃)", "|      |", "│     │"},
    {SYM_CUP3_L1, "SYM_CUP3_L1", "Big Operators - Union (⋃)", "|      |", "│     │"},
    {SYM_CUP3_L2, "SYM_CUP3_L2", "Big Operators - Union (⋃)", "|      |", "│     │"},
    {SYM_CUP3_L3, "SYM_CUP3_L3", "Big Operators - Union (⋃)", "|      |", "│     │"},
    {SYM_CUP3_L4, "SYM_CUP3_L4", "Big Operators - Union (⋃)", "|      |", "│     │"},
    {SYM_CUP3_L5, "SYM_CUP3_L5", "Big Operators - Union (⋃)", " \\    /", "│     │"},
    {SYM_CUP3_L6, "SYM_CUP3_L6", "Big Operators - Union (⋃)", "  \\__/", "╰─────╯"},

    // ========== Big Operators - Intersection (⋂) ==========
    {SYM_CAP1_L0, "SYM_CAP1_L0", "Big Operators - Intersection (⋂)", " / \\", "╭─╮"},
    {SYM_CAP1_L1, "SYM_CAP1_L1", "Big Operators - Intersection (⋂)", "|   |", "│ │"},
    {SYM_CAP1_L2, "SYM_CAP1_L2", "Big Operators - Intersection (⋂)", "|   |", "│ │"},
    {SYM_CAP2_L0, "SYM_CAP2_L0", "Big Operators - Intersection (⋂)", "  __ ", "╭───╮"},
    {SYM_CAP2_L1, "SYM_CAP2_L1", "Big Operators - Intersection (⋂)", " /  \\", "│   │"},
    {SYM_CAP2_L2, "SYM_CAP2_L2", "Big Operators - Intersection (⋂)", "|    |", "│   │"},
    {SYM_CAP2_L3, "SYM_CAP2_L3", "Big Operators - Intersection (⋂)", "|    |", "│   │"},
    {SYM_CAP2_L4, "SYM_CAP2_L4", "Big Operators - Intersection (⋂)", "|    |", "│   │"},
    {SYM_CAP3_L0, "SYM_CAP3_L0", "Big Operators - Intersection (⋂)", "  ____  ", "╭─────╮"},
    {SYM_CAP3_L1, "SYM_CAP3_L1", "Big Operators - Intersection (⋂)", " /    \\", "│     │"},
    {SYM_CAP3_L2, "SYM_CAP3_L2", "Big Operators - Intersection (⋂)", "|      |", "│     │"},
    {SYM_CAP3_L3, "SYM_CAP3_L3", "Big Operators - Intersection (⋂)", "|      |", "│     │"},
    {SYM_CAP3_L4, "SYM_CAP3_L4", "Big Operators - Intersection (⋂)", "|      |", "│     │"},
    {SYM_CAP3_L5, "SYM_CAP3_L5", "Big Operators - Intersection (⋂)", "|      |", "│     │"},
    {SYM_CAP3_L6, "SYM_CAP3_L6", "Big Operators - Intersection (⋂)", "|      |", "│     │"},

    // ========== Delimiters - Parentheses (ASCII) ==========
    {SYM_PAREN_ASCII_3_L_L0, "SYM_PAREN_ASCII_3_L_L0", "Delimiters - Parentheses (ASCII)", "/", ""},
    {SYM_PAREN_ASCII_3_L_L1, "SYM_PAREN_ASCII_3_L_L1", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_3_L_L2, "SYM_PAREN_ASCII_3_L_L2", "Delimiters - Parentheses (ASCII)", "\\",
     ""},
    {SYM_PAREN_ASCII_3_R_L0, "SYM_PAREN_ASCII_3_R_L0", "Delimiters - Parentheses (ASCII)", "\\",
     ""},
    {SYM_PAREN_ASCII_3_R_L1, "SYM_PAREN_ASCII_3_R_L1", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_3_R_L2, "SYM_PAREN_ASCII_3_R_L2", "Delimiters - Parentheses (ASCII)", "/", ""},
    {SYM_PAREN_ASCII_5_L_L0, "SYM_PAREN_ASCII_5_L_L0", "Delimiters - Parentheses (ASCII)", "/", ""},
    {SYM_PAREN_ASCII_5_L_L1, "SYM_PAREN_ASCII_5_L_L1", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_5_L_L2, "SYM_PAREN_ASCII_5_L_L2", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_5_L_L3, "SYM_PAREN_ASCII_5_L_L3", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_5_L_L4, "SYM_PAREN_ASCII_5_L_L4", "Delimiters - Parentheses (ASCII)", "\\",
     ""},
    {SYM_PAREN_ASCII_5_R_L0, "SYM_PAREN_ASCII_5_R_L0", "Delimiters - Parentheses (ASCII)", "\\",
     ""},
    {SYM_PAREN_ASCII_5_R_L1, "SYM_PAREN_ASCII_5_R_L1", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_5_R_L2, "SYM_PAREN_ASCII_5_R_L2", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_5_R_L3, "SYM_PAREN_ASCII_5_R_L3", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_5_R_L4, "SYM_PAREN_ASCII_5_R_L4", "Delimiters - Parentheses (ASCII)", "/", ""},
    {SYM_PAREN_ASCII_7_L_L0, "SYM_PAREN_ASCII_7_L_L0", "Delimiters - Parentheses (ASCII)", "/", ""},
    {SYM_PAREN_ASCII_7_L_L1, "SYM_PAREN_ASCII_7_L_L1", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_7_L_L2, "SYM_PAREN_ASCII_7_L_L2", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_7_L_L3, "SYM_PAREN_ASCII_7_L_L3", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_7_L_L4, "SYM_PAREN_ASCII_7_L_L4", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_7_L_L5, "SYM_PAREN_ASCII_7_L_L5", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_7_L_L6, "SYM_PAREN_ASCII_7_L_L6", "Delimiters - Parentheses (ASCII)", "\\",
     ""},
    {SYM_PAREN_ASCII_7_R_L0, "SYM_PAREN_ASCII_7_R_L0", "Delimiters - Parentheses (ASCII)", "\\",
     ""},
    {SYM_PAREN_ASCII_7_R_L1, "SYM_PAREN_ASCII_7_R_L1", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_7_R_L2, "SYM_PAREN_ASCII_7_R_L2", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_7_R_L3, "SYM_PAREN_ASCII_7_R_L3", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_7_R_L4, "SYM_PAREN_ASCII_7_R_L4", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_7_R_L5, "SYM_PAREN_ASCII_7_R_L5", "Delimiters - Parentheses (ASCII)", "|", ""},
    {SYM_PAREN_ASCII_7_R_L6, "SYM_PAREN_ASCII_7_R_L6", "Delimiters - Parentheses (ASCII)", "/", ""},

    // ========== Delimiters - Brackets (ASCII) ==========
    {SYM_BRACKET_ASCII_3_L_L0, "SYM_BRACKET_ASCII_3_L_L0", "Delimiters - Brackets (ASCII)", "+-",
     ""},
    {SYM_BRACKET_ASCII_3_L_L1, "SYM_BRACKET_ASCII_3_L_L1", "Delimiters - Brackets (ASCII)", "|",
     ""},
    {SYM_BRACKET_ASCII_3_L_L2, "SYM_BRACKET_ASCII_3_L_L2", "Delimiters - Brackets (ASCII)", "+-",
     ""},
    {SYM_BRACKET_ASCII_3_R_L0, "SYM_BRACKET_ASCII_3_R_L0", "Delimiters - Brackets (ASCII)", "-+",
     ""},
    {SYM_BRACKET_ASCII_3_R_L1, "SYM_BRACKET_ASCII_3_R_L1", "Delimiters - Brackets (ASCII)", " |",
     ""},
    {SYM_BRACKET_ASCII_3_R_L2, "SYM_BRACKET_ASCII_3_R_L2", "Delimiters - Brackets (ASCII)", "-+",
     ""},
    {SYM_BRACKET_ASCII_5_L_L0, "SYM_BRACKET_ASCII_5_L_L0", "Delimiters - Brackets (ASCII)", "+-",
     ""},
    {SYM_BRACKET_ASCII_5_L_L1, "SYM_BRACKET_ASCII_5_L_L1", "Delimiters - Brackets (ASCII)", "|",
     ""},
    {SYM_BRACKET_ASCII_5_L_L2, "SYM_BRACKET_ASCII_5_L_L2", "Delimiters - Brackets (ASCII)", "|",
     ""},
    {SYM_BRACKET_ASCII_5_L_L3, "SYM_BRACKET_ASCII_5_L_L3", "Delimiters - Brackets (ASCII)", "|",
     ""},
    {SYM_BRACKET_ASCII_5_L_L4, "SYM_BRACKET_ASCII_5_L_L4", "Delimiters - Brackets (ASCII)", "+-",
     ""},
    {SYM_BRACKET_ASCII_5_R_L0, "SYM_BRACKET_ASCII_5_R_L0", "Delimiters - Brackets (ASCII)", "-+",
     ""},
    {SYM_BRACKET_ASCII_5_R_L1, "SYM_BRACKET_ASCII_5_R_L1", "Delimiters - Brackets (ASCII)", " |",
     ""},
    {SYM_BRACKET_ASCII_5_R_L2, "SYM_BRACKET_ASCII_5_R_L2", "Delimiters - Brackets (ASCII)", " |",
     ""},
    {SYM_BRACKET_ASCII_5_R_L3, "SYM_BRACKET_ASCII_5_R_L3", "Delimiters - Brackets (ASCII)", " |",
     ""},
    {SYM_BRACKET_ASCII_5_R_L4, "SYM_BRACKET_ASCII_5_R_L4", "Delimiters - Brackets (ASCII)", "-+",
     ""},
    {SYM_BRACKET_ASCII_7_L_L0, "SYM_BRACKET_ASCII_7_L_L0", "Delimiters - Brackets (ASCII)", "+-",
     ""},
    {SYM_BRACKET_ASCII_7_L_L1, "SYM_BRACKET_ASCII_7_L_L1", "Delimiters - Brackets (ASCII)", "|",
     ""},
    {SYM_BRACKET_ASCII_7_L_L2, "SYM_BRACKET_ASCII_7_L_L2", "Delimiters - Brackets (ASCII)", "|",
     ""},
    {SYM_BRACKET_ASCII_7_L_L3, "SYM_BRACKET_ASCII_7_L_L3", "Delimiters - Brackets (ASCII)", "|",
     ""},
    {SYM_BRACKET_ASCII_7_L_L4, "SYM_BRACKET_ASCII_7_L_L4", "Delimiters - Brackets (ASCII)", "|",
     ""},
    {SYM_BRACKET_ASCII_7_L_L5, "SYM_BRACKET_ASCII_7_L_L5", "Delimiters - Brackets (ASCII)", "|",
     ""},
    {SYM_BRACKET_ASCII_7_L_L6, "SYM_BRACKET_ASCII_7_L_L6", "Delimiters - Brackets (ASCII)", "+-",
     ""},
    {SYM_BRACKET_ASCII_7_R_L0, "SYM_BRACKET_ASCII_7_R_L0", "Delimiters - Brackets (ASCII)", "-+",
     ""},
    {SYM_BRACKET_ASCII_7_R_L1, "SYM_BRACKET_ASCII_7_R_L1", "Delimiters - Brackets (ASCII)", " |",
     ""},
    {SYM_BRACKET_ASCII_7_R_L2, "SYM_BRACKET_ASCII_7_R_L2", "Delimiters - Brackets (ASCII)", " |",
     ""},
    {SYM_BRACKET_ASCII_7_R_L3, "SYM_BRACKET_ASCII_7_R_L3", "Delimiters - Brackets (ASCII)", " |",
     ""},
    {SYM_BRACKET_ASCII_7_R_L4, "SYM_BRACKET_ASCII_7_R_L4", "Delimiters - Brackets (ASCII)", " |",
     ""},
    {SYM_BRACKET_ASCII_7_R_L5, "SYM_BRACKET_ASCII_7_R_L5", "Delimiters - Brackets (ASCII)", " |",
     ""},
    {SYM_BRACKET_ASCII_7_R_L6, "SYM_BRACKET_ASCII_7_R_L6", "Delimiters - Brackets (ASCII)", "-+",
     ""},

    // ========== Delimiters - Braces (ASCII) ==========
    {SYM_BRACE_ASCII_3_L_L0, "SYM_BRACE_ASCII_3_L_L0", "Delimiters - Braces (ASCII)", "/ ", ""},
    {SYM_BRACE_ASCII_3_L_L1, "SYM_BRACE_ASCII_3_L_L1", "Delimiters - Braces (ASCII)", "{ ", ""},
    {SYM_BRACE_ASCII_3_L_L2, "SYM_BRACE_ASCII_3_L_L2", "Delimiters - Braces (ASCII)", "\\ ", ""},
    {SYM_BRACE_ASCII_3_R_L0, "SYM_BRACE_ASCII_3_R_L0", "Delimiters - Braces (ASCII)", " \\", ""},
    {SYM_BRACE_ASCII_3_R_L1, "SYM_BRACE_ASCII_3_R_L1", "Delimiters - Braces (ASCII)", " }", ""},
    {SYM_BRACE_ASCII_3_R_L2, "SYM_BRACE_ASCII_3_R_L2", "Delimiters - Braces (ASCII)", " /", ""},
    {SYM_BRACE_ASCII_5_L_L0, "SYM_BRACE_ASCII_5_L_L0", "Delimiters - Braces (ASCII)", "/ ", ""},
    {SYM_BRACE_ASCII_5_L_L1, "SYM_BRACE_ASCII_5_L_L1", "Delimiters - Braces (ASCII)", "\\ ", ""},
    {SYM_BRACE_ASCII_5_L_L2, "SYM_BRACE_ASCII_5_L_L2", "Delimiters - Braces (ASCII)", "{ ", ""},
    {SYM_BRACE_ASCII_5_L_L3, "SYM_BRACE_ASCII_5_L_L3", "Delimiters - Braces (ASCII)", "/ ", ""},
    {SYM_BRACE_ASCII_5_L_L4, "SYM_BRACE_ASCII_5_L_L4", "Delimiters - Braces (ASCII)", "\\ ", ""},
    {SYM_BRACE_ASCII_5_R_L0, "SYM_BRACE_ASCII_5_R_L0", "Delimiters - Braces (ASCII)", " \\", ""},
    {SYM_BRACE_ASCII_5_R_L1, "SYM_BRACE_ASCII_5_R_L1", "Delimiters - Braces (ASCII)", " /", ""},
    {SYM_BRACE_ASCII_5_R_L2, "SYM_BRACE_ASCII_5_R_L2", "Delimiters - Braces (ASCII)", " }", ""},
    {SYM_BRACE_ASCII_5_R_L3, "SYM_BRACE_ASCII_5_R_L3", "Delimiters - Braces (ASCII)", " \\", ""},
    {SYM_BRACE_ASCII_5_R_L4, "SYM_BRACE_ASCII_5_R_L4", "Delimiters - Braces (ASCII)", " /", ""},
    {SYM_BRACE_ASCII_7_L_L0, "SYM_BRACE_ASCII_7_L_L0", "Delimiters - Braces (ASCII)", "/ ", ""},
    {SYM_BRACE_ASCII_7_L_L1, "SYM_BRACE_ASCII_7_L_L1", "Delimiters - Braces (ASCII)", "| ", ""},
    {SYM_BRACE_ASCII_7_L_L2, "SYM_BRACE_ASCII_7_L_L2", "Delimiters - Braces (ASCII)", "\\ ", ""},
    {SYM_BRACE_ASCII_7_L_L3, "SYM_BRACE_ASCII_7_L_L3", "Delimiters - Braces (ASCII)", "{ ", ""},
    {SYM_BRACE_ASCII_7_L_L4, "SYM_BRACE_ASCII_7_L_L4", "Delimiters - Braces (ASCII)", "/ ", ""},
    {SYM_BRACE_ASCII_7_L_L5, "SYM_BRACE_ASCII_7_L_L5", "Delimiters - Braces (ASCII)", "| ", ""},
    {SYM_BRACE_ASCII_7_L_L6, "SYM_BRACE_ASCII_7_L_L6", "Delimiters - Braces (ASCII)", "\\ ", ""},
    {SYM_BRACE_ASCII_7_R_L0, "SYM_BRACE_ASCII_7_R_L0", "Delimiters - Braces (ASCII)", " \\", ""},
    {SYM_BRACE_ASCII_7_R_L1, "SYM_BRACE_ASCII_7_R_L1", "Delimiters - Braces (ASCII)", " |", ""},
    {SYM_BRACE_ASCII_7_R_L2, "SYM_BRACE_ASCII_7_R_L2", "Delimiters - Braces (ASCII)", " /", ""},
    {SYM_BRACE_ASCII_7_R_L3, "SYM_BRACE_ASCII_7_R_L3", "Delimiters - Braces (ASCII)", " }", ""},
    {SYM_BRACE_ASCII_7_R_L4, "SYM_BRACE_ASCII_7_R_L4", "Delimiters - Braces (ASCII)", " \\", ""},
    {SYM_BRACE_ASCII_7_R_L5, "SYM_BRACE_ASCII_7_R_L5", "Delimiters - Braces (ASCII)", " |", ""},
    {SYM_BRACE_ASCII_7_R_L6, "SYM_BRACE_ASCII_7_R_L6", "Delimiters - Braces (ASCII)", " /", ""},

    // ========== Vertical Bars ==========
    // Structural (box-drawing, full height - for absolute value, evaluation bar)
    {SYM_VERT_SINGLE, "SYM_VERT_SINGLE", "Vertical Bars", "|", "│"},
    {SYM_VERT_DOUBLE, "SYM_VERT_DOUBLE", "Vertical Bars", "||", "║"},
    // Mathematical (plain pipe - for bra-ket, "such that", single-line absolute value)
    // Uses plain | which is visually shorter than box-drawing │ and won't form junctions
    {SYM_DIVIDES, "SYM_DIVIDES", "Vertical Bars", "|", "|"},
    // Note: SYM_PARALLEL (∥) already defined in Miscellaneous Symbols section

    // ========== Structural Symbols ==========
    {SYM_LFLOOR, "SYM_LFLOOR", "Structural Symbols", "|_", "⌊"},
    {SYM_RFLOOR, "SYM_RFLOOR", "Structural Symbols", "_|", "⌋"},
    {SYM_LCEIL, "SYM_LCEIL", "Structural Symbols", "|-", "⌈"},
    {SYM_RCEIL, "SYM_RCEIL", "Structural Symbols", "-|", "⌉"},

    // ========== Delimiters - Parentheses (Unicode) ==========
    {SYM_PAREN_UNI_3_L_L0, "SYM_PAREN_UNI_3_L_L0", "Delimiters - Parentheses (Unicode)", "", "⎛"},
    {SYM_PAREN_UNI_3_L_L1, "SYM_PAREN_UNI_3_L_L1", "Delimiters - Parentheses (Unicode)", "", "⎜"},
    {SYM_PAREN_UNI_3_L_L2, "SYM_PAREN_UNI_3_L_L2", "Delimiters - Parentheses (Unicode)", "", "⎝"},
    {SYM_PAREN_UNI_3_R_L0, "SYM_PAREN_UNI_3_R_L0", "Delimiters - Parentheses (Unicode)", "", "⎞"},
    {SYM_PAREN_UNI_3_R_L1, "SYM_PAREN_UNI_3_R_L1", "Delimiters - Parentheses (Unicode)", "", "⎟"},
    {SYM_PAREN_UNI_3_R_L2, "SYM_PAREN_UNI_3_R_L2", "Delimiters - Parentheses (Unicode)", "", "⎠"},
    {SYM_PAREN_UNI_5_L_L0, "SYM_PAREN_UNI_5_L_L0", "Delimiters - Parentheses (Unicode)", "", "⎛"},
    {SYM_PAREN_UNI_5_L_L1, "SYM_PAREN_UNI_5_L_L1", "Delimiters - Parentheses (Unicode)", "", "⎜"},
    {SYM_PAREN_UNI_5_L_L2, "SYM_PAREN_UNI_5_L_L2", "Delimiters - Parentheses (Unicode)", "", "⎜"},
    {SYM_PAREN_UNI_5_L_L3, "SYM_PAREN_UNI_5_L_L3", "Delimiters - Parentheses (Unicode)", "", "⎜"},
    {SYM_PAREN_UNI_5_L_L4, "SYM_PAREN_UNI_5_L_L4", "Delimiters - Parentheses (Unicode)", "", "⎝"},
    {SYM_PAREN_UNI_5_R_L0, "SYM_PAREN_UNI_5_R_L0", "Delimiters - Parentheses (Unicode)", "", "⎞"},
    {SYM_PAREN_UNI_5_R_L1, "SYM_PAREN_UNI_5_R_L1", "Delimiters - Parentheses (Unicode)", "", "⎟"},
    {SYM_PAREN_UNI_5_R_L2, "SYM_PAREN_UNI_5_R_L2", "Delimiters - Parentheses (Unicode)", "", "⎟"},
    {SYM_PAREN_UNI_5_R_L3, "SYM_PAREN_UNI_5_R_L3", "Delimiters - Parentheses (Unicode)", "", "⎟"},
    {SYM_PAREN_UNI_5_R_L4, "SYM_PAREN_UNI_5_R_L4", "Delimiters - Parentheses (Unicode)", "", "⎠"},
    {SYM_PAREN_UNI_7_L_L0, "SYM_PAREN_UNI_7_L_L0", "Delimiters - Parentheses (Unicode)", "", "⎛"},
    {SYM_PAREN_UNI_7_L_L1, "SYM_PAREN_UNI_7_L_L1", "Delimiters - Parentheses (Unicode)", "", "⎜"},
    {SYM_PAREN_UNI_7_L_L2, "SYM_PAREN_UNI_7_L_L2", "Delimiters - Parentheses (Unicode)", "", "⎜"},
    {SYM_PAREN_UNI_7_L_L3, "SYM_PAREN_UNI_7_L_L3", "Delimiters - Parentheses (Unicode)", "", "⎜"},
    {SYM_PAREN_UNI_7_L_L4, "SYM_PAREN_UNI_7_L_L4", "Delimiters - Parentheses (Unicode)", "", "⎜"},
    {SYM_PAREN_UNI_7_L_L5, "SYM_PAREN_UNI_7_L_L5", "Delimiters - Parentheses (Unicode)", "", "⎜"},
    {SYM_PAREN_UNI_7_L_L6, "SYM_PAREN_UNI_7_L_L6", "Delimiters - Parentheses (Unicode)", "", "⎝"},
    {SYM_PAREN_UNI_7_R_L0, "SYM_PAREN_UNI_7_R_L0", "Delimiters - Parentheses (Unicode)", "", "⎞"},
    {SYM_PAREN_UNI_7_R_L1, "SYM_PAREN_UNI_7_R_L1", "Delimiters - Parentheses (Unicode)", "", "⎟"},
    {SYM_PAREN_UNI_7_R_L2, "SYM_PAREN_UNI_7_R_L2", "Delimiters - Parentheses (Unicode)", "", "⎟"},
    {SYM_PAREN_UNI_7_R_L3, "SYM_PAREN_UNI_7_R_L3", "Delimiters - Parentheses (Unicode)", "", "⎟"},
    {SYM_PAREN_UNI_7_R_L4, "SYM_PAREN_UNI_7_R_L4", "Delimiters - Parentheses (Unicode)", "", "⎟"},
    {SYM_PAREN_UNI_7_R_L5, "SYM_PAREN_UNI_7_R_L5", "Delimiters - Parentheses (Unicode)", "", "⎟"},
    {SYM_PAREN_UNI_7_R_L6, "SYM_PAREN_UNI_7_R_L6", "Delimiters - Parentheses (Unicode)", "", "⎠"},

    // ========== Delimiters - Brackets (Unicode) ==========
    {SYM_BRACKET_UNI_3_L_L0, "SYM_BRACKET_UNI_3_L_L0", "Delimiters - Brackets (Unicode)", "", "⎡"},
    {SYM_BRACKET_UNI_3_L_L1, "SYM_BRACKET_UNI_3_L_L1", "Delimiters - Brackets (Unicode)", "", "⎢"},
    {SYM_BRACKET_UNI_3_L_L2, "SYM_BRACKET_UNI_3_L_L2", "Delimiters - Brackets (Unicode)", "", "⎣"},
    {SYM_BRACKET_UNI_3_R_L0, "SYM_BRACKET_UNI_3_R_L0", "Delimiters - Brackets (Unicode)", "", "⎤"},
    {SYM_BRACKET_UNI_3_R_L1, "SYM_BRACKET_UNI_3_R_L1", "Delimiters - Brackets (Unicode)", "", "⎥"},
    {SYM_BRACKET_UNI_3_R_L2, "SYM_BRACKET_UNI_3_R_L2", "Delimiters - Brackets (Unicode)", "", "⎦"},
    {SYM_BRACKET_UNI_5_L_L0, "SYM_BRACKET_UNI_5_L_L0", "Delimiters - Brackets (Unicode)", "", "⎡"},
    {SYM_BRACKET_UNI_5_L_L1, "SYM_BRACKET_UNI_5_L_L1", "Delimiters - Brackets (Unicode)", "", "⎢"},
    {SYM_BRACKET_UNI_5_L_L2, "SYM_BRACKET_UNI_5_L_L2", "Delimiters - Brackets (Unicode)", "", "⎢"},
    {SYM_BRACKET_UNI_5_L_L3, "SYM_BRACKET_UNI_5_L_L3", "Delimiters - Brackets (Unicode)", "", "⎢"},
    {SYM_BRACKET_UNI_5_L_L4, "SYM_BRACKET_UNI_5_L_L4", "Delimiters - Brackets (Unicode)", "", "⎣"},
    {SYM_BRACKET_UNI_5_R_L0, "SYM_BRACKET_UNI_5_R_L0", "Delimiters - Brackets (Unicode)", "", "⎤"},
    {SYM_BRACKET_UNI_5_R_L1, "SYM_BRACKET_UNI_5_R_L1", "Delimiters - Brackets (Unicode)", "", "⎥"},
    {SYM_BRACKET_UNI_5_R_L2, "SYM_BRACKET_UNI_5_R_L2", "Delimiters - Brackets (Unicode)", "", "⎥"},
    {SYM_BRACKET_UNI_5_R_L3, "SYM_BRACKET_UNI_5_R_L3", "Delimiters - Brackets (Unicode)", "", "⎥"},
    {SYM_BRACKET_UNI_5_R_L4, "SYM_BRACKET_UNI_5_R_L4", "Delimiters - Brackets (Unicode)", "", "⎦"},
    {SYM_BRACKET_UNI_7_L_L0, "SYM_BRACKET_UNI_7_L_L0", "Delimiters - Brackets (Unicode)", "", "⎡"},
    {SYM_BRACKET_UNI_7_L_L1, "SYM_BRACKET_UNI_7_L_L1", "Delimiters - Brackets (Unicode)", "", "⎢"},
    {SYM_BRACKET_UNI_7_L_L2, "SYM_BRACKET_UNI_7_L_L2", "Delimiters - Brackets (Unicode)", "", "⎢"},
    {SYM_BRACKET_UNI_7_L_L3, "SYM_BRACKET_UNI_7_L_L3", "Delimiters - Brackets (Unicode)", "", "⎢"},
    {SYM_BRACKET_UNI_7_L_L4, "SYM_BRACKET_UNI_7_L_L4", "Delimiters - Brackets (Unicode)", "", "⎢"},
    {SYM_BRACKET_UNI_7_L_L5, "SYM_BRACKET_UNI_7_L_L5", "Delimiters - Brackets (Unicode)", "", "⎢"},
    {SYM_BRACKET_UNI_7_L_L6, "SYM_BRACKET_UNI_7_L_L6", "Delimiters - Brackets (Unicode)", "", "⎣"},
    {SYM_BRACKET_UNI_7_R_L0, "SYM_BRACKET_UNI_7_R_L0", "Delimiters - Brackets (Unicode)", "", "⎤"},
    {SYM_BRACKET_UNI_7_R_L1, "SYM_BRACKET_UNI_7_R_L1", "Delimiters - Brackets (Unicode)", "", "⎥"},
    {SYM_BRACKET_UNI_7_R_L2, "SYM_BRACKET_UNI_7_R_L2", "Delimiters - Brackets (Unicode)", "", "⎥"},
    {SYM_BRACKET_UNI_7_R_L3, "SYM_BRACKET_UNI_7_R_L3", "Delimiters - Brackets (Unicode)", "", "⎥"},
    {SYM_BRACKET_UNI_7_R_L4, "SYM_BRACKET_UNI_7_R_L4", "Delimiters - Brackets (Unicode)", "", "⎥"},
    {SYM_BRACKET_UNI_7_R_L5, "SYM_BRACKET_UNI_7_R_L5", "Delimiters - Brackets (Unicode)", "", "⎥"},
    {SYM_BRACKET_UNI_7_R_L6, "SYM_BRACKET_UNI_7_R_L6", "Delimiters - Brackets (Unicode)", "", "⎦"},

    // ========== Delimiters - Braces (Unicode) ==========
    {SYM_BRACE_UNI_3_L_L0, "SYM_BRACE_UNI_3_L_L0", "Delimiters - Braces (Unicode)", "", "⎧"},
    {SYM_BRACE_UNI_3_L_L1, "SYM_BRACE_UNI_3_L_L1", "Delimiters - Braces (Unicode)", "", "⎨"},
    {SYM_BRACE_UNI_3_L_L2, "SYM_BRACE_UNI_3_L_L2", "Delimiters - Braces (Unicode)", "", "⎩"},
    {SYM_BRACE_UNI_3_R_L0, "SYM_BRACE_UNI_3_R_L0", "Delimiters - Braces (Unicode)", "", "⎫"},
    {SYM_BRACE_UNI_3_R_L1, "SYM_BRACE_UNI_3_R_L1", "Delimiters - Braces (Unicode)", "", "⎬"},
    {SYM_BRACE_UNI_3_R_L2, "SYM_BRACE_UNI_3_R_L2", "Delimiters - Braces (Unicode)", "", "⎭"},
    {SYM_BRACE_UNI_5_L_L0, "SYM_BRACE_UNI_5_L_L0", "Delimiters - Braces (Unicode)", "", "⎧"},
    {SYM_BRACE_UNI_5_L_L1, "SYM_BRACE_UNI_5_L_L1", "Delimiters - Braces (Unicode)", "", "⎨"},
    {SYM_BRACE_UNI_5_L_L2, "SYM_BRACE_UNI_5_L_L2", "Delimiters - Braces (Unicode)", "", "⎨"},
    {SYM_BRACE_UNI_5_L_L3, "SYM_BRACE_UNI_5_L_L3", "Delimiters - Braces (Unicode)", "", "⎨"},
    {SYM_BRACE_UNI_5_L_L4, "SYM_BRACE_UNI_5_L_L4", "Delimiters - Braces (Unicode)", "", "⎩"},
    {SYM_BRACE_UNI_5_R_L0, "SYM_BRACE_UNI_5_R_L0", "Delimiters - Braces (Unicode)", "", "⎫"},
    {SYM_BRACE_UNI_5_R_L1, "SYM_BRACE_UNI_5_R_L1", "Delimiters - Braces (Unicode)", "", "⎬"},
    {SYM_BRACE_UNI_5_R_L2, "SYM_BRACE_UNI_5_R_L2", "Delimiters - Braces (Unicode)", "", "⎬"},
    {SYM_BRACE_UNI_5_R_L3, "SYM_BRACE_UNI_5_R_L3", "Delimiters - Braces (Unicode)", "", "⎬"},
    {SYM_BRACE_UNI_5_R_L4, "SYM_BRACE_UNI_5_R_L4", "Delimiters - Braces (Unicode)", "", "⎭"},
    {SYM_BRACE_UNI_7_L_L0, "SYM_BRACE_UNI_7_L_L0", "Delimiters - Braces (Unicode)", "", "⎧"},
    {SYM_BRACE_UNI_7_L_L1, "SYM_BRACE_UNI_7_L_L1", "Delimiters - Braces (Unicode)", "", "⎨"},
    {SYM_BRACE_UNI_7_L_L2, "SYM_BRACE_UNI_7_L_L2", "Delimiters - Braces (Unicode)", "", "⎨"},
    {SYM_BRACE_UNI_7_L_L3, "SYM_BRACE_UNI_7_L_L3", "Delimiters - Braces (Unicode)", "", "⎨"},
    {SYM_BRACE_UNI_7_L_L4, "SYM_BRACE_UNI_7_L_L4", "Delimiters - Braces (Unicode)", "", "⎨"},
    {SYM_BRACE_UNI_7_L_L5, "SYM_BRACE_UNI_7_L_L5", "Delimiters - Braces (Unicode)", "", "⎨"},
    {SYM_BRACE_UNI_7_L_L6, "SYM_BRACE_UNI_7_L_L6", "Delimiters - Braces (Unicode)", "", "⎩"},
    {SYM_BRACE_UNI_7_R_L0, "SYM_BRACE_UNI_7_R_L0", "Delimiters - Braces (Unicode)", "", "⎫"},
    {SYM_BRACE_UNI_7_R_L1, "SYM_BRACE_UNI_7_R_L1", "Delimiters - Braces (Unicode)", "", "⎬"},
    {SYM_BRACE_UNI_7_R_L2, "SYM_BRACE_UNI_7_R_L2", "Delimiters - Braces (Unicode)", "", "⎬"},
    {SYM_BRACE_UNI_7_R_L3, "SYM_BRACE_UNI_7_R_L3", "Delimiters - Braces (Unicode)", "", "⎬"},
    {SYM_BRACE_UNI_7_R_L4, "SYM_BRACE_UNI_7_R_L4", "Delimiters - Braces (Unicode)", "", "⎬"},
    {SYM_BRACE_UNI_7_R_L5, "SYM_BRACE_UNI_7_R_L5", "Delimiters - Braces (Unicode)", "", "⎬"},
    {SYM_BRACE_UNI_7_R_L6, "SYM_BRACE_UNI_7_R_L6", "Delimiters - Braces (Unicode)", "", "⎭"},

    // ========== Misc ==========
    {SYM_BLACKSQUARE, "SYM_BLACKSQUARE", "Geometry", "#", "■"},
    {SYM_THEREFORE, "SYM_THEREFORE", "Logic", ".:.", "\xe2\x88\xb4"},          // ∴
    {SYM_BECAUSE, "SYM_BECAUSE", "Logic", ".:.", "\xe2\x88\xb5"},              // ∵
    {SYM_PREC, "SYM_PREC", "Relations", "<", "\xe2\x89\xba"},                  // ≺
    {SYM_SUCC, "SYM_SUCC", "Relations", ">", "\xe2\x89\xbb"},                  // ≻
    {SYM_PRECEQ, "SYM_PRECEQ", "Relations", "<=", "\xe2\xaa\xaf"},             // ⪯
    {SYM_SUCCEQ, "SYM_SUCCEQ", "Relations", ">=", "\xe2\xaa\xb0"},             // ⪰
    {SYM_WP, "SYM_WP", "Miscellaneous", "P", "\xe2\x84\x98"},                  // ℘
    {SYM_IMATH, "SYM_IMATH", "Miscellaneous", "i", "\xc4\xb1"},                // ı
    {SYM_JMATH, "SYM_JMATH", "Miscellaneous", "j", "\xc8\xb7"},                // ȷ
    {SYM_NLEQ, "SYM_NLEQ", "Negated Relations", "/<", "\xe2\x89\xb0"},         // ≰
    {SYM_NGEQ, "SYM_NGEQ", "Negated Relations", "/>", "\xe2\x89\xb1"},         // ≱
    {SYM_NSUBSET, "SYM_NSUBSET", "Negated Relations", "/sub", "\xe2\x8a\x84"}, // ⊄
    {SYM_NEQUIV, "SYM_NEQUIV", "Negated Relations", "=/=", "\xe2\x89\xa2"},    // ≢

    // Wide Accent Characters
    {SYM_WIDEHAT_LEFT, "SYM_WIDEHAT_LEFT", "Wide Accents", "/", "\xe2\x95\xb1"},     // ╱
    {SYM_WIDEHAT_RIGHT, "SYM_WIDEHAT_RIGHT", "Wide Accents", "\\", "\xe2\x95\xb2"},  // ╲
    {SYM_WIDEHAT_FILL, "SYM_WIDEHAT_FILL", "Wide Accents", "-", "⎺"},                // ‾
    {SYM_WIDETILDE_FILL, "SYM_WIDETILDE_FILL", "Wide Accents", "~", "\xe2\x88\xbc"}, // ∼

    // ========== Delimiter Pieces (extensible building blocks) ==========
    // Round parentheses (Unicode)
    {SYM_PAREN_UNI_L_TOP, "SYM_PAREN_UNI_L_TOP", "Delimiter Pieces", "", "\xe2\x8e\x9b"}, // ⎛
    {SYM_PAREN_UNI_L_EXT, "SYM_PAREN_UNI_L_EXT", "Delimiter Pieces", "", "\xe2\x8e\x9c"}, // ⎜
    {SYM_PAREN_UNI_L_BOT, "SYM_PAREN_UNI_L_BOT", "Delimiter Pieces", "", "\xe2\x8e\x9d"}, // ⎝
    {SYM_PAREN_UNI_R_TOP, "SYM_PAREN_UNI_R_TOP", "Delimiter Pieces", "", "\xe2\x8e\x9e"}, // ⎞
    {SYM_PAREN_UNI_R_EXT, "SYM_PAREN_UNI_R_EXT", "Delimiter Pieces", "", "\xe2\x8e\x9f"}, // ⎟
    {SYM_PAREN_UNI_R_BOT, "SYM_PAREN_UNI_R_BOT", "Delimiter Pieces", "", "\xe2\x8e\xa0"}, // ⎠
    // Round parentheses (ASCII)
    {SYM_PAREN_ASCII_L_TOP, "SYM_PAREN_ASCII_L_TOP", "Delimiter Pieces", "/", ""},
    {SYM_PAREN_ASCII_L_EXT, "SYM_PAREN_ASCII_L_EXT", "Delimiter Pieces", "|", ""},
    {SYM_PAREN_ASCII_L_BOT, "SYM_PAREN_ASCII_L_BOT", "Delimiter Pieces", "\\", ""},
    {SYM_PAREN_ASCII_R_TOP, "SYM_PAREN_ASCII_R_TOP", "Delimiter Pieces", "\\", ""},
    {SYM_PAREN_ASCII_R_EXT, "SYM_PAREN_ASCII_R_EXT", "Delimiter Pieces", "|", ""},
    {SYM_PAREN_ASCII_R_BOT, "SYM_PAREN_ASCII_R_BOT", "Delimiter Pieces", "/", ""},
    // Square brackets (Unicode)
    {SYM_BRACKET_UNI_L_TOP, "SYM_BRACKET_UNI_L_TOP", "Delimiter Pieces", "", "\xe2\x8e\xa1"}, // ⎡
    {SYM_BRACKET_UNI_L_EXT, "SYM_BRACKET_UNI_L_EXT", "Delimiter Pieces", "", "\xe2\x8e\xa2"}, // ⎢
    {SYM_BRACKET_UNI_L_BOT, "SYM_BRACKET_UNI_L_BOT", "Delimiter Pieces", "", "\xe2\x8e\xa3"}, // ⎣
    {SYM_BRACKET_UNI_R_TOP, "SYM_BRACKET_UNI_R_TOP", "Delimiter Pieces", "", "\xe2\x8e\xa4"}, // ⎤
    {SYM_BRACKET_UNI_R_EXT, "SYM_BRACKET_UNI_R_EXT", "Delimiter Pieces", "", "\xe2\x8e\xa5"}, // ⎥
    {SYM_BRACKET_UNI_R_BOT, "SYM_BRACKET_UNI_R_BOT", "Delimiter Pieces", "", "\xe2\x8e\xa6"}, // ⎦
    // Square brackets (ASCII)
    {SYM_BRACKET_ASCII_L_TOP, "SYM_BRACKET_ASCII_L_TOP", "Delimiter Pieces", "+-", ""},
    {SYM_BRACKET_ASCII_L_EXT, "SYM_BRACKET_ASCII_L_EXT", "Delimiter Pieces", "|", ""},
    {SYM_BRACKET_ASCII_L_BOT, "SYM_BRACKET_ASCII_L_BOT", "Delimiter Pieces", "+-", ""},
    {SYM_BRACKET_ASCII_R_TOP, "SYM_BRACKET_ASCII_R_TOP", "Delimiter Pieces", "-+", ""},
    {SYM_BRACKET_ASCII_R_EXT, "SYM_BRACKET_ASCII_R_EXT", "Delimiter Pieces", " |", ""},
    {SYM_BRACKET_ASCII_R_BOT, "SYM_BRACKET_ASCII_R_BOT", "Delimiter Pieces", "-+", ""},
    // Curly braces (Unicode) - ⎪ U+23AA is the correct extension character
    {SYM_BRACE_UNI_L_TOP, "SYM_BRACE_UNI_L_TOP", "Delimiter Pieces", "", "\xe2\x8e\xa7"}, // ⎧
    {SYM_BRACE_UNI_L_EXT, "SYM_BRACE_UNI_L_EXT", "Delimiter Pieces", "", "\xe2\x8e\xaa"}, // ⎪
    {SYM_BRACE_UNI_L_MID, "SYM_BRACE_UNI_L_MID", "Delimiter Pieces", "", "\xe2\x8e\xa8"}, // ⎨
    {SYM_BRACE_UNI_L_BOT, "SYM_BRACE_UNI_L_BOT", "Delimiter Pieces", "", "\xe2\x8e\xa9"}, // ⎩
    {SYM_BRACE_UNI_R_TOP, "SYM_BRACE_UNI_R_TOP", "Delimiter Pieces", "", "\xe2\x8e\xab"}, // ⎫
    {SYM_BRACE_UNI_R_EXT, "SYM_BRACE_UNI_R_EXT", "Delimiter Pieces", "", "\xe2\x8e\xaa"}, // ⎪
    {SYM_BRACE_UNI_R_MID, "SYM_BRACE_UNI_R_MID", "Delimiter Pieces", "", "\xe2\x8e\xac"}, // ⎬
    {SYM_BRACE_UNI_R_BOT, "SYM_BRACE_UNI_R_BOT", "Delimiter Pieces", "", "\xe2\x8e\xad"}, // ⎭
    // Curly braces (ASCII)
    {SYM_BRACE_ASCII_L_TOP, "SYM_BRACE_ASCII_L_TOP", "Delimiter Pieces", "/ ", ""},
    {SYM_BRACE_ASCII_L_EXT, "SYM_BRACE_ASCII_L_EXT", "Delimiter Pieces", "| ", ""},
    {SYM_BRACE_ASCII_L_MID, "SYM_BRACE_ASCII_L_MID", "Delimiter Pieces", "{ ", ""},
    {SYM_BRACE_ASCII_L_BOT, "SYM_BRACE_ASCII_L_BOT", "Delimiter Pieces", "\\ ", ""},
    {SYM_BRACE_ASCII_R_TOP, "SYM_BRACE_ASCII_R_TOP", "Delimiter Pieces", " \\", ""},
    {SYM_BRACE_ASCII_R_EXT, "SYM_BRACE_ASCII_R_EXT", "Delimiter Pieces", " |", ""},
    {SYM_BRACE_ASCII_R_MID, "SYM_BRACE_ASCII_R_MID", "Delimiter Pieces", " }", ""},
    {SYM_BRACE_ASCII_R_BOT, "SYM_BRACE_ASCII_R_BOT", "Delimiter Pieces", " /", ""},
};

static const int g_symbol_record_count = sizeof(g_symbol_records) / sizeof(g_symbol_records[0]);

// ============================================================================
// Global Symbol Table Instance
// ============================================================================

SymbolTable g_symbols;

// ============================================================================
// Implementation - All functions use g_symbol_records[]
// ============================================================================

void symbols_init(void) {
    g_symbols.records = g_symbol_records;
    g_symbols.count = g_symbol_record_count;
    g_symbols.current_mode = MODE_ASCII;
}

const char *get_symbol(SymbolID id) {
    if (id <= SYM_INVALID || id >= SYM_COUNT) {
        return "?";
    }

    // NEW: Check for override first
    SymbolOverride *override = find_override(id);
    if (override) {
        const char *override_value = (g_symbols.current_mode == MODE_ASCII)
                                         ? override->ascii_override
                                         : override->unicode_override;

        if (override_value) {
            return override_value; // Use override
        }
        // Otherwise fall through to default
    }

    // Fast path: try direct index (most symbol IDs are sequential)
    if (id > 0 && id <= g_symbols.count && g_symbols.records[id - 1].id == id) {
        return (g_symbols.current_mode == MODE_ASCII) ? g_symbols.records[id - 1].ascii
                                                      : g_symbols.records[id - 1].unicode;
    }

    // Fallback: linear search
    for (int i = 0; i < g_symbols.count; i++) {
        if (g_symbols.records[i].id == id) {
            return (g_symbols.current_mode == MODE_ASCII) ? g_symbols.records[i].ascii
                                                          : g_symbols.records[i].unicode;
        }
    }

    return "?";
}

const SymbolRecord *get_symbol_record(SymbolID id) {
    if (id <= SYM_INVALID || id >= SYM_COUNT) {
        return NULL;
    }

    // Direct indexing
    if (id > 0 && id <= g_symbols.count && g_symbols.records[id - 1].id == id) {
        return &g_symbols.records[id - 1];
    }

    // Fallback to linear search
    for (int i = 0; i < g_symbols.count; i++) {
        if (g_symbols.records[i].id == id) {
            return &g_symbols.records[i];
        }
    }
    return NULL;
}

void set_render_mode(RenderMode mode) {
    g_symbols.current_mode = mode;
}

RenderMode get_render_mode(void) {
    return g_symbols.current_mode;
}

void set_symbol(SymbolID id, const char *value) {
    if (id <= SYM_INVALID || id >= SYM_COUNT) {
        return; // Invalid ID
    }

    if (!value) {
        return; // NULL value not allowed
    }

    // Find existing override or create new one
    SymbolOverride *override = find_override(id);

    if (!override) {
        // Create new override
        if (g_override_count >= MAX_OVERRIDES) {
            fprintf(stderr, "Warning: Override table full (max %d)\n", MAX_OVERRIDES);
            return;
        }

        override = &g_overrides[g_override_count];
        override->id = id;
        override->ascii_override = NULL;
        override->unicode_override = NULL;
        g_override_count++;
    }

    // Set the override for current mode
    RenderMode mode = get_unicode_mode() ? MODE_UNICODE : MODE_ASCII;

    if (mode == MODE_ASCII) {
        // Free old override if it exists
        if (override->ascii_override) {
            free(override->ascii_override);
        }
        // Set new value
        override->ascii_override = strdup(value);
    } else {
        // Free old override if it exists
        if (override->unicode_override) {
            free(override->unicode_override);
        }
        // Set new value
        override->unicode_override = strdup(value);
    }
}

SymbolID symbol_name_to_id(const char *name) {
    for (int i = 0; i < g_symbols.count; i++) {
        if (strcmp(g_symbols.records[i].name, name) == 0) {
            return g_symbols.records[i].id;
        }
    }
    return SYM_INVALID;
}

const char *symbol_id_to_name(SymbolID id) {
    const SymbolRecord *rec = get_symbol_record(id);
    return rec ? rec->name : NULL;
}

const char *get_symbol_category(SymbolID id) {
    const SymbolRecord *rec = get_symbol_record(id);
    return rec ? rec->category : "Unknown";
}

void symbols_cleanup(void) {
    // Free all override strings
    for (int i = 0; i < g_override_count; i++) {
        if (g_overrides[i].ascii_override) {
            free(g_overrides[i].ascii_override);
        }
        if (g_overrides[i].unicode_override) {
            free(g_overrides[i].unicode_override);
        }
    }
    g_override_count = 0;
}
