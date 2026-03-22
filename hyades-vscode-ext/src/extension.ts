import * as vscode from 'vscode';
import { exec } from 'child_process';
import { promisify } from 'util';
import { startLspClient, stopLspClient } from './lsp-client';

const execAsync = promisify(exec);

// NBSP character used to identify rendered output
const NBSP = '\u00A0';

// Decoration types (created on activation)
let outputDecoration: vscode.TextEditorDecorationType;
let referenceDecoration: vscode.TextEditorDecorationType;

// Output channel for verbose logging
let outputChannel: vscode.OutputChannel;

// ============================================================================
// Activation
// ============================================================================

export function activate(context: vscode.ExtensionContext) {
    console.log('Cassilda extension activated');

    // Create output channel for verbose logging
    outputChannel = vscode.window.createOutputChannel('Cassilda');
    context.subscriptions.push(outputChannel);

    // Create decoration types from configuration
    createDecorations();

    // Start LSP client for diagnostics, completion, go-to-definition, etc.
    startLspClient(context);

    // Register folding provider
    context.subscriptions.push(
        vscode.languages.registerFoldingRangeProvider(
            { language: 'cassilda' },
            new CassildaFoldingProvider()
        )
    );

    // Register commands
    context.subscriptions.push(
        vscode.commands.registerCommand('cassilda.process', processCurrentFile)
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('cassilda.foldAllOutput', foldAllOutput)
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('cassilda.unfoldAllOutput', unfoldAllOutput)
    );

    // Update decorations when editor changes
    context.subscriptions.push(
        vscode.window.onDidChangeActiveTextEditor(editor => {
            if (editor && editor.document.languageId === 'cassilda') {
                updateDecorations(editor);
            }
        })
    );

    // Update decorations when document changes
    context.subscriptions.push(
        vscode.workspace.onDidChangeTextDocument(event => {
            const editor = vscode.window.activeTextEditor;
            if (editor && event.document === editor.document && 
                editor.document.languageId === 'cassilda') {
                updateDecorations(editor);
            }
        })
    );

    // Process on save (if enabled)
    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument(document => {
            const config = vscode.workspace.getConfiguration('cassilda');

            // Check if this file extension should be processed
            const processExtensions = config.get<string[]>('processExtensions') || ['.cld'];
            const filePath = document.uri.fsPath;
            const fileExt = filePath.substring(filePath.lastIndexOf('.')).toLowerCase();
            const shouldProcess = processExtensions.some(ext => ext.toLowerCase() === fileExt);

            if (shouldProcess && config.get('processOnSave')) {
                // Check exclude patterns
                const excludePatterns = config.get<string[]>('processOnSaveExclude') || [];
                const isExcluded = excludePatterns.some(pattern => {
                    // Simple glob matching: convert ** and * to regex
                    const regex = new RegExp(
                        pattern
                            .replace(/[.+^${}()|[\]\\]/g, '\\$&')  // Escape regex chars except * and ?
                            .replace(/\*\*/g, '.*')                 // ** matches anything
                            .replace(/\*/g, '[^/]*')                // * matches within path segment
                            .replace(/\?/g, '.')                    // ? matches single char
                    );
                    return regex.test(filePath);
                });

                if (!isExcluded) {
                    processFile(filePath);
                }
            }
        })
    );

    // Update decorations on configuration change
    context.subscriptions.push(
        vscode.workspace.onDidChangeConfiguration(event => {
            if (event.affectsConfiguration('cassilda')) {
                createDecorations();
                const editor = vscode.window.activeTextEditor;
                if (editor && editor.document.languageId === 'cassilda') {
                    updateDecorations(editor);
                }
            }
        })
    );

    // Update decorations on theme change
    context.subscriptions.push(
        vscode.window.onDidChangeActiveColorTheme(() => {
            createDecorations();
            const editor = vscode.window.activeTextEditor;
            if (editor && editor.document.languageId === 'cassilda') {
                updateDecorations(editor);
            }
        })
    );

    // Initial decoration update
    if (vscode.window.activeTextEditor?.document.languageId === 'cassilda') {
        updateDecorations(vscode.window.activeTextEditor);
    }
}

export function deactivate(): Thenable<void> | undefined {
    if (outputDecoration) {
        outputDecoration.dispose();
    }
    if (referenceDecoration) {
        referenceDecoration.dispose();
    }
    if (outputChannel) {
        outputChannel.dispose();
    }
    return stopLspClient();
}

// ============================================================================
// Decorations
// ============================================================================

function createDecorations() {
    // Dispose old decorations
    if (outputDecoration) {
        outputDecoration.dispose();
    }
    if (referenceDecoration) {
        referenceDecoration.dispose();
    }

    const config = vscode.workspace.getConfiguration('cassilda');

    // Get theme-aware default colors
    const isLightTheme = vscode.window.activeColorTheme.kind === vscode.ColorThemeKind.Light;

    // Theme-aware defaults
    const defaultOutputBg = isLightTheme ? 'rgba(220, 230, 240, 0.4)' : 'rgba(40, 50, 70, 0.4)';
    const defaultOutputBorder = isLightTheme ? 'rgba(100, 120, 160, 0.6)' : 'rgba(80, 100, 140, 0.6)';
    const defaultReferenceBg = isLightTheme ? 'rgba(200, 230, 200, 0.3)' : 'rgba(60, 80, 60, 0.3)';

    // Get config values (treat empty string as unset)
    const outputBgColor = config.get<string>('outputBackgroundColor')?.trim() || defaultOutputBg;
    const outputBorderColor = config.get<string>('outputBorderColor')?.trim() || defaultOutputBorder;
    const referenceBgColor = config.get<string>('referenceBackgroundColor')?.trim() || defaultReferenceBg;

    // Output lines decoration (background + left border)
    outputDecoration = vscode.window.createTextEditorDecorationType({
        backgroundColor: outputBgColor,
        borderWidth: '0 0 0 3px',
        borderStyle: 'solid',
        borderColor: outputBorderColor,
        isWholeLine: true,
        // Slightly dim the text
        opacity: '0.85'
    });

    // Reference line decoration
    referenceDecoration = vscode.window.createTextEditorDecorationType({
        backgroundColor: referenceBgColor,
        isWholeLine: true,
        fontWeight: 'bold'
    });
}

function updateDecorations(editor: vscode.TextEditor) {
    const document = editor.document;
    const outputRanges: vscode.Range[] = [];
    const referenceRanges: vscode.Range[] = [];

    for (let i = 0; i < document.lineCount; i++) {
        const line = document.lineAt(i);
        const text = line.text;

        // Check for rendered output (contains NBSP)
        if (text.includes(NBSP)) {
            outputRanges.push(line.range);
        }

        // Check for reference line
        if (text.match(/^\s*@cassilda:/)) {
            referenceRanges.push(line.range);
        }
    }

    editor.setDecorations(outputDecoration, outputRanges);
    editor.setDecorations(referenceDecoration, referenceRanges);
}

// ============================================================================
// Folding Provider
// ============================================================================

class CassildaFoldingProvider implements vscode.FoldingRangeProvider {
    provideFoldingRanges(
        document: vscode.TextDocument,
        _context: vscode.FoldingContext,
        _token: vscode.CancellationToken
    ): vscode.FoldingRange[] {
        const ranges: vscode.FoldingRange[] = [];

        let outputBlockStart: number | null = null;
        let inOutputBlock = false;

        for (let i = 0; i < document.lineCount; i++) {
            const line = document.lineAt(i).text;
            const isOutput = line.includes(NBSP);
            const isReference = /^\s*@cassilda:/.test(line);

            // Start of output block: the @cassilda: line
            if (isReference && !inOutputBlock) {
                outputBlockStart = i;
            }

            // Inside output block
            if (isOutput) {
                if (!inOutputBlock && outputBlockStart === null) {
                    // Output without preceding reference (shouldn't happen, but handle it)
                    outputBlockStart = i;
                }
                inOutputBlock = true;
            }

            // End of output block: non-output, non-empty line
            if (inOutputBlock && !isOutput && line.trim().length > 0) {
                if (outputBlockStart !== null) {
                    ranges.push(new vscode.FoldingRange(
                        outputBlockStart,
                        i - 1,
                        vscode.FoldingRangeKind.Region
                    ));
                }
                outputBlockStart = null;
                inOutputBlock = false;
            }
        }

        // Handle output block at end of file
        if (inOutputBlock && outputBlockStart !== null) {
            ranges.push(new vscode.FoldingRange(
                outputBlockStart,
                document.lineCount - 1,
                vscode.FoldingRangeKind.Region
            ));
        }

        // Also fold @label...@end blocks
        let labelStart: number | null = null;
        for (let i = 0; i < document.lineCount; i++) {
            const line = document.lineAt(i).text;

            if (/^\s*@label\s+/.test(line)) {
                labelStart = i;
            } else if (/^\s*@end\b/.test(line) && labelStart !== null) {
                ranges.push(new vscode.FoldingRange(
                    labelStart,
                    i,
                    vscode.FoldingRangeKind.Region
                ));
                labelStart = null;
            }
        }

        // Fold #before_each...#after_each...#end blocks
        let directiveStart: number | null = null;
        for (let i = 0; i < document.lineCount; i++) {
            const line = document.lineAt(i).text;

            if (/^\s*#(before_each|after_each)\b/.test(line)) {
                if (directiveStart === null) {
                    directiveStart = i;
                }
            } else if (/^\s*#end\b/.test(line) && directiveStart !== null) {
                ranges.push(new vscode.FoldingRange(
                    directiveStart,
                    i,
                    vscode.FoldingRangeKind.Region
                ));
                directiveStart = null;
            }
        }

        return ranges;
    }
}

// ============================================================================
// Commands
// ============================================================================

async function processCurrentFile() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        vscode.window.showErrorMessage('No active editor');
        return;
    }

    const config = vscode.workspace.getConfiguration('cassilda');
    const processExtensions = config.get<string[]>('processExtensions') || ['.cld'];
    const filePath = editor.document.uri.fsPath;
    const fileExt = filePath.substring(filePath.lastIndexOf('.')).toLowerCase();
    const shouldProcess = processExtensions.some(ext => ext.toLowerCase() === fileExt);

    if (!shouldProcess) {
        vscode.window.showErrorMessage(
            `Current file extension (${fileExt}) is not configured for Cassilda processing. ` +
            `Add it to cassilda.processExtensions setting.`
        );
        return;
    }

    // Save first
    await editor.document.save();

    // Process
    await processFile(filePath);
}

async function processFile(filePath: string) {
    const config = vscode.workspace.getConfiguration('cassilda');
    const executable = config.get<string>('executablePath') || 'cassilda';
    const configPath = config.get<string>('configPath')?.trim();
    const findConfig = config.get<boolean>('findConfig');
    const verbose = config.get<boolean>('verbose');

    // Build command with library options
    let command = `"${executable}"`;

    // Add config options
    if (configPath) {
        command += ` --config "${configPath}"`;
    } else if (findConfig) {
        command += ' --find-config';
    }

    // Add verbose flag
    if (verbose) {
        command += ' --verbose';
    }

    command += ` process "${filePath}"`;

    try {
        if (verbose) {
            outputChannel.appendLine(`[${new Date().toLocaleTimeString()}] Running: ${command}`);
        }

        let result;
        try {
            result = await execAsync(command);
        } catch (execError: any) {
            // execAsync throws on non-zero exit code OR stderr output
            // Check if it actually failed (non-zero exit) or just has stderr
            if (execError.code !== undefined && execError.code !== 0) {
                // Actually failed
                throw execError;
            }
            // Command succeeded but had stderr output (like EMSDK messages)
            result = execError;
        }

        if (verbose && result.stdout) {
            outputChannel.appendLine(result.stdout);
        }
        if (verbose && result.stderr) {
            outputChannel.appendLine('[stderr] ' + result.stderr);
        }

        // Reload the document to show changes
        const doc = vscode.workspace.textDocuments.find(d => d.uri.fsPath === filePath);
        if (doc) {
            // The file was modified on disk, VS Code should auto-reload
            // But force a refresh of decorations
            const editor = vscode.window.visibleTextEditors.find(e => e.document === doc);
            if (editor) {
                setTimeout(() => updateDecorations(editor), 100);
            }
        }

        if (verbose) {
            outputChannel.appendLine(`[${new Date().toLocaleTimeString()}] Success`);
        }
    } catch (error: any) {
        const errorMsg = error.stderr || error.message || error.toString();

        if (verbose) {
            outputChannel.appendLine(`[${new Date().toLocaleTimeString()}] Error: ${errorMsg}`);
            outputChannel.show(true);
        }

        vscode.window.showErrorMessage(`Cassilda error: ${errorMsg}`);
    }
}

function foldAllOutput() {
    vscode.commands.executeCommand('editor.foldAllMarkerRegions');
}

function unfoldAllOutput() {
    vscode.commands.executeCommand('editor.unfoldAllMarkerRegions');
}
