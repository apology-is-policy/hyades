/**
 * TypeScript bindings for the Hyades WASM module
 *
 * This module provides a type-safe interface to the Hyades parser compiled to WebAssembly.
 */

import * as path from 'path';
import * as fs from 'fs';

// Types for LSP responses

export interface Range {
    start: { line: number; character: number };
    end: { line: number; character: number };
}

export interface HyadesLSPDiagnostic {
    severity: number;
    range: Range;
    message: string;
    source: string;
    code: string;
}

export interface HyadesSymbol {
    name: string;
    kind: number;
    location: {
        range: Range;
    };
}

export interface HyadesCompletion {
    label: string;
    kind: number;
    detail: string;
}

export interface HyadesDefinition {
    uri: string;
    range: Range;
}

export interface HyadesHover {
    contents: {
        kind: string;
        value: string;
    };
    range: Range;
}

export interface HyadesReference {
    uri: string;
    range: Range;
}

export interface SemanticTokensData {
    data: number[];
}

export interface ParseResult {
    success: boolean;
    error?: string;
    error_count: number;
    warning_count: number;
    symbol_count: number;
    lines: number;
}

// WASM module interface
interface HyadesWasmModule {
    _hyades_wasm_init(): void;
    _hyades_lsp_parse(sourcePtr: number): number;
    _hyades_lsp_get_diagnostics(): number;
    _hyades_lsp_get_symbols(): number;
    _hyades_lsp_get_completions(line: number, col: number): number;
    _hyades_lsp_get_definition(line: number, col: number): number;
    _hyades_lsp_get_hover(line: number, col: number): number;
    _hyades_lsp_get_references(line: number, col: number): number;
    _hyades_lsp_get_semantic_tokens(): number;
    _hyades_lsp_free(ptr: number): void;
    _hyades_lsp_clear(): void;
    _hyades_lsp_has_result(): number;
    _malloc(size: number): number;
    _free(ptr: number): void;
    UTF8ToString(ptr: number): string;
    stringToUTF8(str: string, outPtr: number, maxBytes: number): void;
    lengthBytesUTF8(str: string): number;
}

/**
 * Wrapper class for the Hyades WASM module
 */
export class HyadesWasm {
    private module: HyadesWasmModule;
    private initOutput: string[];

    private constructor(module: HyadesWasmModule, initOutput: string[] = []) {
        this.module = module;
        this.initOutput = initOutput;
    }

    /**
     * Get output from WASM initialization (for logging via LSP)
     */
    getInitOutput(): string[] {
        return this.initOutput;
    }

    /**
     * Initialize the WASM module
     */
    static async initialize(): Promise<HyadesWasm> {
        // Try to find the WASM JS loader (compat version for CommonJS)
        const jsPaths = [
            path.join(__dirname, 'hyades-compat.js'),           // Same dir (bundled)
            path.join(__dirname, '..', 'hyades-compat.js'),     // Parent dir (unbundled dist/)
            path.join(process.cwd(), 'hyades-compat.js'),       // Current working dir
        ];

        let jsPath: string | null = null;
        for (const p of jsPaths) {
            if (fs.existsSync(p)) {
                jsPath = p;
                break;
            }
        }

        if (!jsPath) {
            // Fall back to mock implementation when WASM isn't available
            // Note: Use stderr to avoid corrupting LSP protocol on stdout
            process.stderr.write('WASM module not found, using mock implementation\n');
            const mockModule = createMockModule();
            return new HyadesWasm(mockModule as HyadesWasmModule);
        }

        try {
            // Load the Emscripten-generated CommonJS module
            // eslint-disable-next-line @typescript-eslint/no-var-requires
            const createModule = require(jsPath);

            // Collect WASM output to return later (don't print to stdout!)
            const wasmOutput: string[] = [];

            // Provide locateFile so it can find the .wasm file
            // Also redirect print/printErr to avoid corrupting stdio LSP stream
            const wasmDir = path.dirname(jsPath);
            const module = await createModule({
                locateFile: (filename: string) => path.join(wasmDir, filename),
                // Redirect stdout/stderr to avoid corrupting LSP stdio
                print: (text: string) => wasmOutput.push(text),
                printErr: (text: string) => wasmOutput.push(`[WASM err] ${text}`),
            });

            // Store output for later retrieval
            (module as any)._wasmInitOutput = wasmOutput;

            // Initialize the WASM module
            if (module._hyades_wasm_init) {
                module._hyades_wasm_init();
            }

            return new HyadesWasm(module as HyadesWasmModule, wasmOutput);
        } catch (err) {
            // Use stderr to avoid corrupting LSP protocol on stdout
            process.stderr.write(`Failed to load WASM module: ${err}\n`);
            process.stderr.write('Falling back to mock implementation\n');
            const mockModule = createMockModule();
            return new HyadesWasm(mockModule as HyadesWasmModule);
        }
    }

    /**
     * Parse a document
     */
    parse(source: string): ParseResult {
        const sourceBytes = this.module.lengthBytesUTF8(source) + 1;
        const sourcePtr = this.module._malloc(sourceBytes);
        this.module.stringToUTF8(source, sourcePtr, sourceBytes);

        const resultPtr = this.module._hyades_lsp_parse(sourcePtr);
        const resultJson = this.module.UTF8ToString(resultPtr);

        this.module._free(sourcePtr);
        this.module._hyades_lsp_free(resultPtr);

        try {
            return JSON.parse(resultJson);
        } catch {
            return {
                success: false,
                error: 'Failed to parse result',
                error_count: 0,
                warning_count: 0,
                symbol_count: 0,
                lines: 0,
            };
        }
    }

    /**
     * Get diagnostics from the last parse
     */
    getDiagnostics(): HyadesLSPDiagnostic[] {
        const resultPtr = this.module._hyades_lsp_get_diagnostics();
        const resultJson = this.module.UTF8ToString(resultPtr);
        this.module._hyades_lsp_free(resultPtr);

        try {
            return JSON.parse(resultJson);
        } catch {
            return [];
        }
    }

    /**
     * Get symbols from the last parse
     */
    getSymbols(): HyadesSymbol[] {
        const resultPtr = this.module._hyades_lsp_get_symbols();
        const resultJson = this.module.UTF8ToString(resultPtr);
        this.module._hyades_lsp_free(resultPtr);

        try {
            return JSON.parse(resultJson);
        } catch {
            return [];
        }
    }

    /**
     * Get completions at position
     */
    getCompletions(line: number, col: number): HyadesCompletion[] {
        const resultPtr = this.module._hyades_lsp_get_completions(line, col);
        const resultJson = this.module.UTF8ToString(resultPtr);
        this.module._hyades_lsp_free(resultPtr);

        try {
            return JSON.parse(resultJson);
        } catch {
            return [];
        }
    }

    /**
     * Get definition at position
     */
    getDefinition(line: number, col: number): HyadesDefinition | null {
        const resultPtr = this.module._hyades_lsp_get_definition(line, col);
        const resultJson = this.module.UTF8ToString(resultPtr);
        this.module._hyades_lsp_free(resultPtr);

        try {
            const result = JSON.parse(resultJson);
            return result === null ? null : result;
        } catch {
            return null;
        }
    }

    /**
     * Get hover information at position
     */
    getHover(line: number, col: number): HyadesHover | null {
        const resultPtr = this.module._hyades_lsp_get_hover(line, col);
        const resultJson = this.module.UTF8ToString(resultPtr);
        this.module._hyades_lsp_free(resultPtr);

        try {
            const result = JSON.parse(resultJson);
            return result === null ? null : result;
        } catch {
            return null;
        }
    }

    /**
     * Get references at position
     */
    getReferences(line: number, col: number): HyadesReference[] {
        const resultPtr = this.module._hyades_lsp_get_references(line, col);
        const resultJson = this.module.UTF8ToString(resultPtr);
        this.module._hyades_lsp_free(resultPtr);

        try {
            return JSON.parse(resultJson);
        } catch {
            return [];
        }
    }

    /**
     * Get semantic tokens for syntax highlighting
     */
    getSemanticTokens(): SemanticTokensData {
        const resultPtr = this.module._hyades_lsp_get_semantic_tokens();
        const resultJson = this.module.UTF8ToString(resultPtr);
        this.module._hyades_lsp_free(resultPtr);

        try {
            return JSON.parse(resultJson);
        } catch {
            return { data: [] };
        }
    }

    /**
     * Clear cached parse result
     */
    clear(): void {
        this.module._hyades_lsp_clear();
    }

    /**
     * Check if we have a valid parse result
     */
    hasResult(): boolean {
        return this.module._hyades_lsp_has_result() === 1;
    }
}

/**
 * Create a mock module for development/testing
 */
function createMockModule(): Partial<HyadesWasmModule> {
    const strings = new Map<number, string>();
    let nextPtr = 1000;

    return {
        _hyades_wasm_init: () => {},

        _hyades_lsp_parse: (sourcePtr: number): number => {
            const source = strings.get(sourcePtr) || '';

            // Basic mock parsing - just count lines and check for errors
            const lines = source.split('\n').length;
            const result = JSON.stringify({
                success: true,
                error_count: 0,
                warning_count: 0,
                symbol_count: 0,
                lines,
            });

            const ptr = nextPtr++;
            strings.set(ptr, result);
            return ptr;
        },

        _hyades_lsp_get_diagnostics: (): number => {
            const result = JSON.stringify([]);
            const ptr = nextPtr++;
            strings.set(ptr, result);
            return ptr;
        },

        _hyades_lsp_get_symbols: (): number => {
            const result = JSON.stringify([]);
            const ptr = nextPtr++;
            strings.set(ptr, result);
            return ptr;
        },

        _hyades_lsp_get_completions: (_line: number, _col: number): number => {
            // Return some basic completions
            const completions: HyadesCompletion[] = [
                { label: '\\frac', kind: 12, detail: 'Fraction' },
                { label: '\\sum', kind: 12, detail: 'Summation' },
                { label: '\\int', kind: 12, detail: 'Integral' },
                { label: '\\begin', kind: 12, detail: 'Begin environment' },
                { label: '\\end', kind: 12, detail: 'End environment' },
            ];
            const result = JSON.stringify(completions);
            const ptr = nextPtr++;
            strings.set(ptr, result);
            return ptr;
        },

        _hyades_lsp_get_definition: (_line: number, _col: number): number => {
            const result = 'null';
            const ptr = nextPtr++;
            strings.set(ptr, result);
            return ptr;
        },

        _hyades_lsp_get_hover: (_line: number, _col: number): number => {
            const result = 'null';
            const ptr = nextPtr++;
            strings.set(ptr, result);
            return ptr;
        },

        _hyades_lsp_get_references: (_line: number, _col: number): number => {
            const result = JSON.stringify([]);
            const ptr = nextPtr++;
            strings.set(ptr, result);
            return ptr;
        },

        _hyades_lsp_get_semantic_tokens: (): number => {
            const result = JSON.stringify({ data: [] });
            const ptr = nextPtr++;
            strings.set(ptr, result);
            return ptr;
        },

        _hyades_lsp_free: (ptr: number): void => {
            strings.delete(ptr);
        },

        _hyades_lsp_clear: (): void => {},

        _hyades_lsp_has_result: (): number => 1,

        _malloc: (size: number): number => {
            const ptr = nextPtr++;
            strings.set(ptr, '');
            return ptr;
        },

        _free: (ptr: number): void => {
            strings.delete(ptr);
        },

        UTF8ToString: (ptr: number): string => {
            return strings.get(ptr) || '';
        },

        stringToUTF8: (str: string, outPtr: number, _maxBytes: number): void => {
            strings.set(outPtr, str);
        },

        lengthBytesUTF8: (str: string): number => {
            return Buffer.byteLength(str, 'utf8');
        },
    };
}
