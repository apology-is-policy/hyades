// parser_recovery.h - Error recovery helpers for parsers
//
// Provides synchronization and recovery mechanisms for continuing
// parsing after encountering errors, allowing collection of multiple
// errors in a single parse pass.

#pragma once

#include "error.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Recovery Points
// ============================================================================

// Synchronization token types
typedef enum {
    SYNC_NONE = 0,
    SYNC_BRACE_CLOSE,   // }
    SYNC_BRACKET_CLOSE, // ]
    SYNC_PAREN_CLOSE,   // )
    SYNC_DOLLAR,        // $ (end of inline math)
    SYNC_DOUBLE_DOLLAR, // $$ (end of display math)
    SYNC_BEGIN,         // \begin
    SYNC_END,           // \end
    SYNC_PARAGRAPH,     // Blank line
    SYNC_NEWLINE,       // Single newline
    SYNC_COMMAND,       // Any \command
    SYNC_EOF            // End of file
} SyncTokenType;

// Recovery context for tracking parser state during error recovery
typedef struct {
    ParseErrorList *errors;    // Error list to accumulate errors
    int recovery_depth;        // Nesting depth for recovery
    int max_recovery_attempts; // Maximum recovery attempts per error
    int current_attempts;      // Current recovery attempts
    bool in_recovery;          // Currently recovering from an error
    SyncTokenType last_sync;   // Last synchronization point type
} RecoveryContext;

// ============================================================================
// Recovery Context Management
// ============================================================================

// Initialize a recovery context
void recovery_context_init(RecoveryContext *ctx, ParseErrorList *errors);

// Check if we should attempt recovery (not at max attempts)
bool recovery_should_try(RecoveryContext *ctx);

// Mark that we're entering recovery mode
void recovery_enter(RecoveryContext *ctx);

// Mark that we've exited recovery mode
void recovery_exit(RecoveryContext *ctx);

// Check if currently in recovery mode
bool recovery_in_progress(RecoveryContext *ctx);

// ============================================================================
// Synchronization
// ============================================================================

// Skip characters until we find a synchronization point
// Returns the type of sync point found
// source: pointer to current position (updated on return)
// end: end of source buffer
SyncTokenType recovery_sync_to(const char **source, const char *end, SyncTokenType *allowed,
                               int n_allowed);

// Check if character sequence matches a sync point
bool recovery_is_sync_point(const char *source, SyncTokenType type);

// Get sync point as string (for error messages)
const char *recovery_sync_name(SyncTokenType type);

// ============================================================================
// Error Recovery Strategies
// ============================================================================

// Skip to matching close delimiter
// Returns position after the close delimiter, or NULL if not found
const char *recovery_skip_to_close_brace(const char *source, const char *end);
const char *recovery_skip_to_close_bracket(const char *source, const char *end);
const char *recovery_skip_to_close_paren(const char *source, const char *end);

// Skip to end of math environment
const char *recovery_skip_to_end_inline_math(const char *source, const char *end);
const char *recovery_skip_to_end_display_math(const char *source, const char *end);

// Skip to end of current paragraph (blank line)
const char *recovery_skip_to_paragraph_end(const char *source, const char *end);

// Skip to matching \end{name}
const char *recovery_skip_to_end_env(const char *source, const char *end, const char *env_name);

// ============================================================================
// Balanced Delimiter Tracking
// ============================================================================

// Track balanced delimiters during recovery
typedef struct {
    int brace_depth;   // { } nesting
    int bracket_depth; // [ ] nesting
    int paren_depth;   // ( ) nesting
    int dollar_depth;  // $ $ nesting (0 or 1)
    int env_depth;     // \begin/\end nesting
} DelimiterBalance;

// Initialize delimiter balance tracker
void delimiter_balance_init(DelimiterBalance *db);

// Update balance for a character
// Returns true if all delimiters are balanced
bool delimiter_balance_update(DelimiterBalance *db, char c);

// Update balance for a command
void delimiter_balance_update_cmd(DelimiterBalance *db, const char *cmd, const char *env_name);

// Check if all delimiters are balanced
bool delimiter_balance_is_balanced(const DelimiterBalance *db);

// Get first unbalanced delimiter type
const char *delimiter_balance_first_unbalanced(const DelimiterBalance *db);

#ifdef __cplusplus
}
#endif
