#pragma once

// ============================================================================
// Symbol IDs - Complete catalog of all renderable glyphs
// ============================================================================

typedef enum {
    SYM_INVALID = 0,

    // ========== Greek Letters (lowercase) ==========
    SYM_ALPHA = 1,
    SYM_BETA,
    SYM_GAMMA,
    SYM_DELTA,
    SYM_EPSILON,
    SYM_VAREPSILON,
    SYM_ZETA,
    SYM_ETA,
    SYM_THETA,
    SYM_VARTHETA,
    SYM_IOTA,
    SYM_KAPPA,
    SYM_LAMBDA,
    SYM_MU,
    SYM_NU,
    SYM_XI,
    SYM_OMICRON,
    SYM_PI,
    SYM_VARPI,
    SYM_RHO,
    SYM_VARRHO,
    SYM_SIGMA,
    SYM_VARSIGMA,
    SYM_TAU,
    SYM_UPSILON,
    SYM_PHI,
    SYM_VARPHI,
    SYM_CHI,
    SYM_PSI,
    SYM_OMEGA,

    // ========== Greek Letters (uppercase) ==========
    SYM_ALPHA_UPPER,
    SYM_BETA_UPPER,
    SYM_GAMMA_UPPER,
    SYM_DELTA_UPPER,
    SYM_THETA_UPPER,
    SYM_LAMBDA_UPPER,
    SYM_XI_UPPER,
    SYM_PI_UPPER,
    SYM_SIGMA_UPPER,
    SYM_UPSILON_UPPER,
    SYM_PHI_UPPER,
    SYM_PSI_UPPER,
    SYM_OMEGA_UPPER,

    // ========== Binary Operators ==========
    SYM_PLUS,
    SYM_MINUS,
    SYM_TIMES,
    SYM_CDOT,
    SYM_DIV,
    SYM_PM,
    SYM_MP,

    // ========== Relational Operators ==========
    SYM_EQ,
    SYM_NEQ,
    SYM_LT,
    SYM_GT,
    SYM_LEQ,
    SYM_GEQ,
    SYM_EQUIV,
    SYM_APPROX,
    SYM_LL,     // ≪ much less than
    SYM_GG,     // ≫ much greater than
    SYM_SIM,    // ∼ similar
    SYM_SIMEQ,  // ≃ similar or equal
    SYM_CONG,   // ≅ congruent
    SYM_PROPTO, // ∝ proportional to
    SYM_MID,    // ∣ divides
    SYM_NMID,   // ∤ does not divide

    // ========== Set Theory ==========
    SYM_IN,
    SYM_NOTIN,
    SYM_NI,
    SYM_SUBSET,
    SYM_SUBSETEQ,
    SYM_SUPSET,
    SYM_SUPSETEQ,
    SYM_CUP,
    SYM_CAP,
    SYM_EMPTYSET,
    SYM_SETMINUS,   // ∖ set minus
    SYM_VARNOTHING, // ∅ empty set (variant)

    // ========== Logic ==========
    SYM_FORALL,
    SYM_EXISTS,
    SYM_NEXISTS, // ∄ does not exist
    SYM_NEG,
    SYM_LAND,
    SYM_LOR,
    SYM_IMPLIES,
    SYM_IFF,
    SYM_BOT,    // ⊥ bottom/false
    SYM_VDASH,  // ⊢ turnstile (proves/entails)
    SYM_DASHV,  // ⊣ reverse turnstile
    SYM_MODELS, // ⊨ models/satisfies

    // ========== Hebrew Letters ==========
    SYM_ALEPH,  // ℵ aleph (cardinality)
    SYM_BETH,   // ℶ beth
    SYM_GIMEL,  // ℷ gimel
    SYM_DALETH, // ℸ daleth

    // ========== Arrows ==========
    SYM_RIGHTARROW,
    SYM_LEFTARROW,
    SYM_LEFTRIGHTARROW,
    SYM_RIGHTARROW_DOUBLE,
    SYM_LEFTARROW_DOUBLE,
    SYM_LEFTRIGHTARROW_DOUBLE,
    SYM_MAPSTO,
    SYM_LONGMAPSTO,       // ⟼ long maps to
    SYM_LONGRIGHTARROW,   // ⟶ long right arrow
    SYM_LONGLEFTARROW,    // ⟵ long left arrow
    SYM_HOOKRIGHTARROW,   // ↪ hook right arrow
    SYM_HOOKLEFTARROW,    // ↩ hook left arrow
    SYM_UPARROW,          // ↑ up arrow
    SYM_DOWNARROW,        // ↓ down arrow
    SYM_UPDOWNARROW,      // ↕ up-down arrow
    SYM_UPARROW_DOUBLE,   // ⇑ double up arrow
    SYM_DOWNARROW_DOUBLE, // ⇓ double down arrow
    SYM_NEARROW,          // ↗ northeast arrow
    SYM_SEARROW,          // ↘ southeast arrow
    SYM_NWARROW,          // ↖ northwest arrow
    SYM_SWARROW,          // ↙ southwest arrow

    // ========== Miscellaneous ==========
    SYM_INFTY,
    SYM_PARTIAL,
    SYM_NABLA,
    SYM_HBAR,
    SYM_PRIME,
    SYM_DPRIME, // ″ double prime
    SYM_TPRIME, // ‴ triple prime
    SYM_ANGLE,
    SYM_PERP,
    SYM_TOP, // ⊤ - top/true/transpose
    SYM_PARALLEL,
    SYM_BULLET,
    SYM_CIRC,
    SYM_STAR,
    SYM_DAG,
    SYM_DDAG,
    SYM_ELL,

    // ========== Binary Circled ==========
    SYM_OPLUS,
    SYM_OMINUS,
    SYM_OTIMES,
    SYM_ODOT,

    // ========== Dots ==========
    SYM_LDOTS,
    SYM_CDOTS,
    SYM_VDOTS, // ⋮ vertical dots
    SYM_DDOTS, // ⋱ diagonal dots

    // ========== Delimiters ==========
    SYM_LANGLE, // ⟨ left angle bracket
    SYM_RANGLE, // ⟩ right angle bracket

    // ========== Geometry ==========
    SYM_TRIANGLE, // △ triangle
    SYM_SQUARE,   // □ square

    // ========== Fractions ==========
    SYM_FRAC_BAR,

    // ========== Square Roots ==========
    SYM_SQRT_VINCULUM,
    SYM_SQRT_HOOK,

    // ========== Big Operators (Σ - size 1) ==========
    SYM_SIGMA1_L0,
    SYM_SIGMA1_L1,
    SYM_SIGMA1_L2,

    // ========== Big Operators (Σ - size 2) ==========
    SYM_SIGMA2_L0,
    SYM_SIGMA2_L1,
    SYM_SIGMA2_L2,
    SYM_SIGMA2_L3,
    SYM_SIGMA2_L4,

    // ========== Big Operators (Σ - size 3) ==========
    SYM_SIGMA3_L0,
    SYM_SIGMA3_L1,
    SYM_SIGMA3_L2,
    SYM_SIGMA3_L3,
    SYM_SIGMA3_L4,
    SYM_SIGMA3_L5,
    SYM_SIGMA3_L6,

    // ========== Big Operators (Π - size 1) ==========
    SYM_PROD1_L0,
    SYM_PROD1_L1,
    SYM_PROD1_L2,

    // ========== Big Operators (Π - size 2) ==========
    SYM_PROD2_L0,
    SYM_PROD2_L1,
    SYM_PROD2_L2,
    SYM_PROD2_L3,
    SYM_PROD2_L4,

    // ========== Big Operators (Π - size 3) ==========
    SYM_PROD3_L0,
    SYM_PROD3_L1,
    SYM_PROD3_L2,
    SYM_PROD3_L3,
    SYM_PROD3_L4,
    SYM_PROD3_L5,
    SYM_PROD3_L6,

    // ========== Big Operators (∫ - size 1) ==========
    SYM_INT1_L0,
    SYM_INT1_L1,
    SYM_INT1_L2,

    // ========== Big Operators (∫ - size 2) ==========
    SYM_INT2_L0,
    SYM_INT2_L1,
    SYM_INT2_L2,
    SYM_INT2_L3,
    SYM_INT2_L4,

    // ========== Big Operators (∫ - size 3) ==========
    SYM_INT3_L0,
    SYM_INT3_L1,
    SYM_INT3_L2,
    SYM_INT3_L3,
    SYM_INT3_L4,
    SYM_INT3_L5,
    SYM_INT3_L6,

    // ========== Contour Integral (∮ - size 1) ==========
    SYM_OINT1_L0,
    SYM_OINT1_L1,
    SYM_OINT1_L2,

    // ========== Contour Integral (∮ - size 2) ==========
    SYM_OINT2_L0,
    SYM_OINT2_L1,
    SYM_OINT2_L2,
    SYM_OINT2_L3,
    SYM_OINT2_L4,

    // ========== Contour Integral (∮ - size 3) ==========
    SYM_OINT3_L0,
    SYM_OINT3_L1,
    SYM_OINT3_L2,
    SYM_OINT3_L3,
    SYM_OINT3_L4,
    SYM_OINT3_L5,
    SYM_OINT3_L6,

    // ========== Coproduct (∐ - size 1) ==========
    SYM_COPROD1_L0,
    SYM_COPROD1_L1,
    SYM_COPROD1_L2,

    // ========== Coproduct (∐ - size 2) ==========
    SYM_COPROD2_L0,
    SYM_COPROD2_L1,
    SYM_COPROD2_L2,
    SYM_COPROD2_L3,
    SYM_COPROD2_L4,

    // ========== Coproduct (∐ - size 3) ==========
    SYM_COPROD3_L0,
    SYM_COPROD3_L1,
    SYM_COPROD3_L2,
    SYM_COPROD3_L3,
    SYM_COPROD3_L4,
    SYM_COPROD3_L5,
    SYM_COPROD3_L6,

    // ========== Big Operators (∪ - size 1) ==========
    SYM_CUP1_L0,
    SYM_CUP1_L1,
    SYM_CUP1_L2,

    // ========== Big Operators (∪ - size 2) ==========
    SYM_CUP2_L0,
    SYM_CUP2_L1,
    SYM_CUP2_L2,
    SYM_CUP2_L3,
    SYM_CUP2_L4,

    // ========== Big Operators (∪ - size 3) ==========
    SYM_CUP3_L0,
    SYM_CUP3_L1,
    SYM_CUP3_L2,
    SYM_CUP3_L3,
    SYM_CUP3_L4,
    SYM_CUP3_L5,
    SYM_CUP3_L6,

    // ========== Big Operators (∩ - size 1) ==========
    SYM_CAP1_L0,
    SYM_CAP1_L1,
    SYM_CAP1_L2,

    // ========== Big Operators (∩ - size 2) ==========
    SYM_CAP2_L0,
    SYM_CAP2_L1,
    SYM_CAP2_L2,
    SYM_CAP2_L3,
    SYM_CAP2_L4,

    // ========== Big Operators (∩ - size 3) ==========
    SYM_CAP3_L0,
    SYM_CAP3_L1,
    SYM_CAP3_L2,
    SYM_CAP3_L3,
    SYM_CAP3_L4,
    SYM_CAP3_L5,
    SYM_CAP3_L6,

    // ========== Parentheses (round, size 3, ASCII) ==========
    SYM_PAREN_ASCII_3_L_L0,
    SYM_PAREN_ASCII_3_L_L1,
    SYM_PAREN_ASCII_3_L_L2,
    SYM_PAREN_ASCII_3_R_L0,
    SYM_PAREN_ASCII_3_R_L1,
    SYM_PAREN_ASCII_3_R_L2,

    // ========== Parentheses (round, size 5, ASCII) ==========
    SYM_PAREN_ASCII_5_L_L0,
    SYM_PAREN_ASCII_5_L_L1,
    SYM_PAREN_ASCII_5_L_L2,
    SYM_PAREN_ASCII_5_L_L3,
    SYM_PAREN_ASCII_5_L_L4,
    SYM_PAREN_ASCII_5_R_L0,
    SYM_PAREN_ASCII_5_R_L1,
    SYM_PAREN_ASCII_5_R_L2,
    SYM_PAREN_ASCII_5_R_L3,
    SYM_PAREN_ASCII_5_R_L4,

    // ========== Parentheses (round, size 7, ASCII) ==========
    SYM_PAREN_ASCII_7_L_L0,
    SYM_PAREN_ASCII_7_L_L1,
    SYM_PAREN_ASCII_7_L_L2,
    SYM_PAREN_ASCII_7_L_L3,
    SYM_PAREN_ASCII_7_L_L4,
    SYM_PAREN_ASCII_7_L_L5,
    SYM_PAREN_ASCII_7_L_L6,
    SYM_PAREN_ASCII_7_R_L0,
    SYM_PAREN_ASCII_7_R_L1,
    SYM_PAREN_ASCII_7_R_L2,
    SYM_PAREN_ASCII_7_R_L3,
    SYM_PAREN_ASCII_7_R_L4,
    SYM_PAREN_ASCII_7_R_L5,
    SYM_PAREN_ASCII_7_R_L6,

    // ========== Parentheses (round, size 3, Unicode) ==========
    SYM_PAREN_UNI_3_L_L0,
    SYM_PAREN_UNI_3_L_L1,
    SYM_PAREN_UNI_3_L_L2,
    SYM_PAREN_UNI_3_R_L0,
    SYM_PAREN_UNI_3_R_L1,
    SYM_PAREN_UNI_3_R_L2,

    // ========== Parentheses (round, size 5, Unicode) ==========
    SYM_PAREN_UNI_5_L_L0,
    SYM_PAREN_UNI_5_L_L1,
    SYM_PAREN_UNI_5_L_L2,
    SYM_PAREN_UNI_5_L_L3,
    SYM_PAREN_UNI_5_L_L4,
    SYM_PAREN_UNI_5_R_L0,
    SYM_PAREN_UNI_5_R_L1,
    SYM_PAREN_UNI_5_R_L2,
    SYM_PAREN_UNI_5_R_L3,
    SYM_PAREN_UNI_5_R_L4,

    // ========== Parentheses (round, size 7, Unicode) ==========
    SYM_PAREN_UNI_7_L_L0,
    SYM_PAREN_UNI_7_L_L1,
    SYM_PAREN_UNI_7_L_L2,
    SYM_PAREN_UNI_7_L_L3,
    SYM_PAREN_UNI_7_L_L4,
    SYM_PAREN_UNI_7_L_L5,
    SYM_PAREN_UNI_7_L_L6,
    SYM_PAREN_UNI_7_R_L0,
    SYM_PAREN_UNI_7_R_L1,
    SYM_PAREN_UNI_7_R_L2,
    SYM_PAREN_UNI_7_R_L3,
    SYM_PAREN_UNI_7_R_L4,
    SYM_PAREN_UNI_7_R_L5,
    SYM_PAREN_UNI_7_R_L6,

    // ========== Square brackets (size 3, ASCII) ==========
    SYM_BRACKET_ASCII_3_L_L0,
    SYM_BRACKET_ASCII_3_L_L1,
    SYM_BRACKET_ASCII_3_L_L2,
    SYM_BRACKET_ASCII_3_R_L0,
    SYM_BRACKET_ASCII_3_R_L1,
    SYM_BRACKET_ASCII_3_R_L2,

    // ========== Square brackets (size 5, ASCII) ==========
    SYM_BRACKET_ASCII_5_L_L0,
    SYM_BRACKET_ASCII_5_L_L1,
    SYM_BRACKET_ASCII_5_L_L2,
    SYM_BRACKET_ASCII_5_L_L3,
    SYM_BRACKET_ASCII_5_L_L4,
    SYM_BRACKET_ASCII_5_R_L0,
    SYM_BRACKET_ASCII_5_R_L1,
    SYM_BRACKET_ASCII_5_R_L2,
    SYM_BRACKET_ASCII_5_R_L3,
    SYM_BRACKET_ASCII_5_R_L4,

    // ========== Square brackets (size 7, ASCII) ==========
    SYM_BRACKET_ASCII_7_L_L0,
    SYM_BRACKET_ASCII_7_L_L1,
    SYM_BRACKET_ASCII_7_L_L2,
    SYM_BRACKET_ASCII_7_L_L3,
    SYM_BRACKET_ASCII_7_L_L4,
    SYM_BRACKET_ASCII_7_L_L5,
    SYM_BRACKET_ASCII_7_L_L6,
    SYM_BRACKET_ASCII_7_R_L0,
    SYM_BRACKET_ASCII_7_R_L1,
    SYM_BRACKET_ASCII_7_R_L2,
    SYM_BRACKET_ASCII_7_R_L3,
    SYM_BRACKET_ASCII_7_R_L4,
    SYM_BRACKET_ASCII_7_R_L5,
    SYM_BRACKET_ASCII_7_R_L6,

    // ========== Square brackets (size 3, Unicode) ==========
    SYM_BRACKET_UNI_3_L_L0,
    SYM_BRACKET_UNI_3_L_L1,
    SYM_BRACKET_UNI_3_L_L2,
    SYM_BRACKET_UNI_3_R_L0,
    SYM_BRACKET_UNI_3_R_L1,
    SYM_BRACKET_UNI_3_R_L2,

    // ========== Square brackets (size 5, Unicode) ==========
    SYM_BRACKET_UNI_5_L_L0,
    SYM_BRACKET_UNI_5_L_L1,
    SYM_BRACKET_UNI_5_L_L2,
    SYM_BRACKET_UNI_5_L_L3,
    SYM_BRACKET_UNI_5_L_L4,
    SYM_BRACKET_UNI_5_R_L0,
    SYM_BRACKET_UNI_5_R_L1,
    SYM_BRACKET_UNI_5_R_L2,
    SYM_BRACKET_UNI_5_R_L3,
    SYM_BRACKET_UNI_5_R_L4,

    // ========== Square brackets (size 7, Unicode) ==========
    SYM_BRACKET_UNI_7_L_L0,
    SYM_BRACKET_UNI_7_L_L1,
    SYM_BRACKET_UNI_7_L_L2,
    SYM_BRACKET_UNI_7_L_L3,
    SYM_BRACKET_UNI_7_L_L4,
    SYM_BRACKET_UNI_7_L_L5,
    SYM_BRACKET_UNI_7_L_L6,
    SYM_BRACKET_UNI_7_R_L0,
    SYM_BRACKET_UNI_7_R_L1,
    SYM_BRACKET_UNI_7_R_L2,
    SYM_BRACKET_UNI_7_R_L3,
    SYM_BRACKET_UNI_7_R_L4,
    SYM_BRACKET_UNI_7_R_L5,
    SYM_BRACKET_UNI_7_R_L6,

    // ========== Curly braces (size 3, ASCII) ==========
    SYM_BRACE_ASCII_3_L_L0,
    SYM_BRACE_ASCII_3_L_L1,
    SYM_BRACE_ASCII_3_L_L2,
    SYM_BRACE_ASCII_3_R_L0,
    SYM_BRACE_ASCII_3_R_L1,
    SYM_BRACE_ASCII_3_R_L2,

    // ========== Curly braces (size 5, ASCII) ==========
    SYM_BRACE_ASCII_5_L_L0,
    SYM_BRACE_ASCII_5_L_L1,
    SYM_BRACE_ASCII_5_L_L2,
    SYM_BRACE_ASCII_5_L_L3,
    SYM_BRACE_ASCII_5_L_L4,
    SYM_BRACE_ASCII_5_R_L0,
    SYM_BRACE_ASCII_5_R_L1,
    SYM_BRACE_ASCII_5_R_L2,
    SYM_BRACE_ASCII_5_R_L3,
    SYM_BRACE_ASCII_5_R_L4,

    // ========== Curly braces (size 7, ASCII) ==========
    SYM_BRACE_ASCII_7_L_L0,
    SYM_BRACE_ASCII_7_L_L1,
    SYM_BRACE_ASCII_7_L_L2,
    SYM_BRACE_ASCII_7_L_L3,
    SYM_BRACE_ASCII_7_L_L4,
    SYM_BRACE_ASCII_7_L_L5,
    SYM_BRACE_ASCII_7_L_L6,
    SYM_BRACE_ASCII_7_R_L0,
    SYM_BRACE_ASCII_7_R_L1,
    SYM_BRACE_ASCII_7_R_L2,
    SYM_BRACE_ASCII_7_R_L3,
    SYM_BRACE_ASCII_7_R_L4,
    SYM_BRACE_ASCII_7_R_L5,
    SYM_BRACE_ASCII_7_R_L6,

    // ========== Curly braces (size 3, Unicode) ==========
    SYM_BRACE_UNI_3_L_L0,
    SYM_BRACE_UNI_3_L_L1,
    SYM_BRACE_UNI_3_L_L2,
    SYM_BRACE_UNI_3_R_L0,
    SYM_BRACE_UNI_3_R_L1,
    SYM_BRACE_UNI_3_R_L2,

    // ========== Curly braces (size 5, Unicode) ==========
    SYM_BRACE_UNI_5_L_L0,
    SYM_BRACE_UNI_5_L_L1,
    SYM_BRACE_UNI_5_L_L2,
    SYM_BRACE_UNI_5_L_L3,
    SYM_BRACE_UNI_5_L_L4,
    SYM_BRACE_UNI_5_R_L0,
    SYM_BRACE_UNI_5_R_L1,
    SYM_BRACE_UNI_5_R_L2,
    SYM_BRACE_UNI_5_R_L3,
    SYM_BRACE_UNI_5_R_L4,

    // ========== Curly braces (size 7, Unicode) ==========
    SYM_BRACE_UNI_7_L_L0,
    SYM_BRACE_UNI_7_L_L1,
    SYM_BRACE_UNI_7_L_L2,
    SYM_BRACE_UNI_7_L_L3,
    SYM_BRACE_UNI_7_L_L4,
    SYM_BRACE_UNI_7_L_L5,
    SYM_BRACE_UNI_7_L_L6,
    SYM_BRACE_UNI_7_R_L0,
    SYM_BRACE_UNI_7_R_L1,
    SYM_BRACE_UNI_7_R_L2,
    SYM_BRACE_UNI_7_R_L3,
    SYM_BRACE_UNI_7_R_L4,
    SYM_BRACE_UNI_7_R_L5,
    SYM_BRACE_UNI_7_R_L6,

    // ========== Vertical bars ==========
    // Structural (box-drawing, full height - use for absolute value, evaluation)
    SYM_VERT_SINGLE, // │ U+2502 - connects with table borders
    SYM_VERT_DOUBLE, // ║ U+2551 - connects with table borders
    // Mathematical (shorter - use for bra-ket notation, "such that", etc.)
    SYM_DIVIDES, // ∣ U+2223 - mathematical divides/vertical bar
    // Note: SYM_PARALLEL (∥ U+2225) is already defined in Miscellaneous Symbols

    // ========== Floor/Ceiling symbols ==========
    SYM_LFLOOR,
    SYM_RFLOOR,
    SYM_LCEIL,
    SYM_RCEIL,

    // ========== Misc (append new symbols here) ==========
    SYM_BLACKSQUARE, // ■ filled square (QED)
    SYM_THEREFORE,   // ∴ therefore
    SYM_BECAUSE,     // ∵ because
    SYM_PREC,        // ≺ precedes
    SYM_SUCC,        // ≻ succeeds
    SYM_PRECEQ,      // ⪯ precedes or equal
    SYM_SUCCEQ,      // ⪰ succeeds or equal
    SYM_WP,          // ℘ Weierstrass p
    SYM_IMATH,       // ı dotless i
    SYM_JMATH,       // ȷ dotless j
    SYM_NLEQ,        // ≰ not less or equal
    SYM_NGEQ,        // ≱ not greater or equal
    SYM_NSUBSET,     // ⊄ not a subset
    SYM_NEQUIV,      // ≢ not equivalent

    // ========== Wide Accent Characters ==========
    SYM_WIDEHAT_LEFT,   // ╱ left stroke of wide hat
    SYM_WIDEHAT_RIGHT,  // ╲ right stroke of wide hat
    SYM_WIDEHAT_FILL,   // ‾ fill for wide hat
    SYM_WIDETILDE_FILL, // ∼ fill for wide tilde

    // ========== Delimiter Pieces (extensible building blocks) ==========
    // Round parentheses
    SYM_PAREN_UNI_L_TOP,
    SYM_PAREN_UNI_L_EXT,
    SYM_PAREN_UNI_L_BOT,
    SYM_PAREN_UNI_R_TOP,
    SYM_PAREN_UNI_R_EXT,
    SYM_PAREN_UNI_R_BOT,
    SYM_PAREN_ASCII_L_TOP,
    SYM_PAREN_ASCII_L_EXT,
    SYM_PAREN_ASCII_L_BOT,
    SYM_PAREN_ASCII_R_TOP,
    SYM_PAREN_ASCII_R_EXT,
    SYM_PAREN_ASCII_R_BOT,
    // Square brackets
    SYM_BRACKET_UNI_L_TOP,
    SYM_BRACKET_UNI_L_EXT,
    SYM_BRACKET_UNI_L_BOT,
    SYM_BRACKET_UNI_R_TOP,
    SYM_BRACKET_UNI_R_EXT,
    SYM_BRACKET_UNI_R_BOT,
    SYM_BRACKET_ASCII_L_TOP,
    SYM_BRACKET_ASCII_L_EXT,
    SYM_BRACKET_ASCII_L_BOT,
    SYM_BRACKET_ASCII_R_TOP,
    SYM_BRACKET_ASCII_R_EXT,
    SYM_BRACKET_ASCII_R_BOT,
    // Curly braces (4 pieces: top/ext/mid/bot)
    SYM_BRACE_UNI_L_TOP,
    SYM_BRACE_UNI_L_EXT,
    SYM_BRACE_UNI_L_MID,
    SYM_BRACE_UNI_L_BOT,
    SYM_BRACE_UNI_R_TOP,
    SYM_BRACE_UNI_R_EXT,
    SYM_BRACE_UNI_R_MID,
    SYM_BRACE_UNI_R_BOT,
    SYM_BRACE_ASCII_L_TOP,
    SYM_BRACE_ASCII_L_EXT,
    SYM_BRACE_ASCII_L_MID,
    SYM_BRACE_ASCII_L_BOT,
    SYM_BRACE_ASCII_R_TOP,
    SYM_BRACE_ASCII_R_EXT,
    SYM_BRACE_ASCII_R_MID,
    SYM_BRACE_ASCII_R_BOT,

    // ========== TOTAL COUNT (keep at end) ==========
    SYM_COUNT
} SymbolID;

// ============================================================================
// Render Modes
// ============================================================================

typedef enum { MODE_ASCII = 0, MODE_UNICODE = 1 } RenderMode;

// ============================================================================
// Symbol Metadata Record
// ============================================================================

typedef struct {
    SymbolID id;
    const char *name;     // e.g. "SYM_ALPHA" for template export
    const char *category; // e.g. "Greek Letters (Lowercase)"
    const char *ascii;    // ASCII rendering
    const char *unicode;  // Unicode rendering
} SymbolRecord;

// ============================================================================
// Symbol Table Structure
// ============================================================================

typedef struct {
    const SymbolRecord *records; // Array of all symbol records
    int count;                   // Number of records
    RenderMode current_mode;     // Current rendering mode
} SymbolTable;

// ============================================================================
// Global Symbol Table
// ============================================================================

extern SymbolTable g_symbols;

// ============================================================================
// Symbol Table API
// ============================================================================

// Initialize with defaults
void symbols_init(void);

// Get symbol string for current mode
const char *get_symbol(SymbolID id);

// Get symbol metadata
const SymbolRecord *get_symbol_record(SymbolID id);

// Set mode (affects subsequent get_symbol calls)
void set_render_mode(RenderMode mode);
RenderMode get_render_mode(void);

// Override a symbol (for current mode)
void set_symbol(SymbolID id, const char *value);

// Symbol name <-> ID conversion (for \setsym command parsing)
SymbolID symbol_name_to_id(const char *name);
const char *symbol_id_to_name(SymbolID id);

// Get category for a symbol
const char *get_symbol_category(SymbolID id);

// Cleanup
void symbols_cleanup(void);