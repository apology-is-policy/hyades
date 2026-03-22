// Hyades MCP Server — Cloudflare Worker
//
// Implements MCP JSON-RPC protocol for the "render" tool.
// Wraps the render-only Hyades WASM binary.

// Polyfill: Emscripten glue accesses self.location.href to find the script
// directory. Cloudflare Workers don't provide location, so we stub it.
if (typeof location === "undefined") {
  // @ts-expect-error — intentional polyfill for Emscripten compatibility
  globalThis.location = { href: "/" };
}

import wasmModule from "./hyades-render.wasm";
import { TOOL_DESCRIPTION } from "./tool-description";

// Emscripten module interface
interface HyadesModule {
  ccall: (
    name: string,
    returnType: string,
    argTypes: string[],
    args: unknown[]
  ) => unknown;
  cwrap: (
    name: string,
    returnType: string,
    argTypes: string[]
  ) => (...args: unknown[]) => unknown;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
}

// Cached module instance (persists across requests in same isolate)
let modulePromise: Promise<HyadesModule> | null = null;

// Import the Emscripten JS glue
// @ts-expect-error — bundled Emscripten glue has no type declarations
import createHyadesModule from "./hyades-render.js";

function getModule(): Promise<HyadesModule> {
  if (!modulePromise) {
    modulePromise = createHyadesModule({
      // Prevent Emscripten from constructing URLs for supplementary files
      locateFile: (path: string) => path,
      instantiateWasm(
        imports: WebAssembly.Imports,
        callback: (instance: WebAssembly.Instance) => void
      ) {
        WebAssembly.instantiate(wasmModule, imports).then((instance) => {
          callback(instance);
        });
        return {};
      },
    }) as Promise<HyadesModule>;
  }
  return modulePromise;
}

// MCP protocol types
interface JsonRpcRequest {
  jsonrpc: "2.0";
  id?: string | number;
  method: string;
  params?: Record<string, unknown>;
}

interface JsonRpcResponse {
  jsonrpc: "2.0";
  id: string | number | null;
  result?: unknown;
  error?: { code: number; message: string; data?: unknown };
}

// Tool definition
const RENDER_TOOL = {
  name: "render",
  description: TOOL_DESCRIPTION,
  inputSchema: {
    type: "object" as const,
    properties: {
      source: {
        type: "string",
        description:
          "Complete Hyades document source. Use $$...$$ for display math, $...$ for inline math. Plain text outside math is rendered as prose.",
      },
      width: {
        type: "integer",
        description:
          "Output width in characters (default: 80). Use 80 for standard terminal width. Avoid going below 60 for complex math.",
        default: 80,
      },
      ascii: {
        type: "boolean",
        description:
          "Use ASCII-only output instead of Unicode (default: false)",
        default: false,
      },
    },
    required: ["source"],
  },
};

// Server info
const SERVER_INFO = {
  name: "hyades",
  version: "1.0.0",
};

const PROTOCOL_VERSION = "2024-11-05";

// Safety limits
const MAX_BODY_BYTES = 64 * 1024; // 64 KB — generous for any JSON-RPC request
const MAX_SOURCE_LENGTH = 16_000; // ~16K chars — well beyond any realistic formula

// Render using WASM module
async function render(
  source: string,
  width: number,
  ascii: boolean
): Promise<string> {
  const mod = await getModule();
  const result = mod.ccall(
    "hyades_mcp_render",
    "string",
    ["string", "number", "number"],
    [source, width, ascii ? 1 : 0]
  ) as string;
  return result;
}

// Handle MCP JSON-RPC request
async function handleRpc(req: JsonRpcRequest): Promise<JsonRpcResponse> {
  const id = req.id ?? null;

  switch (req.method) {
    case "initialize":
      return {
        jsonrpc: "2.0",
        id,
        result: {
          protocolVersion: PROTOCOL_VERSION,
          capabilities: { tools: {} },
          serverInfo: SERVER_INFO,
        },
      };

    case "notifications/initialized":
      // Client notification, no response needed for notifications
      // but if it has an id, respond
      if (id !== null) {
        return { jsonrpc: "2.0", id, result: {} };
      }
      return null as unknown as JsonRpcResponse;

    case "ping":
      return { jsonrpc: "2.0", id, result: {} };

    case "tools/list":
      return {
        jsonrpc: "2.0",
        id,
        result: { tools: [RENDER_TOOL] },
      };

    case "tools/call": {
      const params = req.params as {
        name: string;
        arguments?: { source?: string; width?: number; ascii?: boolean };
      };

      if (params.name !== "render") {
        return {
          jsonrpc: "2.0",
          id,
          error: { code: -32602, message: `Unknown tool: ${params.name}` },
        };
      }

      const args = params.arguments ?? {};
      if (!args.source) {
        return {
          jsonrpc: "2.0",
          id,
          result: {
            content: [{ type: "text", text: "Error: 'source' is required" }],
            isError: true,
          },
        };
      }

      if (args.source.length > MAX_SOURCE_LENGTH) {
        return {
          jsonrpc: "2.0",
          id,
          result: {
            content: [
              {
                type: "text",
                text: `Source too large (${args.source.length} chars, max ${MAX_SOURCE_LENGTH})`,
              },
            ],
            isError: true,
          },
        };
      }

      try {
        const t0 = Date.now();
        const output = await render(
          args.source,
          args.width ?? 80,
          args.ascii ?? false
        );
        console.log(
          `render: ${args.source.length} chars, ${args.width ?? 80}w, ${Date.now() - t0}ms`
        );

        const isError = output.startsWith("ERROR: ");
        return {
          jsonrpc: "2.0",
          id,
          result: {
            content: [
              {
                type: "text",
                text: isError ? output.slice(7) : output,
              },
            ],
            isError,
          },
        };
      } catch (e) {
        return {
          jsonrpc: "2.0",
          id,
          result: {
            content: [
              {
                type: "text",
                text: `Internal error: ${e instanceof Error ? e.message : String(e)}`,
              },
            ],
            isError: true,
          },
        };
      }
    }

    default:
      return {
        jsonrpc: "2.0",
        id,
        error: { code: -32601, message: `Method not found: ${req.method}` },
      };
  }
}

// CORS headers
function corsHeaders(): HeadersInit {
  return {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type",
  };
}

export default {
  async fetch(request: Request): Promise<Response> {
    const url = new URL(request.url);

    // CORS preflight
    if (request.method === "OPTIONS") {
      return new Response(null, { status: 204, headers: corsHeaders() });
    }

    // RFC 9728 Protected Resource Metadata — tells MCP clients (including
    // claude.ai) that this server requires no authorization.
    if (url.pathname === "/.well-known/oauth-protected-resource") {
      return new Response(
        JSON.stringify({
          resource: `${url.origin}/mcp`,
          // No authorization_servers — signals this is an open/authless resource
        }),
        {
          headers: {
            "Content-Type": "application/json",
            ...corsHeaders(),
          },
        }
      );
    }

    // Health check
    if (url.pathname === "/health" && request.method === "GET") {
      return new Response(JSON.stringify({ status: "ok", ...SERVER_INFO }), {
        headers: { "Content-Type": "application/json", ...corsHeaders() },
      });
    }

    // MCP endpoint
    if (url.pathname === "/mcp") {
      // GET /mcp — SSE endpoint for server-to-client notifications.
      // This server never sends unsolicited notifications, and Cloudflare
      // Workers can't hold long-lived connections (they time out).
      // Return 405 so MCP clients know not to retry.
      if (request.method === "GET") {
        return new Response(
          JSON.stringify({
            jsonrpc: "2.0",
            id: null,
            error: {
              code: -32000,
              message: "SSE not supported — this server uses POST-only Streamable HTTP",
            },
          }),
          {
            status: 405,
            headers: {
              "Content-Type": "application/json",
              Allow: "POST, OPTIONS",
              ...corsHeaders(),
            },
          }
        );
      }

      // POST /mcp — JSON-RPC
      if (request.method === "POST") {
        // Cap request body size before parsing to prevent memory abuse.
        const contentLength = parseInt(
          request.headers.get("Content-Length") ?? "0",
          10
        );
        if (contentLength > MAX_BODY_BYTES) {
          return new Response(
            JSON.stringify({
              jsonrpc: "2.0",
              id: null,
              error: {
                code: -32600,
                message: `Request too large (max ${MAX_BODY_BYTES} bytes)`,
              },
            }),
            {
              status: 413,
              headers: {
                "Content-Type": "application/json",
                ...corsHeaders(),
              },
            }
          );
        }

        // Read body with size guard (Content-Length can be absent or spoofed)
        let rawBody: string;
        try {
          const buf = await request.arrayBuffer();
          if (buf.byteLength > MAX_BODY_BYTES) {
            return new Response(
              JSON.stringify({
                jsonrpc: "2.0",
                id: null,
                error: {
                  code: -32600,
                  message: `Request too large (max ${MAX_BODY_BYTES} bytes)`,
                },
              }),
              {
                status: 413,
                headers: {
                  "Content-Type": "application/json",
                  ...corsHeaders(),
                },
              }
            );
          }
          rawBody = new TextDecoder().decode(buf);
        } catch {
          return new Response(
            JSON.stringify({
              jsonrpc: "2.0",
              id: null,
              error: { code: -32700, message: "Failed to read request body" },
            }),
            {
              status: 400,
              headers: {
                "Content-Type": "application/json",
                ...corsHeaders(),
              },
            }
          );
        }

        let body: JsonRpcRequest;
        try {
          body = JSON.parse(rawBody) as JsonRpcRequest;
        } catch {
          return new Response(
            JSON.stringify({
              jsonrpc: "2.0",
              id: null,
              error: { code: -32700, message: "Parse error" },
            }),
            {
              status: 400,
              headers: {
                "Content-Type": "application/json",
                ...corsHeaders(),
              },
            }
          );
        }

        const response = await handleRpc(body);

        // Notifications (no id) don't get a response
        if (!response) {
          return new Response(null, { status: 202, headers: corsHeaders() });
        }

        return new Response(JSON.stringify(response), {
          headers: { "Content-Type": "application/json", ...corsHeaders() },
        });
      }
    }

    return new Response("Not Found", { status: 404, headers: corsHeaders() });
  },
};
