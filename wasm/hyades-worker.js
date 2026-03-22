import { Hyades } from './hyades-api.js';

let hyades = null;

async function init() {
    hyades = await Hyades.create({ wasmPath: './' });
    self.postMessage({ type: 'ready', version: hyades.version });
}

const handlers = {
    lspParseAndGetTokens({ source }) {
        hyades.lspParse(source);
        return hyades.lspGetSemanticTokensRaw();
    },
    lspGetHover({ line, col }) { return hyades.lspGetHover(line, col); },
    lspGetDefinition({ line, col }) { return hyades.lspGetDefinition(line, col); },
    cassilda({ input }) { return hyades.cassilda(input); },
};

self.onmessage = (e) => {
    const { id, method, params } = e.data;
    if (!hyades) { self.postMessage({ id, result: null, error: 'Not ready' }); return; }
    try {
        const result = handlers[method](params || {});
        if (result instanceof Uint32Array) {
            self.postMessage({ id, result, error: null }, [result.buffer]);
        } else {
            self.postMessage({ id, result, error: null });
        }
    } catch (err) {
        self.postMessage({ id, result: null, error: err.message });
    }
};

init();
