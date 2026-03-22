// parse_options.h - Configuration options for Hyades parsing
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Parse Options
// ============================================================================

typedef struct {
    // Error collection
    bool collect_all_errors; // Collect multiple errors (default: true)
    int max_errors;          // Maximum errors to collect (default: 100, 0 = unlimited)

    // Symbol tracking (for LSP features)
    bool build_symbol_table; // Track macro/lambda definitions (default: true)
    bool track_references;   // Track symbol references (default: true)

    // Source mapping
    bool build_source_map; // Build transformed→original position map (default: true)

    // Validation strictness
    // Note: Strict mode is always on - no lenient fallback
    bool validate_delimiters; // Check for matching delimiters (default: true)
    bool validate_references; // Check for undefined references (default: true)
    bool validate_arity;      // Check macro/lambda argument counts (default: true)

    // Recovery behavior
    bool continue_on_error; // Try to continue parsing after errors (default: true)

    // Debug/diagnostic options
    bool include_source_context; // Include source snippets in error messages
    int context_lines;           // Lines of context to include (default: 2)

} HyadesParseOptions;

// ============================================================================
// Option Defaults
// ============================================================================

// Get default parse options (strict parsing with all features enabled)
static inline HyadesParseOptions hyades_parse_options_default(void) {
    HyadesParseOptions opts = {.collect_all_errors = true,
                               .max_errors = 100,
                               .build_symbol_table = true,
                               .track_references = true,
                               .build_source_map = true,
                               .validate_delimiters = true,
                               .validate_references = true,
                               .validate_arity = true,
                               .continue_on_error = true,
                               .include_source_context = true,
                               .context_lines = 2};
    return opts;
}

// Get minimal options (fast parsing, minimal features)
static inline HyadesParseOptions hyades_parse_options_minimal(void) {
    HyadesParseOptions opts = {.collect_all_errors = false,
                               .max_errors = 1,
                               .build_symbol_table = false,
                               .track_references = false,
                               .build_source_map = false,
                               .validate_delimiters = true,
                               .validate_references = false,
                               .validate_arity = false,
                               .continue_on_error = false,
                               .include_source_context = false,
                               .context_lines = 0};
    return opts;
}

// Get LSP-optimized options (all features for IDE integration)
static inline HyadesParseOptions hyades_parse_options_lsp(void) {
    HyadesParseOptions opts = {.collect_all_errors = true,
                               .max_errors = 200,
                               .build_symbol_table = true,
                               .track_references = true,
                               .build_source_map = true,
                               .validate_delimiters = true,
                               .validate_references = true,
                               .validate_arity = true,
                               .continue_on_error = true,
                               .include_source_context = true,
                               .context_lines = 3};
    return opts;
}

#ifdef __cplusplus
}
#endif
