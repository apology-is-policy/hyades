/**
 * Cassilda Web Application
 * Monaco-based editor with Hyades WASM integration
 */

// NBSP character used to identify rendered output (matches extension.ts)
const NBSP = '\u00A0';

// Storage key for localStorage
const STORAGE_KEY = 'cassilda-tabs';

// Global state
let editor = null;
let hyades = null;
let outputDecorations = [];
let referenceDecorations = [];

// Tab system state
let tabs = [];          // Array of { id, name, content, modified, isPreview }
let activeTabId = null; // Currently active tab ID
let tabIdCounter = 0;   // For generating unique tab IDs
let previewTabId = null; // ID of the current preview tab (only one at a time)

// Flag to prevent marking tab modified during programmatic changes
let isProgrammaticChange = false;

// LSP state
let lspParseTimeout = null;
let lspParsed = false;
let semanticTokensChangeEmitter = null; // Fires to tell Monaco to re-request tokens
let cachedSemanticTokens = null; // Pre-computed after each parse

// Decoration debounce state (used by updateDecorations full rescan)
let decorationTimeout = null;

// Document processing guard
let processingInFlight = false;

// ============================================================================
// Web Worker Proxy
// ============================================================================

class HyadesWorkerProxy {
    #worker; #nextId = 0; #pending = new Map();
    #readyResolve; #ready;
    version = '';

    constructor(path) {
        this.#worker = new Worker(path, { type: 'module' });
        this.#ready = new Promise(r => { this.#readyResolve = r; });
        this.#worker.onmessage = (e) => {
            const msg = e.data;
            if (msg.type === 'ready') { this.version = msg.version; this.#readyResolve(); return; }
            if (msg.type === 'error') { console.error('Worker:', msg.message); return; }
            const p = this.#pending.get(msg.id);
            if (!p) return;
            this.#pending.delete(msg.id);
            msg.error ? p.reject(new Error(msg.error)) : p.resolve(msg.result);
        };
        this.#worker.onerror = () => {
            for (const [, p] of this.#pending) p.reject(new Error('Worker error'));
            this.#pending.clear();
        };
    }

    whenReady() { return this.#ready; }

    #call(method, params = {}) {
        const id = this.#nextId++;
        return new Promise((resolve, reject) => {
            this.#pending.set(id, { resolve, reject });
            this.#worker.postMessage({ id, method, params });
        });
    }

    lspParseAndGetTokens(source) { return this.#call('lspParseAndGetTokens', { source }); }
    lspGetHover(line, col) { return this.#call('lspGetHover', { line, col }); }
    lspGetDefinition(line, col) { return this.#call('lspGetDefinition', { line, col }); }
    cassilda(input) { return this.#call('cassilda', { input }); }
}

// ============================================================================
// Monaco Editor Setup
// ============================================================================

// Configure Monaco loader
require.config({
    paths: {
        'vs': 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs'
    }
});

// Load Monaco and initialize
require(['vs/editor/editor.main'], async function() {
    // Register Cassilda language
    registerCassildaLanguage();

    // Create editor
    createEditor();

    // Load WASM module
    await initializeHyades();

    // Setup event listeners
    setupEventListeners();

    // Load tabs from storage or create default
    loadTabsFromStorage();
});

// ============================================================================
// Cassilda Language Definition (from tmLanguage.json)
// ============================================================================

function registerCassildaLanguage() {
    // Register the language
    monaco.languages.register({ id: 'cassilda', extensions: ['.cld'] });

    // Define tokens (matching LSP semantic tokens and Tree-sitter grammar)
    // Note: In Monarch, @ has special meaning, so we use character class [@] to match literal @
    monaco.languages.setMonarchTokensProvider('cassilda', {
        tokenizer: {
            root: [
                // Rendered output (lines containing NBSP - appears after target_prefix)
                [/^.*\u00A0.*$/, 'comment.output'],

                // Comments
                [/%.*/, 'comment'],

                // Directives with values
                [/#(?:source_prefix|target_prefix|comment_char|output_prefix|output_suffix)\b/, 'keyword'],

                // Block directives
                [/#(?:before_each|after_each|end)\b/, 'keyword'],

                // Macro parameter reference ${name}
                [/\$\{/, { token: 'punctuation.special', next: '@parameterRef' }],

                // Label definition - use [@] to match literal @
                [/[@]label\s+/, { token: 'keyword', next: '@labelName' }],

                // Label end
                [/[@]end\b/, 'keyword'],

                // Cassilda reference - match everything on the line to avoid state bleeding
                [/([@]cassilda:)([ \t]*)([\w-][\w\-,\s]*)/, ['keyword', 'white', 'variable']],
                [/[@]cassilda:/, 'keyword'],  // Fallback for empty reference

                // Display math
                [/\$\$/, { token: 'keyword.math', next: '@displayMath' }],

                // Inline math (not ${...} which is macro parameter)
                [/\$(?!\{)/, { token: 'keyword.math', next: '@inlineMath' }],

                // Strings
                [/"[^"]*"/, 'string'],

                // Verbatim - capture delimiter and match until closing
                [/\\verb(.)/, { token: 'keyword', next: '@verbatim.$1' }],

                // Macro definition: \macro<signature>{body}
                [/\\macro/, { token: 'keyword', next: '@macroSignature' }],

                // Begin/end commands (keywords)
                [/\\(?:begin|end)\b/, 'keyword'],

                // Any \command - semantic tokens provide precise classification
                [/\\[a-zA-Z_][a-zA-Z0-9_]*/, 'function'],

                // Numbers
                [/[0-9]+\.?[0-9]*/, 'number'],

                // Angle brackets (type) - used for macro signatures
                [/[<>]/, 'type'],

                // Braces (punctuation.bracket)
                [/[{}]/, 'punctuation.bracket'],

                // Square brackets (punctuation.bracket)
                [/[\[\]]/, 'punctuation.bracket'],

                // Whitespace
                [/\s+/, 'white'],

                // Any other text
                [/./, 'source'],
            ],

            // Parameter reference: ${name}
            parameterRef: [
                [/[a-zA-Z_][a-zA-Z0-9_-]*/, 'variable.parameter'],
                [/\}/, { token: 'punctuation.special', next: '@pop' }],
                [/./, 'variable.parameter'],
            ],

            // Label name after @label
            labelName: [
                [/[\w-]+/, { token: 'variable', next: '@pop' }],
                [/\s+/, 'white'],
                [/./, { token: 'source', next: '@pop' }],
            ],

            // Macro signature: <\name[params]{...}>
            macroSignature: [
                [/</, 'type'],
                [/>/, { token: 'type', next: '@pop' }],
                [/\\[a-zA-Z_][a-zA-Z0-9_]*/, 'function.definition'],
                [/\[/, { token: 'punctuation.bracket', next: '@macroParams' }],
                [/\{/, { token: 'punctuation.bracket', next: '@macroArgPlaceholder' }],
                [/\s+/, 'white'],
                [/./, 'source'],
            ],

            // Macro parameters: [param1, param2=default]
            macroParams: [
                [/\]/, { token: 'punctuation.bracket', next: '@pop' }],
                [/[a-zA-Z_][a-zA-Z0-9_-]*/, 'variable.parameter'],
                [/=/, 'operator'],
                [/,/, 'punctuation'],
                [/\s+/, 'white'],
                [/./, 'source'],
            ],

            // Macro argument placeholder: {...} in signature
            macroArgPlaceholder: [
                [/\}/, { token: 'punctuation.bracket', next: '@pop' }],
                [/\{/, { token: 'punctuation.bracket', next: '@macroArgPlaceholder' }],
                [/./, 'string'],
            ],

            // Display math mode $$...$$
            displayMath: [
                [/\$\$/, { token: 'keyword.math', next: '@pop' }],
                { include: '@mathContent' }
            ],

            // Inline math mode $...$
            inlineMath: [
                [/\$/, { token: 'keyword.math', next: '@pop' }],
                { include: '@mathContent' }
            ],

            // Math content (shared between display and inline)
            // Kept lightweight - semantic tokens provide precise classification
            mathContent: [
                [/[_^]/, 'operator.subscript'],
                [/\\[a-zA-Z_][a-zA-Z0-9_]*/, 'function'],
                [/[0-9]+\.?[0-9]*/, 'number'],
                [/[{}]/, 'punctuation.bracket'],
                [/[+\-*/=&|<>()[\]]/, 'operator'],
                [/[a-zA-Z]/, 'variable.math'],
                [/\s+/, 'white'],
            ],

            // Verbatim content - match until closing delimiter (passed as $S2)
            verbatim: [
                [/./, {
                    cases: {
                        '$0==$S2': { token: 'string.verbatim', next: '@pop' },
                        '@default': 'string.verbatim'
                    }
                }],
            ]
        }
    });

    // Define theme colors (matching LSP semantic tokens and Tree-sitter)
    // =========================================================================
    // CUSTOMIZE COLORS HERE - token rules map token names to colors
    // Aligned with LSP semantic token types and Tree-sitter highlight queries
    // =========================================================================
    monaco.editor.defineTheme('cassilda-dark', {
        base: 'vs-dark',
        inherit: true,
        semanticHighlighting: true,
        semanticTokenColors: {
            comment:        '#6A9955',
            keyword:        '#C586C0',
            parameter:      '#FFCB6B',
            variable:       '#9CDCFE',
            function:       '#DCDCAA',
            'function.defaultLibrary': '#DCDCAA',
            macro:          '#DCDCAA',
            string:         '#CE9178',
            number:         '#B5CEA8',
            operator:       '#D4D4D4',
            class:          '#FFD700',
            namespace:      '#4EC9B0',
        },
        rules: [
            // Comments (green) - LSP: SEM_TOK_COMMENT
            { token: 'comment', foreground: '6A9955' },
            { token: 'comment.output', foreground: 'D4D4D4' },  // Rendered output lines

            // Keywords (pink) - LSP: SEM_TOK_KEYWORD
            // Directives, @label/@end, \begin/\end, \macro, math delimiters
            { token: 'keyword', foreground: 'C586C0' },
            { token: 'keyword.math', foreground: 'C586C0' },  // $ and $$

            // Control commands (blue) - LSP: SEM_TOK_KEYWORD
            // \if, \else, \let, \assign, \recall, etc.
            { token: 'keyword.control', foreground: '569CD6' },

            // Built-in functions (yellow) - LSP: SEM_TOK_FUNCTION + DEFAULT_LIBRARY
            // Layout: \table, \hbox, \vbox, \hrule, etc.
            // Math: \frac, \sum, \int, Greek letters, etc.
            { token: 'function.builtin', foreground: 'DCDCAA' },

            // User-defined functions/commands (light yellow) - LSP: SEM_TOK_FUNCTION
            { token: 'function', foreground: 'DCDCAA' },

            // Macro definition name - LSP: SEM_TOK_FUNCTION (in definition context)
            { token: 'function.definition', foreground: 'DCDCAA', fontStyle: 'bold' },

            // Variables (light blue) - LSP: SEM_TOK_VARIABLE
            { token: 'variable', foreground: '9CDCFE' },
            { token: 'variable.math', foreground: '9CDCFE' },  // Math variables (x, y, n)

            // Parameters (orange-ish) - LSP: SEM_TOK_PARAMETER
            // Macro parameters in ${name} and [param] definitions
            { token: 'variable.parameter', foreground: 'FFCB6B' },

            // Type (teal) - angle brackets and environment names
            // Tree-sitter: @type
            { token: 'type', foreground: '4EC9B0' },

            // Strings (orange) - LSP: SEM_TOK_STRING
            { token: 'string', foreground: 'CE9178' },
            { token: 'string.verbatim', foreground: 'CE9178' },

            // Numbers (light green) - LSP: SEM_TOK_NUMBER
            { token: 'number', foreground: 'B5CEA8' },

            // Operators - LSP: SEM_TOK_OPERATOR
            { token: 'operator', foreground: 'D4D4D4' },
            // Subscript/superscript operators in math (distinct color)
            { token: 'operator.subscript', foreground: 'C586C0' },

            // Punctuation
            { token: 'punctuation', foreground: 'D4D4D4' },
            { token: 'punctuation.bracket', foreground: 'FFD700' },  // Braces: gold
            { token: 'punctuation.special', foreground: 'C586C0' },  // ${...}: pink

            // Default source text
            { token: 'source', foreground: 'D4D4D4' },
        ],
        // =========================================================================
        // CUSTOMIZE EDITOR COLORS HERE
        // =========================================================================
        colors: {
            'editor.background': '#000000', // #1e1e1e
            'editor.foreground': '#f0f8ff',
            'editorLineNumber.foreground': '#5a5a5a',
            'editorLineNumber.activeForeground': '#c6c6c6',
            'editor.selectionBackground': '#264f78',
            'editor.lineHighlightBackground': '#2a2a2a',
        }
    });
}

// ============================================================================
// Editor Creation
// ============================================================================

function createEditor() {
    editor = monaco.editor.create(document.getElementById('editor'), {
        value: '',
        language: 'cassilda',
        theme: 'cassilda-dark',
        fontFamily: 'JuliaMono, Menlo, Monaco, Courier New, monospace',
        fontSize: 13,
        fontWeight: '300',
        lineHeight: 18,
        minimap: { enabled: false },
        scrollBeyondLastLine: false,
        automaticLayout: true,
        wordWrap: 'off',
        renderWhitespace: 'none',
        folding: true,
        lineNumbers: 'on',
        glyphMargin: false,
        lineDecorationsWidth: 5,
        lineNumbersMinChars: 4,
        padding: { top: 8, bottom: 8 },
        smoothScrolling: true,
        cursorBlinking: 'smooth',
        cursorSmoothCaretAnimation: 'on',
        // Disable ambiguous unicode character highlighting
        unicodeHighlight: {
            ambiguousCharacters: false,
            invisibleCharacters: false,
            nonBasicASCII: false,
        },
    });

    // Add custom keybindings to Monaco
    // Use addAction for more control over keybindings
    editor.addAction({
        id: 'cassilda-run',
        label: 'Run Cassilda',
        keybindings: [
            monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter,
        ],
        run: () => {
            processDocument();
        }
    });

    editor.addAction({
        id: 'cassilda-save',
        label: 'Save File',
        keybindings: [
            monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS,
        ],
        run: () => {
            saveFile();
        }
    });

    // Update cursor position display
    editor.onDidChangeCursorPosition((e) => {
        document.getElementById('cursorPosition').textContent =
            `Ln ${e.position.lineNumber}, Col ${e.position.column}`;
    });

    // Track modifications (only for user edits, not programmatic changes)
    editor.onDidChangeModelContent((e) => {
        if (!isProgrammaticChange) {
            markTabModified(true);
        }
        // Only rescan decorations if the edit could affect @cassilda: lines.
        // NBSP (output) lines only change via processDocument(), never typing.
        if (!isProgrammaticChange) {
            updateDecorationsIncremental(e.changes);
        }
        // Invalidate cached tokens — positions are wrong after any edit
        cachedSemanticTokens = null;
        // Trigger LSP parse for hover/go-to-definition
        triggerLspParse();
    });

    // Register folding provider for output blocks
    monaco.languages.registerFoldingRangeProvider('cassilda', {
        provideFoldingRanges: function(model) {
            return computeFoldingRanges(model);
        }
    });
}

// ============================================================================
// Decorations (matching VS Code extension)
// ============================================================================

const REFERENCE_RE = /^\s*@cassilda:/;
const OUTPUT_DECO_OPTIONS = {
    isWholeLine: true,
    className: 'output-line-decoration',
    linesDecorationsClassName: 'output-line-gutter'
};
const REFERENCE_DECO_OPTIONS = {
    isWholeLine: true,
    className: 'reference-line-decoration'
};

// Full rescan - used by processDocument() and switchToTab()
function updateDecorations() {
    if (decorationTimeout) { clearTimeout(decorationTimeout); decorationTimeout = null; }
    if (!editor) return;

    const model = editor.getModel();
    const lineCount = model.getLineCount();

    const outputRanges = [];
    const referenceRanges = [];

    for (let i = 1; i <= lineCount; i++) {
        const lineContent = model.getLineContent(i);

        if (lineContent.includes(NBSP)) {
            outputRanges.push({
                range: new monaco.Range(i, 1, i, 1),
                options: OUTPUT_DECO_OPTIONS
            });
        }

        if (REFERENCE_RE.test(lineContent)) {
            referenceRanges.push({
                range: new monaco.Range(i, 1, i, 1),
                options: REFERENCE_DECO_OPTIONS
            });
        }
    }

    outputDecorations = editor.deltaDecorations(outputDecorations, outputRanges);
    referenceDecorations = editor.deltaDecorations(referenceDecorations, referenceRanges);
}

// Incremental update for typing - only rescan if @cassilda: lines were touched.
// NBSP (output) decorations are tracked by Monaco and shift automatically;
// we never need to rescan for them during typing since NBSP can't be typed.
function updateDecorationsIncremental(changes) {
    if (!editor) return;
    const model = editor.getModel();

    // Check if any change could affect a @cassilda: line
    let needsReferenceRescan = false;
    for (const change of changes) {
        // If the old or new text contains @cassilda:, rescan references
        if (change.text.includes('@cassilda:')) {
            needsReferenceRescan = true;
            break;
        }
        // If any affected line was/is a @cassilda: line, rescan
        for (let line = change.range.startLineNumber; line <= change.range.endLineNumber; line++) {
            if (line <= model.getLineCount() && REFERENCE_RE.test(model.getLineContent(line))) {
                needsReferenceRescan = true;
                break;
            }
        }
        if (needsReferenceRescan) break;
    }

    if (needsReferenceRescan) {
        const lineCount = model.getLineCount();
        const referenceRanges = [];
        for (let i = 1; i <= lineCount; i++) {
            if (REFERENCE_RE.test(model.getLineContent(i))) {
                referenceRanges.push({
                    range: new monaco.Range(i, 1, i, 1),
                    options: REFERENCE_DECO_OPTIONS
                });
            }
        }
        referenceDecorations = editor.deltaDecorations(referenceDecorations, referenceRanges);
    }
    // Output decorations: Monaco auto-adjusts decoration ranges when lines shift,
    // so no work needed here during typing.
}

// ============================================================================
// Folding (matching VS Code extension)
// ============================================================================

function computeFoldingRanges(model) {
    const ranges = [];
    const lineCount = model.getLineCount();

    let outputBlockStart = null;
    let inOutputBlock = false;

    // Fold output blocks
    for (let i = 1; i <= lineCount; i++) {
        const line = model.getLineContent(i);
        const isOutput = line.includes(NBSP);
        const isReference = /^\s*@cassilda:/.test(line);

        if (isReference && !inOutputBlock) {
            outputBlockStart = i;
        }

        if (isOutput) {
            if (!inOutputBlock && outputBlockStart === null) {
                outputBlockStart = i;
            }
            inOutputBlock = true;
        }

        if (inOutputBlock && !isOutput && line.trim().length > 0) {
            if (outputBlockStart !== null) {
                ranges.push({
                    start: outputBlockStart,
                    end: i - 1,
                    kind: monaco.languages.FoldingRangeKind.Region
                });
            }
            outputBlockStart = null;
            inOutputBlock = false;
        }
    }

    if (inOutputBlock && outputBlockStart !== null) {
        ranges.push({
            start: outputBlockStart,
            end: lineCount,
            kind: monaco.languages.FoldingRangeKind.Region
        });
    }

    // Fold @label...@end blocks
    let labelStart = null;
    for (let i = 1; i <= lineCount; i++) {
        const line = model.getLineContent(i);

        if (/^\s*@label\s+/.test(line)) {
            labelStart = i;
        } else if (/^\s*@end\b/.test(line) && labelStart !== null) {
            ranges.push({
                start: labelStart,
                end: i,
                kind: monaco.languages.FoldingRangeKind.Region
            });
            labelStart = null;
        }
    }

    // Fold #before_each...#end blocks
    let directiveStart = null;
    for (let i = 1; i <= lineCount; i++) {
        const line = model.getLineContent(i);

        if (/^\s*#(before_each|after_each)\b/.test(line)) {
            if (directiveStart === null) {
                directiveStart = i;
            }
        } else if (/^\s*#end\b/.test(line) && directiveStart !== null) {
            ranges.push({
                start: directiveStart,
                end: i,
                kind: monaco.languages.FoldingRangeKind.Region
            });
            directiveStart = null;
        }
    }

    return ranges;
}

// ============================================================================
// Hyades WASM Integration
// ============================================================================

async function initializeHyades() {
    const statusDot = document.getElementById('statusDot');
    const statusText = document.getElementById('statusText');

    statusDot.className = 'status-dot loading';
    statusText.textContent = 'Loading WASM...';

    try {
        // Try Web Worker first (keeps WASM off the main thread)
        try {
            const proxy = new HyadesWorkerProxy('./hyades-worker.js');
            await proxy.whenReady();
            hyades = proxy;
        } catch (workerErr) {
            console.warn('Worker failed, falling back to main thread:', workerErr);
            const module = await import('./hyades-api.js');
            hyades = await module.Hyades.create({ wasmPath: './' });
        }

        statusDot.className = 'status-dot ready';
        statusText.textContent = 'Ready';

        document.getElementById('versionInfo').textContent = `v${hyades.version}`;

        // Register LSP providers after WASM is loaded
        registerLspProviders();
    } catch (err) {
        console.error('Failed to load Hyades WASM:', err);
        statusDot.className = 'status-dot error';
        statusText.textContent = 'WASM Error';
    }
}

// ============================================================================
// LSP Integration (Hover, Go-to-Definition)
// ============================================================================

let lspParseGeneration = 0;

function triggerLspParse() {
    if (!hyades) return;
    if (lspParseTimeout) clearTimeout(lspParseTimeout);
    lspParseTimeout = setTimeout(async () => {
        const source = editor.getValue();
        const gen = ++lspParseGeneration;
        try {
            let tokens;
            if (hyades.lspParseAndGetTokens) {
                // Worker path: single round-trip for parse + tokens
                tokens = await hyades.lspParseAndGetTokens(source);
            } else {
                // Fallback: direct synchronous calls
                hyades.lspParse(source);
                tokens = hyades.lspGetSemanticTokensRaw();
            }
            if (gen !== lspParseGeneration) return; // stale
            lspParsed = true;
            cachedSemanticTokens = tokens;
            // Tell Monaco to pick up the new tokens on the next frame
            requestAnimationFrame(() => {
                if (semanticTokensChangeEmitter) {
                    semanticTokensChangeEmitter.fire();
                }
            });
        } catch (err) {
            console.error('LSP parse error:', err);
        }
    }, 300);
}

function registerLspProviders() {
    // Hover provider - shows macro/lambda documentation
    monaco.languages.registerHoverProvider('cassilda', {
        provideHover: async function(model, position) {
            if (!hyades || !lspParsed) return null;
            const hover = await hyades.lspGetHover(position.lineNumber - 1, position.column - 1);
            if (!hover) return null;
            return {
                contents: [{ value: hover.contents.value }],
                range: new monaco.Range(
                    hover.range.start.line + 1, hover.range.start.character + 1,
                    hover.range.end.line + 1, hover.range.end.character + 1
                )
            };
        }
    });

    // Definition provider - F12 / Ctrl+Click to go to definition
    monaco.languages.registerDefinitionProvider('cassilda', {
        provideDefinition: async function(model, position) {
            if (!hyades || !lspParsed) return null;
            const def = await hyades.lspGetDefinition(position.lineNumber - 1, position.column - 1);
            if (!def) return null;
            return {
                uri: model.uri,
                range: new monaco.Range(
                    def.range.start.line + 1, def.range.start.character + 1,
                    def.range.end.line + 1, def.range.end.character + 1
                )
            };
        }
    });

    // Semantic tokens provider - rich highlighting from WASM LSP
    // Token types must match SemanticTokenType enum order in hyades_parse_api.h
    const tokenTypes = [
        'namespace', 'type', 'class', 'enum', 'interface', 'struct',
        'typeParameter', 'parameter', 'variable', 'property', 'enumMember',
        'event', 'function', 'method', 'macro', 'keyword', 'modifier',
        'comment', 'string', 'number', 'regexp', 'operator'
    ];

    const tokenModifiers = [
        'declaration', 'definition', 'readonly', 'static', 'deprecated',
        'abstract', 'async', 'modification', 'documentation', 'defaultLibrary'
    ];

    const legend = { tokenTypes, tokenModifiers };

    // Event emitter to signal Monaco to re-request semantic tokens after parse
    const emitter = new monaco.Emitter();
    semanticTokensChangeEmitter = emitter;

    monaco.languages.registerDocumentSemanticTokensProvider('cassilda', {
        onDidChange: emitter.event,
        getLegend: () => legend,
        provideDocumentSemanticTokens: (model) => {
            if (!cachedSemanticTokens) return null;
            return { data: cachedSemanticTokens };
        },
        releaseDocumentSemanticTokens: () => {}
    });
}

// ============================================================================
// Document Processing
// ============================================================================

async function processDocument() {
    if (!hyades) {
        alert('Hyades WASM not loaded yet. Please wait...');
        return;
    }
    if (processingInFlight) return;
    processingInFlight = true;

    const statusDot = document.getElementById('statusDot');
    const statusText = document.getElementById('statusText');

    statusDot.className = 'status-dot loading';
    statusText.textContent = 'Processing...';

    try {
        const input = editor.getValue();
        const result = await hyades.cassilda(input);

        // Update editor content (programmatically - don't promote preview tabs)
        const position = editor.getPosition();
        isProgrammaticChange = true;
        editor.setValue(result);
        isProgrammaticChange = false;

        // Try to restore cursor position
        if (position) {
            const model = editor.getModel();
            const lineCount = model.getLineCount();
            const newLine = Math.min(position.lineNumber, lineCount);
            const newCol = Math.min(position.column, model.getLineMaxColumn(newLine));
            editor.setPosition({ lineNumber: newLine, column: newCol });
        }

        updateFileStatus();
        updateDecorations();

        statusDot.className = 'status-dot ready';
        statusText.textContent = 'Ready';
    } catch (err) {
        console.error('Processing error:', err);
        statusDot.className = 'status-dot error';
        statusText.textContent = 'Error';
        alert('Error processing document: ' + err.message);
    } finally {
        processingInFlight = false;
    }
}

// ============================================================================
// Tab System
// ============================================================================

function generateTabId() {
    return `tab-${Date.now()}-${tabIdCounter++}`;
}

function createTab(name = null, content = null, isPreview = false) {
    const id = generateTabId();
    let noContent = !content;

    const tab = {
        id: id,
        name: name || `untitled-${tabs.length + 1}.cld`,
        content: content || getDefaultContent(),
        modified: false,
        isPreview: isPreview
    };

    // If creating a preview tab, close any existing preview tab first
    if (isPreview && previewTabId) {
        const existingPreviewIndex = tabs.findIndex(t => t.id === previewTabId);
        if (existingPreviewIndex !== -1) {
            tabs.splice(existingPreviewIndex, 1);
        }
    }

    tabs.push(tab);

    if (isPreview) {
        previewTabId = id;
    }

    renderTabs();
    switchToTab(id);

    // Don't save preview tabs to storage
    if (!isPreview) {
        saveTabsToStorage();
    }

    return tab;
}

// Promote a preview tab to a regular tab
function promotePreviewTab(tabId = null) {
    const id = tabId || activeTabId;
    const tab = tabs.find(t => t.id === id);

    if (tab && tab.isPreview) {
        tab.isPreview = false;
        if (previewTabId === id) {
            previewTabId = null;
        }
        renderTabs();
        saveTabsToStorage();
    }
}

function closeTab(tabId) {
    const tabIndex = tabs.findIndex(t => t.id === tabId);
    if (tabIndex === -1) return;

    const tab = tabs[tabIndex];

    // Don't warn for preview tabs, warn if modified
    if (!tab.isPreview && tab.modified && !confirm(`Close "${tab.name}" without saving?`)) {
        return;
    }

    // Clear preview tab reference if closing it
    if (tab.isPreview && previewTabId === tabId) {
        previewTabId = null;
    }

    // Remove tab
    tabs.splice(tabIndex, 1);

    // If we closed the active tab, switch to another
    if (activeTabId === tabId) {
        if (tabs.length > 0) {
            // Switch to the tab at the same position, or the last one
            const newIndex = Math.min(tabIndex, tabs.length - 1);
            switchToTab(tabs[newIndex].id);
        } else {
            // No tabs left, create a new one
            createTab();
            return; // createTab will handle rendering and saving
        }
    }

    renderTabs();
    saveTabsToStorage();
}

function switchToTab(tabId) {
    const tab = tabs.find(t => t.id === tabId);
    if (!tab) return;

    // Save current tab content before switching
    if (activeTabId) {
        const currentTab = tabs.find(t => t.id === activeTabId);
        if (currentTab) {
            currentTab.content = editor.getValue();
        }
    }

    // Switch to new tab (programmatically - don't mark as modified)
    activeTabId = tabId;
    isProgrammaticChange = true;
    editor.setValue(tab.content);
    isProgrammaticChange = false;
    updateFileStatus();
    updateDecorations();
    triggerLspParse();
    renderTabs();
    saveTabsToStorage();
}

function renameTab(tabId, newName) {
    const tab = tabs.find(t => t.id === tabId);
    if (tab) {
        tab.name = newName;
        renderTabs();
        updateFileStatus();
        saveTabsToStorage();
    }
}

function markTabModified(modified = true) {
    const tab = tabs.find(t => t.id === activeTabId);
    if (tab) {
        // If editing a preview tab, promote it to regular
        if (tab.isPreview && modified) {
            promotePreviewTab(tab.id);
        }

        if (tab.modified !== modified) {
            tab.modified = modified;
            renderTabs();
            saveTabsToStorage();
        }
    }
}

function getActiveTab() {
    return tabs.find(t => t.id === activeTabId);
}

function switchToNextTab(direction) {
    if (tabs.length <= 1) return;

    const currentIndex = tabs.findIndex(t => t.id === activeTabId);
    let newIndex = currentIndex + direction;

    // Wrap around
    if (newIndex < 0) newIndex = tabs.length - 1;
    if (newIndex >= tabs.length) newIndex = 0;

    switchToTab(tabs[newIndex].id);
}

function renderTabs() {
    const tabList = document.getElementById('tabList');
    tabList.innerHTML = '';

    tabs.forEach(tab => {
        const tabEl = document.createElement('div');
        tabEl.className = `tab${tab.id === activeTabId ? ' active' : ''}${tab.modified ? ' modified' : ''}${tab.isPreview ? ' preview' : ''}`;
        tabEl.dataset.tabId = tab.id;

        const nameEl = document.createElement('span');
        nameEl.className = 'tab-name';
        nameEl.textContent = tab.name;

        // Double-click on preview tab name promotes it
        nameEl.addEventListener('dblclick', (e) => {
            e.stopPropagation();
            if (tab.isPreview) {
                promotePreviewTab(tab.id);
            } else {
                promptRenameTab(tab.id);
            }
        });

        const closeEl = document.createElement('button');
        closeEl.className = 'tab-close';
        closeEl.innerHTML = '×';
        closeEl.title = tab.isPreview ? 'Close preview' : 'Close tab';
        closeEl.addEventListener('click', (e) => {
            e.stopPropagation();
            closeTab(tab.id);
        });

        tabEl.appendChild(nameEl);
        tabEl.appendChild(closeEl);

        // Single click switches to tab
        tabEl.addEventListener('click', () => {
            switchToTab(tab.id);
        });

        tabList.appendChild(tabEl);
    });
}

function promptRenameTab(tabId) {
    const tab = tabs.find(t => t.id === tabId);
    if (!tab) return;

    const newName = prompt('Rename tab:', tab.name);
    if (newName && newName.trim()) {
        renameTab(tabId, newName.trim());
    }
}

// ============================================================================
// Storage
// ============================================================================

function saveTabsToStorage() {
    // Save current editor content to active tab
    const activeTab = getActiveTab();
    if (activeTab && !activeTab.isPreview) {
        activeTab.content = editor.getValue();
    }

    // Filter out preview tabs for storage
    const tabsToSave = tabs.filter(t => !t.isPreview);

    // Determine which tab to restore - prefer non-preview active tab
    let savedActiveTabId = activeTabId;
    if (activeTab && activeTab.isPreview) {
        // If active tab is preview, save the most recent non-preview tab
        const nonPreviewTabs = tabs.filter(t => !t.isPreview);
        if (nonPreviewTabs.length > 0) {
            savedActiveTabId = nonPreviewTabs[nonPreviewTabs.length - 1].id;
        } else {
            savedActiveTabId = null;
        }
    }

    const data = {
        tabs: tabsToSave,
        activeTabId: savedActiveTabId,
        tabIdCounter: tabIdCounter
    };

    try {
        localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
    } catch (e) {
        console.warn('Failed to save tabs to localStorage:', e);
    }
}

function loadTabsFromStorage() {
    try {
        const stored = localStorage.getItem(STORAGE_KEY);
        if (stored) {
            const data = JSON.parse(stored);
            tabs = (data.tabs || []).map(t => ({ ...t, isPreview: false })); // Ensure no preview tabs loaded
            tabIdCounter = data.tabIdCounter || 0;
            previewTabId = null;

            if (tabs.length > 0) {
                renderTabs();
                // Switch to the previously active tab, or the first one
                const targetTabId = data.activeTabId && tabs.find(t => t.id === data.activeTabId)
                    ? data.activeTabId
                    : tabs[0].id;
                switchToTab(targetTabId);
                return;
            }
        }
    } catch (e) {
        console.warn('Failed to load tabs from localStorage:', e);
    }

    // No stored tabs, create default
    createTab('welcome.cld', getDefaultContent());
    setTimeout(() => {
        processDocument();
    }, 0);
}

// ============================================================================
// File Operations (updated for tabs)
// ============================================================================

function newFile() {
    createTab();
}

function openFile() {
    document.getElementById('fileInput').click();
}

function handleFileOpen(event) {
    const file = event.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
        createTab(file.name, e.target.result);
    };
    reader.readAsText(file);

    // Reset input so same file can be selected again
    event.target.value = '';
}

function saveFile() {
    const tab = getActiveTab();
    if (!tab) return;

    const content = editor.getValue();
    const blob = new Blob([content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);

    const a = document.createElement('a');
    a.href = url;
    a.download = tab.name;
    a.click();

    URL.revokeObjectURL(url);

    tab.modified = false;
    tab.content = content;
    renderTabs();
    saveTabsToStorage();
}

function updateFileStatus() {
    const tab = getActiveTab();
    if (tab) {
        document.getElementById('fileName').textContent = tab.name;
        document.getElementById('fileStatus').className = tab.modified ? 'file-status modified' : 'file-status';
    }
}

// ============================================================================
// Event Listeners
// ============================================================================

function setupEventListeners() {
    // Toolbar buttons
    document.getElementById('btnNew').addEventListener('click', newFile);
    document.getElementById('btnOpen').addEventListener('click', openFile);
    document.getElementById('btnSave').addEventListener('click', saveFile);
    document.getElementById('btnRun').addEventListener('click', processDocument);

    // Tab bar add button
    document.getElementById('btnAddTab').addEventListener('click', () => {
        createTab();
    });

    document.getElementById('btnFold').addEventListener('click', () => {
        editor.getAction('editor.foldAll').run();
    });

    document.getElementById('btnUnfold').addEventListener('click', () => {
        editor.getAction('editor.unfoldAll').run();
    });

    // File input
    document.getElementById('fileInput').addEventListener('change', handleFileOpen);

    // Side panel
    document.getElementById('btnExamples').addEventListener('click', toggleSidePanel);
    document.getElementById('toggleSidePanel').addEventListener('click', toggleSidePanel);
    document.getElementById('closeSidePanel').addEventListener('click', closeSidePanel);

    // Side panel search
    document.getElementById('exampleSearch').addEventListener('input', (e) => {
        renderExamplesLibrary(e.target.value);
    });

    // Help modal
    document.getElementById('btnHelp').addEventListener('click', showHelp);
    document.getElementById('closeHelp').addEventListener('click', hideHelp);
    document.querySelector('#helpModal .modal-backdrop').addEventListener('click', hideHelp);

    // Keyboard shortcuts (for when editor doesn't have focus)
    document.addEventListener('keydown', (e) => {
        // Ctrl/Cmd+Enter - Process (catch before context menu)
        if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
            e.preventDefault();
            e.stopPropagation();
            processDocument();
            return false;
        }
        // Ctrl/Cmd+S - Save (prevent browser save dialog)
        else if ((e.ctrlKey || e.metaKey) && e.key === 's') {
            e.preventDefault();
            e.stopPropagation();
            saveFile();
            return false;
        }
        // Ctrl/Cmd+O - Open (prevent browser default)
        else if ((e.ctrlKey || e.metaKey) && e.key === 'o') {
            e.preventDefault();
            openFile();
        }
        // Ctrl/Cmd+N - New tab (prevent browser default)
        else if ((e.ctrlKey || e.metaKey) && e.key === 'n') {
            e.preventDefault();
            newFile();
        }
        // Ctrl/Cmd+W - Close current tab
        else if ((e.ctrlKey || e.metaKey) && e.key === 'w') {
            e.preventDefault();
            if (activeTabId) {
                closeTab(activeTabId);
            }
        }
        // Ctrl+Tab - Next tab
        else if (e.ctrlKey && e.key === 'Tab' && !e.shiftKey) {
            e.preventDefault();
            switchToNextTab(1);
        }
        // Ctrl+Shift+Tab - Previous tab
        else if (e.ctrlKey && e.key === 'Tab' && e.shiftKey) {
            e.preventDefault();
            switchToNextTab(-1);
        }
        // Escape - Close modals
        else if (e.key === 'Escape') {
            hideExamples();
            hideHelp();
        }
    }, true);  // Use capture phase to get event before Monaco

    // Warn before leaving with unsaved changes
    window.addEventListener('beforeunload', (e) => {
        const hasUnsaved = tabs.some(t => t.modified);
        if (hasUnsaved) {
            e.preventDefault();
            e.returnValue = '';
        }
    });
}

// ============================================================================
// Examples Side Panel
// ============================================================================

let sidePanelOpen = false;

function toggleSidePanel() {
    sidePanelOpen = !sidePanelOpen;
    const panel = document.getElementById('sidePanel');
    const toggle = document.getElementById('toggleSidePanel');

    if (sidePanelOpen) {
        panel.classList.add('open');
        toggle.classList.add('hidden');
        renderExamplesLibrary();
    } else {
        panel.classList.remove('open');
        toggle.classList.remove('hidden');
    }
}

function closeSidePanel() {
    sidePanelOpen = false;
    document.getElementById('sidePanel').classList.remove('open');
    document.getElementById('toggleSidePanel').classList.remove('hidden');
}

function renderExamplesLibrary(filter = '') {
    const container = document.getElementById('examplesContent');
    const filterLower = filter.toLowerCase();

    let html = '';
    let hasResults = false;

    for (const [category, examples] of Object.entries(EXAMPLES_LIBRARY)) {
        // Filter examples
        const filteredExamples = examples.filter(ex =>
            filter === '' ||
            ex.name.toLowerCase().includes(filterLower) ||
            ex.description.toLowerCase().includes(filterLower) ||
            category.toLowerCase().includes(filterLower)
        );

        if (filteredExamples.length === 0) continue;
        hasResults = true;

        html += `
            <div class="example-category" data-category="${category}">
                <button class="example-category-header">
                    <span class="chevron">▼</span>
                    <span>${category}</span>
                    <span class="count">${filteredExamples.length}</span>
                </button>
                <div class="example-category-items">
                    ${filteredExamples.map(ex => `
                        <button class="example-item" data-category="${category}" data-name="${ex.name}">
                            <div class="example-item-name">${ex.name}</div>
                            <div class="example-item-desc">${ex.description}</div>
                        </button>
                    `).join('')}
                </div>
            </div>
        `;
    }

    if (!hasResults) {
        html = '<div class="no-results">No examples match your search</div>';
    }

    container.innerHTML = html;

    // Add event listeners
    container.querySelectorAll('.example-category-header').forEach(header => {
        header.addEventListener('click', () => {
            header.parentElement.classList.toggle('collapsed');
        });
    });

    container.querySelectorAll('.example-item').forEach(item => {
        let clickTimer = null;

        // Single click - preview (opens in preview tab)
        // Use timer to distinguish from double-click
        item.addEventListener('click', (e) => {
            // If there's already a pending click, this is a double-click
            if (clickTimer) {
                clearTimeout(clickTimer);
                clickTimer = null;
                return; // Let dblclick handler take over
            }

            // Set a timer - if no second click within 250ms, treat as single click
            clickTimer = setTimeout(() => {
                clickTimer = null;
                const category = item.dataset.category;
                const name = item.dataset.name;
                loadExampleAsPreview(category, name);

                // Highlight selected item
                container.querySelectorAll('.example-item').forEach(i => i.classList.remove('selected'));
                item.classList.add('selected');
            }, 250);
        });

        // Double click - open as permanent tab
        item.addEventListener('dblclick', (e) => {
            // Clear the single-click timer
            if (clickTimer) {
                clearTimeout(clickTimer);
                clickTimer = null;
            }

            const category = item.dataset.category;
            const name = item.dataset.name;
            loadExampleAsPermanent(category, name);

            // Highlight selected item
            container.querySelectorAll('.example-item').forEach(i => i.classList.remove('selected'));
            item.classList.add('selected');
        });
    });
}

function loadExampleAsPreview(category, name) {
    const examples = EXAMPLES_LIBRARY[category];
    if (!examples) return;

    const example = examples.find(ex => ex.name === name);
    if (!example) return;

    const fileName = name.toLowerCase().replace(/\s+/g, '_') + '.cld';

    // Create as preview tab (will replace existing preview)
    createTab(fileName, example.content, true);

    // Process immediately so user sees the rendered output
    setTimeout(() => {
        processDocument();
    }, 100);
}

function loadExampleAsPermanent(category, name) {
    const examples = EXAMPLES_LIBRARY[category];
    if (!examples) return;

    const example = examples.find(ex => ex.name === name);
    if (!example) return;

    const fileName = name.toLowerCase().replace(/\s+/g, '_') + '.cld';

    // If current tab is a preview of the same content, just promote it
    const activeTab = getActiveTab();
    if (activeTab && activeTab.isPreview && activeTab.name === fileName) {
        promotePreviewTab();
    } else {
        // Create as permanent tab
        createTab(fileName, example.content, false);
    }

    // Process immediately
    setTimeout(() => {
        processDocument();
    }, 100);
}

// Legacy modal functions (kept for backward compatibility)
function showExamples() {
    toggleSidePanel();
}

function hideExamples() {
    closeSidePanel();
}

// ============================================================================
// Help
// ============================================================================

function showHelp() {
    document.getElementById('helpModal').classList.add('open');
}

function hideHelp() {
    document.getElementById('helpModal').classList.remove('open');
}

// ============================================================================
// Default Content
// ============================================================================

function getDefaultContent() {
    return `@label welcome
    Hello! Type some TeX and press [Ctrl]/[Cmd]+[Enter]. 🙂 This is \\hyades{}*,
    a typesetting engine that renders TeX-like source to Unicode (or ASCII,
    your choice). Here's the Taylor series formula to start you up:

    $$ f(x) = \\overbrace{\\sum_{k=0}^{n} \\frac{f^{(k)}(a)}{k!}(x-a)^k}^{\\mathit{Taylor polynomial}} + \\underbrace{R_n(x)}_{\\mathit{Remainder}} $$

    Lagrange remainder:

    $$ R_n(x) = \\frac{f^{(n+1)}(\\xi)}{(n+1)!}(x-a)^{n+1} $$
    
    \\center{Or ask AI to do it for you with the \\hyades{} MCP (Connector):}
    \\center{\\tightframe{https://hyades-mcp.apg.workers.dev/mcp}}
    \\vskip{1}\\hrule

    *) Check out the Library (the blue button on the right) to explore
    all its capabilities! Or delete this and start typesetting your own stuff right away.
@end

@cassilda: welcome

#output_prefix " "
`;
}
