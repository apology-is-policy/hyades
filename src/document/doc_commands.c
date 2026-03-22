// doc_commands.c - Document command definitions for Hyades

#include "doc_commands.h"
#include <stddef.h>
#include <string.h>

// ============================================================================
// Command Definition Table
// ============================================================================

// Single source of truth for all document commands
static const DocCommandDef g_commands[] = {
    // Rendering options
    {"setunicode", CMD_CAT_RENDER, 1, 1, "Enable/disable Unicode output (0 or 1)"},
    {"setmathitalic", CMD_CAT_RENDER, 1, 1, "Enable/disable math italic (0 or 1)"},

    // Layout options
    {"setwidth", CMD_CAT_LAYOUT, 1, 1, "Set output width in columns"},
    {"sethyphenate", CMD_CAT_LAYOUT, 1, 1, "Enable/disable hyphenation (0 or 1)"},
    {"sethyphenminleft", CMD_CAT_LAYOUT, 1, 1, "Minimum chars before hyphen"},
    {"sethyphenminright", CMD_CAT_LAYOUT, 1, 1, "Minimum chars after hyphen"},

    // Spacing options
    {"setparskip", CMD_CAT_SPACING, 1, 1, "Blank lines between paragraphs"},
    {"setmathabove", CMD_CAT_SPACING, 1, 1, "Blank lines before display math"},
    {"setmathbelow", CMD_CAT_SPACING, 1, 1, "Blank lines after display math"},

    // Line breaking algorithm options
    {"linebreaker", CMD_CAT_LINEBREAK, 1, 1, "Algorithm: 'greedy', 'knuth', or 'raggedright'"},
    {"setlinepenalty", CMD_CAT_LINEBREAK, 1, 1, "Knuth-Plass: base cost per line"},
    {"sethyphenpenalty", CMD_CAT_LINEBREAK, 1, 1, "Knuth-Plass: cost for hyphenating"},
    {"setconsechyphenpenalty", CMD_CAT_LINEBREAK, 1, 1,
     "Knuth-Plass: cost for consecutive hyphens"},
    {"settolerance", CMD_CAT_LINEBREAK, 1, 1, "Knuth-Plass: max stretch tolerance"},
    // Greedy linebreaker options
    {"setshortthreshold", CMD_CAT_LINEBREAK, 1, 1, "Greedy: min word len for relaxed scoring"},
    {"setlinkthreshold", CMD_CAT_LINEBREAK, 1, 1, "Greedy: max token width for symmetric spacing"},
    {"setspreaddistance", CMD_CAT_LINEBREAK, 1, 1, "Greedy: penalty distance for gap spreading"},
    {"setneighbordivisor", CMD_CAT_LINEBREAK, 1, 1, "Greedy: neighbor penalty divisor (0=disable)"},
    {"setspreaddivisor", CMD_CAT_LINEBREAK, 1, 1, "Greedy: spread penalty divisor"},
    {"setminscore", CMD_CAT_LINEBREAK, 1, 1, "Greedy: min score to receive extra space"},

    // Symbol customization
    {"setmode", CMD_CAT_SYMBOL, 1, 1, "Set render mode: 'ascii' or 'unicode'"},
    {"setsym", CMD_CAT_SYMBOL, 2, 2, "Set symbol: \\setsym{SYM_NAME}{value}"},

    // Vertical spacing commands
    {"vskip", CMD_CAT_VERTICAL, 0, 1, "Insert vertical space (default 1 line)"},
    {"smallskip", CMD_CAT_VERTICAL, 0, 0, "Insert 1 line of vertical space"},
    {"medskip", CMD_CAT_VERTICAL, 0, 0, "Insert 2 lines of vertical space"},
    {"bigskip", CMD_CAT_VERTICAL, 0, 0, "Insert 3 lines of vertical space"},

    // Extended skip commands (support negative values for overlapping)
    {"xvskip", CMD_CAT_VERTICAL, 1, 1, "Extended vertical skip (negative for overlap)"},
    {"xhskip", CMD_CAT_VERTICAL, 1, 1, "Extended horizontal skip (negative for overlap)"},

    // Diagnostics commands
    {"diagnostics", CMD_CAT_DIAGNOSTICS, 1, 1,
     "Enable diagnostic logging (all, off, macros, system, layout, math, merge)"},
    {"printdiagnostics", CMD_CAT_DIAGNOSTICS, 0, 0, "Output accumulated diagnostic logs"},
    {"showdiag", CMD_CAT_DIAGNOSTICS, 1, 1, "Render content and append its diagnostics inline"},
    {"diag_expand", CMD_CAT_DIAGNOSTICS, 1, 1, "Log macro expansion of content to diagnostics"},

    // Terminator
    {NULL, 0, 0, 0, NULL}};

// ============================================================================
// Command Registry Access
// ============================================================================

const DocCommandDef *get_document_command(const char *name) {
    if (!name) return NULL;

    for (const DocCommandDef *cmd = g_commands; cmd->name; cmd++) {
        if (strcmp(cmd->name, name) == 0) {
            return cmd;
        }
    }
    return NULL;
}

bool is_document_command(const char *name) {
    return get_document_command(name) != NULL;
}

bool is_command_category(const char *name, DocCommandCategory category) {
    const DocCommandDef *cmd = get_document_command(name);
    return cmd && cmd->category == category;
}

const DocCommandDef *get_all_document_commands(void) {
    return g_commands;
}

int get_document_command_count(void) {
    int count = 0;
    for (const DocCommandDef *cmd = g_commands; cmd->name; cmd++) {
        count++;
    }
    return count;
}
