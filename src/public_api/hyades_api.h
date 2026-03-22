// docparser.h - Document-level parser (wraps compositor safely)
#pragma once

#include <stdbool.h>

#include "compositor/compositor.h"
#include "hyades_kernel_types.h"
#include "utils/error.h"

typedef enum { DOC_TEXT, DOC_DISPLAY, DOC_COMMAND, DOC_PREFORMATTED, DOC_BOX_LAYOUT } DocNodeKind;

typedef struct DocCommand {
    char *name;
    char **args;
    int n_args;
} DocCommand;

typedef struct DocNode {
    DocNodeKind kind;

    union {
        // DOC_TEXT
        struct {
            char *text;      // Text content (may have inline $...$)
            int text_offset; // Byte offset in original input
            int text_length; // Length in bytes
        };

        // DOC_DISPLAY
        struct {
            char *math_src;  // Math source (content of $$...$$)
            int math_offset; // Byte offset in original input
            int math_length; // Length in bytes
        };

        // DOC_COMMAND
        DocCommand cmd;

        struct {
            char *preformatted_text;
            int preformatted_offset;
            int preformatted_length;
        };

        // DOC_BOX_LAYOUT
        BoxLayout *box_layout; // Box layout to render after commands
    };

    struct DocNode *next; // Linked list
} DocNode;

// Main composition function
char *compose_document(const char *input, const CompOptions *opt, ParseError *err);

// With mapping support (for editor integration)
char *compose_document_with_map(const char *input, const CompOptions *opt, MapCtx *mc,
                                ParseError *err);
