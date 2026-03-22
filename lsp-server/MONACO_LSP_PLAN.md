# Monaco LSP Integration Plan

## Overview
Add LSP features (hover, go-to-definition) to the Monaco-based web app at `wasm/`.

## Current State
- WASM already exports LSP functions in `wasm/hyades_lsp.c`
- Web app uses `hyades-api.js` to wrap WASM
- Monaco editor set up in `cassilda-app.js`

## Tasks

### Task 1: Ensure ES6 WASM Build Has LSP Exports

Check `wasm/CMakeLists.txt` around line 195 - the `hyades_wasm` target (ES6 build) needs the same exports as `hyades_wasm_compat`.

**File:** `wasm/CMakeLists.txt`

Add to `hyades_wasm` target's EXPORTED_FUNCTIONS (if not already):
```
"_hyades_lsp_parse"
"_hyades_lsp_get_hover"
"_hyades_lsp_get_definition"
"_hyades_lsp_clear"
"_hyades_lsp_free"
```

Then rebuild:
```bash
cd wasm/build && make hyades_wasm
```

### Task 2: Extend hyades-api.js with LSP Methods

**File:** `wasm/hyades-api.js`

Add to the `Hyades` class after the existing methods (around line 140):

```javascript
    // =========================================================================
    // LSP Features
    // =========================================================================

    /**
     * Parse document for LSP features (hover, definitions, etc.)
     * Must be called before using other LSP methods.
     * @param {string} source - Document source code
     */
    lspParse(source) {
        const sourcePtr = this.#module.allocateUTF8(source);
        this.#module._hyades_lsp_parse(sourcePtr);
        this.#module._free(sourcePtr);
    }

    /**
     * Get hover information at position
     * @param {number} line - 0-based line number
     * @param {number} col - 0-based column number
     * @returns {Object|null} Hover info with contents and range, or null
     */
    lspGetHover(line, col) {
        const resultPtr = this.#module._hyades_lsp_get_hover(line, col);
        const resultJson = this.#module.UTF8ToString(resultPtr);
        this.#module._hyades_lsp_free(resultPtr);

        try {
            const result = JSON.parse(resultJson);
            return result === null ? null : result;
        } catch {
            return null;
        }
    }

    /**
     * Get definition location at position
     * @param {number} line - 0-based line number
     * @param {number} col - 0-based column number
     * @returns {Object|null} Definition location with range, or null
     */
    lspGetDefinition(line, col) {
        const resultPtr = this.#module._hyades_lsp_get_definition(line, col);
        const resultJson = this.#module.UTF8ToString(resultPtr);
        this.#module._hyades_lsp_free(resultPtr);

        try {
            const result = JSON.parse(resultJson);
            return result === null ? null : result;
        } catch {
            return null;
        }
    }

    /**
     * Clear LSP state (call when switching documents)
     */
    lspClear() {
        this.#module._hyades_lsp_clear();
    }
```

Also add `allocateUTF8` helper if needed, or use existing pattern.

### Task 3: Add Monaco Providers in cassilda-app.js

**File:** `wasm/cassilda-app.js`

#### 3a. Add LSP state tracking (near top, after line ~25):

```javascript
// LSP state
let lspParseTimeout = null;
let lspParsed = false;
```

#### 3b. Add LSP parse trigger function (after initializeHyades):

```javascript
// ============================================================================
// LSP Integration
// ============================================================================

function triggerLspParse() {
    if (!hyades) return;

    // Debounce - wait 300ms after last change
    if (lspParseTimeout) {
        clearTimeout(lspParseTimeout);
    }

    lspParseTimeout = setTimeout(() => {
        const source = editor.getValue();
        hyades.lspParse(source);
        lspParsed = true;
    }, 300);
}
```

#### 3c. Register hover provider (in registerCassildaLanguage or after editor creation):

```javascript
// Register LSP hover provider
monaco.languages.registerHoverProvider('cassilda', {
    provideHover: function(model, position) {
        if (!hyades || !lspParsed) return null;

        const hover = hyades.lspGetHover(position.lineNumber - 1, position.column - 1);
        if (!hover) return null;

        return {
            contents: [{ value: hover.contents.value }],
            range: new monaco.Range(
                hover.range.start.line + 1,
                hover.range.start.character + 1,
                hover.range.end.line + 1,
                hover.range.end.character + 1
            )
        };
    }
});
```

#### 3d. Register definition provider:

```javascript
// Register LSP definition provider
monaco.languages.registerDefinitionProvider('cassilda', {
    provideDefinition: function(model, position) {
        if (!hyades || !lspParsed) return null;

        const def = hyades.lspGetDefinition(position.lineNumber - 1, position.column - 1);
        if (!def) return null;

        return {
            uri: model.uri,
            range: new monaco.Range(
                def.range.start.line + 1,
                def.range.start.character + 1,
                def.range.end.line + 1,
                def.range.end.character + 1
            )
        };
    }
});
```

#### 3e. Hook up document change event (in createEditor, add to onDidChangeModelContent):

```javascript
editor.onDidChangeModelContent(() => {
    if (!isProgrammaticChange) {
        markTabModified(true);
    }
    updateDecorations();
    triggerLspParse();  // <-- ADD THIS LINE
});
```

#### 3f. Trigger initial parse after loading content:

In `loadTabsFromStorage()` or wherever initial content is set, add:
```javascript
triggerLspParse();
```

### Task 4: Test

1. Rebuild WASM: `cd wasm/build && make hyades_wasm`
2. Serve the web app: `cd wasm && python3 -m http.server 8080`
3. Open http://localhost:8080
4. Test hover on macros
5. Test F12/Ctrl+Click for go-to-definition

## Files to Modify

| File | Changes |
|------|---------|
| `wasm/CMakeLists.txt` | Add LSP exports to hyades_wasm target |
| `wasm/hyades-api.js` | Add lspParse, lspGetHover, lspGetDefinition, lspClear methods |
| `wasm/cassilda-app.js` | Add LSP state, providers, and change hooks |

## Verification Checklist

- [ ] Hover on `\macro<\name...>` definition shows info
- [ ] Hover on `\name{...}` call shows macro info
- [ ] Hover on `\begin{name}` shows macro info
- [ ] Hover on `\recall<STD::RANGE>` shows stdlib docs
- [ ] F12 on macro call jumps to definition
- [ ] F12 on stdlib macro does nothing (no definition in file)
- [ ] Hover updates after editing document
