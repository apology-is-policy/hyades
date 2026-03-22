// mcp_server.c — Native Hyades MCP server (stdio transport)
//
// Reads JSON-RPC requests from stdin (one per line), writes responses to stdout.
// Implements the MCP protocol subset: initialize, tools/list, tools/call, ping.
//
// Usage:
//   In Claude Desktop / Claude Code MCP config:
//   { "mcpServers": { "hyades": { "command": "/path/to/hyades-mcp" } } }

#include "hyades.h"
#include "mcp_json.h"
#include "mcp_tool_description.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MCP_SERVER_NAME "hyades"
#define MCP_SERVER_VERSION HYADES_VERSION_STRING
#define MCP_PROTOCOL_VERSION "2024-11-05"

#define MAX_SOURCE_LENGTH 16000 // Match the remote server's limit

// ---------------------------------------------------------------------------
// I/O helpers
// ---------------------------------------------------------------------------

// Read one line from stdin. Returns newly allocated string, or NULL on EOF.
static char *read_line(void) {
    size_t capacity = 4096;
    size_t len = 0;
    char *line = malloc(capacity);
    if (!line) return NULL;

    int c;
    while ((c = getchar()) != EOF && c != '\n') {
        if (len + 1 >= capacity) {
            capacity *= 2;
            char *tmp = realloc(line, capacity);
            if (!tmp) {
                free(line);
                return NULL;
            }
            line = tmp;
        }
        line[len++] = (char)c;
    }

    if (c == EOF && len == 0) {
        free(line);
        return NULL;
    }

    line[len] = '\0';

    // Strip trailing \r (CRLF on Windows)
    if (len > 0 && line[len - 1] == '\r')
        line[--len] = '\0';

    return line;
}

// ---------------------------------------------------------------------------
// Response builders
// ---------------------------------------------------------------------------

// Send a JSON-RPC result response. `result_json` is a raw JSON value string.
static void send_result(const char *id, const char *result_json) {
    // id can be a number or quoted string — pass through as-is
    fprintf(stdout, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}\n", id, result_json);
    fflush(stdout);
}

// Send a JSON-RPC error response.
static void send_error(const char *id, int code, const char *message) {
    char *esc_msg = mcp_json_escape(message);
    fprintf(stdout, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":%d,\"message\":\"%s\"}}\n",
            id, code, esc_msg);
    fflush(stdout);
    free(esc_msg);
}

// Send a tool result (content array with text).
static void send_tool_result(const char *id, const char *text, bool is_error) {
    char *esc = mcp_json_escape(text);
    fprintf(stdout,
            "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":"
            "{\"content\":[{\"type\":\"text\",\"text\":\"%s\"}],"
            "\"isError\":%s}}\n",
            id, esc, is_error ? "true" : "false");
    fflush(stdout);
    free(esc);
}

// ---------------------------------------------------------------------------
// Tool schema (JSON for tools/list response)
// ---------------------------------------------------------------------------

static void send_tools_list(const char *id) {
    // Build the tool description as escaped JSON string
    char *esc_desc = mcp_json_escape(MCP_TOOL_DESCRIPTION);

    fprintf(stdout,
            "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"tools\":[{"
            "\"name\":\"render\","
            "\"description\":\"%s\","
            "\"inputSchema\":{"
            "\"type\":\"object\","
            "\"properties\":{"
            "\"source\":{"
            "\"type\":\"string\","
            "\"description\":\"Complete Hyades document source. "
            "Use $$...$$ for display math, $...$ for inline math. "
            "Plain text outside math is rendered as prose.\""
            "},"
            "\"width\":{"
            "\"type\":\"integer\","
            "\"description\":\"Output width in characters (default: 80).\","
            "\"default\":80"
            "},"
            "\"ascii\":{"
            "\"type\":\"boolean\","
            "\"description\":\"Use ASCII-only output (default: false).\","
            "\"default\":false"
            "}"
            "},"
            "\"required\":[\"source\"]"
            "}"
            "}]}}\n",
            id, esc_desc);
    fflush(stdout);
    free(esc_desc);
}

// ---------------------------------------------------------------------------
// Request dispatch
// ---------------------------------------------------------------------------

// Extract the "id" field as a raw JSON token (number or string with quotes).
// Returns "null" if no id (notification). Caller must free().
static char *extract_id(const char *json) {
    // Look for "id": in the top-level object
    const char *p = strstr(json, "\"id\"");
    if (!p) return strdup("null");

    p += 4; // past "id"
    while (*p == ' ' || *p == '\t' || *p == ':') p++;

    if (*p == '"') {
        // String id — find closing quote
        const char *end = p + 1;
        while (*end && *end != '"') {
            if (*end == '\\') end++;
            end++;
        }
        size_t len = (size_t)(end - p + 1);
        char *id = malloc(len + 1);
        memcpy(id, p, len);
        id[len] = '\0';
        return id;
    } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
        // Numeric id
        const char *end = p;
        if (*end == '-') end++;
        while (*end >= '0' && *end <= '9') end++;
        size_t len = (size_t)(end - p);
        char *id = malloc(len + 1);
        memcpy(id, p, len);
        id[len] = '\0';
        return id;
    } else if (strncmp(p, "null", 4) == 0) {
        return strdup("null");
    }

    return strdup("null");
}

static void handle_request(const char *json) {
    char *method = mcp_json_get_string(json, "method");
    char *id = extract_id(json);

    if (!method) {
        send_error(id, -32600, "Invalid request: missing method");
        free(id);
        return;
    }

    // --- initialize ---
    if (strcmp(method, "initialize") == 0) {
        send_result(id, "{\"protocolVersion\":\"" MCP_PROTOCOL_VERSION "\","
                        "\"capabilities\":{\"tools\":{}},"
                        "\"serverInfo\":{\"name\":\"" MCP_SERVER_NAME "\","
                        "\"version\":\"" MCP_SERVER_VERSION "\"}}");
    }
    // --- notifications/initialized ---
    else if (strcmp(method, "notifications/initialized") == 0) {
        // Notification — no response unless it has an id
        if (strcmp(id, "null") != 0) {
            send_result(id, "{}");
        }
    }
    // --- ping ---
    else if (strcmp(method, "ping") == 0) {
        send_result(id, "{}");
    }
    // --- tools/list ---
    else if (strcmp(method, "tools/list") == 0) {
        send_tools_list(id);
    }
    // --- tools/call ---
    else if (strcmp(method, "tools/call") == 0) {
        char *params = mcp_json_get_object(json, "params");
        if (!params) {
            send_error(id, -32602, "Missing params");
            goto cleanup;
        }

        char *tool_name = mcp_json_get_string(params, "name");
        if (!tool_name || strcmp(tool_name, "render") != 0) {
            send_error(id, -32602, tool_name ? "Unknown tool" : "Missing tool name");
            free(tool_name);
            free(params);
            goto cleanup;
        }
        free(tool_name);

        char *args = mcp_json_get_object(params, "arguments");
        free(params);

        if (!args) {
            send_tool_result(id, "Error: missing arguments", true);
            goto cleanup;
        }

        char *source = mcp_json_get_string(args, "source");
        if (!source) {
            send_tool_result(id, "Error: 'source' is required", true);
            free(args);
            goto cleanup;
        }

        if (strlen(source) > MAX_SOURCE_LENGTH) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Source too large (%zu chars, max %d)", strlen(source),
                     MAX_SOURCE_LENGTH);
            send_tool_result(id, msg, true);
            free(source);
            free(args);
            goto cleanup;
        }

        int width = mcp_json_get_int(args, "width", 80);
        bool ascii = mcp_json_get_bool(args, "ascii", false);
        free(args);

        // Render
        hyades_reset_state();

        HyadesOptions opts = hyades_default_options();
        if (width > 0) opts.width = width;
        opts.unicode = !ascii;
        opts.math_italic = !ascii;

        HyadesError error;
        hyades_error_init(&error);

        char *result = hyades_render(source, &opts, &error);
        free(source);

        if (result) {
            bool render_err = (strncmp(result, "ERROR: ", 7) == 0);
            if (render_err) {
                send_tool_result(id, result + 7, true);
            } else {
                int nwarn = hyades_render_warning_count();
                if (nwarn > 0) {
                    // Append warning block to rendered output
                    size_t result_len = strlen(result);
                    size_t extra = 32; // header + padding
                    for (int i = 0; i < nwarn; i++)
                        extra += strlen(hyades_render_warning_message(i)) + 4; // "- " + "\n"
                    char *combined = malloc(result_len + extra);
                    if (combined) {
                        size_t pos = 0;
                        memcpy(combined, result, result_len);
                        pos = result_len;
                        pos += sprintf(combined + pos, "\n\n[warnings]\n");
                        for (int i = 0; i < nwarn; i++)
                            pos +=
                                sprintf(combined + pos, "- %s\n", hyades_render_warning_message(i));
                        combined[pos] = '\0';
                        send_tool_result(id, combined, false);
                        free(combined);
                    } else {
                        send_tool_result(id, result, false);
                    }
                } else {
                    send_tool_result(id, result, false);
                }
            }
            free(result);
        } else {
            send_tool_result(id, error.message, true);
        }
    }
    // --- unknown method ---
    else {
        char msg[256];
        snprintf(msg, sizeof(msg), "Method not found: %s", method);
        send_error(id, -32601, msg);
    }

cleanup:
    free(method);
    free(id);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
    hyades_init();

    char *line;
    while ((line = read_line()) != NULL) {
        // Skip empty lines
        if (line[0] == '\0') {
            free(line);
            continue;
        }

        handle_request(line);
        free(line);
    }

    hyades_shutdown();
    return 0;
}
