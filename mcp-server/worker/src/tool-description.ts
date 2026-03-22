// Filtered version of llms-full.txt for the MCP tool description.
// Removes: stdlib macros (Section 7), Cassilda (Section 11), Subnivean VM,
// CLI-specific sections, and examples that depend on stdlib.

export const TOOL_DESCRIPTION = `\
Render Hyades source to Unicode/ASCII text art. Hyades uses LaTeX-like syntax \
but outputs multi-line plain text instead of PDF. It supports math (fractions, \
integrals, matrices, Greek letters), tables with box-drawing frames, lists, \
flex-like layout (hbox/vbox), user-defined macros, and computation.

IMPORTANT: Always display the rendered output to the user directly in your response. \
The output is pre-formatted multi-line text art that depends on exact character \
alignment. You MUST use a fenced code block (triple backticks) or equivalent \
monospace/preformatted element to preserve spacing. Never display the output as \
regular prose text — column alignment, box-drawing characters, and fraction bars \
will break without a monospace font.

Input is a complete Hyades document. Plain text outside math delimiters is \
rendered as prose paragraphs. Use \`$...$\` for inline math and \`$$...$$\` for \
display math (centered on its own line).

---

## Math

Supports ~95% of LaTeX math syntax.

### Inline vs Display

\`\`\`
Inline math: $a^2 + b^2 = c^2$ flows with text.

Display math gets its own block:

$$ a^2 + b^2 = c^2 $$
\`\`\`

Use \`$...$\` for inline, \`$$...$$\` for display. Display math is centered.

### Variables and Operators

\`\`\`
$$ a + b - c $$
$$ a \\times b \\div c $$
$$ a \\cdot b $$
$$ \\pm x \\quad \\mp y $$
\`\`\`

### Superscripts and Subscripts

\`\`\`
$$ x^2 \\quad x_i \\quad x_i^2 \\quad x^{n+1} \\quad x_{i,j} \\quad a^{b^c} $$
\`\`\`

Use braces for multi-character exponents/subscripts: \`x^{n+1}\`, \`x_{i,j}\`.

**Bracing rules:** Use braces for multi-token expressions: \`x^{n+1}\`, \`x_{i,j}\`. \
Commands that take arguments (like \`\\mathbf{v}\`) work without extra braces: \
\`x_\\mathbf{v}\` is equivalent to \`x_{\\mathbf{v}}\`, matching TeX behavior.

### Fractions

\`\`\`
$$ \\frac{a}{b} $$
$$ \\frac{x^2 + 1}{\\sqrt{x^2 - 1}} $$
$$ \\frac{\\partial f}{\\partial x} $$
\`\`\`

Fractions nest to infinite depth. \`\\dfrac\` and \`\\tfrac\` are accepted as aliases for \`\\frac\`.

### Roots

\`\`\`
$$ \\sqrt{x} $$
$$ \\sqrt[3]{x} $$
$$ \\sqrt[n]{x^2 + y^2} $$
\`\`\`

Optional argument gives the nth root: \`\\sqrt[3]{x}\`.

### Parentheses and Grouping

\`\`\`
$$ (a + b) \\quad [a + b] \\quad \\{a + b\\} \\quad |x| \\quad \\Vert x \\Vert $$
\`\`\`

Curly braces need escaping: \`\\{\` and \`\\}\`.

### Auto-Scaling Delimiters (\\left/\\right)

\`\\left\` and \`\\right\` make delimiters grow to match their contents:

\`\`\`
$$ \\left( \\frac{a}{b} \\right) \\quad \\left[ \\frac{a}{b} \\right] \\quad \\left| \\frac{a}{b} \\right| $$
\`\`\`

Works with all delimiter types: \`( ) [ ] \\{ \\} | \\| \\lfloor \\rfloor \\lceil \\rceil \\langle \\rangle\`.

Use \`\\left.\` or \`\\right.\` for invisible (null) delimiters — essential for evaluation bars:

\`\`\`
$$ \\left.\\frac{df}{dx}\\right|_{x=0} $$
\`\`\`

Use \`\\middle\` inside \`\\left...\\right\` for scaled interior delimiters:

\`\`\`
$$ \\left( \\frac{a}{b} \\middle| \\frac{c}{d} \\right) $$
\`\`\`

### Explicit Delimiter Sizing (\\big through \\Bigg)

\`\\big\`, \`\\Big\`, \`\\bigg\`, \`\\Bigg\` set delimiter sizes explicitly.

\`\\bigl\`/\`\\bigr\`, \`\\Bigl\`/\`\\Bigr\`, \`\\biggl\`/\`\\biggr\`, \`\\Biggl\`/\`\\Biggr\` produce standalone one-sided delimiters:

\`\`\`
$$ f(x)\\bigr|_{x=0} $$
\`\`\`

### Floor and Ceiling

\`\`\`
$$ \\lfloor x \\rfloor $$
$$ \\lceil x \\rceil $$
$$ \\left\\lfloor \\frac{x}{2} \\right\\rfloor $$
\`\`\`

### Greek Letters

Lowercase: \`\\alpha \\beta \\gamma \\delta \\epsilon \\zeta \\eta \\theta \\lambda \\mu \\nu \\xi \\pi \\rho \\sigma \\tau \\phi \\varphi \\chi \\psi \\omega\`

Uppercase (only where different from Latin): \`\\Gamma \\Delta \\Theta \\Lambda \\Xi \\Pi \\Sigma \\Phi \\Psi \\Omega\`

### Relations

\`\`\`
$$ a \\neq b \\quad a \\leq b \\quad a \\geq b $$
$$ a \\ll b \\quad a \\gg b $$
$$ a \\approx b \\quad a \\equiv b \\quad a \\sim b \\quad a \\propto b $$
$$ a \\prec b \\quad a \\succ b \\quad a \\preceq b \\quad a \\succeq b $$
\`\`\`

Definition symbols: \`\\coloneqq\` (≔), \`\\eqqcolon\` (≕), and \`:=\` (parsed as a single relation ≔):

\`\`\`
$$ \\Phi := S $$
$$ f \\coloneqq x^2 + 1 $$
\`\`\`

Colon alone is treated as punctuation (no space before, space after), useful in set-builder notation:

\`\`\`
$$ \\{x : x > 0\\} $$
\`\`\`

The \`\\not\` prefix negates a relation: \`\\not=\` → ≠, \`\\not\\in\` → ∉, \`\\not\\leq\` → ≰, \`\\not\\equiv\` → ≢, \`\\not\\subset\` → ⊄, \`\\not\\exists\` → ∄

### Set Theory

\`\`\`
$$ x \\in A \\quad x \\notin A $$
$$ A \\subset B \\quad A \\subseteq B \\quad A \\cup B \\quad A \\cap B \\quad \\emptyset $$
$$ A \\setminus B $$
\`\`\`

### Logic

\`\`\`
$$ \\forall x \\in \\mathbb{R} \\quad \\exists x $$
$$ p \\implies q \\quad p \\iff q $$
$$ \\neg p \\quad p \\land q \\quad p \\lor q $$
$$ \\therefore \\quad \\because \\quad \\top \\quad \\bot $$
\`\`\`

### Arrows

\`\\rightarrow\`, \`\\Rightarrow\`, \`\\mapsto\`, \`\\longmapsto\`, \`\\hookrightarrow\`, \`\\hookleftarrow\`, \`\\leftarrow\`, \`\\leftrightarrow\`, \`\\Leftarrow\`, \`\\Leftrightarrow\`, \`\\uparrow\`, \`\\downarrow\`, \`\\updownarrow\`, \`\\nearrow\`, \`\\searrow\`, \`\\nwarrow\`, \`\\swarrow\`.

Extensible arrows with text labels:

\`\`\`
$$ A \\xrightarrow{f} B \\xleftarrow{g} C $$
\`\`\`

### Standard Functions

Standard functions are set in upright type:

\`\`\`
$$ \\sin x \\quad \\cos(x) \\quad \\tan x $$
$$ \\log x \\quad \\log_2 x \\quad \\ln x \\quad \\exp(x) $$
$$ \\sin^2 x + \\cos^2 x = 1 $$
\`\`\`

Also: \`\\arcsin\`, \`\\arccos\`, \`\\arctan\`, \`\\sinh\`, \`\\cosh\`, \`\\tanh\`, \`\\det\`, \`\\dim\`, \`\\ker\`, \`\\deg\`, \`\\gcd\`, \`\\sup\`, \`\\inf\`, \`\\max\`, \`\\min\`, \`\\argmax\`, \`\\argmin\`, \`\\limsup\`, \`\\liminf\`.

### Custom Function Names (\\fn / \\operatorname)

\`\\fn{name}\` or \`\\operatorname{name}\` produces an upright function name for functions Hyades doesn't know:

\`\`\`
$$ \\fn{softmax}\\left(x_i\\right) = \\frac{e^{x_i}}{\\sum_j e^{x_j}} $$
$$ \\operatorname{Tr}(A) $$
\`\`\`

### Text in Math (\\text)

\`\\text{words}\` inserts upright text inside math mode:

\`\`\`
$$ x = 0 \\text{ if } y > 0 $$
\`\`\`

### Accents and Decorations

Single-character: \`\\hat{x}\`, \`\\bar{x}\`, \`\\tilde{x}\`, \`\\vec{x}\`, \`\\dot{x}\`, \`\\ddot{x}\`, \`\\acute{x}\`, \`\\grave{x}\`, \`\\breve{x}\`, \`\\check{x}\`

Wide accents: \`\\overline{AB}\`, \`\\underline{text}\`, \`\\overrightarrow{AB}\`, \`\\overleftarrow{AB}\`, \`\\widehat{xyz}\`, \`\\widetilde{xyz}\`

### Overset and Underset

\`\`\`
$$ \\overset{n}{=} $$
$$ \\underset{x}{y} $$
$$ \\stackrel{\\text{def}}{=} $$
\`\`\`

\`\\overset{top}{base}\` places annotation above base; \`\\underset{bot}{base}\` below. \`\\stackrel\` is an alias for \`\\overset\`.

### Overbrace and Underbrace

\`\`\`
$$ \\overbrace{a + b + c}^{3 \\text{ terms}} $$
$$ \\underbrace{x + y + z}_{n \\text{ terms}} $$
\`\`\`

### Primes

\`\`\`
$$ f'(x) \\quad f''(x) \\quad f'''(x) \\quad f^{(n)}(x) $$
\`\`\`

### Math Fonts

\`\\mathbf{v}\` → bold, \`\\mathbb{R}\` → blackboard bold (ℕℤℚℝℂ), \`\\mathcal{L}\` → calligraphic, \`\\mathfrak{R}\` → Fraktur (𝔄𝔅ℭ), \`\\mathsf{A}\` → sans-serif (𝖠𝖡𝖢), \`\\mathscr{A}\` → script (same as \\mathcal), \`\\boldsymbol{\\alpha}\` → bold (works with Greek: 𝛂𝛃, and symbols: \`\\boldsymbol{\\nabla}\` → 𝛁, \`\\boldsymbol{\\partial}\` → 𝛛)

### Dots and Ellipses

\`\\ldots\` (low dots, for commas), \`\\cdots\` (centered, for operators), \`\\vdots\` (vertical), \`\\ddots\` (diagonal, for matrices)

### Summation

\`\`\`
$$ \\sum_{i=1}^{n} x_i $$
\`\`\`

\`\\Sum\` and \`\\SUM\` give progressively larger variants.

### Products

\`\`\`
$$ \\prod_{i=1}^{n} x_i $$
\`\`\`

\`\\Prod\` and \`\\PROD\` give larger variants.

### Integrals

\`\`\`
$$ \\int_a^b f(x) \\, dx $$
$$ \\iint f(x,y) \\, dx\\,dy $$
$$ \\oint_C f(z) \\, dz $$
\`\`\`

Also: \`\\iiint\`, \`\\oiint\`. Larger variants: \`\\Int\`, \`\\INT\`, etc.

### Limits

\`\\lim\` places its subscript directly below:

\`\`\`
$$ \\lim_{x \\to 0} \\frac{\\sin x}{x} = 1 $$
$$ \\max_{x \\in [0,1]} f(x) $$
$$ \\argmax_{\\theta} L(\\theta) $$
\`\`\`

### Calculus Operators

\`\`\`
$$ \\frac{dy}{dx} \\quad \\frac{d^2y}{dx^2} \\quad \\frac{\\partial f}{\\partial x} $$
$$ \\nabla f \\quad \\nabla \\cdot \\mathbf{F} \\quad \\nabla \\times \\mathbf{F} \\quad \\nabla^2 f $$
\`\`\`

### Matrices

Six types with different delimiters. \`&\` separates columns, \`\\\\\` or \`;\` separates rows.
LaTeX-style \`\\begin{pmatrix}...\\end{pmatrix}\` also works for all matrix types.

\`\`\`
$$ \\pmatrix{a & b \\\\ c & d} $$    % (parentheses)
$$ \\bmatrix{a & b \\\\ c & d} $$    % [brackets]
$$ \\Bmatrix{a & b \\\\ c & d} $$    % {braces}
$$ \\vmatrix{a & b \\\\ c & d} $$    % |bars| (determinants)
$$ \\Vmatrix{a & b \\\\ c & d} $$    % ‖double bars‖ (norms)
$$ \\matrix{a & b \\\\ c & d} $$     % no delimiters
\`\`\`

### Cases (Piecewise Functions)

\`\`\`
$$f(x) = \\cases{
    x    & \\text{if } x > 0 \\\\
    0    & \\text{if } x = 0 \\\\
    -x   & \\text{if } x < 0
}$$
\`\`\`

### Aligned Equations

\`&\` marks the alignment point:

\`\`\`
$$\\aligned{
    f(x) &= x^2 + 2x + 1 \\\\
         &= (x + 1)^2
}$$
\`\`\`

Use \`\\intertext{...}\` inside aligned to insert prose between rows. Intertext is left-aligned at the document margin while equations remain centered:

\`\`\`
$$\\aligned{
    x &= a \\\\
    \\intertext{where}
    a &= b + c
}$$
\`\`\`

\`\\tag\` works on aligned blocks — the tag is right-justified and vertically centered:

\`\`\`
$$\\aligned{
    f(x) &= x^2 + 2x + 1 \\\\
         &= (x + 1)^2
} \\tag{7}$$
\`\`\`

### Boxed

\`\`\`
$$ \\boxed{E = mc^2} $$
\`\`\`

Draws a box-drawing frame around the content.

### Phantom

\`\`\`
$$ a + \\phantom{bbb} + c $$
\`\`\`

Invisible placeholder — renders as whitespace with the same dimensions as the content.

### Smash

\`\`\`
$$ \\sqrt{\\smash{y^3}} $$
\`\`\`

\`\\smash{content}\` renders content but collapses its height to a single line (the baseline row). Used to prevent tall content from affecting surrounding constructs like roots or delimiters.

### Substack

\`\`\`
$$ \\sum_{\\substack{i=1 \\\\ j>0}} x_{ij} $$
\`\`\`

Stacks multiple lines vertically (for multi-line subscripts/superscripts).

### Tag

\`\`\`
$$ E = mc^2 \\tag{1} $$
\`\`\`

Equation tag rendered as \`(text)\`.

### Number Theory

\`\`\`
$$ a \\equiv b \\pmod{n} $$
$$ a \\bmod n \\quad a \\mid b $$
$$ \\binom{n}{k} $$
\`\`\`

### Style Commands (No-ops)

\`\\displaystyle\`, \`\\textstyle\`, \`\\scriptstyle\`, \`\\scriptscriptstyle\` are accepted but transparent (Hyades has no math size concept). \`\\notag\` and \`\\nonumber\` are also silently ignored. This allows pasting LaTeX source that uses these commands without errors.

### Math Spacing

\`\\!\` (negative thin), \`\\,\` (thin), \`\\:\` (medium), \`\\;\` (thick), \`\\quad\` (1em), \`\\qquad\` (2em)

### Atom-Type Overrides (\\mathord, \\mathbin, \\mathrel)

Override the default spacing category of a symbol. Analogous to TeX's \`{=}\` idiom for suppressing operator spacing, but explicit and readable:

\`\`\`
$a \\mathord{=} b$          %% "a=b"  — = treated as ordinary symbol (no spacing)
$a = b$                    %% "a = b" — default relation spacing
$f \\mathrel{:} A \\to B$    %% "f : A → B" — : treated as relation (symmetric spacing)
$a : b$                    %% "a: b"  — default punctuation spacing
$a \\mathbin{\\triangle} b$  %% "a △ b" — triangle with binary operator spacing
\`\`\`

- \`\\mathord{X}\`: Suppress spacing — X renders as an ordinary symbol with no surrounding gaps. Use this instead of TeX's \`{=}\` or \`{}=\` idiom which does not work in Hyades.
- \`\\mathbin{X}\`: Force binary operator spacing (gap on both sides).
- \`\\mathrel{X}\`: Force relation spacing (gap on both sides).

### Binary Operators

\`\\oplus\`, \`\\otimes\`, \`\\odot\`, \`\\circ\`, \`\\bullet\`, \`\\star\`, \`\\dagger\`, \`\\ddagger\`

### Geometry

\`\\angle\`, \`\\triangle\`, \`\\perp\`, \`\\parallel\`

### Famous Equations (all valid input)

\`\`\`
$$ x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a} $$                                                   %% Quadratic formula
$$ e^{i\\pi} + 1 = 0 $$                                                                            %% Euler's identity
$$ f(x) = \\frac{1}{\\sigma\\sqrt{2\\pi}} e^{-\\frac{(x-\\mu)^2}{2\\sigma^2}} $$                        %% Normal distribution
$$ \\fn{Attention}(Q, K, V) = \\fn{softmax}\\left(\\frac{QK^\\top}{\\sqrt{d_k}}\\right) V $$            %% Attention mechanism
$$ e^x = \\sum_{n=0}^{\\infty} \\frac{x^n}{n!} $$                                                   %% Taylor series
$$ \\int_{-\\infty}^{\\infty} e^{-x^2} dx = \\sqrt{\\pi} $$                                           %% Gaussian integral
$$ P(A|B) = \\frac{P(B|A) P(A)}{P(B)} $$                                                           %% Bayes' theorem
$$ \\nabla \\times \\mathbf{E} = -\\frac{\\partial \\mathbf{B}}{\\partial t} $$                          %% Maxwell's equation
$$ i\\hbar \\frac{\\partial}{\\partial t} \\Psi = \\hat{H} \\Psi $$                                      %% Schrodinger equation
$$ \\fn{softmax}(x_i) = \\frac{e^{x_i}}{\\sum_j e^{x_j}} $$                                         %% Softmax
\`\`\`

---

## Text Formatting

\`\`\`
\\textbf{bold text}
\\textit{italic text}
\\texttt{monospace text}
\\textbf{\\textit{bold italic}}
\`\`\`

Nesting \`\\textbf\` and \`\\textit\` produces bold-italic text.

Inline math \`$...$\` inside \`\\textit\` or \`\\textbf\` is rendered as math (not styled as text):

\`\`\`
\\textit{Let $f(x) = x^2$ be a function}
\`\`\`

### Verbatim (No Processing)

\`\`\`
\\verb|raw \\commands { } are preserved|
\`\`\`

### Special Characters

\`\\textbackslash\` → \\, \`\\textdollar\` → $, \`\\textpercent\` → %, \`\\textampersand\` → &, \`\\texthash\` → #

### Text Spacing Commands

\`\\ \` (backslash-space) — explicit inter-word space. Useful after abbreviations: \`i.e.\\ \$f\$\` renders without extra sentence spacing.
\`\\@\` — marks the next period as sentence-ending (silently consumed).
\`\\,\` — thin space (works in both text and math mode).

---

## Paragraphs and Line Breaking

Text flows into paragraphs automatically. A single newline is a space; a blank line starts a new paragraph.

### Line-Breaking Modes

\`\`\`
\\linebreaker{greedy}       % Default. Fast, good results.
\\linebreaker{knuth}        % TeX-style optimal. Minimizes ugliness.
\\linebreaker{raggedright}  % Left-aligned, no justification.
\`\`\`

### Hyphenation

\`\\sethyphenate{true}\` / \`\\sethyphenate{false}\`

---

## Tables

Tables use \`\\table\` with nested \`\\row\` and \`\\col\`:

\`\`\`
\\table[width:40, frame:single, pad:{l:1,r:1}]{
    \\row[frame:{b:double}]{
        \\col{Name}
        \\col{Value}
    }
    \\row{
        \\col{Alpha}
        \\col{0.05}
    }
    \\row{
        \\col{Beta}
        \\col{0.95}
    }
}
\`\`\`

### Table Properties

| Property | Values | Default |
|----------|--------|---------|
| \`width\` / \`w\` | number, \`auto\` | parent width |
| \`frame\` / \`f\` | \`none\`, \`dotted\`, \`single\`, \`rounded\`, \`double\`, \`bold\` | \`single\` |
| \`border\` | same as frame (outer edges only) | unset |
| \`pad\` / \`p\` | number or \`{t:N, b:N, l:N, r:N}\` | 0 |
| \`align\` / \`a\` | \`l\`/\`left\`, \`c\`/\`center\`, \`r\`/\`right\` | \`l\` |
| \`valign\` / \`va\` | \`t\`/\`top\`, \`m\`/\`middle\`, \`b\`/\`bottom\` | \`t\` |

Row properties: \`frame\`, \`pad\`, \`align\`, \`valign\`, \`height\`.
Column properties: \`width\`, \`align\`, \`valign\`, \`frame\`, \`pad\`, \`span\`, \`reset\`.

Frame styles: \`none\` (space), \`dotted\` (┄┆), \`single\` (─│), \`rounded\` (╭╮╰╯ corners), \`double\` (═║), \`bold\` (━┃)

Compound edges: \`frame:{t:double, b:single, l:none, r:none}\`, \`pad:{l:1, r:1}\`

Properties cascade: table → column (down rows) → row → cell.

IMPORTANT: When a row overrides a property (e.g. \`frame:{b:double}\`), that override is inherited by ALL subsequent rows. To stop it, the next row must explicitly reset it (e.g. \`frame:{b:single}\`). The \`border\` property on the table level controls the outer edges and cannot be overridden by row or column \`frame\`.

---

## Lists

\`\`\`
\\fancylist{
    - First item
    - Second item with longer text that
      wraps to the next line
    - Third item
      - Nested child
        - Grandchild
}
\`\`\`

Three nesting levels supported. Uses ●/○ bullets in Unicode, - in ASCII.

---

## Layout Primitives

Flex-like box model for spatial control.

### Box Types

\`\`\`
\\begin[width]{hbox}              % horizontal (side by side)
    \\child[width][align]{content}
\\end{hbox}

\\begin[top|middle|bottom]{hbox}  % hbox with vertical alignment
    \\child[width][align]{content}
\\end{hbox}

\\begin[width]{vbox}              % vertical (stacked)
    \\child[width][align]{content}
\\end{vbox}
\`\`\`

Child width: fixed number, \`auto\` (fill remaining), \`intrinsic\` (natural width), or omitted (equal share).
When multiple children omit width, they split the remaining space equally (after fixed-width children).
Horizontal alignment: \`left\`, \`center\`, \`right\`.
Vertical alignment for hbox: set on \`\\begin\` bracket — \`top\` (default), \`middle\`, \`bottom\`.

Width inheritance: boxes without an explicit width inherit their parent's width. \`\\setwidth{N}\` sets the document-level width.

### Rules

\`\`\`
\\hrule[width]{left_cap}{fill_char}{right_cap}
\\vrule[height]{top}{fill}{bottom}
\\intersect_rules{...}          % fix junction characters at rule crossings
\`\`\`

Width/height can be a number or \`auto\`. An \`auto\`-width \`\\hrule\` in a vbox stretches to the box width. An \`auto\`-height \`\\vrule\` in an hbox stretches to match the tallest sibling.

\`\\intersect_rules\` scans all cells, checks neighboring line arms, and replaces with correct junction glyphs (┌ ┬ ┐ ├ ┼ ┤ └ ┴ ┘). Handles single, double, bold, and mixed styles.

### Measuring Content

\`\`\`
\\measure<content_name,width_var,height_var>{content to measure}
\`\`\`

Renders content invisibly, stores it under \`content_name\` (retrieve with \`\\recall<content_name>\`), and stores the measured width and height into integer variables.

### Spacing

\`\\vskip{N}\` (N blank lines), \`\\hskip{N}\` (N spaces)

### Common Layout Patterns

Two-column with gutter:
\`\`\`
\\begin{hbox}
    \\child{Left column}
    \\child[2]{}
    \\child{Right column}
\\end{hbox}
\`\`\`

Centering via auto spacers:
\`\`\`
\\begin{hbox}
    \\child[auto]{}
    \\child[intrinsic]{Centered content}
    \\child[auto]{}
\\end{hbox}
\`\`\`

Indentation:
\`\`\`
\\begin{hbox}
    \\child[4]{}
    \\child[auto]{Indented content}
\\end{hbox}
\`\`\`

---

## User-Defined Macros

\`\`\`
\\macro<\\greet{name}>{Hello, \${name}!}
\\greet{World}       % → Hello, World!

\\macro<\\fullname{first}{last}>{\${first} \${last}}
\\fullname{John}{Doe}  % → John Doe

% Optional parameters with defaults
\\macro<\\heading[char=-]{title}>{
\${title}
\\hrule[20]{}{\${char}}{}
}
\\heading{Default}           % uses -
\\heading[char:=]{Custom}    % uses =
\`\`\`

---

## Document Settings

\`\`\`
\\setwidth{80}              % Output width in columns (default: 80)
\\setunicode{true}          % Use Unicode symbols (default: true)
\\setmathitalic{true}       % Italicize math variables (default: true)
\\linebreaker{greedy}       % Line-breaking algorithm
\\sethyphenate{true}        % Enable hyphenation
\\setparskip{1}             % Blank lines between paragraphs
\\setmathabove{1}           % Blank lines above display math (default: 0)
\\setmathbelow{1}           % Blank lines below display math (default: 0)
\`\`\`

---

## Computation

Variables, loops, and conditionals are available.

### Variables and Arithmetic

\`\`\`
\\let<x>{10}
\\let<y>{20}
\\let<sum>{\\add{\\valueof<x>,\\valueof<y>}}
Result: \\valueof<sum>       % → Result: 30
\`\`\`

### Conditionals

\`\`\`
\\let<n>{42}
\\if{\\gt{\\valueof<n>,0}}{positive}\\else{non-positive}
\`\`\`

### Loops

\`\`\`
\\let<i>{1}
\\begin{loop}
    \\exit_when{\\gt{\\valueof<i>,5}}
    \\valueof<i>\\hskip{1}
    \\inc<i>
\\end{loop}
% Output: 1 2 3 4 5
\`\`\`

### Content Storage

\`\\assign<name>{content}\` stores text, \`\\recall<name>\` retrieves it.
\`\\let\`/\`\\valueof\` is for integers; \`\\assign\`/\`\\recall\` is for text.

### Lambdas

\`\`\`
\\lambda<double>[x]{\\mul{\\recall<x>,2}}
\\recall<double>[5]             % → 10
\`\`\`

### Arrays

\`\`\`
\\let<nums[]>{[10, 20, 30]}
\\valueof<nums>[0]              % → 10
\\len<nums>                     % → 3
\\push<nums>{40}                % append
\`\`\`

---

## Quick Reference

### Math Mode

| Input | Output |
|-------|--------|
| \`$x^2$\` | x² |
| \`$x_i$\` | xᵢ |
| \`$\\frac{a}{b}$\` | a/b (stacked) |
| \`$\\sqrt{x}$\` | √x |
| \`$\\sum_{i=1}^n$\` | Σ with limits |
| \`$\\int_a^b$\` | ∫ with limits |
| \`$\\prod_{i=1}^n$\` | ∏ with limits |
| \`$\\lim_{x \\to 0}$\` | lim with subscript below |
| \`$\\limsup_{n}$\` | lim sup with subscript below |
| \`$\\liminf_{n}$\` | lim inf with subscript below |
| \`$\\alpha, \\beta, \\gamma$\` | α, β, γ |
| \`$\\mathbb{R}$\` | ℝ |
| \`$\\mathbf{v}$\` | 𝐯 |
| \`$\\leq, \\geq, \\neq$\` | ≤, ≥, ≠ |
| \`$\\in, \\forall, \\exists$\` | ∈, ∀, ∃ |
| \`$\\rightarrow, \\Rightarrow$\` | →, ⇒ |
| \`$\\infty, \\partial, \\nabla$\` | ∞, ∂, ∇ |
| \`$f'(x)$\` | 𝑓′(𝑥) |
| \`$\\binom{n}{k}$\` | binomial coefficient |
| \`$\\lfloor x \\rfloor$\` | ⌊x⌋ |
| \`$\\fn{name}$\` | custom function name |
| \`$\\operatorname{name}$\` | same as \\fn |
| \`$\\text{words}$\` | upright text in math |
| \`$\\ll, \\gg$\` | ≪, ≫ |
| \`$\\mathord{=}$\` | suppress operator spacing |
| \`$\\mathbin{:}$\` | force binary operator spacing |
| \`$\\mathrel{:}$\` | force relation spacing |
| \`$\\prec, \\succ$\` | ≺, ≻ |
| \`$\\neg, \\land, \\lor$\` | ¬, ∧, ∨ |
| \`$\\subseteq, \\setminus$\` | ⊆, ∖ |
| \`$\\mid$\` | ∣ (divides) |
| \`$\\therefore, \\because$\` | ∴, ∵ |
| \`$\\wp, \\imath, \\jmath$\` | ℘, ı, ȷ |
| \`$\\Box$\` | □ |
| \`$\\not=$\` | ≠ (\\not prefix negates relations) |
| \`$\\left.\\right\\|$\` | invisible delimiter (evaluation bar) |
| \`$\\overset{n}{=}$\` | annotation above base |
| \`$\\underset{x}{y}$\` | annotation below base |
| \`$\\boxed{x}$\` | framed box around content |
| \`$\\phantom{x}$\` | invisible spacer |
| \`$\\smash{x}$\` | collapse height to baseline |
| \`$\\xrightarrow{f}$\` | extensible arrow with label |
| \`$\\substack{a \\\\ b}$\` | stacked lines |
| \`$\\mathfrak{R}$\` | Fraktur (𝔄𝔅ℭ) |
| \`$\\mathsf{A}$\` | sans-serif (𝖠𝖡𝖢) |
| \`$\\boldsymbol{\\alpha}$\` | bold Greek (𝛂𝛃) |
| \`$\\coloneqq$\` | ≔ (definition) |
| \`$\\eqqcolon$\` | ≕ (reverse definition) |
| \`$a := b$\` | ≔ (combined relation) |

### Text Commands

| Command | Effect |
|---------|--------|
| \`\\textbf{x}\` | **bold** |
| \`\\textit{x}\` | *italic* |
| \`\\texttt{x}\` | monospace |
| \`\\verb\\|x\\|\` | verbatim |
| \`\\hrule\` | horizontal line |
| \`\\vskip{N}\` | N blank lines |
| \`\\hskip{N}\` | N spaces |

### Table Syntax

\`\`\`
\\table[width:W, frame:STYLE, pad:{l:L,r:R}, border:STYLE, align:A]{
    \\row[frame:{b:STYLE}]{
        \\col[width:N, align:A]{content}
    }
}
\`\`\`

### Layout Syntax

\`\`\`
\\begin[width]{vbox}
    \\child[width][h_align]{content}
\\end{vbox}

\\begin[top|middle|bottom]{hbox}
    \\child[width][h_align]{content}
\\end{hbox}
\`\`\`

### Settings

\`\`\`
\\setwidth{N}           \\setunicode{true|false}
\\setmathitalic{true}   \\linebreaker{greedy|knuth|raggedright}
\\sethyphenate{true}    \\setparskip{N}
\\setmathabove{N}       \\setmathbelow{N}
\`\`\`
`;
