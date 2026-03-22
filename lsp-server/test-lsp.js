// test-lsp.js - Simple LSP server test client
const { spawn } = require('child_process');
const path = require('path');

const serverPath = path.join(__dirname, 'dist', 'server.js');
console.log('Starting LSP server:', serverPath);

const server = spawn('node', [serverPath, '--stdio'], {
    stdio: ['pipe', 'pipe', 'pipe']
});

let buffer = '';

function send(obj) {
    const json = JSON.stringify(obj);
    const msg = `Content-Length: ${Buffer.byteLength(json)}\r\n\r\n${json}`;
    console.log('\n>>> Sending:', obj.method || `response to ${obj.id}`);
    server.stdin.write(msg);
}

function parseMessages(data) {
    buffer += data.toString();

    while (true) {
        const headerEnd = buffer.indexOf('\r\n\r\n');
        if (headerEnd === -1) break;

        const header = buffer.slice(0, headerEnd);
        const lengthMatch = header.match(/Content-Length:\s*(\d+)/i);
        if (!lengthMatch) break;

        const contentLength = parseInt(lengthMatch[1], 10);
        const contentStart = headerEnd + 4;

        if (buffer.length < contentStart + contentLength) break;

        const content = buffer.slice(contentStart, contentStart + contentLength);
        buffer = buffer.slice(contentStart + contentLength);

        try {
            const msg = JSON.parse(content);
            console.log('\n<<< Received:', JSON.stringify(msg, null, 2));
        } catch (e) {
            console.log('\n<<< Raw:', content);
        }
    }
}

server.stdout.on('data', parseMessages);
server.stderr.on('data', d => console.error('STDERR:', d.toString()));
server.on('error', e => console.error('Error:', e));
server.on('close', code => console.log('\nServer exited with code:', code));

// Step 1: Initialize
console.log('\n=== Step 1: Initialize ===');
send({
    jsonrpc: '2.0',
    id: 1,
    method: 'initialize',
    params: {
        processId: process.pid,
        capabilities: {
            textDocument: {
                publishDiagnostics: { relatedInformation: true }
            }
        },
        rootUri: null,
        clientInfo: { name: 'test-client', version: '1.0.0' }
    }
});

setTimeout(() => {
    // Step 2: Initialized notification
    console.log('\n=== Step 2: Initialized ===');
    send({
        jsonrpc: '2.0',
        method: 'initialized',
        params: {}
    });
}, 200);

setTimeout(() => {
    // Step 3: Open a document with errors
    // Test: macro line tracking, lambda tracking, unclosed $$
    console.log('\n=== Step 3: Open document with errors ===');
    send({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: 'file:///test.cld',
                languageId: 'cassilda',
                version: 1,
                text: `% Define a macro
\\macro<\\greeting>{Hello, World!}

% Define a lambda function
\\lambda<square>[x]{\\calc{x * x}}

% Use them
\\greeting
Result: \\square[5]

% Unclosed math
$$oops this is not closed
`
            }
        }
    });
}, 400);

setTimeout(() => {
    // Step 4: Request document symbols
    console.log('\n=== Step 4: Request document symbols ===');
    send({
        jsonrpc: '2.0',
        id: 2,
        method: 'textDocument/documentSymbol',
        params: {
            textDocument: { uri: 'file:///test.cld' }
        }
    });
}, 600);

setTimeout(() => {
    // Step 5: Request completions after typing backslash
    console.log('\n=== Step 5: Request completions ===');
    send({
        jsonrpc: '2.0',
        id: 3,
        method: 'textDocument/completion',
        params: {
            textDocument: { uri: 'file:///test.cld' },
            position: { line: 3, character: 20 }
        }
    });
}, 800);

// Give time for responses then exit
setTimeout(() => {
    console.log('\n=== Test complete, shutting down ===');
    send({
        jsonrpc: '2.0',
        id: 99,
        method: 'shutdown',
        params: null
    });
}, 1500);

setTimeout(() => {
    send({
        jsonrpc: '2.0',
        method: 'exit',
        params: null
    });
}, 1700);

setTimeout(() => {
    server.kill();
    process.exit(0);
}, 2000);
