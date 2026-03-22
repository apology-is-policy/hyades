// math_symbols.c - Symbol mapping and rendering for math
// Verbatim from original render.c

#include "math_internal.h"

const char *map_op_char(int unicode, char op) {
    if (!unicode) {
        switch (op) {
        case '.': return "*";   // \cdot -> '*'
        case 'x': return "x";   // \times -> 'x'
        case '/': return "/";   // \div -> '/'
        case 'S': return "/";   // literal slash (inline division)
        case 'V': return "|";   // literal vertical bar (divides)
        case 'U': return "U";   // \cup -> 'U'
        case 'n': return "n";   // \cap -> 'n'
        case '\\': return "\\"; // \setminus
        case '&': return "/\\"; // \land
        case '|': return "\\/"; // \lor
        case 'o': return "o";   // \circ
        case 'b': return "*";   // \bullet
        case 'a': return "*";   // \ast
        case 's': return "*";   // \star
        case 'P': return "(+)"; // \oplus
        case 'M': return "(-)"; // \ominus
        case 'X': return "(x)"; // \otimes
        case 'O': return "(.)"; // \odot
        case 'd': return "+";   // \dagger
        case 'D': return "++";  // \ddagger
        case 'm': return "mod"; // \bmod
        case 'p': return "+/-"; // \pm
        case 'q': return "-/+"; // \mp
        default: {
            static char buf[2];
            buf[0] = (char)op;
            buf[1] = 0;
            return buf;
        }
        }
    }
    switch (op) {
    case '-': return "−";   // U+2212 minus sign
    case '*': return "⋅";   // U+22C5 dot operator
    case '.': return "⋅";   // U+22C5 dot operator
    case 'x': return "×";   // U+00D7 multiplication sign
    case '/': return "÷";   // U+00F7 division sign
    case 'S': return "/";   // literal slash (inline division) - stays as slash
    case 'V': return "∣";   // U+2223 divides - proper "divides" symbol
    case 'U': return "∪";   // U+222A union
    case 'n': return "∩";   // U+2229 intersection
    case '\\': return "∖";  // U+2216 set minus
    case '&': return "∧";   // U+2227 logical and
    case '|': return "∨";   // U+2228 logical or
    case 'o': return "∘";   // U+2218 ring operator
    case 'b': return "•";   // U+2022 bullet
    case 'a': return "∗";   // U+2217 asterisk operator
    case 's': return "⋆";   // U+22C6 star operator
    case 'P': return "⊕";   // U+2295 circled plus
    case 'M': return "⊖";   // U+2296 circled minus
    case 'X': return "⊗";   // U+2297 circled times
    case 'O': return "⊙";   // U+2299 circled dot
    case 'd': return "†";   // U+2020 dagger
    case 'D': return "‡";   // U+2021 double dagger
    case 'm': return "mod"; // \bmod - renders as word
    case 'p': return "±";   // U+00B1 plus-minus sign
    case 'q': return "∓";   // U+2213 minus-plus sign
    default: {
        static char buf[2];
        buf[0] = (char)op;
        buf[1] = 0;
        return buf;
    }
    }
}

// ---------- Operator symbol mapping ----------
const char *map_rel_str(int unicode, const char *op) {
    // Comparison relations
    if (strcmp(op, "\\leq") == 0) return get_symbol(SYM_LEQ);
    if (strcmp(op, "\\geq") == 0) return get_symbol(SYM_GEQ);
    if (strcmp(op, "\\neq") == 0) return get_symbol(SYM_NEQ);
    if (strcmp(op, "\\approx") == 0) return get_symbol(SYM_APPROX);
    if (strcmp(op, "\\equiv") == 0) return get_symbol(SYM_EQUIV);
    if (strcmp(op, "\\ll") == 0) return get_symbol(SYM_LL);
    if (strcmp(op, "\\gg") == 0) return get_symbol(SYM_GG);
    if (strcmp(op, "\\sim") == 0) return get_symbol(SYM_SIM);
    if (strcmp(op, "\\simeq") == 0) return get_symbol(SYM_SIMEQ);
    if (strcmp(op, "\\cong") == 0) return get_symbol(SYM_CONG);
    if (strcmp(op, "\\propto") == 0) return get_symbol(SYM_PROPTO);
    if (strcmp(op, "\\mid") == 0) return get_symbol(SYM_MID);
    if (strcmp(op, "\\nmid") == 0) return get_symbol(SYM_NMID);

    // Set relations
    if (strcmp(op, "\\subset") == 0) return get_symbol(SYM_SUBSET);
    if (strcmp(op, "\\supset") == 0) return get_symbol(SYM_SUPSET);
    if (strcmp(op, "\\subseteq") == 0) return get_symbol(SYM_SUBSETEQ);
    if (strcmp(op, "\\supseteq") == 0) return get_symbol(SYM_SUPSETEQ);
    if (strcmp(op, "\\in") == 0) return get_symbol(SYM_IN);
    if (strcmp(op, "\\notin") == 0) return get_symbol(SYM_NOTIN);
    if (strcmp(op, "\\ni") == 0) return get_symbol(SYM_NI);

    // Arrows and implications (treated as relations for spacing)
    if (strcmp(op, "\\implies") == 0 || strcmp(op, "\\Rightarrow") == 0)
        return get_symbol(SYM_RIGHTARROW_DOUBLE);
    if (strcmp(op, "\\iff") == 0 || strcmp(op, "\\Leftrightarrow") == 0)
        return get_symbol(SYM_LEFTRIGHTARROW_DOUBLE);
    if (strcmp(op, "\\Leftarrow") == 0) return get_symbol(SYM_LEFTARROW_DOUBLE);
    if (strcmp(op, "\\to") == 0 || strcmp(op, "\\rightarrow") == 0)
        return get_symbol(SYM_RIGHTARROW);
    if (strcmp(op, "\\leftarrow") == 0 || strcmp(op, "\\gets") == 0)
        return get_symbol(SYM_LEFTARROW);
    if (strcmp(op, "\\leftrightarrow") == 0) return get_symbol(SYM_LEFTRIGHTARROW);
    if (strcmp(op, "\\mapsto") == 0) return get_symbol(SYM_MAPSTO);

    // Order relations
    if (strcmp(op, "\\prec") == 0) return get_symbol(SYM_PREC);
    if (strcmp(op, "\\succ") == 0) return get_symbol(SYM_SUCC);
    if (strcmp(op, "\\preceq") == 0) return get_symbol(SYM_PRECEQ);
    if (strcmp(op, "\\succeq") == 0) return get_symbol(SYM_SUCCEQ);

    // Negated relations
    if (strcmp(op, "\\nleq") == 0) return get_symbol(SYM_NLEQ);
    if (strcmp(op, "\\ngeq") == 0) return get_symbol(SYM_NGEQ);
    if (strcmp(op, "\\nsubset") == 0) return get_symbol(SYM_NSUBSET);
    if (strcmp(op, "\\nequiv") == 0) return get_symbol(SYM_NEQUIV);

    // Definition symbols
    if (strcmp(op, ":=") == 0 || strcmp(op, "\\coloneqq") == 0 || strcmp(op, "\\Coloneqq") == 0)
        return unicode ? "\xE2\x89\x94" : ":=";                                // ≔ U+2254
    if (strcmp(op, "\\eqqcolon") == 0) return unicode ? "\xE2\x89\x95" : "=:"; // ≕ U+2255

    // Fallback: return the command string itself
    return op;
}

const char *map_symbol_token(const char *s) {
    int unicode = get_unicode_mode();

    if (strcmp(s, "\\hyades") == 0) return unicode ? "𝐻𝑦𝑎δες" : "Hyades";

    // Spacing
    if (strcmp(s, "\\,") == 0) return " ";
    if (strcmp(s, "\\:") == 0) return " ";
    if (strcmp(s, "\\;") == 0) return "  ";
    if (strcmp(s, "\\quad") == 0) return "    ";
    if (strcmp(s, "\\qquad") == 0) return "        ";
    if (strcmp(s, "\\!") == 0) return ""; // ignore negative thin space
    // TeX linebreak
    //if (strcmp(s,"\\\\")==0) return " ";
    if (strcmp(s, "\\dots") == 0 || strcmp(s, "\\ldots") == 0) return get_symbol(SYM_LDOTS);
    // control-space: "\ " or bare "\" token → emit a single space
    if (strcmp(s, "\\ ") == 0 || strcmp(s, "\\") == 0) return " ";

    // arrows / implications
    if (strcmp(s, "\\to") == 0) return get_symbol(SYM_RIGHTARROW);
    if (strcmp(s, "\\mapsto") == 0) return get_symbol(SYM_MAPSTO);
    if (strcmp(s, "\\implies") == 0) return get_symbol(SYM_IMPLIES);
    if (strcmp(s, "\\iff") == 0) return get_symbol(SYM_IFF);

    // ellipses (you already added \dots; include \cdots if you like)
    if (strcmp(s, "\\dots") == 0 || strcmp(s, "\\ldots") == 0) return get_symbol(SYM_LDOTS);
    if (strcmp(s, "\\cdots") == 0) return get_symbol(SYM_CDOTS);

    // Selected math symbols
    if (strcmp(s, "\\cdot") == 0) return get_symbol(SYM_CDOT);
    if (strcmp(s, "\\times") == 0) return get_symbol(SYM_TIMES);
    if (strcmp(s, "\\in") == 0) return get_symbol(SYM_IN);
    if (strcmp(s, "\\forall") == 0) return get_symbol(SYM_FORALL);
    if (strcmp(s, "\\exists") == 0) return get_symbol(SYM_EXISTS);
    if (strcmp(s, "\\nexists") == 0) return get_symbol(SYM_NEXISTS);
    if (strcmp(s, "\\nabla") == 0) return get_symbol(SYM_NABLA);

    // Logic symbols
    if (strcmp(s, "\\vdash") == 0) return get_symbol(SYM_VDASH);
    if (strcmp(s, "\\dashv") == 0) return get_symbol(SYM_DASHV);
    if (strcmp(s, "\\models") == 0) return get_symbol(SYM_MODELS);
    if (strcmp(s, "\\vDash") == 0) return get_symbol(SYM_MODELS);

    // Hebrew letters
    if (strcmp(s, "\\aleph") == 0) return get_symbol(SYM_ALEPH);
    if (strcmp(s, "\\beth") == 0) return get_symbol(SYM_BETH);
    if (strcmp(s, "\\gimel") == 0) return get_symbol(SYM_GIMEL);
    if (strcmp(s, "\\daleth") == 0) return get_symbol(SYM_DALETH);
    if (strcmp(s, "\\partial") == 0) return get_symbol(SYM_PARTIAL);

    if (strcmp(s, "\\subset") == 0) return unicode ? "\xE2\x8A\x82" : "subset";     // ⊂
    if (strcmp(s, "\\supset") == 0) return unicode ? "\xE2\x8A\x83" : "supset";     // ⊃
    if (strcmp(s, "\\subseteq") == 0) return unicode ? "\xE2\x8A\x86" : "subseteq"; // ⊆
    if (strcmp(s, "\\supseteq") == 0) return unicode ? "\xE2\x8A\x87" : "supseteq"; // ⊇
    if (strcmp(s, "\\in") == 0) return unicode ? "\xE2\x88\x88" : "in";             // ∈
    if (strcmp(s, "\\notin") == 0) return unicode ? "\xE2\x88\x89" : "/in"; // ∉ (ASCII fallback)
    if (strcmp(s, "\\ni") == 0 || strcmp(s, "\\owns") == 0)
        return unicode ? "\xE2\x88\x8B" : "ni"; // ∋

    if (strcmp(s, "\\cup") == 0) return unicode ? "\xE2\x88\xAA" : "U"; // ∪
    if (strcmp(s, "\\cap") == 0) return unicode ? "\xE2\x88\xA9" : "n"; // ∩

    if (strcmp(s, "\\mathbb") == 0) return "ℙ"; // shouldn’t be reached if parser consumed it

    // Common operator control words → upright ASCII/Unicode text (no italics)
    // (We just return their ASCII letters; “uprightness” is enforced by not applying cursive.)
    if (!strcmp(s, "\\sin") || !strcmp(s, "\\cos") || !strcmp(s, "\\tan") || !strcmp(s, "\\cot") ||
        !strcmp(s, "\\sec") || !strcmp(s, "\\csc") || !strcmp(s, "\\arcsin") ||
        !strcmp(s, "\\arccos") || !strcmp(s, "\\arctan") || !strcmp(s, "\\arccot") ||
        !strcmp(s, "\\arcsec") || !strcmp(s, "\\arccsc") || !strcmp(s, "\\sinh") ||
        !strcmp(s, "\\cosh") || !strcmp(s, "\\tanh") || !strcmp(s, "\\coth") ||
        !strcmp(s, "\\sech") || !strcmp(s, "\\csch") || !strcmp(s, "\\log") || !strcmp(s, "\\ln") ||
        !strcmp(s, "\\exp") || !strcmp(s, "\\lim") || !strcmp(s, "\\limsup") ||
        !strcmp(s, "\\liminf") || !strcmp(s, "\\sup") || !strcmp(s, "\\inf") ||
        !strcmp(s, "\\max") || !strcmp(s, "\\min") || !strcmp(s, "\\det") || !strcmp(s, "\\dim") ||
        !strcmp(s, "\\deg") || !strcmp(s, "\\rank") || !strcmp(s, "\\tr") ||
        !strcmp(s, "\\trace") || !strcmp(s, "\\span") || !strcmp(s, "\\ker") ||
        !strcmp(s, "\\im") || !strcmp(s, "\\Im") || !strcmp(s, "\\Re") || !strcmp(s, "\\gcd") ||
        !strcmp(s, "\\lcm") || !strcmp(s, "\\Pr") || !strcmp(s, "\\mod") || !strcmp(s, "\\bmod") ||
        !strcmp(s, "\\pmod") || !strcmp(s, "\\Hom") || !strcmp(s, "\\End") || !strcmp(s, "\\Aut") ||
        !strcmp(s, "\\id") || !strcmp(s, "\\arg") || !strcmp(s, "\\argmin") ||
        !strcmp(s, "\\argmax") || !strcmp(s, "\\sgn") || !strcmp(s, "\\sign") ||
        !strcmp(s, "\\supp") || !strcmp(s, "\\card") || !strcmp(s, "\\Var") ||
        !strcmp(s, "\\Cov")) {
        return s + 1; // drop the backslash → “sin”, “log”, …
    }

    // Escaped braces → render as literal braces
    if (!strcmp(s, "\\{")) return "{";
    if (!strcmp(s, "\\}")) return "}";

    // Escaped special characters → render as literals
    if (!strcmp(s, "\\%")) return "%";
    if (!strcmp(s, "\\$")) return "$";
    if (!strcmp(s, "\\&")) return "&";
    if (!strcmp(s, "\\#")) return "#";

    // Greek?
    const char *g = map_greek(s);
    if (g) return g;

    // Fallback: leave as-is
    return s;
}

Box symbol_box(const char *s) {
    const int unicode = get_unicode_mode();
    const int cursive = get_math_cursive_mode();
    StyleKind style = current_style();

    // Set theory
    if (!strcmp(s, "\\subset")) return text_box(get_symbol(SYM_SUBSET));
    if (!strcmp(s, "\\supset")) return text_box(get_symbol(SYM_SUPSET));
    if (!strcmp(s, "\\subseteq")) return text_box(get_symbol(SYM_SUBSETEQ));
    if (!strcmp(s, "\\supseteq")) return text_box(get_symbol(SYM_SUPSETEQ));
    if (!strcmp(s, "\\in")) return text_box(get_symbol(SYM_IN));
    if (!strcmp(s, "\\notin")) return text_box(get_symbol(SYM_NOTIN));
    if (!strcmp(s, "\\ni")) return text_box(get_symbol(SYM_NI));
    if (!strcmp(s, "\\cup")) return text_box(get_symbol(SYM_CUP));
    if (!strcmp(s, "\\cap")) return text_box(get_symbol(SYM_CAP));
    if (!strcmp(s, "\\emptyset")) return text_box(get_symbol(SYM_EMPTYSET));
    if (!strcmp(s, "\\setminus")) return text_box(get_symbol(SYM_SETMINUS));
    if (!strcmp(s, "\\varnothing")) return text_box(get_symbol(SYM_VARNOTHING));

    // Inequalities & Equivalence
    if (!strcmp(s, "\\leq")) return text_box(get_symbol(SYM_LEQ));
    if (!strcmp(s, "\\geq")) return text_box(get_symbol(SYM_GEQ));
    if (!strcmp(s, "\\neq")) return text_box(get_symbol(SYM_NEQ));
    if (!strcmp(s, "\\equiv")) return text_box(get_symbol(SYM_EQUIV));
    if (!strcmp(s, "\\approx")) return text_box(get_symbol(SYM_APPROX));
    if (!strcmp(s, "\\ll")) return text_box(get_symbol(SYM_LL));
    if (!strcmp(s, "\\gg")) return text_box(get_symbol(SYM_GG));
    if (!strcmp(s, "\\sim")) return text_box(get_symbol(SYM_SIM));
    if (!strcmp(s, "\\simeq")) return text_box(get_symbol(SYM_SIMEQ));
    if (!strcmp(s, "\\cong")) return text_box(get_symbol(SYM_CONG));
    if (!strcmp(s, "\\propto")) return text_box(get_symbol(SYM_PROPTO));
    // Mid (divides symbol) - uses short mathematical bar
    if (!strcmp(s, "\\mid")) return text_box(get_symbol(SYM_DIVIDES));
    if (!strcmp(s, "\\nmid")) return text_box(get_symbol(SYM_NMID));
    // Conditional probability: P(A\given B) or P(A\cond B) - uses short mathematical bar
    if (!strcmp(s, "\\given") || !strcmp(s, "\\cond")) {
        return text_box(get_symbol(SYM_DIVIDES));
    }

    // Standalone vertical bar (used in bra-ket notation like ⟨φ|A|ψ⟩)
    // Use the shorter mathematical bar (∣ U+2223) which doesn't connect with table borders
    if (!strcmp(s, "|")) {
        return text_box(get_symbol(SYM_DIVIDES));
    }

    // Parallel symbol (∥ U+2225)
    if (!strcmp(s, "\\parallel") || !strcmp(s, "\\|")) {
        return text_box(get_symbol(SYM_PARALLEL));
    }

    // Arrows
    if (!strcmp(s, "\\to") || !strcmp(s, "\\rightarrow"))
        return text_box(get_symbol(SYM_RIGHTARROW));
    if (!strcmp(s, "\\leftarrow") || !strcmp(s, "\\gets"))
        return text_box(get_symbol(SYM_LEFTARROW));
    if (!strcmp(s, "\\leftrightarrow")) return text_box(get_symbol(SYM_LEFTRIGHTARROW));
    if (!strcmp(s, "\\Rightarrow") || !strcmp(s, "\\implies"))
        return text_box(get_symbol(SYM_RIGHTARROW_DOUBLE));
    if (!strcmp(s, "\\Leftarrow")) return text_box(get_symbol(SYM_LEFTARROW_DOUBLE));
    if (!strcmp(s, "\\Leftrightarrow") || !strcmp(s, "\\iff"))
        return text_box(get_symbol(SYM_LEFTRIGHTARROW_DOUBLE));
    if (!strcmp(s, "\\mapsto")) return text_box(get_symbol(SYM_MAPSTO));
    if (!strcmp(s, "\\longmapsto")) return text_box(get_symbol(SYM_LONGMAPSTO));
    if (!strcmp(s, "\\longrightarrow")) return text_box(get_symbol(SYM_LONGRIGHTARROW));
    if (!strcmp(s, "\\longleftarrow")) return text_box(get_symbol(SYM_LONGLEFTARROW));
    if (!strcmp(s, "\\hookrightarrow")) return text_box(get_symbol(SYM_HOOKRIGHTARROW));
    if (!strcmp(s, "\\hookleftarrow")) return text_box(get_symbol(SYM_HOOKLEFTARROW));
    if (!strcmp(s, "\\uparrow")) return text_box(get_symbol(SYM_UPARROW));
    if (!strcmp(s, "\\downarrow")) return text_box(get_symbol(SYM_DOWNARROW));
    if (!strcmp(s, "\\updownarrow")) return text_box(get_symbol(SYM_UPDOWNARROW));
    if (!strcmp(s, "\\Uparrow")) return text_box(get_symbol(SYM_UPARROW_DOUBLE));
    if (!strcmp(s, "\\Downarrow")) return text_box(get_symbol(SYM_DOWNARROW_DOUBLE));
    if (!strcmp(s, "\\nearrow")) return text_box(get_symbol(SYM_NEARROW));
    if (!strcmp(s, "\\searrow")) return text_box(get_symbol(SYM_SEARROW));
    if (!strcmp(s, "\\nwarrow")) return text_box(get_symbol(SYM_NWARROW));
    if (!strcmp(s, "\\swarrow")) return text_box(get_symbol(SYM_SWARROW));

    // Plus-minus and related
    if (!strcmp(s, "\\pm")) return text_box(get_symbol(SYM_PM));
    if (!strcmp(s, "\\mp")) return text_box(get_symbol(SYM_MP));

    // Binary operators
    if (!strcmp(s, "\\oplus")) return text_box(get_symbol(SYM_OPLUS));
    if (!strcmp(s, "\\ominus")) return text_box(get_symbol(SYM_OMINUS));
    if (!strcmp(s, "\\otimes")) return text_box(get_symbol(SYM_OTIMES));
    if (!strcmp(s, "\\odot")) return text_box(get_symbol(SYM_ODOT));

    // Physics/Science
    if (!strcmp(s, "\\hbar")) return text_box(get_symbol(SYM_HBAR));
    if (!strcmp(s, "\\ell")) return text_box(get_symbol(SYM_ELL));

    // Logic
    if (!strcmp(s, "\\forall")) return text_box(get_symbol(SYM_FORALL));
    if (!strcmp(s, "\\exists")) return text_box(get_symbol(SYM_EXISTS));
    if (!strcmp(s, "\\nexists")) return text_box(get_symbol(SYM_NEXISTS));
    if (!strcmp(s, "\\neg") || !strcmp(s, "\\lnot")) return text_box(get_symbol(SYM_NEG));
    if (!strcmp(s, "\\land") || !strcmp(s, "\\wedge"))
        return unicode ? text_box("∧") : text_box("/\\");
    if (!strcmp(s, "\\lor") || !strcmp(s, "\\vee"))
        return unicode ? text_box("∨") : text_box("\\/");
    if (!strcmp(s, "\\bot")) return text_box(get_symbol(SYM_BOT));
    if (!strcmp(s, "\\vdash")) return text_box(get_symbol(SYM_VDASH));
    if (!strcmp(s, "\\dashv")) return text_box(get_symbol(SYM_DASHV));
    if (!strcmp(s, "\\models") || !strcmp(s, "\\vDash")) return text_box(get_symbol(SYM_MODELS));

    // Hebrew letters
    if (!strcmp(s, "\\aleph")) return text_box(get_symbol(SYM_ALEPH));
    if (!strcmp(s, "\\beth")) return text_box(get_symbol(SYM_BETH));
    if (!strcmp(s, "\\gimel")) return text_box(get_symbol(SYM_GIMEL));
    if (!strcmp(s, "\\daleth")) return text_box(get_symbol(SYM_DALETH));

    // Misc useful
    if (!strcmp(s, "\\circ")) return text_box(get_symbol(SYM_CIRC));
    if (!strcmp(s, "\\bullet")) return text_box(get_symbol(SYM_BULLET));
    if (!strcmp(s, "\\star")) return text_box(get_symbol(SYM_STAR));
    if (!strcmp(s, "\\dag") || !strcmp(s, "\\dagger")) return text_box(get_symbol(SYM_DAG));
    if (!strcmp(s, "\\ddag") || !strcmp(s, "\\ddagger")) return text_box(get_symbol(SYM_DDAG));
    if (!strcmp(s, "\\ast")) return text_box(get_symbol(SYM_STAR)); // alias for star
    if (!strcmp(s, "\\prime")) return text_box(get_symbol(SYM_PRIME));
    if (!strcmp(s, "\\dprime")) return text_box(get_symbol(SYM_DPRIME));
    if (!strcmp(s, "\\tprime")) return text_box(get_symbol(SYM_TPRIME));
    if (!strcmp(s, "\\angle")) return text_box(get_symbol(SYM_ANGLE));
    if (!strcmp(s, "\\perp")) return text_box(get_symbol(SYM_PERP));
    if (!strcmp(s, "\\top")) return text_box(get_symbol(SYM_TOP));
    if (!strcmp(s, "\\parallel")) return text_box(get_symbol(SYM_PARALLEL));
    if (!strcmp(s, "\\div")) return text_box(get_symbol(SYM_DIV));

    // Geometry
    if (!strcmp(s, "\\triangle")) return text_box(get_symbol(SYM_TRIANGLE));
    if (!strcmp(s, "\\square")) return text_box(get_symbol(SYM_SQUARE));
    if (!strcmp(s, "\\Box")) return text_box(get_symbol(SYM_SQUARE));
    if (!strcmp(s, "\\blacksquare")) return text_box(get_symbol(SYM_BLACKSQUARE));

    // Logic connectives
    if (!strcmp(s, "\\therefore")) return text_box(get_symbol(SYM_THEREFORE));
    if (!strcmp(s, "\\because")) return text_box(get_symbol(SYM_BECAUSE));

    // Definition symbols
    if (!strcmp(s, "\\coloneqq") || !strcmp(s, "\\Coloneqq"))
        return text_box(unicode ? "\xE2\x89\x94" : ":=");                           // ≔
    if (!strcmp(s, "\\eqqcolon")) return text_box(unicode ? "\xE2\x89\x95" : "=:"); // ≕

    // Order relations
    if (!strcmp(s, "\\prec")) return text_box(get_symbol(SYM_PREC));
    if (!strcmp(s, "\\succ")) return text_box(get_symbol(SYM_SUCC));
    if (!strcmp(s, "\\preceq")) return text_box(get_symbol(SYM_PRECEQ));
    if (!strcmp(s, "\\succeq")) return text_box(get_symbol(SYM_SUCCEQ));

    // Miscellaneous symbols
    if (!strcmp(s, "\\wp")) return text_box(get_symbol(SYM_WP));
    if (!strcmp(s, "\\imath")) return text_box(get_symbol(SYM_IMATH));
    if (!strcmp(s, "\\jmath")) return text_box(get_symbol(SYM_JMATH));

    // Negated relations
    if (!strcmp(s, "\\nleq")) return text_box(get_symbol(SYM_NLEQ));
    if (!strcmp(s, "\\ngeq")) return text_box(get_symbol(SYM_NGEQ));
    if (!strcmp(s, "\\nsubset")) return text_box(get_symbol(SYM_NSUBSET));
    if (!strcmp(s, "\\nequiv")) return text_box(get_symbol(SYM_NEQUIV));

    // Delimiters (angle brackets)
    if (!strcmp(s, "\\langle")) return text_box(get_symbol(SYM_LANGLE));
    if (!strcmp(s, "\\rangle")) return text_box(get_symbol(SYM_RANGLE));

    // ========== END PHASE 1 ADDITIONS ==========

    // Other symbols
    if (!strcmp(s, "\\infty")) return text_box(get_symbol(SYM_INFTY));

    // Dots
    if (!strcmp(s, "\\dots") || !strcmp(s, "\\ldots")) return text_box(get_symbol(SYM_LDOTS));
    if (!strcmp(s, "\\cdots")) return text_box(get_symbol(SYM_CDOTS));
    if (!strcmp(s, "\\vdots")) return text_box(get_symbol(SYM_VDOTS));
    if (!strcmp(s, "\\ddots")) return text_box(get_symbol(SYM_DDOTS));

    // Spacing commands
    if (!strcmp(s, "\\,")) return make_box(1, 1, 0);     // thin space
    if (!strcmp(s, "\\;")) return make_box(2, 1, 0);     // medium space
    if (!strcmp(s, "\\quad")) return make_box(4, 1, 0);  // quad
    if (!strcmp(s, "\\qquad")) return make_box(8, 1, 0); // double quad

    // Control words first (spacing, Greek, \cdot, \forall, etc.)
    if (s && s[0] == '\\') {
        const char *mapped = map_symbol_token(s);
        // If a style is active (e.g. \boldsymbol), apply the transform
        if (style != STYLE_NORMAL && unicode) {
            size_t mlen = strlen(mapped);
            size_t mp = 0;
            uint32_t mbuf[512];
            int mn = 0;
            while (mp < mlen && mn < 512) {
                uint32_t cp = utf8_next(mapped, mlen, &mp);
                if (style == STYLE_BOLD)
                    cp = to_bold(cp);
                else if (style == STYLE_FRAKTUR)
                    cp = to_fraktur(cp);
                else if (style == STYLE_SANS)
                    cp = to_sans(cp);
                mbuf[mn++] = cp;
            }
            return text_box_from_utf32(mbuf, mn);
        }
        return text_box(mapped); // upright; explicit glyphs/words
    }

    // Operator words kept upright (we added is_upright_operator_word earlier)
    if (is_upright_operator_word(s)) return text_box(s);

    // Plain identifiers / words / digits
    if (!unicode) {
        // ASCII fallback: show as-is
        return text_box(s);
    }

    // Unicode path: apply explicit style first; otherwise cursive
    uint32_t buf[512];
    int n = 0;
    size_t len = strlen(s), p = 0;
    while (p < len && n < 512) {
        uint32_t cp = utf8_next(s, len, &p);

        if (style == STYLE_BOLD)
            cp = to_bold(cp);
        else if (style == STYLE_BLACKBOARD)
            cp = to_blackboard(cp);
        else if (style == STYLE_SCRIPT)
            cp = to_script(cp);
        else if (style == STYLE_ITALIC)
            cp = latin_to_math_italic(cp);
        else if (style == STYLE_FRAKTUR)
            cp = to_fraktur(cp);
        else if (style == STYLE_SANS)
            cp = to_sans(cp);
        else if (style == STYLE_ROMAN)
            ; // force upright — skip italic even when cursive is on
        else if (cursive)
            cp = latin_to_math_italic(cp); // from earlier patch

        buf[n++] = cp;
    }
    return text_box_from_utf32(buf, n);
}

// Check if a \command is known to the math symbol system.
// Returns 1 if the command would be explicitly handled by symbol_box or map_symbol_token,
// 0 if it would fall through to the generic identifier path.
int is_known_math_symbol(const char *s) {
    if (!s || s[0] != '\\') return 0;

    // Check map_symbol_token (Greek, spacing, trig functions, basic math, etc.)
    if (map_symbol_token(s) != s) return 1;

    // Check all explicit symbol_box entries that map_symbol_token doesn't cover.
    // This must stay in sync with symbol_box() above.
    static const char *known[] = {
        // Set theory
        "\\subset",
        "\\supset",
        "\\subseteq",
        "\\supseteq",
        "\\in",
        "\\notin",
        "\\ni",
        "\\cup",
        "\\cap",
        "\\emptyset",
        "\\setminus",
        "\\varnothing",
        // Inequalities & Equivalence
        "\\leq",
        "\\geq",
        "\\neq",
        "\\equiv",
        "\\approx",
        "\\ll",
        "\\gg",
        "\\sim",
        "\\simeq",
        "\\cong",
        "\\propto",
        "\\mid",
        "\\nmid",
        "\\given",
        "\\cond",
        // Arrows
        "\\to",
        "\\rightarrow",
        "\\leftarrow",
        "\\gets",
        "\\leftrightarrow",
        "\\Rightarrow",
        "\\implies",
        "\\Leftarrow",
        "\\Leftrightarrow",
        "\\iff",
        "\\mapsto",
        "\\longmapsto",
        "\\longrightarrow",
        "\\longleftarrow",
        "\\hookrightarrow",
        "\\hookleftarrow",
        "\\uparrow",
        "\\downarrow",
        "\\updownarrow",
        "\\Uparrow",
        "\\Downarrow",
        "\\nearrow",
        "\\searrow",
        "\\nwarrow",
        "\\swarrow",
        // Plus-minus
        "\\pm",
        "\\mp",
        // Binary operators
        "\\oplus",
        "\\ominus",
        "\\otimes",
        "\\odot",
        // Physics
        "\\hbar",
        "\\ell",
        // Logic
        "\\forall",
        "\\exists",
        "\\nexists",
        "\\neg",
        "\\lnot",
        "\\land",
        "\\wedge",
        "\\lor",
        "\\vee",
        "\\bot",
        "\\vdash",
        "\\dashv",
        "\\models",
        "\\vDash",
        // Hebrew
        "\\aleph",
        "\\beth",
        "\\gimel",
        "\\daleth",
        // Misc
        "\\circ",
        "\\bullet",
        "\\star",
        "\\dag",
        "\\dagger",
        "\\ddag",
        "\\ddagger",
        "\\ast",
        "\\prime",
        "\\dprime",
        "\\tprime",
        "\\angle",
        "\\perp",
        "\\top",
        "\\parallel",
        "\\div",
        // Geometry
        "\\triangle",
        "\\square",
        "\\Box",
        "\\blacksquare",
        // Logic connectives
        "\\therefore",
        "\\because",
        // Order
        "\\prec",
        "\\succ",
        "\\preceq",
        "\\succeq",
        // Misc symbols
        "\\wp",
        "\\imath",
        "\\jmath",
        // Negated relations
        "\\nleq",
        "\\ngeq",
        "\\nsubset",
        "\\nequiv",
        // Delimiters
        "\\langle",
        "\\rangle",
        // Other
        "\\infty",
        "\\partial",
        "\\nabla",
        // Dots
        "\\dots",
        "\\ldots",
        "\\cdots",
        "\\vdots",
        "\\ddots",
        // Spacing (also in map_symbol_token, but be safe)
        "\\,",
        "\\;",
        "\\quad",
        "\\qquad",
        // Escaped chars
        "\\{",
        "\\}",
        "\\%",
        "\\$",
        "\\&",
        "\\#",
        "\\|",
        "\\ ",
        // Delimiter commands used by parser (not caught as symbols but valid)
        "\\left",
        "\\right",
        "\\middle",
        "\\lbrace",
        "\\rbrace",
        "\\lvert",
        "\\rvert",
        "\\Vert",
        "\\lVert",
        "\\rVert",
        "\\lfloor",
        "\\rfloor",
        "\\lceil",
        "\\rceil",
        // No-op style/tag commands (transparent in Hyades)
        "\\displaystyle",
        "\\textstyle",
        "\\scriptstyle",
        "\\scriptscriptstyle",
        "\\notag",
        "\\nonumber",
        // Definition symbols
        "\\coloneqq",
        "\\Coloneqq",
        "\\eqqcolon",
    };
    static const int nknown = sizeof(known) / sizeof(known[0]);

    for (int i = 0; i < nknown; i++) {
        if (strcmp(s, known[i]) == 0) return 1;
    }

    return 0;
}
