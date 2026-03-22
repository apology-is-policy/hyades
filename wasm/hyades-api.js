/**
 * Hyades - Unicode Mathematical Typesetting
 * JavaScript API for the WASM module
 * 
 * @example
 * import { Hyades } from './hyades-api.js';
 * 
 * const hyades = await Hyades.create();
 * 
 * // Render a math expression
 * const result = hyades.renderMath('\\frac{a}{b}');
 * console.log(result);
 * 
 * // Render a full document
 * const doc = hyades.render('Hello $x^2$ world\n$$\\sum_{i=1}^n i$$');
 * console.log(doc);
 * 
 * // Process a Cassilda document
 * const cld = hyades.cassilda('@label test\n$$x^2$$\n@end\n@cassilda: test');
 * console.log(cld);
 */

// Default path to WASM files (can be overridden)
const DEFAULT_WASM_PATH = './';

/**
 * Hyades rendering engine
 */
export class Hyades {
    #module = null;
    #render = null;
    #renderMath = null;
    #cassilda = null;
    #free = null;
    #isError = null;
    
    /**
     * Private constructor - use Hyades.create() instead
     */
    constructor(module) {
        this.#module = module;
        
        // Wrap C functions
        this.#render = module.cwrap('hyades_wasm_render', 'number', ['string', 'number']);
        this.#renderMath = module.cwrap('hyades_wasm_render_math', 'number', ['string', 'number']);
        this.#cassilda = module.cwrap('hyades_wasm_cassilda_process', 'number', ['string']);
        this.#free = module.cwrap('hyades_wasm_free', null, ['number']);
        this.#isError = module.cwrap('hyades_wasm_is_error', 'number', ['number']);
    }
    
    /**
     * Create a new Hyades instance
     * @param {Object} options - Configuration options
     * @param {string} options.wasmPath - Path to WASM files (default: './')
     * @returns {Promise<Hyades>} Initialized Hyades instance
     */
    static async create(options = {}) {
        const wasmPath = options.wasmPath || DEFAULT_WASM_PATH;
        
        // Dynamic import of the Emscripten-generated module
        // This assumes the module is built with MODULARIZE=1 and EXPORT_ES6=1
        const createModule = await import(`${wasmPath}hyades.js`);
        
        // Initialize the WASM module
        const module = await createModule.default({
            // Optional: customize module loading
            locateFile: (path) => `${wasmPath}${path}`
        });
        
        // Call init
        module._hyades_wasm_init();
        
        return new Hyades(module);
    }
    
    /**
     * Create instance from pre-loaded module (for custom loading scenarios)
     * @param {Object} module - Emscripten module instance
     * @returns {Hyades}
     */
    static fromModule(module) {
        module._hyades_wasm_init();
        return new Hyades(module);
    }
    
    /**
     * Helper to call a WASM function and handle the result
     */
    #callAndFree(fn, ...args) {
        const ptr = fn(...args);
        if (!ptr) {
            throw new Error('WASM function returned null');
        }
        
        const result = this.#module.UTF8ToString(ptr);
        this.#free(ptr);
        
        // Check for error
        if (result.startsWith('ERROR: ')) {
            throw new HyadesError(result.substring(7));
        }
        
        return result;
    }
    
    /**
     * Render a Hyades document to Unicode text
     * @param {string} input - Hyades source document
     * @param {Object} options - Rendering options
     * @param {number} options.width - Output width (default: 80)
     * @returns {string} Rendered Unicode text
     * @throws {HyadesError} If rendering fails
     */
    render(input, options = {}) {
        const width = options.width || 0;
        return this.#callAndFree(this.#render, input, width);
    }
    
    /**
     * Render a TeX math expression
     * @param {string} mathSrc - TeX math source (without $$ delimiters)
     * @param {Object} options - Rendering options
     * @param {number} options.width - Output width (default: 80)
     * @returns {string} Rendered math as Unicode text
     * @throws {HyadesError} If rendering fails
     */
    renderMath(mathSrc, options = {}) {
        const width = options.width || 0;
        return this.#callAndFree(this.#renderMath, mathSrc, width);
    }
    
    /**
     * Process a Cassilda document
     * @param {string} input - Cassilda document source
     * @returns {string} Processed document with rendered segments
     * @throws {HyadesError} If processing fails
     */
    cassilda(input) {
        return this.#callAndFree(this.#cassilda, input);
    }
    
    /**
     * Get the Hyades version
     * @returns {string} Version string
     */
    get version() {
        return this.#module.UTF8ToString(this.#module._hyades_wasm_version());
    }

    // =========================================================================
    // LSP Features (hover, go-to-definition)
    // =========================================================================

    /**
     * Parse document for LSP features
     * @param {string} source - Document source code
     */
    lspParse(source) {
        const sourceBytes = this.#module.lengthBytesUTF8(source) + 1;
        const sourcePtr = this.#module._malloc(sourceBytes);
        this.#module.stringToUTF8(source, sourcePtr, sourceBytes);
        this.#module._hyades_lsp_parse(sourcePtr);
        this.#module._free(sourcePtr);
    }

    /**
     * Get hover information at position (0-based line/col)
     * @returns {Object|null} { contents: { value }, range: { start, end } }
     */
    lspGetHover(line, col) {
        const ptr = this.#module._hyades_lsp_get_hover(line, col);
        const json = this.#module.UTF8ToString(ptr);
        this.#module._hyades_lsp_free(ptr);
        try {
            const r = JSON.parse(json);
            return r === null ? null : r;
        } catch { return null; }
    }

    /**
     * Get definition location at position (0-based line/col)
     * @returns {Object|null} { range: { start, end } }
     */
    lspGetDefinition(line, col) {
        const ptr = this.#module._hyades_lsp_get_definition(line, col);
        const json = this.#module.UTF8ToString(ptr);
        this.#module._hyades_lsp_free(ptr);
        try {
            const r = JSON.parse(json);
            return r === null ? null : r;
        } catch { return null; }
    }

    /** Clear LSP state */
    lspClear() {
        this.#module._hyades_lsp_clear();
    }

    /**
     * Get semantic tokens for syntax highlighting (JSON version)
     * @returns {Object} { data: number[] } - delta-encoded token data
     */
    lspGetSemanticTokens() {
        const ptr = this.#module._hyades_lsp_get_semantic_tokens();
        const json = this.#module.UTF8ToString(ptr);
        this.#module._hyades_lsp_free(ptr);
        try {
            return JSON.parse(json);
        } catch {
            return { data: [] };
        }
    }

    /**
     * Get semantic tokens as raw Uint32Array (no JSON overhead)
     * @returns {Uint32Array|null} delta-encoded token data, or null if empty
     */
    lspGetSemanticTokensRaw() {
        const count = this.#module._hyades_lsp_compute_semantic_tokens();
        if (count === 0) return null;
        const ptr = this.#module._hyades_lsp_semantic_tokens_ptr();
        // Read directly from WASM heap and copy (heap may relocate on next alloc)
        return new Uint32Array(
            this.#module.HEAPU32.buffer, ptr, count
        ).slice();
    }
}

/**
 * Error thrown by Hyades operations
 */
export class HyadesError extends Error {
    constructor(message) {
        super(message);
        this.name = 'HyadesError';
    }
}

/**
 * Convenience function for one-off rendering without managing instance
 * @param {string} mathSrc - TeX math source
 * @param {Object} options - Options including wasmPath
 * @returns {Promise<string>} Rendered result
 */
export async function renderMath(mathSrc, options = {}) {
    const hyades = await Hyades.create(options);
    return hyades.renderMath(mathSrc, options);
}

/**
 * Convenience function for one-off document rendering
 * @param {string} input - Hyades document source
 * @param {Object} options - Options including wasmPath
 * @returns {Promise<string>} Rendered result
 */
export async function render(input, options = {}) {
    const hyades = await Hyades.create(options);
    return hyades.render(input, options);
}

export default Hyades;