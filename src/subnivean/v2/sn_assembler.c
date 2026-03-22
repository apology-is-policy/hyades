// sn_assembler.c - Inline Subnivean Assembly
//
// Two-pass assembler: pass 1 collects labels, pass 2 emits instructions.
// Supports mnemonics, labels, .const/.sym directives, and comments.

#include "sn_assembler.h"
#include "function.h"
#include "opcode.h"
#include "value.h"
#include "vm.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_LABELS 64
#define MAX_LINE_LEN 256
#define MAX_LINES 1024

// ============================================================================
// Label Table
// ============================================================================

typedef struct {
    char name[64];
    int instruction_index; // Index into emitted instruction array
} Label;

// ============================================================================
// Opcode Lookup
// ============================================================================

// Look up an opcode by mnemonic name. Returns -1 if not found.
static int opcode_from_name(const char *name) {
    for (int i = 0; i < OP_COUNT; i++) {
        if (opcode_info[i].name && strcmp(opcode_info[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// Line Classification
// ============================================================================

typedef enum {
    LINE_BLANK,
    LINE_COMMENT,
    LINE_LABEL,
    LINE_DIRECTIVE_CONST,
    LINE_DIRECTIVE_SYM,
    LINE_INSTRUCTION,
    LINE_ERROR,
} LineKind;

typedef struct {
    LineKind kind;
    char mnemonic[64];     // For instructions: the opcode name
    char operand_str[128]; // For instructions: operand text (integer or label)
    char label_name[64];   // For labels: the label name
    char string_val[256];  // For .const: the string content
    char sym_name[64];     // For .sym: the symbol name
    bool has_operand;
} ParsedLine;

// Trim leading whitespace, return pointer into the same buffer.
static const char *skip_whitespace(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

// Parse a single line into a ParsedLine struct.
static ParsedLine parse_line(const char *line) {
    ParsedLine pl = {0};
    const char *p = skip_whitespace(line);

    // Blank line
    if (*p == '\0' || *p == '\n' || *p == '\r') {
        pl.kind = LINE_BLANK;
        return pl;
    }

    // Comment
    if (*p == ';') {
        pl.kind = LINE_COMMENT;
        return pl;
    }

    // .const "string"
    if (strncmp(p, ".const ", 7) == 0 || strncmp(p, ".const\t", 7) == 0) {
        p = skip_whitespace(p + 7);
        if (*p == '"') {
            p++; // skip opening quote
            int i = 0;
            while (*p && *p != '"' && i < (int)sizeof(pl.string_val) - 1) {
                if (*p == '\\' && *(p + 1)) {
                    p++;
                    switch (*p) {
                    case 'n': pl.string_val[i++] = '\n'; break;
                    case 't': pl.string_val[i++] = '\t'; break;
                    case '\\': pl.string_val[i++] = '\\'; break;
                    case '"': pl.string_val[i++] = '"'; break;
                    default: pl.string_val[i++] = *p; break;
                    }
                } else {
                    pl.string_val[i++] = *p;
                }
                p++;
            }
            pl.string_val[i] = '\0';
            pl.kind = LINE_DIRECTIVE_CONST;
        } else {
            pl.kind = LINE_ERROR;
        }
        return pl;
    }

    // .sym name
    if (strncmp(p, ".sym ", 4) == 0 || strncmp(p, ".sym\t", 4) == 0) {
        p = skip_whitespace(p + 4);
        int i = 0;
        while (*p && !isspace((unsigned char)*p) && i < (int)sizeof(pl.sym_name) - 1) {
            pl.sym_name[i++] = *p++;
        }
        pl.sym_name[i] = '\0';
        if (i > 0) {
            pl.kind = LINE_DIRECTIVE_SYM;
        } else {
            pl.kind = LINE_ERROR;
        }
        return pl;
    }

    // Check for label (identifier followed by colon)
    {
        const char *q = p;
        while (*q && (isalnum((unsigned char)*q) || *q == '_')) q++;
        if (q > p && *q == ':') {
            size_t len = q - p;
            if (len < sizeof(pl.label_name)) {
                memcpy(pl.label_name, p, len);
                pl.label_name[len] = '\0';
                pl.kind = LINE_LABEL;
                return pl;
            }
        }
    }

    // Instruction: MNEMONIC [operand]
    {
        int i = 0;
        while (*p && !isspace((unsigned char)*p) && i < (int)sizeof(pl.mnemonic) - 1) {
            pl.mnemonic[i++] = *p++;
        }
        pl.mnemonic[i] = '\0';

        // Skip whitespace to find operand
        p = skip_whitespace(p);

        // Check for inline comment
        if (*p == ';') {
            // No operand, rest is comment
            pl.has_operand = false;
        } else if (*p && *p != '\n' && *p != '\r') {
            // Operand present
            pl.has_operand = true;
            i = 0;
            while (*p && *p != ';' && *p != '\n' && *p != '\r' &&
                   i < (int)sizeof(pl.operand_str) - 1) {
                pl.operand_str[i++] = *p++;
            }
            // Trim trailing whitespace from operand
            while (i > 0 && (pl.operand_str[i - 1] == ' ' || pl.operand_str[i - 1] == '\t')) {
                i--;
            }
            pl.operand_str[i] = '\0';
        }

        pl.kind = LINE_INSTRUCTION;
        return pl;
    }
}

// ============================================================================
// Main Assembler
// ============================================================================

Function *sn_assemble(VM *vm, const char *source, char *error_msg, int error_size) {
    if (!source) {
        snprintf(error_msg, error_size, "NULL source");
        return NULL;
    }

    // Split source into lines
    const char *lines[MAX_LINES];
    int line_lengths[MAX_LINES];
    int n_lines = 0;

    const char *p = source;
    while (*p && n_lines < MAX_LINES) {
        lines[n_lines] = p;
        const char *eol = p;
        while (*eol && *eol != '\n') eol++;
        line_lengths[n_lines] = (int)(eol - p);
        n_lines++;
        p = *eol ? eol + 1 : eol;
    }

    // Parse all lines
    ParsedLine parsed[MAX_LINES];
    for (int i = 0; i < n_lines; i++) {
        // Copy line to temp buffer for null-termination
        char buf[MAX_LINE_LEN];
        int len = line_lengths[i] < MAX_LINE_LEN - 1 ? line_lengths[i] : MAX_LINE_LEN - 1;
        memcpy(buf, lines[i], len);
        buf[len] = '\0';
        parsed[i] = parse_line(buf);
    }

    // Create function
    Function *f = function_new("sn_asm", 0);
    if (!f) {
        snprintf(error_msg, error_size, "Failed to allocate function");
        return NULL;
    }

    // ========================================================================
    // Pass 1: Collect labels and process directives into constant pool
    // ========================================================================

    Label labels[MAX_LABELS];
    int n_labels = 0;
    int instruction_index = 0; // Counts emitted instructions

    for (int i = 0; i < n_lines; i++) {
        switch (parsed[i].kind) {
        case LINE_BLANK:
        case LINE_COMMENT: break;

        case LINE_LABEL:
            if (n_labels >= MAX_LABELS) {
                snprintf(error_msg, error_size, "Line %d: too many labels (max %d)", i + 1,
                         MAX_LABELS);
                function_decref(f);
                return NULL;
            }
            strncpy(labels[n_labels].name, parsed[i].label_name, sizeof(labels[n_labels].name) - 1);
            labels[n_labels].name[sizeof(labels[n_labels].name) - 1] = '\0';
            labels[n_labels].instruction_index = instruction_index;
            n_labels++;
            break;

        case LINE_DIRECTIVE_CONST: {
            // Add string to constant pool
            String *s = string_new(parsed[i].string_val, strlen(parsed[i].string_val));
            int idx = function_add_string(f, s);
            string_decref(s);
            // Store the pool index back for pass 2 reference
            // (not needed since .const doesn't emit instructions)
            (void)idx;
            break;
        }

        case LINE_DIRECTIVE_SYM: {
            // Intern symbol and add to constant pool
            Symbol *sym = vm_intern(vm, parsed[i].sym_name);
            function_add_symbol(f, sym);
            break;
        }

        case LINE_INSTRUCTION: instruction_index++; break;

        case LINE_ERROR:
            snprintf(error_msg, error_size, "Line %d: parse error", i + 1);
            function_decref(f);
            return NULL;
        }
    }

    // ========================================================================
    // Pass 2: Emit instructions
    // ========================================================================

    instruction_index = 0;

    for (int i = 0; i < n_lines; i++) {
        if (parsed[i].kind != LINE_INSTRUCTION) continue;

        int op = opcode_from_name(parsed[i].mnemonic);
        if (op < 0) {
            snprintf(error_msg, error_size, "Line %d: unknown mnemonic '%s'", i + 1,
                     parsed[i].mnemonic);
            function_decref(f);
            return NULL;
        }

        int32_t operand = 0;

        if (opcode_info[op].has_operand) {
            if (!parsed[i].has_operand) {
                snprintf(error_msg, error_size, "Line %d: %s requires an operand", i + 1,
                         parsed[i].mnemonic);
                function_decref(f);
                return NULL;
            }

            // Try parsing as integer first
            char *endptr;
            long val = strtol(parsed[i].operand_str, &endptr, 10);
            if (*endptr == '\0' && endptr != parsed[i].operand_str) {
                // It's a plain integer
                if (op == OP_JUMP || op == OP_JUMP_IF || op == OP_JUMP_UNLESS || op == OP_LOOP) {
                    // For jump instructions with integer operands, use as-is (raw offset)
                    operand = (int32_t)val;
                } else {
                    operand = (int32_t)val;
                }
            } else {
                // Must be a label reference (only valid for jump/loop instructions)
                if (op != OP_JUMP && op != OP_JUMP_IF && op != OP_JUMP_UNLESS && op != OP_LOOP) {
                    snprintf(error_msg, error_size,
                             "Line %d: '%s' is not a valid integer operand for %s", i + 1,
                             parsed[i].operand_str, parsed[i].mnemonic);
                    function_decref(f);
                    return NULL;
                }

                // Find label
                int target = -1;
                for (int j = 0; j < n_labels; j++) {
                    if (strcmp(labels[j].name, parsed[i].operand_str) == 0) {
                        target = labels[j].instruction_index;
                        break;
                    }
                }
                if (target < 0) {
                    snprintf(error_msg, error_size, "Line %d: undefined label '%s'", i + 1,
                             parsed[i].operand_str);
                    function_decref(f);
                    return NULL;
                }

                // Compute relative offset
                if (op == OP_LOOP) {
                    // LOOP jumps backward: offset = current + 1 - target
                    operand = (int32_t)(instruction_index + 1 - target);
                } else {
                    // JUMP/JUMP_IF/JUMP_UNLESS: offset = target - current - 1
                    operand = (int32_t)(target - instruction_index - 1);
                }
            }
        } else if (parsed[i].has_operand) {
            // Instruction doesn't take an operand but one was given — warning, ignore
        }

        function_emit(f, (OpCode)op, operand);
        instruction_index++;
    }

    // Append HALT if the last instruction isn't already HALT
    if (f->code_len == 0 || f->code[f->code_len - 1].op != OP_HALT) {
        function_emit(f, OP_HALT, 0);
    }

    return f;
}
