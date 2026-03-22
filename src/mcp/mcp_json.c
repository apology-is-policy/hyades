// mcp_json.c — Minimal JSON helpers for MCP JSON-RPC
//
// Scans for "key": patterns and extracts values. Handles strings, integers,
// booleans, and nested objects. Not a general parser — assumes well-formed
// JSON from MCP clients.

#include "mcp_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal: find the value position after "key":
// ---------------------------------------------------------------------------

// Skip whitespace
static const char *skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

// Skip a JSON string (starting at opening quote), return pointer past closing quote
static const char *skip_string(const char *p) {
    if (*p != '"') return p;
    p++;
    while (*p && *p != '"') {
        if (*p == '\\') p++; // skip escaped char
        p++;
    }
    if (*p == '"') p++;
    return p;
}

// Find the value position for a given key in a JSON object.
// Returns pointer to the first non-whitespace char of the value, or NULL.
static const char *find_value(const char *json, const char *key) {
    size_t key_len = strlen(key);
    const char *p = json;

    while (*p) {
        // Look for a quoted key
        const char *q = strstr(p, "\"");
        if (!q) return NULL;
        q++; // past opening quote

        // Check if this key matches
        if (strncmp(q, key, key_len) == 0 && q[key_len] == '"') {
            // Found "key" — skip to colon and value
            const char *after = q + key_len + 1; // past closing quote
            after = skip_ws(after);
            if (*after == ':') {
                after++;
                return skip_ws(after);
            }
        }

        // Skip past this string to continue searching
        p = skip_string(q - 1);
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

// Unescape a JSON string value (between quotes). Returns newly allocated string.
static char *unescape_string(const char *start, size_t len) {
    char *out = malloc(len + 1);
    if (!out) return NULL;

    char *w = out;
    const char *end = start + len;
    for (const char *r = start; r < end; r++) {
        if (*r == '\\' && r + 1 < end) {
            r++;
            switch (*r) {
            case '"': *w++ = '"'; break;
            case '\\': *w++ = '\\'; break;
            case 'n': *w++ = '\n'; break;
            case 'r': *w++ = '\r'; break;
            case 't': *w++ = '\t'; break;
            case '/': *w++ = '/'; break;
            default:
                *w++ = '\\';
                *w++ = *r;
                break;
            }
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
    return out;
}

char *mcp_json_get_string(const char *json, const char *key) {
    const char *val = find_value(json, key);
    if (!val || *val != '"') return NULL;

    val++; // past opening quote
    const char *end = val;
    while (*end && *end != '"') {
        if (*end == '\\') end++; // skip escaped char
        end++;
    }

    return unescape_string(val, (size_t)(end - val));
}

int mcp_json_get_int(const char *json, const char *key, int default_val) {
    const char *val = find_value(json, key);
    if (!val) return default_val;

    // Handle numbers and negative numbers
    if (*val == '-' || isdigit((unsigned char)*val)) {
        return atoi(val);
    }
    return default_val;
}

bool mcp_json_get_bool(const char *json, const char *key, bool default_val) {
    const char *val = find_value(json, key);
    if (!val) return default_val;

    if (strncmp(val, "true", 4) == 0) return true;
    if (strncmp(val, "false", 5) == 0) return false;
    return default_val;
}

char *mcp_json_get_object(const char *json, const char *key) {
    const char *val = find_value(json, key);
    if (!val) return NULL;

    char open, close;
    if (*val == '{') {
        open = '{';
        close = '}';
    } else if (*val == '[') {
        open = '[';
        close = ']';
    } else
        return NULL;

    // Find matching close bracket, respecting nesting and strings
    int depth = 0;
    const char *p = val;
    while (*p) {
        if (*p == '"') {
            p = skip_string(p);
            continue;
        }
        if (*p == open)
            depth++;
        else if (*p == close) {
            depth--;
            if (depth == 0) {
                size_t len = (size_t)(p - val + 1);
                char *result = malloc(len + 1);
                if (!result) return NULL;
                memcpy(result, val, len);
                result[len] = '\0';
                return result;
            }
        }
        p++;
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

char *mcp_json_escape(const char *s) {
    if (!s) return strdup("");

    size_t len = strlen(s);
    size_t extra = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\\' || *p == '"' || *p == '\n' || *p == '\r' || *p == '\t') extra++;
    }

    char *escaped = malloc(len + extra + 1);
    if (!escaped) return strdup("");

    char *out = escaped;
    for (const char *p = s; *p; p++) {
        switch (*p) {
        case '\\':
            *out++ = '\\';
            *out++ = '\\';
            break;
        case '"':
            *out++ = '\\';
            *out++ = '"';
            break;
        case '\n':
            *out++ = '\\';
            *out++ = 'n';
            break;
        case '\r':
            *out++ = '\\';
            *out++ = 'r';
            break;
        case '\t':
            *out++ = '\\';
            *out++ = 't';
            break;
        default: *out++ = *p; break;
        }
    }
    *out = '\0';
    return escaped;
}
