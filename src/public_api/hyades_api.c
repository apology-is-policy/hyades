#include <stdio.h>
#include <stdlib.h>

#include "document/macro_expand.h"
#include "hyades_api.h"
#include "hyades_kernel.h"

char *compose_document_with_map(const char *input, const CompOptions *opt_in, MapCtx *mc,
                                ParseError *err) {
    // Copy options (will be modified by commands)
    CompOptions opt = opt_in ? *opt_in : default_options();

    // Store original document width (before any \setwidth commands)
    // This allows detecting document-level content vs nested content
    opt.document_width = opt.width;

    // Keep macro registry alive during parsing AND rendering
    // (needed for \lineroutine which expands macros during rendering)
    macro_registry_keep_alive(true);

    // Parse document as vbox
    BoxLayout *doc_vbox = parse_document_as_vbox(input, opt.width, err);
    if (!doc_vbox) {
        macro_registry_keep_alive(false);
        if (err && err->code == PARSE_OK) {
            err->code = PARSE_ERR_INTERNAL;
            snprintf(err->message, sizeof(err->message), "Failed to parse document");
        }
        return NULL;
    }

    // Resolve widths
    box_layout_resolve_widths(doc_vbox, opt.width);

    // Render (executes commands, renders children)
    Box *box = box_layout_render(doc_vbox, &opt, err);

    // Done with rendering - release macro registry
    macro_registry_keep_alive(false);

    box_layout_free(doc_vbox);

    if (!box) {
        if (err && err->code == PARSE_OK) {
            err->code = PARSE_ERR_INTERNAL;
            snprintf(err->message, sizeof(err->message), "Failed to render document");
        }
        return NULL;
    }

    // Convert box to string
    char *result = box_to_string(box);
    box_free(box);
    free(box);

    // TODO: Handle mapping context if needed
    // For now, mapping is simplified
    if (mc) {
        mc->row = 0;
        mc->col = 0;
    }

    return result;
}

char *compose_document(const char *input, const CompOptions *opt, ParseError *err) {
    return compose_document_with_map(input, opt, NULL, err);
}
