// mcp_json.h — Minimal JSON helpers for MCP JSON-RPC
//
// NOT a general JSON parser. Handles the fixed schema of MCP requests
// and builds well-formed JSON responses via snprintf.

#ifndef MCP_JSON_H
#define MCP_JSON_H

#include <stdbool.h>

// ---------------------------------------------------------------------------
// Parsing — extract values from a JSON string by key name
// ---------------------------------------------------------------------------

// Extract a string value for "key". Returns newly allocated string, or NULL.
// Handles escaped characters in the value. Caller must free().
char *mcp_json_get_string(const char *json, const char *key);

// Extract an integer value for "key". Returns default_val if not found.
int mcp_json_get_int(const char *json, const char *key, int default_val);

// Extract a boolean value for "key". Returns default_val if not found.
bool mcp_json_get_bool(const char *json, const char *key, bool default_val);

// Extract the substring for a nested object/array value at "key".
// Returns newly allocated string containing the { ... } or [ ... ] block,
// or NULL if not found. Caller must free().
char *mcp_json_get_object(const char *json, const char *key);

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

// Escape a string for inclusion in JSON (handles \, ", \n, \r, \t).
// Returns newly allocated string. Caller must free().
char *mcp_json_escape(const char *s);

#endif // MCP_JSON_H
