import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;
let outputChannel: vscode.OutputChannel | undefined;

/**
 * Start the LSP client and connect to the Hyades language server
 */
export async function startLspClient(context: vscode.ExtensionContext): Promise<void> {
    // Create output channel early for debugging
    outputChannel = vscode.window.createOutputChannel('Hyades Language Server');
    context.subscriptions.push(outputChannel);

    outputChannel.appendLine('Starting Hyades LSP client...');
    outputChannel.appendLine(`Extension path: ${context.extensionPath}`);

    // Resolve symlinks to get the real extension path
    const fs = require('fs');
    const os = require('os');
    const realExtensionPath = fs.realpathSync(context.extensionPath);
    outputChannel.appendLine(`Real extension path: ${realExtensionPath}`);

    // Search for the LSP server in order of preference:
    // 1. Installed location (platform-dependent)
    // 2. Dev-tree location (adjacent to extension)
    const installedDir = process.platform === 'win32'
        ? path.join(process.env.LOCALAPPDATA || path.join(os.homedir(), 'AppData', 'Local'), 'hyades', 'lsp-server')
        : path.join(os.homedir(), '.local', 'share', 'hyades', 'lsp-server');

    const candidates = [
        path.join(installedDir, 'server.js'),
        path.join(realExtensionPath, '..', 'lsp-server', 'dist', 'server.js'),
    ];

    let serverModule: string | undefined;
    for (const candidate of candidates) {
        outputChannel.appendLine(`Looking for server at: ${candidate}`);
        if (fs.existsSync(candidate)) {
            serverModule = candidate;
            break;
        }
    }

    if (!serverModule) {
        outputChannel.appendLine('ERROR: Server not found at any known location');
        outputChannel.show();
        return;
    }

    outputChannel.appendLine('Server found, starting...');

    // Server options - spawn as separate process with stdio
    const serverOptions: ServerOptions = {
        run: {
            command: 'node',
            args: [serverModule, '--stdio'],
            transport: TransportKind.stdio
        },
        debug: {
            command: 'node',
            args: [serverModule, '--stdio'],
            transport: TransportKind.stdio
        }
    };

    // Client options with middleware to debug semantic tokens
    const clientOptions: LanguageClientOptions = {
        // Register the server for cassilda documents
        documentSelector: [{ scheme: 'file', language: 'cassilda' }],
        synchronize: {
            // Notify the server about file changes to .cld files in the workspace
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.cld')
        },
        outputChannel: outputChannel,
        // Middleware to log semantic token requests
        middleware: {
            provideDocumentSemanticTokens: async (document, token, next) => {
                outputChannel?.appendLine(`[Middleware] Semantic tokens requested for ${document.uri}`);
                const result = await next(document, token);
                outputChannel?.appendLine(`[Middleware] Semantic tokens result: ${result ? result.data.length / 5 : 0} tokens`);
                return result;
            }
        }
    };

    // Create and start the client
    client = new LanguageClient(
        'hyadesLsp',
        'Hyades Language Server',
        serverOptions,
        clientOptions
    );

    // Start the client (this will also launch the server)
    try {
        outputChannel.appendLine('Calling client.start()...');
        await client.start();
        outputChannel.appendLine('Client started successfully!');
    } catch (error) {
        outputChannel.appendLine(`ERROR starting client: ${error}`);
        outputChannel.show();
        return;
    }

    // Add to subscriptions for cleanup
    context.subscriptions.push({
        dispose: () => {
            if (client) {
                client.stop();
            }
        }
    });

    outputChannel.appendLine('LSP client is now running');

    // Log currently open documents
    outputChannel.appendLine('Currently open documents:');
    for (const doc of vscode.workspace.textDocuments) {
        outputChannel.appendLine(`  - ${doc.uri.toString()} (lang: ${doc.languageId})`);
    }
}

/**
 * Stop the LSP client
 */
export function stopLspClient(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}

/**
 * Check if the LSP client is running
 */
export function isLspClientRunning(): boolean {
    return client !== undefined && client.isRunning();
}
