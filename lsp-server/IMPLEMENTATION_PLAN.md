# Hyades LSP - Remaining Implementation Plan

## Current State

### Working
- Native build compiles with all LSP infrastructure
- LSP server communicates via JSON-RPC (tested)
- Mock completions work (`\frac`, `\sum`, etc.)
- VSCode extension has LSP client configured
- `parse_document_as_vbox_lsp()` tracks `$$` and `\begin{vbox/hbox}` delimiters

### Not Yet Working
- WASM module not built with LSP exports
- No actual parsing/diagnostics (mock only)
- No symbol collection (macros, lambdas, counters)
- No source position mapping through transformations

---

## Phase 1: WASM Integration

**Goal:** Get the real C parser running in the LSP server

### 1.1 Build WASM with LSP Exports

```bash
cd wasm/build
rm -rf *
emcmake cmake ..
make -j4
```

Verify exports exist:
```bash
wasm-objdump -x hyades.wasm | grep hyades_lsp
```

### 1.2 Copy WASM to LSP Server

```bash
cp wasm/build/hyades.js lsp-server/
cp wasm/build/hyades.wasm lsp-server/
```

### 1.3 Update `hyades-wasm.ts` to Load Real WASM

Replace mock loading with actual Emscripten module loading:

```typescript
// In initialize():
if (modulePath?.endsWith('.js')) {
    // Load Emscripten module
    const createModule = require(modulePath);
    const module = await createModule();
    module._hyades_wasm_init();
    return new HyadesWasm(module);
}
```

### 1.4 Test Real Parsing

Update `test-lsp.js` to verify:
- Diagnostics for unclosed `$$` are reported
- Parse errors appear in diagnostics

---

## Phase 2: Diagnostic Improvements

**Goal:** Report useful errors to the IDE

### 2.1 Delimiter Tracking Enhancements

**File:** `src/document/document_parser.c`

Add tracking for:
- [ ] Inline math `$...$` (DELIM_SINGLE_DOLLAR)
- [ ] All `\begin{X}...\end{X}` pairs (not just vbox/hbox)
- [ ] Brace matching in macro arguments

```c
// In parsing loop, add:
if (*p == '$' && p[1] != '$') {
    // Track inline math delimiter
    lsp_ctx_push_delimiter(&lsp_ctx, DELIM_SINGLE_DOLLAR, "$", NULL, pos);
}
```

### 2.2 Error Message Quality

**File:** `src/utils/error.c`

Add helper for formatted error messages with context:
```c
void parse_error_add_with_context(ParseErrorList *list,
                                   ParseErrorCode code,
                                   int line, int col,
                                   const char *source_line,
                                   const char *fmt, ...);
```

### 2.3 Macro Expansion Errors

**File:** `src/document/macro_expand.c`

In `expand_all_macros_lsp()`:
- Report undefined macro usage
- Report argument count mismatches
- Report recursive expansion limits

---

## Phase 3: Symbol Collection

**Goal:** Enable go-to-definition, hover, find-references

### 3.1 Macro Definition Collection

**File:** `src/macro/user/macro.c`

Add `LspSymbolTable *` parameter to `macro_process_document()`:

```c
char *macro_process_document_lsp(const char *input,
                                  MacroRegistry *reg,
                                  LspSymbolTable *symbols,
                                  char *error_msg, int error_size);
```

When parsing `\macro<\name>{body}`:
```c
if (symbols) {
    Symbol *sym = lsp_symbol_table_add(symbols, name, SYMKIND_MACRO,
                                        line, col, end_line, end_col);
    symbol_set_signature(sym, param_signature);
    symbol_set_body_preview(sym, body);
}
```

### 3.2 Lambda/Value Collection

**File:** `src/document/calc.c`

Add symbol recording to scope binding functions:

```c
void scope_bind_lambda(Scope *s, const char *name, Lambda *lambda) {
    // ... existing code ...

    // Record in symbol table if available
    CalcContext *ctx = scope_get_context(s);  // Need to add this
    if (ctx && ctx->symbols) {
        Symbol *sym = lsp_symbol_table_add(ctx->symbols, name, SYMKIND_LAMBDA,
                                            ctx->current_line, ctx->current_col, 0, 0);
        // Build signature from lambda params
        char sig[256];
        build_lambda_signature(lambda, sig, sizeof(sig));
        symbol_set_signature(sym, sig);
    }
}
```

### 3.3 Reference Tracking

When a macro/lambda/value is used (not defined), record the reference:

```c
// In macro expansion when expanding \name:
if (symbols) {
    lsp_symbol_table_add_reference(symbols, name, line, col, end_line, end_col, false);
}
```

---

## Phase 4: Source Position Mapping

**Goal:** Map error positions back to original source

### 4.1 Track Comment Stripping

**File:** `src/document/document_parser.c`

Update `strip_tex_comments()` to record mappings:

```c
char *strip_tex_comments_lsp(const char *in, SourceMap *map) {
    // When removing a comment, record the mapping
    // so positions after the comment map correctly
    if (map) {
        source_map_add_deletion(map, start_line, start_col,
                                end_line, end_col);
    }
}
```

### 4.2 Track Macro Expansion

When `\macro` expands to its body:
- Record that positions in expanded text map to macro definition
- Or record that they map to macro invocation site

### 4.3 Use Mapping in Error Reporting

When creating a diagnostic:
```c
int orig_line, orig_col;
if (source_map_lookup(map, trans_line, &orig_line, &orig_col)) {
    // Use original position in error
}
```

---

## Phase 5: LSP Feature Completion

### 5.1 Hover Information

**File:** `src/public_api/hyades_parse_api.c`

Enhance `hyades_get_hover()`:
- Show macro signature and body preview
- Show lambda parameters
- Show counter/value current value (if computable)

### 5.2 Go-to-Definition

Already implemented via symbol table, but needs:
- [ ] Handle references correctly (not just definition sites)
- [ ] Support jumping to \input'd files

### 5.3 Find All References

Already scaffolded, needs:
- [ ] Collect all references during parsing
- [ ] Return correct file URIs for multi-file projects

### 5.4 Document Outline

Group symbols by kind:
- Macros
- Lambdas
- Counters
- Labels (Cassilda)

---

## Phase 6: VSCode Extension Polish

### 6.1 Semantic Highlighting (Optional)

Add semantic token provider for richer highlighting:
- Macro names
- Lambda names
- Math mode content
- Comments

### 6.2 Code Actions

- "Define macro" when hovering undefined `\name`
- "Extract to macro" for selected text

### 6.3 Snippets

Add completion snippets:
```json
{
  "\\begin{vbox}": "\\begin{vbox}\n\t$0\n\\end{vbox}",
  "\\macro": "\\macro<\\\\$1>{$2}"
}
```

---

## Implementation Order

### Sprint 1: Core Functionality
1. [ ] Build WASM with LSP exports
2. [ ] Load real WASM in server
3. [ ] Verify diagnostics for unclosed delimiters work
4. [ ] Test in VSCode

### Sprint 2: Symbol Collection
5. [ ] Add symbol collection to macro_process_document
6. [ ] Add symbol collection to calc.c
7. [ ] Test go-to-definition
8. [ ] Test hover

### Sprint 3: Polish
9. [ ] Enhanced delimiter tracking
10. [ ] Source position mapping
11. [ ] Better error messages
12. [ ] Document outline

---

## Testing Checklist

### Diagnostics
- [ ] Unclosed `$$` shows error on correct line
- [ ] Mismatched `\begin{X}...\end{Y}` shows error
- [ ] Unclosed `$` shows error
- [ ] Undefined macro warning

### Navigation
- [ ] Ctrl+Click on macro name jumps to definition
- [ ] Hover on macro shows signature
- [ ] Find all references works

### Completion
- [ ] `\` triggers command completion
- [ ] User-defined macros appear in completion
- [ ] Lambdas appear in completion

### Integration
- [ ] Extension activates on .cld files
- [ ] Server starts without errors
- [ ] No memory leaks in long sessions
