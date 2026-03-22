// cassilda.h - Cassilda document processor for Hyades
// Renders labeled segments and embeds output at reference points

#ifndef CASSILDA_H
#define CASSILDA_H

#include <stdbool.h>

// ============================================================================
// Cassilda - The Hyades Document Embedder
// ============================================================================
//
// Cassilda processes .cld files (and source files with Hyades references),
// rendering labeled segments and inserting the output at reference points.
//
// Syntax:
//   #comment_char "//"     - Set line comment prefix (empty for .cld files)
//   #output_prefix "| "    - Prefix for each rendered output line
//   #output_suffix ""      - Suffix for each rendered output line
//   #before_each           - Hyades commands to run before each render
//   #after_each            - Hyades commands to run after each render
//   #end                   - Explicit end (optional, new directive closes previous)
//
//   @label name            - Define a named segment
//   @end                   - End segment definition
//
//   @cassilda: name        - Insert rendered output of segment here
//   @cassilda: a, b, c     - Insert multiple segments concatenated
//
// Output lines are prefixed with NBSP (U+00A0) for identification,
// allowing Cassilda to update output on subsequent runs.
//
// ============================================================================

// Configuration parsed from directives
typedef struct {
    char *source_prefix; // Prefix for parsing source files (e.g., "//", "#", "")
    char *target_prefix; // Prefix for rendered output lines (e.g., " * ", "# ")
    char *before_each;   // Hyades code to prepend to each segment
    char *after_each;    // Hyades code to append to each segment
    int default_width;   // Default render width
} CassildaConfig;

// A labeled segment
typedef struct {
    char *name;    // Segment label
    char *content; // Hyades source content
} CassildaSegment;

// A reference point (where to insert rendered output)
typedef struct {
    int line_number; // Line in source where reference appears
    char **labels;   // Labels to render (comma-separated parsed)
    int n_labels;    // Number of labels
} CassildaReference;

// Parsed Cassilda document
typedef struct {
    CassildaConfig config;

    CassildaSegment *segments;
    int n_segments;
    int segments_capacity;

    CassildaReference *references;
    int n_references;
    int references_capacity;

    // Original source lines (for reconstruction)
    char **lines;
    int n_lines;
    int lines_capacity;

    // Optional: library context for external label lookup (opaque pointer)
    void *library_ctx;

    // Optional: source filename (for extension-based config)
    char *filename;
} CassildaDocument;

// ============================================================================
// API
// ============================================================================

// Parse a Cassilda document from string
// filename is optional (can be NULL) - used to infer default comment_char from extension
// target_prefix_hint is optional (can be NULL) - used to detect rendered output for deletion
// Returns NULL on error, sets error_msg
CassildaDocument *cassilda_parse(const char *input, const char *filename,
                                 const char *target_prefix_hint, char *error_msg, int error_size);

// Free a parsed document
void cassilda_free(CassildaDocument *doc);

// Process a document: render segments and insert at reference points
// Returns newly allocated string with rendered output, or NULL on error
char *cassilda_process(CassildaDocument *doc, char *error_msg, int error_size);

// Process with library context for external label lookup (opaque pointer)
char *cassilda_process_with_library(CassildaDocument *doc, void *lib_ctx, char *error_msg,
                                    int error_size);

// Convenience: parse and process in one step
// Returns newly allocated string, or NULL on error
char *cassilda_run(const char *input, char *error_msg, int error_size);

// Parse and process with library context (opaque pointer)
char *cassilda_run_with_library(const char *input, void *lib_ctx, char *error_msg, int error_size);

// Check if a document has stale output (for CI verification)
// Returns true if output is up-to-date, false if re-processing would change it
bool cassilda_check(const char *input, char *error_msg, int error_size);

// Clean rendered output: remove all lines containing NBSP
// Also triggered by #!clear directive in the document
// Returns newly allocated string with rendered lines removed
char *cassilda_clean(const char *input, const char *target_prefix_hint);

// ============================================================================
// Configuration Defaults
// ============================================================================

// Initialize config with defaults
void cassilda_config_init(CassildaConfig *config);

// Free config resources
void cassilda_config_free(CassildaConfig *config);

// ============================================================================
// Segment Lookup
// ============================================================================

// Find a segment by name, returns NULL if not found
CassildaSegment *cassilda_find_segment(CassildaDocument *doc, const char *name);

// Render a single segment to string
// Returns newly allocated string, or NULL on error
char *cassilda_render_segment(CassildaDocument *doc, const char *name, char *error_msg,
                              int error_size);

#endif // CASSILDA_H