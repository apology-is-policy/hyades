#pragma once
#include <stdbool.h>
#include <stdint.h>

// Box: 1 unicode codepoint per cell
typedef struct Box {
    int w, h;
    int baseline;
    uint32_t *cells;
    uint8_t *meta;   // Optional per-cell metadata (NULL if not needed)
    uint16_t *style; // Optional per-cell ANSI style (NULL if not needed)
                     // Encoding: (bg_sgr << 8) | fg_sgr
                     // fg: 0=default, 30-37=standard, 90-97=bright
                     // bg: 0=default, 40-47=standard, 100-107=bright
    int sub_shift;   // Extra downward shift for subscript attachment (0 = default)
    int sup_shift;   // Extra downward shift for superscript attachment (0 = default)
                     // Used by tall delimiters to place scripts flush with edges
    int tag_width;   // Width of \tag content at right edge (0 = no tag)
                     // Used by display math renderer for right-justification
    uint8_t
        *row_flags; // Per-row alignment flags (NULL = none). Allocated to h entries when needed.
} Box;

#define BOX_ROW_FLAG_FULL_LEFT 0x01 // Left-align this row to container margin, even when centering

// Cell metadata flags
#define CELL_META_NONE 0x00
#define CELL_META_NO_CONNECT 0x01 // Don't participate in junction fixup

Box make_box(int w, int h, int baseline);
void box_free(Box *b);

// Metadata helpers - allocate meta array if needed, set/get flags
void box_ensure_meta(Box *b);
void box_set_meta(Box *b, int x, int y, uint8_t flags);
uint8_t box_get_meta(const Box *b, int x, int y);

// Per-cell ANSI style helpers (lazy-allocated, same pattern as meta)
void box_ensure_style(Box *b);
void box_set_cell_style(Box *b, int x, int y, uint16_t s);
uint16_t box_get_cell_style(const Box *b, int x, int y);

typedef enum {
    ACCENT_HAT,            // \hat{x}   - circumflex (^)
    ACCENT_BAR,            // \bar{x}   - overline (macron)
    ACCENT_TILDE,          // \tilde{x} - tilde (~)
    ACCENT_DOT,            // \dot{x}   - single dot above
    ACCENT_DDOT,           // \ddot{x}  - double dot (umlaut)
    ACCENT_VEC,            // \vec{v}   - vector arrow
    ACCENT_ACUTE,          // \acute{x} - acute accent
    ACCENT_GRAVE,          // \grave{x} - grave accent
    ACCENT_BREVE,          // \breve{x} - breve
    ACCENT_CHECK,          // \check{x} - háček/caron
    ACCENT_UNDERLINE,      // \underline{x} - underline
    ACCENT_OVERRIGHTARROW, // \overrightarrow{AB} - right arrow over
    ACCENT_OVERLEFTARROW,  // \overleftarrow{AB} - left arrow over
} AccentKind;

// AST kinds
typedef enum {
    AST_SYMBOL,
    AST_BINOP,
    AST_BINREL,
    AST_FRACTION,
    AST_SUP,
    AST_SUB,
    AST_SUPSUB,
    AST_GROUP,
    AST_PAREN,
    AST_SQRT,

    AST_FUNCTION,
    AST_TEXT,

    // Big operators (all with optional limits+body)
    AST_SUM,
    AST_PROD,
    AST_INT,
    AST_IINT,  // Double integral \iint
    AST_IIINT, // Triple integral \iiint
    AST_OINT,  // Contour integral \oint
    AST_OIINT, // Surface integral \oiint
    AST_BIGCUP,
    AST_BIGCAP,
    AST_COPROD, // Coproduct \coprod
    AST_FORALL,
    AST_EXISTS,
    AST_LIMFUNC, // Limit-style functions: lim, max, min, sup, inf, etc.
    AST_STYLE,

    AST_MATRIX,

    AST_ACCENT,

    AST_OVERBRACE,  // \overbrace{content}^{label}
    AST_UNDERBRACE, // \underbrace{content}_{label}

    AST_EMBED, // Embedded document construct (e.g., \cases, \aligned inside math)

    AST_OVERSET,  // \overset{label}{base}, \underset{label}{base}, \stackrel
    AST_BOXED,    // \boxed{content}
    AST_PHANTOM,  // \phantom{content}
    AST_SMASH,    // \smash{content}
    AST_XARROW,   // \xrightarrow{text}, \xleftarrow{text}
    AST_SUBSTACK, // \substack{a \\ b \\ c}
    AST_TAG       // \tag{text}
} AstKind;

typedef enum {
    STYLE_NORMAL = 0, // no explicit style
    STYLE_BOLD,       // \mathbf
    STYLE_BLACKBOARD, // \mathbb
    STYLE_SCRIPT,     // \mathcal
    STYLE_ITALIC,     // \mathit  (force italic even when mathitalic is off)
    STYLE_ROMAN,      // \mathrm  (force upright even when mathitalic is on)
    STYLE_FRAKTUR,    // \mathfrak
    STYLE_SANS        // \mathsf
} StyleKind;

typedef struct {
    StyleKind kind;
    struct Ast *child;
} AstStyle;

typedef enum {
    PAREN_ROUND = 0, // (...), sizes 1/3/5/7
    PAREN_SQUARE,    // [...]
    PAREN_CURLY,     // \lbrace...\rbrace
    PAREN_VBAR,      // |...| or \lvert...\rvert  (single vertical bar)
    PAREN_DVBAR,     // \Vert...\Vert (double vertical bar)
    PAREN_FLOOR,     // \lfloor...\rfloor
    PAREN_CEIL,      // \lceil...\rceil
    PAREN_ANGLE,     // \langle...\rangle (angle brackets)
    PAREN_NONE       // invisible delimiter (\left. or \right.)
} ParenType;

typedef struct Ast Ast;

// --- add a payload struct ---
typedef struct {
    int rows, cols;
    Ast **cells;      // row-major, size rows*cols
    int cell_padding; // horizontal padding per cell (default 2 for matrices, 1 for binomials)
} Matrix;

typedef struct {
    char op; // '+', '-', '*', '.', 'x', '='
    Ast *lhs;
    Ast *rhs;
} BinOp;

typedef struct {
    char op[16];
    Ast *lhs;
    Ast *rhs;
} BinRel;

typedef struct {
    Ast *numer;
    Ast *denom;
} Fraction;

typedef struct {
    Ast *base;
    Ast *sup; // for SUB this is subscript
} SupOrSub;

typedef struct {
    Ast *base;
    Ast *sup;
    Ast *sub;
} SupSub;

typedef struct {
    Ast *child;
} Group;

typedef struct {
    Ast *pchild;
    ParenType ptype;       // left delimiter type (or only type when symmetric)
    ParenType right_ptype; // right delimiter type (set to ptype for symmetric)
    int size;              // 1 or 3/5/7 for tall
} Paren;

typedef struct {
    Ast *index; // optional
    Ast *rad;
} SqrtOp;

// Generic "limits op" (used by sum, prod, int, union, intersection, quantifiers)
typedef struct {
    Ast *lower; // may be NULL
    Ast *upper; // may be NULL
    Ast *body;  // expression to the right (non-NULL)
    int size;   // 1..3 (manual size step)
} LimOp;

// Limit-style function operators (lim, max, min, sup, inf, etc.)
// Renders as: operator name centered, with optional limits below/above
typedef struct {
    char name[16]; // "lim", "max", "min", "sup", "inf", etc.
    Ast *lower;    // may be NULL (e.g., x → 0)
    Ast *upper;    // may be NULL (rarely used, but supported)
    Ast *body;     // expression to the right
} LimFunc;

typedef struct {
    Ast *child;
} Function;

typedef struct {
    char text[512]; // Larger buffer for text strings
} Text;

typedef struct {
    AccentKind akind;
    Ast *child;
} Accent;

typedef struct {
    Ast *content; // The expression being braced
    Ast *label;   // Optional label (NULL if none)
} Brace;

// Embedded document construct types
typedef enum {
    EMBED_CASES,  // \cases{...}
    EMBED_ALIGNED // \aligned{...}
} EmbedKind;

typedef struct {
    EmbedKind ekind;
    char *content; // Raw content string (owned, must be freed)
} Embed;

// Overset/underset: annotation stacked above/below base
typedef struct {
    Ast *base;
    Ast *annotation;
    bool is_over; // true = above (overset), false = below (underset)
} Overset;

// Extensible arrow: \xrightarrow{text} / \xleftarrow{text}
typedef struct {
    Ast *label;
    bool is_right; // true = right arrow, false = left arrow
} XArrow;

// Substack: vertically stacked rows (for use inside limits)
typedef struct {
    int num_rows;
    Ast **rows; // array of expressions, one per row
} Substack;

struct Ast {
    AstKind kind;
    union {
        struct {
            char text[32];
        } sym;     // SYMBOL
        BinOp bin; // BINOP
        BinRel binrel;
        Fraction frac;   // FRACTION
        SupOrSub sup;    // SUP/SUB
        Group group;     // GROUP
        Paren paren;     // PAREN
        SqrtOp sqrt;     // SQRT
        LimOp lim;       // SUM/PROD/INT/BIGCUP/BIGCAP/FORALL/EXISTS
        LimFunc limfunc; // LIMFUNC (lim, max, min, sup, inf, etc.)
        SupSub supsub;
        AstStyle style;
        Matrix matrix;
        Function func;
        Text text;
        Accent accent;
        Brace brace; // OVERBRACE/UNDERBRACE
        Embed embed;
        Overset overset;   // OVERSET (overset/underset/stackrel)
        Group boxed;       // BOXED (reuse Group - single child)
        Group phantom;     // PHANTOM (reuse Group - single child)
        Group smash;       // SMASH (reuse Group - single child)
        XArrow xarrow;     // XARROW (xrightarrow/xleftarrow)
        Substack substack; // SUBSTACK
        Group tag;         // TAG (reuse Group - single child)
    };
};

// Rendering
Box render_ast(const Ast *node);

// Factories
Ast *ast_symbol(const char *txt);
Ast *ast_binop(char op, Ast *lhs, Ast *rhs);
Ast *ast_binop_str(const char *op, Ast *lhs, Ast *rhs);
Ast *ast_fraction(Ast *numer, Ast *denom);
Ast *ast_sup(Ast *base, Ast *sup);
Ast *ast_sub(Ast *base, Ast *sub);
Ast *ast_supsub(Ast *base, Ast *sup, Ast *sub);
Ast *ast_group(Ast *child);
Ast *ast_paren(Ast *child, ParenType t, int size);
Ast *ast_paren_asym(Ast *child, ParenType left, ParenType right, int size);
Ast *ast_sqrt(Ast *index, Ast *rad);

// Limits-op factories
Ast *ast_sum(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_prod(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_int(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_iint(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_iiint(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_oint(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_oiint(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_bigcup(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_bigcap(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_coprod(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_forall(Ast *lower, Ast *upper, Ast *body, int size);
Ast *ast_exists(Ast *lower, Ast *upper, Ast *body, int size);

// Limit-style function operators (lim, max, min, sup, inf, etc.)
Ast *ast_limfunc(const char *name, Ast *lower, Ast *upper, Ast *body);

Ast *ast_style(StyleKind k, Ast *child);

Ast *ast_matrix(int rows, int cols, Ast **cells, int cell_padding);

Ast *ast_function(Ast *child);
Ast *ast_text(const char *str);

Ast *ast_accent(AccentKind k, Ast *child);

Ast *ast_overbrace(Ast *content, Ast *label);
Ast *ast_underbrace(Ast *content, Ast *label);

Ast *ast_embed(EmbedKind k, const char *content);

Ast *ast_overset(Ast *base, Ast *annotation, bool is_over);
Ast *ast_boxed(Ast *child);
Ast *ast_phantom(Ast *child);
Ast *ast_smash(Ast *child);
Ast *ast_xarrow(Ast *label, bool is_right);
Ast *ast_substack(int num_rows, Ast **rows);
Ast *ast_tag(Ast *child);

// Free AST
void ast_free(Ast *node);
