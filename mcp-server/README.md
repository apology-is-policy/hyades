# Hyades MCP Server

An MCP (Model Context Protocol) server that exposes Hyades math rendering as a tool for AI assistants. Deployed as a Cloudflare Worker with a render-only WASM binary.

## Architecture

```
mcp-server/
├── wasm/           # Render-only WASM build (no Cassilda, Subnivean, stdlib)
│   ├── CMakeLists.txt
│   ├── mcp_bindings.c
│   └── build.sh
└── worker/         # Cloudflare Worker + MCP handler
    ├── src/index.ts
    ├── wrangler.toml
    └── package.json
```

## Build & Deploy

### 1. Build WASM (requires Emscripten)

```bash
cd mcp-server/wasm
./build.sh
# Produces: build/hyades-render.js + build/hyades-render.wasm
```

### 2. Set up Worker

```bash
cd mcp-server/worker
npm install
npm run copy-wasm   # Copies WASM artifacts to src/
```

### 3. Local development

```bash
npx wrangler dev
# Server runs at http://localhost:8787
```

### 4. Test

```bash
# Initialize
curl -X POST http://localhost:8787/mcp \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test"}}}'

# Render math
curl -X POST http://localhost:8787/mcp \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"render","arguments":{"source":"$$\\\\frac{a}{b}$$"}}}'
```

### 5. Deploy

```bash
npx wrangler deploy
```

## MCP Tool

**Tool name**: `render`

| Parameter | Type    | Default | Description                              |
|-----------|---------|---------|------------------------------------------|
| `source`  | string  | —       | Hyades/LaTeX source (required)           |
| `width`   | integer | 60      | Output width in characters               |
| `ascii`   | boolean | false   | ASCII-only output (no Unicode)           |

## Connect from Claude Code

```bash
claude mcp add --transport http hyades https://hyades-mcp.apg.workers.dev/mcp
```
