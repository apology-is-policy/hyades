/**
 * Hyades Language Server
 *
 * A Language Server Protocol implementation for Hyades documents (.cld files).
 * Provides syntax error diagnostics, symbol navigation, and autocompletion.
 *
 * Usage:
 *   node dist/server.js --stdio
 */

import {
    createConnection,
    TextDocuments,
    Diagnostic,
    DiagnosticSeverity,
    ProposedFeatures,
    InitializeParams,
    InitializeResult,
    TextDocumentSyncKind,
    CompletionItem,
    CompletionItemKind,
    TextDocumentPositionParams,
    Definition,
    Location,
    Hover,
    MarkupKind,
    DocumentSymbol,
    SymbolKind,
    SemanticTokensLegend,
    SemanticTokens,
    SemanticTokensParams,
} from 'vscode-languageserver/node';

import { TextDocument } from 'vscode-languageserver-textdocument';
import { HyadesWasm, HyadesLSPDiagnostic, HyadesSymbol, HyadesCompletion } from './hyades-wasm';

// Handle unhandled rejections to prevent connection drops
process.on('unhandledRejection', (reason, promise) => {
    // Log to stderr only (not stdout, which would corrupt LSP protocol)
    // Using connection.console would be better but it may not be available yet
});

// Create a connection for the server
const connection = createConnection(ProposedFeatures.all);

// Create a document manager
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

// WASM module instance
let hyades: HyadesWasm | null = null;

// Document URI to parsed state mapping
const documentStates = new Map<string, boolean>();

// Semantic token types (must match C enum SemanticTokenType)
const tokenTypes = [
    'namespace',      // 0 - STD:: prefix
    'type',           // 1
    'class',          // 2
    'enum',           // 3
    'interface',      // 4
    'struct',         // 5
    'typeParameter',  // 6
    'parameter',      // 7 - Macro parameters ${name}
    'variable',       // 8 - References, counters
    'property',       // 9
    'enumMember',     // 10
    'event',          // 11
    'function',       // 12 - Built-in commands
    'method',         // 13
    'macro',          // 14 - User-defined macros
    'keyword',        // 15 - @label, @end, \begin, \end
    'modifier',       // 16
    'comment',        // 17 - % comments
    'string',         // 18 - Verbatim content
    'number',         // 19 - Numeric literals
    'regexp',         // 20
    'operator',       // 21 - Math operators
];

const tokenModifiers = [
    'declaration',
    'definition',
    'readonly',
    'static',
    'deprecated',
    'abstract',
    'async',
    'modification',
    'documentation',
    'defaultLibrary',
];

const legend: SemanticTokensLegend = {
    tokenTypes,
    tokenModifiers,
};

connection.onInitialize(async (params: InitializeParams): Promise<InitializeResult> => {
    connection.console.log('Hyades Language Server initializing...');

    // Initialize WASM module
    try {
        hyades = await HyadesWasm.initialize();
        // Log WASM init output through LSP (not stdout!)
        for (const line of hyades.getInitOutput()) {
            connection.console.log(`WASM: ${line}`);
        }
        connection.console.log('WASM module loaded successfully');
    } catch (error) {
        connection.console.error(`Failed to load WASM module: ${error}`);
    }

    return {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Full,
            completionProvider: {
                resolveProvider: true,
                triggerCharacters: ['\\', '{', '[', '<'],
            },
            definitionProvider: true,
            hoverProvider: true,
            referencesProvider: true,
            documentSymbolProvider: true,
            semanticTokensProvider: {
                legend,
                full: true,
            },
        },
    };
});

connection.onInitialized(() => {
    connection.console.log('Hyades Language Server initialized');

    // Validate any documents that were opened before WASM finished loading
    const allDocs = documents.all();
    connection.console.log(`Documents count at init: ${allDocs.length}`);
    connection.console.log(`WASM loaded: ${hyades !== null}`);

    if (hyades) {
        for (const doc of allDocs) {
            connection.console.log(`Re-validating: ${doc.uri}`);
            validateDocument(doc);
        }
    }
});

// Validate a document and send diagnostics
async function validateDocument(document: TextDocument): Promise<void> {
    connection.console.log(`validateDocument called for: ${document.uri}`);
    if (!hyades) {
        connection.console.warn('WASM module not loaded, skipping validation');
        return;
    }

    const text = document.getText();
    const uri = document.uri;

    try {
        // Parse the document
        connection.console.log(`Parsing document (${text.length} chars)...`);
        const parseResult = hyades.parse(text);

        if (!parseResult.success) {
            connection.console.error(`Parse failed: ${parseResult.error}`);
            return;
        }

        // Get diagnostics
        const lspDiagnostics = hyades.getDiagnostics();
        connection.console.log(`Got ${lspDiagnostics.length} diagnostics`);
        const diagnostics: Diagnostic[] = lspDiagnostics.map(diag => ({
            severity: mapSeverity(diag.severity),
            range: diag.range,
            message: diag.message,
            source: diag.source || 'hyades',
            code: diag.code,
        }));

        // Send diagnostics
        connection.console.log(`Sending ${diagnostics.length} diagnostics to ${uri}`);
        connection.sendDiagnostics({ uri, diagnostics });

        // Mark document as parsed
        documentStates.set(uri, true);

    } catch (error) {
        connection.console.error(`Validation error: ${error}`);
        // WASM memory might be corrupted - try to reinitialize
        if (String(error).includes('memory access out of bounds')) {
            connection.console.warn('WASM memory corrupted, attempting to reinitialize...');
            try {
                hyades = await HyadesWasm.initialize();
                connection.console.log('WASM reinitialized successfully');
            } catch (reinitError) {
                connection.console.error(`Failed to reinitialize WASM: ${reinitError}`);
                hyades = null;
            }
        }
    }
}

function mapSeverity(severity: number): DiagnosticSeverity {
    switch (severity) {
        case 1: return DiagnosticSeverity.Error;
        case 2: return DiagnosticSeverity.Warning;
        case 3: return DiagnosticSeverity.Information;
        case 4: return DiagnosticSeverity.Hint;
        default: return DiagnosticSeverity.Error;
    }
}

// Document open handler
documents.onDidOpen(event => {
    connection.console.log(`Document opened: ${event.document.uri} (lang: ${event.document.languageId})`);
    validateDocument(event.document);
});

// Document change handler
documents.onDidChangeContent(change => {
    connection.console.log(`Document changed: ${change.document.uri}`);
    validateDocument(change.document);
});

// Completion handler
connection.onCompletion((params: TextDocumentPositionParams): CompletionItem[] => {
    if (!hyades) return [];

    const document = documents.get(params.textDocument.uri);
    if (!document) return [];

    // Ensure document is parsed
    if (!documentStates.get(params.textDocument.uri)) {
        hyades.parse(document.getText());
    }

    const completions = hyades.getCompletions(params.position.line, params.position.character);

    return completions.map((c, index) => ({
        label: c.label,
        kind: mapCompletionKind(c.kind),
        detail: c.detail,
        data: index,
    }));
});

connection.onCompletionResolve((item: CompletionItem): CompletionItem => {
    // Could add documentation here
    return item;
});

function mapCompletionKind(kind: number): CompletionItemKind {
    switch (kind) {
        case 12: return CompletionItemKind.Function;
        case 13: return CompletionItemKind.Variable;
        case 14: return CompletionItemKind.Constant;
        case 18: return CompletionItemKind.TypeParameter;
        case 5: return CompletionItemKind.Class;
        default: return CompletionItemKind.Text;
    }
}

// Go to definition handler
connection.onDefinition((params: TextDocumentPositionParams): Definition | null => {
    if (!hyades) return null;

    const document = documents.get(params.textDocument.uri);
    if (!document) return null;

    // Ensure document is parsed
    if (!documentStates.get(params.textDocument.uri)) {
        hyades.parse(document.getText());
    }

    const definition = hyades.getDefinition(params.position.line, params.position.character);

    if (!definition) return null;

    return Location.create(params.textDocument.uri, definition.range);
});

// Hover handler
connection.onHover((params: TextDocumentPositionParams): Hover | null => {
    if (!hyades) return null;

    const document = documents.get(params.textDocument.uri);
    if (!document) return null;

    // Ensure document is parsed
    if (!documentStates.get(params.textDocument.uri)) {
        hyades.parse(document.getText());
    }

    const hoverResult = hyades.getHover(params.position.line, params.position.character);
    if (!hoverResult) return null;

    return {
        contents: {
            kind: MarkupKind.Markdown,
            value: hoverResult.contents.value,
        },
        range: hoverResult.range,
    };
});

// References handler
connection.onReferences((params): Location[] => {
    if (!hyades) return [];

    const document = documents.get(params.textDocument.uri);
    if (!document) return [];

    // Ensure document is parsed
    if (!documentStates.get(params.textDocument.uri)) {
        hyades.parse(document.getText());
    }

    const references = hyades.getReferences(params.position.line, params.position.character);

    return references.map(ref => Location.create(params.textDocument.uri, ref.range));
});

// Document symbols handler
connection.onDocumentSymbol((params): DocumentSymbol[] => {
    if (!hyades) return [];

    const document = documents.get(params.textDocument.uri);
    if (!document) return [];

    // Ensure document is parsed
    if (!documentStates.get(params.textDocument.uri)) {
        hyades.parse(document.getText());
    }

    const symbols = hyades.getSymbols();

    return symbols.map(sym => ({
        name: sym.name,
        kind: mapSymbolKind(sym.kind),
        range: sym.location.range,
        selectionRange: sym.location.range,
    }));
});

function mapSymbolKind(kind: number): SymbolKind {
    switch (kind) {
        case 12: return SymbolKind.Function;
        case 13: return SymbolKind.Variable;
        case 14: return SymbolKind.Constant;
        case 18: return SymbolKind.Array;
        case 5: return SymbolKind.Class;
        default: return SymbolKind.Variable;
    }
}

// Semantic tokens handler - using the proper languages.semanticTokens API
connection.languages.semanticTokens.on((params: SemanticTokensParams): SemanticTokens => {
    try {
        connection.console.log(`Semantic tokens requested for: ${params.textDocument.uri}`);

        if (!hyades) {
            connection.console.log('No hyades instance');
            return { data: [] };
        }

        const document = documents.get(params.textDocument.uri);
        if (!document) {
            connection.console.log('Document not found');
            return { data: [] };
        }

        // Ensure document is parsed
        if (!documentStates.get(params.textDocument.uri)) {
            hyades.parse(document.getText());
            documentStates.set(params.textDocument.uri, true);
        }

        const tokens = hyades.getSemanticTokens();

        // Validate token data format
        if (!tokens || !Array.isArray(tokens.data)) {
            connection.console.log('Invalid tokens format');
            return { data: [] };
        }

        // Ensure all values are valid unsigned integers
        const validData = tokens.data.map((v: number) => Math.max(0, Math.floor(v)));

        connection.console.log(`Returning ${validData.length / 5} tokens`);
        return { data: validData };
    } catch (error) {
        connection.console.error(`Semantic tokens error: ${error}`);
        return { data: [] };
    }
});

// Document close handler
documents.onDidClose(e => {
    documentStates.delete(e.document.uri);
    connection.sendDiagnostics({ uri: e.document.uri, diagnostics: [] });
});

// Shutdown handler - respond to editor's shutdown request
connection.onShutdown(() => {
    connection.console.log('Shutdown requested, cleaning up...');
    documentStates.clear();
    hyades = null;
});

// Exit handler - terminate the process
connection.onExit(() => {
    process.exit(0);
});

// Start listening
documents.listen(connection);
connection.listen();
