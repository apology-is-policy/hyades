const { readFileSync } = require('fs');
const { HyadesWasm } = require('./dist/hyades-wasm.js');

async function main() {
    const text = readFileSync('/Users/northkillpd/projects/texascii/local/tests/cl_syntax_comprehensive.cld', 'utf8');
    console.log(`File size: ${text.length} chars`);
    
    console.log('Initializing WASM...');
    const start1 = Date.now();
    const hyades = await HyadesWasm.initialize();
    console.log(`WASM init: ${Date.now() - start1}ms`);
    
    console.log('Parsing...');
    const start2 = Date.now();
    const result = hyades.parse(text);
    console.log(`Parse: ${Date.now() - start2}ms, success: ${result.success}`);
    
    console.log('Getting diagnostics...');
    const start3 = Date.now();
    const diags = hyades.getDiagnostics();
    console.log(`Diagnostics: ${Date.now() - start3}ms, count: ${diags.length}`);
    
    console.log('Getting semantic tokens...');
    const start4 = Date.now();
    const tokens = hyades.getSemanticTokens();
    console.log(`Semantic tokens: ${Date.now() - start4}ms, count: ${tokens?.data?.length / 5 || 0}`);
    
    console.log('Getting symbols...');
    const start5 = Date.now();
    const symbols = hyades.getSymbols();
    console.log(`Symbols: ${Date.now() - start5}ms, count: ${symbols.length}`);
    
    console.log('Getting completions at 0,0...');
    const start6 = Date.now();
    const completions = hyades.getCompletions(0, 0);
    console.log(`Completions: ${Date.now() - start6}ms, count: ${completions.length}`);
    
    console.log('Done!');
    process.exit(0);
}

main().catch(e => { console.error(e); process.exit(1); });
