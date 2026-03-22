// hyades.h - Hyades Public API
// Unicode mathematical typesetting library
//
// Copyright (c) 2024-2025. MIT License.

#ifndef HYADES_H
#define HYADES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Version
// ============================================================================

#define HYADES_VERSION_MAJOR 1
#define HYADES_VERSION_MINOR 0
#define HYADES_VERSION_PATCH 0
#define HYADES_VERSION_STRING "1.0.0"

// ============================================================================
// Error Handling
// ============================================================================

typedef enum {
    HYADES_OK = 0,
    HYADES_ERR_SYNTAX,      // Parse error in input
    HYADES_ERR_MATH_SYNTAX, // Error in math expression
    HYADES_ERR_UNSUPPORTED, // Unsupported feature
    HYADES_ERR_MEMORY,      // Out of memory
    HYADES_ERR_INTERNAL,    // Internal error
    HYADES_ERR_NOT_FOUND,   // Segment not found (Cassilda)
    HYADES_ERR_IO           // I/O error
} HyadesErrorCode;

typedef struct {
    HyadesErrorCode code; // Error code (HYADES_OK on success)
    int line;             // 1-based line number (0 if unknown)
    int column;           // 1-based column number (0 if unknown)
    char message[256];    // Human-readable error message
} HyadesError;

// Initialize error struct (sets to OK state)
void hyades_error_init(HyadesError *err);

// ============================================================================
// Warnings (non-fatal issues from the last render)
// ============================================================================

#define HYADES_MAX_WARNINGS 16

// Get number of warnings from the last hyades_render() call.
// Warnings are non-fatal issues (e.g., unknown commands rendered verbatim).
int hyades_render_warning_count(void);

// Get warning message by index (0-based). Returns NULL if out of range.
const char *hyades_render_warning_message(int index);

// ============================================================================
// Rendering Options
// ============================================================================

typedef struct {
    int width;        // Output width in characters (default: 80)
    bool unicode;     // Use Unicode symbols (default: true)
    bool math_italic; // Use math italic for variables (default: true)
} HyadesOptions;

// Get default options
HyadesOptions hyades_default_options(void);

// ============================================================================
// Hyades Core API - Plain Hyades Document Rendering
// ============================================================================

// Render a Hyades document to Unicode text
//
// Parameters:
//   input   - Hyades source document (null-terminated UTF-8 string)
//   options - Rendering options (NULL for defaults)
//   error   - Error output (NULL to ignore errors)
//
// Returns:
//   Newly allocated string containing rendered output, or NULL on error.
//   Caller must free() the returned string.
//
// Example:
//   const char *src = "Hello $x^2$ world\n$$\\frac{a}{b}$$";
//   char *out = hyades_render(src, NULL, NULL);
//   printf("%s", out);
//   free(out);
//
char *hyades_render(const char *input, const HyadesOptions *options, HyadesError *error);

// ============================================================================
// Cassilda API - Document Embedding
// ============================================================================

// Process a Cassilda document (.cld file or source with @cassilda references)
//
// Cassilda processes labeled segments and inserts rendered output at reference
// points. This is the main entry point for the Cassilda document processor.
//
// Parameters:
//   input - Cassilda document (null-terminated UTF-8 string)
//   error - Error output (NULL to ignore errors)
//
// Returns:
//   Newly allocated string with rendered output inserted, or NULL on error.
//   Caller must free() the returned string.
//
// Cassilda Syntax:
//   #comment_char "//"     - Set line comment prefix
//   #output_prefix "| "    - Prefix for rendered output lines
//   #before_each ... #end  - Hyades commands to prepend to each segment
//
//   @label name            - Define a named segment
//   @end                   - End segment definition
//
//   @cassilda: name        - Insert rendered output here
//
// Example:
//   const char *cld =
//       "#output_prefix \"\"\n"
//       "@label quad\n"
//       "$$x = \\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}$$\n"
//       "@end\n"
//       "@cassilda: quad\n";
//   char *out = hyades_cassilda_process(cld, NULL);
//   printf("%s", out);
//   free(out);
//
char *hyades_cassilda_process(const char *input, HyadesError *error);

// Check if a Cassilda document is up-to-date
//
// Returns true if reprocessing would produce the same output.
// Useful for CI/build systems to verify documentation is current.
//
// Parameters:
//   input - Cassilda document (null-terminated UTF-8 string)
//   error - Error output (NULL to ignore errors)
//
// Returns:
//   true if document is up-to-date, false if stale or on error.
//
bool hyades_cassilda_check(const char *input, HyadesError *error);

// Render a single named segment from a Cassilda document
//
// Parameters:
//   input        - Cassilda document containing segment definitions
//   segment_name - Name of the segment to render
//   error        - Error output (NULL to ignore errors)
//
// Returns:
//   Newly allocated string with rendered segment, or NULL on error.
//   Caller must free() the returned string.
//
char *hyades_cassilda_render_segment(const char *input, const char *segment_name,
                                     HyadesError *error);

// ============================================================================
// Initialization (Optional)
// ============================================================================

// Initialize the Hyades library
//
// This is called automatically on first use, but can be called explicitly
// for predictable initialization timing. Safe to call multiple times.
//
void hyades_init(void);

// Shutdown the Hyades library
//
// Frees internal resources. Optional - resources are freed at program exit.
// After calling this, hyades_init() must be called before next use.
//
void hyades_shutdown(void);

// Reset all per-render state
//
// Clears global state that accumulates between renders, including:
// - User macro registry
// - Line registry (for lineroutine)
// - Verbatim store
// Call before each render in long-running processes (like WASM) to ensure
// clean state between renders.
//
void hyades_reset_state(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HYADES_H