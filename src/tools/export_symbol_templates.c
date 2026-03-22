/*
 * export_symbol_templates.c
 * 
 * SIMPLE VERSION - Uses metadata from SymbolRecord
 * 
 * No need to duplicate symbol names or categories!
 * Everything comes from g_symbol_records[] in symbols.c
 */

#include "math/renderer/symbols.h"
#include <stdio.h>
#include <string.h>

void write_header(FILE *f, const char *mode) {
    fprintf(f, "%% Symbol Table Template - %s Mode\n", mode);
    fprintf(f, "%% Auto-generated from symbol table\n");
    fprintf(f, "%%\n");
    fprintf(f, "%% This file shows ALL available symbols and their default %s values.\n", mode);
    fprintf(f, "%% To customize a symbol:\n");
    fprintf(f, "%%   1. Uncomment the line (remove leading %%)\n");
    fprintf(f, "%%   2. Change the value to your preferred glyph\n");
    fprintf(f, "%%   3. Save and load with \\input{symbols_%s.hy} or use -c flag\n", mode);
    fprintf(f, "%%\n");
    fprintf(f, "%% Example customizations:\n");
    fprintf(f, "%%   \\setsym{SYM_ALPHA}{🔥}           %% Change alpha to fire emoji\n");
    fprintf(f, "%%   \\setsym{SYM_SIGMA1_L0}{===}      %% Fancy sigma top line\n");
    fprintf(f, "%%\n\n");
    fprintf(f, "%% Set rendering mode\n");
    fprintf(f, "\\setmode{%s}\n\n", mode);
}

void write_section(FILE *f, const char *title) {
    fprintf(f, "\n");
    fprintf(f, "%% ═══════════════════════════════════════════════════════════════\n");
    fprintf(f, "%% %s\n", title);
    fprintf(f, "%% ═══════════════════════════════════════════════════════════════\n");
    fprintf(f, "\n");
}

int main() {
    // Initialize symbol table
    symbols_init();

    printf("Exporting symbol table templates...\n");
    printf("Total symbols: %d\n\n", g_symbols.count);

    // Generate ASCII template
    FILE *f_ascii = fopen("symbols_ascii.hy", "w");
    if (!f_ascii) {
        fprintf(stderr, "Error: Cannot create symbols_ascii.hy\n");
        return 1;
    }
    write_header(f_ascii, "ascii");

    // Generate Unicode template
    FILE *f_unicode = fopen("symbols_unicode.hy", "w");
    if (!f_unicode) {
        fprintf(stderr, "Error: Cannot create symbols_unicode.hy\n");
        fclose(f_ascii);
        return 1;
    }
    write_header(f_unicode, "unicode");

    // Iterate through all symbol records
    const char *current_category = NULL;

    for (int i = 0; i < g_symbols.count; i++) {
        const SymbolRecord *rec = &g_symbols.records[i];

        // Write category header if changed
        if (!current_category || strcmp(rec->category, current_category) != 0) {
            write_section(f_ascii, rec->category);
            write_section(f_unicode, rec->category);
            current_category = rec->category;
        }

        // Write symbol entries (commented out by default with %)
        fprintf(f_ascii, "%% \\setsym{%s}{%s}\n", rec->name, rec->ascii);
        fprintf(f_unicode, "%% \\setsym{%s}{%s}\n", rec->name, rec->unicode);
    }

    // Footer
    fprintf(f_ascii, "\n%% End of symbol table (%d symbols)\n", g_symbols.count);
    fprintf(f_unicode, "\n%% End of symbol table (%d symbols)\n", g_symbols.count);

    fclose(f_ascii);
    fclose(f_unicode);

    printf("✓ Generated symbols_ascii.hy (%d symbols)\n", g_symbols.count);
    printf("✓ Generated symbols_unicode.hy (%d symbols)\n", g_symbols.count);
    printf("\nFiles ready for user customization!\n");
    printf("Users can uncomment and modify any symbol to customize rendering.\n");

    return 0;
}