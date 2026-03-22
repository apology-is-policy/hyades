// builtin_docs.h - Static documentation registry for built-in commands
//
// This provides hover documentation for LaTeX/Hyades commands that don't
// have user-defined source locations.

#ifndef BUILTIN_DOCS_H
#define BUILTIN_DOCS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;        // Command name with backslash, e.g., "\\frac"
    const char *signature;   // Parameter signature, e.g., "{num}{denom}"
    const char *description; // Brief description
    const char *example;     // Example usage (optional, can be NULL)
    const char *renders_as;  // What it renders to (optional, can be NULL)
} BuiltinDoc;

// =============================================================================
// Math Constructs
// =============================================================================

static const BuiltinDoc g_math_constructs[] = {
    // Fractions
    {"\\frac", "{numerator}{denominator}", "Fraction", "\\frac{a+b}{2}",
     "displays as stacked fraction"},
    {"\\dfrac", "{numerator}{denominator}", "Display-style fraction (larger)", "\\dfrac{1}{2}",
     NULL},
    {"\\tfrac", "{numerator}{denominator}", "Text-style fraction (smaller)", "\\tfrac{1}{2}", NULL},

    // Roots
    {"\\sqrt", "{expression}", "Square root", "\\sqrt{x^2+1}", "√(x²+1)"},
    {"\\sqrt", "[n]{expression}", "Nth root", "\\sqrt[3]{8}", "³√8"},

    // Big operators with limits
    {"\\sum", "_{lower}^{upper}", "Summation", "\\sum_{i=0}^{n} i", "Σ with limits"},
    {"\\prod", "_{lower}^{upper}", "Product", "\\prod_{i=1}^{n} i", "Π with limits"},
    {"\\int", "_{lower}^{upper}", "Integral", "\\int_0^\\infty e^{-x}dx", "∫ with limits"},
    {"\\iint", "_{domain}", "Double integral", "\\iint_D f(x,y)dA", "∬"},
    {"\\iiint", "_{domain}", "Triple integral", "\\iiint_V f dV", "∭"},
    {"\\oint", "_{curve}", "Contour integral", "\\oint_C F \\cdot dr", "∮"},
    {"\\oiint", "_{surface}", "Surface integral", "\\oiint_S F \\cdot dS", "∯"},
    {"\\bigcup", "_{index}", "Big union", "\\bigcup_{i=1}^n A_i", "⋃"},
    {"\\bigcap", "_{index}", "Big intersection", "\\bigcap_{i=1}^n A_i", "⋂"},
    {"\\coprod", "_{index}", "Coproduct", "\\coprod_{i} X_i", "∐"},

    // Limit-style operators
    {"\\lim", "_{var \\to value}", "Limit", "\\lim_{x \\to 0} \\frac{\\sin x}{x}",
     "lim with subscript"},
    {"\\limsup", "_{n \\to \\infty}", "Limit superior", "\\limsup_{n} a_n", NULL},
    {"\\liminf", "_{n \\to \\infty}", "Limit inferior", "\\liminf_{n} a_n", NULL},
    {"\\max", "_{constraint}", "Maximum", "\\max_{x \\in S} f(x)", NULL},
    {"\\min", "_{constraint}", "Minimum", "\\min_{x} f(x)", NULL},
    {"\\sup", "_{set}", "Supremum", "\\sup_{x} f(x)", NULL},
    {"\\inf", "_{set}", "Infimum", "\\inf_{x} f(x)", NULL},
    {"\\argmax", "_{var}", "Argument of maximum", "\\argmax_x f(x)", NULL},
    {"\\argmin", "_{var}", "Argument of minimum", "\\argmin_x f(x)", NULL},

    // Common functions
    {"\\sin", "", "Sine function", "\\sin(x)", "sin"},
    {"\\cos", "", "Cosine function", "\\cos(\\theta)", "cos"},
    {"\\tan", "", "Tangent function", "\\tan(x)", "tan"},
    {"\\cot", "", "Cotangent function", "\\cot(x)", "cot"},
    {"\\sec", "", "Secant function", "\\sec(x)", "sec"},
    {"\\csc", "", "Cosecant function", "\\csc(x)", "csc"},
    {"\\arcsin", "", "Inverse sine", "\\arcsin(x)", "arcsin"},
    {"\\arccos", "", "Inverse cosine", "\\arccos(x)", "arccos"},
    {"\\arctan", "", "Inverse tangent", "\\arctan(x)", "arctan"},
    {"\\sinh", "", "Hyperbolic sine", "\\sinh(x)", "sinh"},
    {"\\cosh", "", "Hyperbolic cosine", "\\cosh(x)", "cosh"},
    {"\\tanh", "", "Hyperbolic tangent", "\\tanh(x)", "tanh"},
    {"\\log", "", "Logarithm", "\\log(x)", "log"},
    {"\\ln", "", "Natural logarithm", "\\ln(x)", "ln"},
    {"\\exp", "", "Exponential function", "\\exp(x)", "exp"},
    {"\\det", "", "Determinant", "\\det(A)", "det"},
    {"\\gcd", "", "Greatest common divisor", "\\gcd(a,b)", "gcd"},
    {"\\lcm", "", "Least common multiple", "\\lcm(a,b)", "lcm"},
    {"\\Pr", "", "Probability", "\\Pr(A|B)", "Pr"},
    {"\\arg", "", "Argument (of complex number)", "\\arg(z)", "arg"},

    // Binomials
    {"\\binom", "{n}{k}", "Binomial coefficient", "\\binom{n}{k}", "(n choose k)"},
    {"\\tbinom", "{n}{k}", "Text-style binomial", "\\tbinom{n}{k}", NULL},
    {"\\dbinom", "{n}{k}", "Display-style binomial", "\\dbinom{n}{k}", NULL},

    // Matrices
    {"\\matrix", "{rows}", "Plain matrix (no delimiters)", "\\matrix{a & b \\\\ c & d}", NULL},
    {"\\pmatrix", "{rows}", "Matrix with parentheses", "\\pmatrix{a & b \\\\ c & d}", "(a b; c d)"},
    {"\\bmatrix", "{rows}", "Matrix with square brackets", "\\bmatrix{a & b \\\\ c & d}",
     "[a b; c d]"},
    {"\\Bmatrix", "{rows}", "Matrix with curly braces", "\\Bmatrix{a & b \\\\ c & d}",
     "{a b; c d}"},
    {"\\vmatrix", "{rows}", "Matrix with vertical bars (determinant)",
     "\\vmatrix{a & b \\\\ c & d}", "|a b; c d|"},
    {"\\Vmatrix", "{rows}", "Matrix with double vertical bars (norm)",
     "\\Vmatrix{a & b \\\\ c & d}", "‖a b; c d‖"},

    // Special constructs
    {"\\cases", "{lines}", "Piecewise cases", "\\cases{x & x \\ge 0 \\\\ -x & x < 0}", NULL},
    {"\\aligned", "{lines}", "Aligned equations", "\\aligned{a &= b \\\\ c &= d}", NULL},

    // Delimiters
    {"\\left", "(delimiter)", "Auto-sized left delimiter", "\\left( \\frac{a}{b} \\right)", NULL},
    {"\\right", "(delimiter)", "Auto-sized right delimiter", "\\left[ x \\right]", NULL},
    {"\\big", "(delimiter)", "Size 1 delimiter", "\\big(", NULL},
    {"\\Big", "(delimiter)", "Size 2 delimiter", "\\Big(", NULL},
    {"\\bigg", "(delimiter)", "Size 3 delimiter", "\\bigg(", NULL},
    {"\\Bigg", "(delimiter)", "Size 4 delimiter", "\\Bigg(", NULL},
    {"\\lfloor", "", "Left floor", "\\lfloor x \\rfloor", "⌊x⌋"},
    {"\\rfloor", "", "Right floor", "\\lfloor x \\rfloor", "⌊x⌋"},
    {"\\lceil", "", "Left ceiling", "\\lceil x \\rceil", "⌈x⌉"},
    {"\\rceil", "", "Right ceiling", "\\lceil x \\rceil", "⌈x⌉"},
    {"\\langle", "", "Left angle bracket", "\\langle v \\rangle", "⟨v⟩"},
    {"\\rangle", "", "Right angle bracket", "\\langle v \\rangle", "⟨v⟩"},
    {"\\lvert", "", "Left vertical bar", "\\lvert x \\rvert", "|x|"},
    {"\\rvert", "", "Right vertical bar", "\\lvert x \\rvert", "|x|"},
    {"\\Vert", "", "Double vertical bar (norm)", "\\Vert x \\Vert", "‖x‖"},
    {"\\lbrace", "", "Left curly brace", "\\lbrace x \\rbrace", "{x}"},
    {"\\rbrace", "", "Right curly brace", "\\lbrace x \\rbrace", "{x}"},

    {NULL, NULL, NULL, NULL, NULL} // Sentinel
};

// =============================================================================
// Accents and Decorations
// =============================================================================

static const BuiltinDoc g_accents[] = {
    {"\\hat", "{expr}", "Hat accent", "\\hat{x}", "x̂"},
    {"\\widehat", "{expr}", "Wide hat accent", "\\widehat{ABC}", NULL},
    {"\\bar", "{expr}", "Bar accent", "\\bar{x}", "x̄"},
    {"\\overline", "{expr}", "Overline", "\\overline{AB}", NULL},
    {"\\tilde", "{expr}", "Tilde accent", "\\tilde{x}", "x̃"},
    {"\\widetilde", "{expr}", "Wide tilde", "\\widetilde{ABC}", NULL},
    {"\\vec", "{expr}", "Vector arrow", "\\vec{v}", "v⃗"},
    {"\\dot", "{expr}", "Dot accent (derivative)", "\\dot{x}", "ẋ"},
    {"\\ddot", "{expr}", "Double dot (2nd derivative)", "\\ddot{x}", "ẍ"},
    {"\\acute", "{expr}", "Acute accent", "\\acute{e}", "é"},
    {"\\grave", "{expr}", "Grave accent", "\\grave{a}", "à"},
    {"\\breve", "{expr}", "Breve accent", "\\breve{a}", "ă"},
    {"\\check", "{expr}", "Caron/check accent", "\\check{c}", "č"},
    {"\\underline", "{expr}", "Underline", "\\underline{text}", NULL},
    {"\\overbrace", "{expr}^{label}", "Overbrace with label", "\\overbrace{a+b}^{sum}", "╭─┴─╮"},
    {"\\underbrace", "{expr}_{label}", "Underbrace with label", "\\underbrace{a+b}_{sum}", "╰─┬─╯"},
    {"\\overrightarrow", "{expr}", "Right arrow over expression", "\\overrightarrow{AB}", NULL},
    {"\\overleftarrow", "{expr}", "Left arrow over expression", "\\overleftarrow{AB}", NULL},

    {NULL, NULL, NULL, NULL, NULL}};

// =============================================================================
// Math Alphabets / Styles
// =============================================================================

static const BuiltinDoc g_styles[] = {
    {"\\mathbf", "{expr}", "Bold math", "\\mathbf{x}", "𝐱"},
    {"\\mathbb", "{expr}", "Blackboard bold", "\\mathbb{R}", "ℝ"},
    {"\\mathcal", "{expr}", "Calligraphic", "\\mathcal{L}", "ℒ"},
    {"\\mathrm", "{expr}", "Roman (upright)", "\\mathrm{const}", NULL},
    {"\\mathit", "{expr}", "Italic math", "\\mathit{var}", NULL},
    {"\\mathsf", "{expr}", "Sans-serif math", "\\mathsf{A}", NULL},
    {"\\mathtt", "{expr}", "Typewriter math", "\\mathtt{code}", NULL},
    {"\\text", "{text}", "Text in math mode", "\\text{where } x > 0", NULL},
    {"\\textbf", "{text}", "Bold text in math", "\\textbf{Note:}", NULL},
    {"\\textit", "{text}", "Italic text in math", "\\textit{emphasis}", NULL},

    {NULL, NULL, NULL, NULL, NULL}};

// =============================================================================
// Greek Letters
// =============================================================================

static const BuiltinDoc g_greek[] = {
    // Lowercase
    {"\\alpha", "", "Greek alpha", NULL, "α"},
    {"\\beta", "", "Greek beta", NULL, "β"},
    {"\\gamma", "", "Greek gamma", NULL, "γ"},
    {"\\delta", "", "Greek delta", NULL, "δ"},
    {"\\epsilon", "", "Greek epsilon", NULL, "ε"},
    {"\\varepsilon", "", "Greek epsilon (variant)", NULL, "ϵ"},
    {"\\zeta", "", "Greek zeta", NULL, "ζ"},
    {"\\eta", "", "Greek eta", NULL, "η"},
    {"\\theta", "", "Greek theta", NULL, "θ"},
    {"\\vartheta", "", "Greek theta (variant)", NULL, "ϑ"},
    {"\\iota", "", "Greek iota", NULL, "ι"},
    {"\\kappa", "", "Greek kappa", NULL, "κ"},
    {"\\lambda", "", "Greek lambda", NULL, "λ"},
    {"\\mu", "", "Greek mu", NULL, "μ"},
    {"\\nu", "", "Greek nu", NULL, "ν"},
    {"\\xi", "", "Greek xi", NULL, "ξ"},
    {"\\pi", "", "Greek pi", NULL, "π"},
    {"\\varpi", "", "Greek pi (variant)", NULL, "ϖ"},
    {"\\rho", "", "Greek rho", NULL, "ρ"},
    {"\\varrho", "", "Greek rho (variant)", NULL, "ϱ"},
    {"\\sigma", "", "Greek sigma", NULL, "σ"},
    {"\\varsigma", "", "Greek sigma (final)", NULL, "ς"},
    {"\\tau", "", "Greek tau", NULL, "τ"},
    {"\\upsilon", "", "Greek upsilon", NULL, "υ"},
    {"\\phi", "", "Greek phi", NULL, "φ"},
    {"\\varphi", "", "Greek phi (variant)", NULL, "ϕ"},
    {"\\chi", "", "Greek chi", NULL, "χ"},
    {"\\psi", "", "Greek psi", NULL, "ψ"},
    {"\\omega", "", "Greek omega", NULL, "ω"},

    // Uppercase
    {"\\Gamma", "", "Greek Gamma (uppercase)", NULL, "Γ"},
    {"\\Delta", "", "Greek Delta (uppercase)", NULL, "Δ"},
    {"\\Theta", "", "Greek Theta (uppercase)", NULL, "Θ"},
    {"\\Lambda", "", "Greek Lambda (uppercase)", NULL, "Λ"},
    {"\\Xi", "", "Greek Xi (uppercase)", NULL, "Ξ"},
    {"\\Pi", "", "Greek Pi (uppercase)", NULL, "Π"},
    {"\\Sigma", "", "Greek Sigma (uppercase)", NULL, "Σ"},
    {"\\Upsilon", "", "Greek Upsilon (uppercase)", NULL, "Υ"},
    {"\\Phi", "", "Greek Phi (uppercase)", NULL, "Φ"},
    {"\\Psi", "", "Greek Psi (uppercase)", NULL, "Ψ"},
    {"\\Omega", "", "Greek Omega (uppercase)", NULL, "Ω"},

    {NULL, NULL, NULL, NULL, NULL}};

// =============================================================================
// Symbols and Operators
// =============================================================================

static const BuiltinDoc g_symbols[] = {
    // Binary operators
    {"\\times", "", "Multiplication sign", "a \\times b", "×"},
    {"\\cdot", "", "Centered dot", "a \\cdot b", "⋅"},
    {"\\div", "", "Division sign", "a \\div b", "÷"},
    {"\\pm", "", "Plus-minus", "x \\pm y", "±"},
    {"\\mp", "", "Minus-plus", "x \\mp y", "∓"},
    {"\\ast", "", "Asterisk operator", "a \\ast b", "∗"},
    {"\\star", "", "Star operator", "a \\star b", "⋆"},
    {"\\circ", "", "Composition operator", "f \\circ g", "∘"},
    {"\\bullet", "", "Bullet operator", NULL, "•"},

    // Relations
    {"\\leq", "", "Less than or equal", "x \\leq y", "≤"},
    {"\\geq", "", "Greater than or equal", "x \\geq y", "≥"},
    {"\\neq", "", "Not equal", "x \\neq y", "≠"},
    {"\\equiv", "", "Equivalent/identical", "a \\equiv b", "≡"},
    {"\\approx", "", "Approximately equal", "x \\approx y", "≈"},
    {"\\sim", "", "Similar to", "A \\sim B", "∼"},
    {"\\simeq", "", "Similar or equal", "A \\simeq B", "≃"},
    {"\\cong", "", "Congruent", "A \\cong B", "≅"},
    {"\\propto", "", "Proportional to", "y \\propto x", "∝"},
    {"\\ll", "", "Much less than", "x \\ll y", "≪"},
    {"\\gg", "", "Much greater than", "x \\gg y", "≫"},
    {"\\prec", "", "Precedes", "a \\prec b", "≺"},
    {"\\succ", "", "Succeeds", "a \\succ b", "≻"},
    {"\\perp", "", "Perpendicular", "a \\perp b", "⊥"},
    {"\\parallel", "", "Parallel", "a \\parallel b", "∥"},

    // Set theory
    {"\\in", "", "Element of", "x \\in A", "∈"},
    {"\\notin", "", "Not element of", "x \\notin A", "∉"},
    {"\\ni", "", "Contains as element", "A \\ni x", "∋"},
    {"\\subset", "", "Proper subset", "A \\subset B", "⊂"},
    {"\\subseteq", "", "Subset or equal", "A \\subseteq B", "⊆"},
    {"\\supset", "", "Proper superset", "A \\supset B", "⊃"},
    {"\\supseteq", "", "Superset or equal", "A \\supseteq B", "⊇"},
    {"\\cup", "", "Union", "A \\cup B", "∪"},
    {"\\cap", "", "Intersection", "A \\cap B", "∩"},
    {"\\setminus", "", "Set difference", "A \\setminus B", "∖"},
    {"\\emptyset", "", "Empty set", NULL, "∅"},

    // Logic
    {"\\forall", "", "For all", "\\forall x", "∀"},
    {"\\exists", "", "There exists", "\\exists x", "∃"},
    {"\\nexists", "", "Does not exist", "\\nexists x", "∄"},
    {"\\neg", "", "Logical not", "\\neg p", "¬"},
    {"\\land", "", "Logical and", "p \\land q", "∧"},
    {"\\lor", "", "Logical or", "p \\lor q", "∨"},
    {"\\implies", "", "Implies", "p \\implies q", "⇒"},
    {"\\iff", "", "If and only if", "p \\iff q", "⇔"},
    {"\\vdash", "", "Proves/yields", "\\Gamma \\vdash \\phi", "⊢"},
    {"\\models", "", "Models/satisfies", "M \\models \\phi", "⊨"},

    // Arrows
    {"\\to", "", "Right arrow", "f: A \\to B", "→"},
    {"\\rightarrow", "", "Right arrow", "x \\rightarrow y", "→"},
    {"\\leftarrow", "", "Left arrow", "x \\leftarrow y", "←"},
    {"\\leftrightarrow", "", "Left-right arrow", "x \\leftrightarrow y", "↔"},
    {"\\Rightarrow", "", "Double right arrow", "A \\Rightarrow B", "⇒"},
    {"\\Leftarrow", "", "Double left arrow", "A \\Leftarrow B", "⇐"},
    {"\\Leftrightarrow", "", "Double left-right arrow", "A \\Leftrightarrow B", "⇔"},
    {"\\mapsto", "", "Maps to", "x \\mapsto f(x)", "↦"},
    {"\\uparrow", "", "Up arrow", NULL, "↑"},
    {"\\downarrow", "", "Down arrow", NULL, "↓"},
    {"\\nearrow", "", "Northeast arrow", NULL, "↗"},
    {"\\searrow", "", "Southeast arrow", NULL, "↘"},
    {"\\nwarrow", "", "Northwest arrow", NULL, "↖"},
    {"\\swarrow", "", "Southwest arrow", NULL, "↙"},

    // Miscellaneous
    {"\\infty", "", "Infinity", "\\lim_{x \\to \\infty}", "∞"},
    {"\\partial", "", "Partial derivative", "\\frac{\\partial f}{\\partial x}", "∂"},
    {"\\nabla", "", "Nabla/gradient", "\\nabla f", "∇"},
    {"\\hbar", "", "Reduced Planck constant", NULL, "ℏ"},
    {"\\ell", "", "Script l", NULL, "ℓ"},
    {"\\Re", "", "Real part", "\\Re(z)", "ℜ"},
    {"\\Im", "", "Imaginary part", "\\Im(z)", "ℑ"},
    {"\\aleph", "", "Aleph (cardinal)", "\\aleph_0", "ℵ"},
    {"\\wp", "", "Weierstrass p", NULL, "℘"},
    {"\\angle", "", "Angle", "\\angle ABC", "∠"},
    {"\\triangle", "", "Triangle", "\\triangle ABC", "△"},
    {"\\square", "", "Square", NULL, "□"},
    {"\\diamond", "", "Diamond", NULL, "◇"},
    {"\\clubsuit", "", "Club suit", NULL, "♣"},
    {"\\diamondsuit", "", "Diamond suit", NULL, "♢"},
    {"\\heartsuit", "", "Heart suit", NULL, "♡"},
    {"\\spadesuit", "", "Spade suit", NULL, "♠"},

    // Dots
    {"\\ldots", "", "Low dots", "1, 2, \\ldots, n", "…"},
    {"\\cdots", "", "Centered dots", "1 + 2 + \\cdots + n", "⋯"},
    {"\\vdots", "", "Vertical dots", NULL, "⋮"},
    {"\\ddots", "", "Diagonal dots", NULL, "⋱"},

    {NULL, NULL, NULL, NULL, NULL}};

// =============================================================================
// Hyades Calc Commands
// =============================================================================

static const BuiltinDoc g_calc[] = {
    // Variables and assignment
    {"\\let", "<name>{value}", "Create/set a counter variable", "\\let<x>{0}", NULL},
    {"\\assign", "<name>{content}", "Assign content (fully expanded)", "\\assign<text>{Hello}",
     NULL},
    {"\\valueof", "<name>", "Get counter value", "\\valueof<x>", NULL},
    {"\\inc", "<name>", "Increment counter by 1", "\\inc<x>", NULL},
    {"\\dec", "<name>", "Decrement counter by 1", "\\dec<x>", NULL},

    // Lambdas
    {"\\lambda", "<name>[params]{body}", "Define a lambda/function",
     "\\lambda<double>[x]{\\mul{\\recall<x>,2}}", NULL},
    {"\\recall", "<name>[args]", "Call a lambda or get content", "\\recall<double>[5]", NULL},

    // Arithmetic
    {"\\add", "{a,b}", "Addition", "\\add{3,4}", "7"},
    {"\\sub", "{a,b}", "Subtraction", "\\sub{10,3}", "7"},
    {"\\mul", "{a,b}", "Multiplication", "\\mul{3,4}", "12"},
    {"\\div", "{a,b}", "Integer division", "\\div{10,3}", "3"},
    {"\\mod", "{a,b}", "Modulo", "\\mod{10,3}", "1"},

    // Comparisons (return 1 for true, 0 for false)
    {"\\eq", "{a,b}", "Equal", "\\eq{1,1}", "1"},
    {"\\ne", "{a,b}", "Not equal", "\\ne{1,2}", "1"},
    {"\\lt", "{a,b}", "Less than", "\\lt{1,2}", "1"},
    {"\\gt", "{a,b}", "Greater than", "\\gt{2,1}", "1"},
    {"\\le", "{a,b}", "Less than or equal", "\\le{1,1}", "1"},
    {"\\ge", "{a,b}", "Greater than or equal", "\\ge{2,1}", "1"},

    // Logic
    {"\\and", "{a,b}", "Logical AND", "\\and{1,1}", "1"},
    {"\\or", "{a,b}", "Logical OR", "\\or{0,1}", "1"},
    {"\\not", "{a}", "Logical NOT", "\\not{0}", "1"},

    // Control flow
    {"\\if", "{cond}{then}\\else{else}", "Conditional",
     "\\if{\\gt{x,0}}{positive}\\else{non-positive}", NULL},
    {"\\begin{loop}", "", "Start a loop", "\\begin{loop}...\\end{loop}", NULL},
    {"\\exit_when", "{condition}", "Exit loop when true", "\\exit_when{\\ge{\\valueof<i>,10}}",
     NULL},

    // Arrays
    {"\\let", "<arr[]>{[values]}", "Create counter array", "\\let<nums[]>{[1,2,3]}", NULL},
    {"\\push", "<arr>{value}", "Push to array/stack", "\\push<stack>{item}", NULL},
    {"\\pop", "<arr>", "Pop from array/stack", "\\pop<stack>", NULL},
    {"\\peek", "<arr>", "Peek top of stack", "\\peek<stack>", NULL},
    {"\\enqueue", "<arr>{value}", "Add to queue", "\\enqueue<q>{item}", NULL},
    {"\\dequeue", "<arr>", "Remove from queue", "\\dequeue<q>", NULL},
    {"\\len", "<arr>", "Array length", "\\len<nums>", NULL},

    {NULL, NULL, NULL, NULL, NULL}};

// =============================================================================
// Hyades Document Commands
// =============================================================================

static const BuiltinDoc g_document[] = {
    // Macros
    {"\\macro", "<\\name[params]>{body}", "Define a macro",
     "\\macro<\\greet[name]>{Hello ${name}!}", NULL},

    // Layout
    {"\\begin{hbox}", "", "Horizontal box", "\\begin{hbox}...\\end{hbox}", NULL},
    {"\\begin{vbox}", "", "Vertical box", "\\begin{vbox}...\\end{vbox}", NULL},
    {"\\child", "[width]{content}", "Child in hbox/vbox", "\\child[10]{text}", NULL},

    // Settings
    {"\\setunicode", "{true|false}", "Enable/disable Unicode output", "\\setunicode{true}", NULL},
    {"\\setwidth", "{n}", "Set output width", "\\setwidth{80}", NULL},
    {"\\setmathitalic", "{true|false}", "Enable/disable italic math", "\\setmathitalic{true}",
     NULL},

    // Verbatim
    {"\\verb", "|content|", "Verbatim content (any delimiter)", "\\verb|\\frac|", NULL},

    // Miscellaneous
    {"\\measure", "<c,w,h>{content}", "Measure content dimensions", "\\measure<c,w,h>{text}", NULL},
    {"\\lineroutine", "<func>{content}", "Apply function to each line",
     "\\lineroutine<prefix>{text}", NULL},
    {"\\ref", "<name>", "Reference a hygienized name", "\\ref<counter>", NULL},

    {NULL, NULL, NULL, NULL, NULL}};

// =============================================================================
// Lookup Function
// =============================================================================

static const BuiltinDoc *builtin_doc_lookup(const char *name) {
    if (!name) return NULL;

    // Search all tables
    const BuiltinDoc *tables[] = {g_math_constructs, g_accents, g_styles,   g_greek,
                                  g_symbols,         g_calc,    g_document, NULL};

    for (int t = 0; tables[t]; t++) {
        for (int i = 0; tables[t][i].name; i++) {
            if (strcmp(tables[t][i].name, name) == 0) {
                return &tables[t][i];
            }
        }
    }

    return NULL;
}

// Format documentation as markdown for LSP hover
// Returns allocated string, caller must free
static char *builtin_doc_format(const BuiltinDoc *doc) {
    if (!doc) return NULL;

    char *buf = malloc(1024);
    if (!buf) return NULL;

    int len = 0;

    // Command with signature
    len += snprintf(buf + len, 1024 - len, "**%s**", doc->name);
    if (doc->signature && doc->signature[0]) {
        len += snprintf(buf + len, 1024 - len, "%s", doc->signature);
    }
    len += snprintf(buf + len, 1024 - len, "\n\n");

    // Description
    len += snprintf(buf + len, 1024 - len, "%s", doc->description);

    // Renders as
    if (doc->renders_as && doc->renders_as[0]) {
        len += snprintf(buf + len, 1024 - len, " → %s", doc->renders_as);
    }
    len += snprintf(buf + len, 1024 - len, "\n");

    // Example
    if (doc->example && doc->example[0]) {
        len += snprintf(buf + len, 1024 - len, "\n*Example:* `%s`", doc->example);
    }

    return buf;
}

#endif // BUILTIN_DOCS_H
