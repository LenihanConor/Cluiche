import { useState, useRef, useCallback, useEffect, KeyboardEvent } from 'react';
import { theme, useBridgeRequest } from '@dia/editor-ui';

// --- Types ---
type OutputKind = 'stdout' | 'stderr' | 'echo' | 'system';

interface OutputLine {
    kind: OutputKind;
    text: string;
}

// --- Color mapping ---
const kindColor: Record<OutputKind, string> = {
    stdout: theme.text,      // white/grey
    stderr: theme.error,     // red
    echo:   theme.textMuted, // grey
    system: theme.warning,   // yellow
};

export const App = () => {
    const [input, setInput] = useState('');
    const [output, setOutput] = useState<OutputLine[]>([
        { kind: 'system', text: 'Python Console ready. Type Python code and press Enter.' }
    ]);
    const [busy, setBusy] = useState(false);
    const outputEndRef = useRef<HTMLDivElement>(null);
    const fileInputRef = useRef<HTMLInputElement>(null);
    const sendRequest = useBridgeRequest<Record<string, unknown>>();

    // Auto-scroll to bottom on new output
    useEffect(() => {
        outputEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    }, [output]);

    const appendLines = useCallback((lines: OutputLine[]) => {
        setOutput(prev => [...prev, ...lines]);
    }, []);

    const handleExecute = useCallback(async () => {
        const code = input.trim();
        if (!code || busy) return;
        setInput('');
        setBusy(true);
        appendLines([{ kind: 'echo', text: `>>> ${code}` }]);
        try {
            const result = await sendRequest('python_console.execute', { code });
            const lines: OutputLine[] = [];
            if (result?.stdout) lines.push({ kind: 'stdout', text: String(result.stdout) });
            if (result?.stderr) lines.push({ kind: 'stderr', text: String(result.stderr) });
            if (lines.length === 0) lines.push({ kind: 'system', text: '(no output)' });
            appendLines(lines);
        } catch (e) {
            appendLines([{ kind: 'stderr', text: String(e) }]);
        } finally {
            setBusy(false);
        }
    }, [input, busy, sendRequest, appendLines]);

    const handleKeyDown = useCallback((e: KeyboardEvent<HTMLInputElement>) => {
        if (e.key === 'Enter') handleExecute();
    }, [handleExecute]);

    const handleRunFile = useCallback(async (e: React.ChangeEvent<HTMLInputElement>) => {
        const file = e.target.files?.[0];
        if (!file || busy) return;
        // Reset the input so the same file can be re-selected
        e.target.value = '';
        setBusy(true);
        appendLines([{ kind: 'system', text: `Running file: ${file.name}` }]);
        try {
            // Use the file path if available (desktop/CEF env), otherwise use name as hint
            const path = (file as File & { path?: string }).path ?? file.name;
            const result = await sendRequest('python_console.run_file', { path });
            const lines: OutputLine[] = [];
            if (result?.stdout) lines.push({ kind: 'stdout', text: String(result.stdout) });
            if (result?.stderr) lines.push({ kind: 'stderr', text: String(result.stderr) });
            const exitCode = (result?.exitCode as number) ?? 0;
            lines.push({ kind: exitCode === 0 ? 'system' : 'stderr', text: `Exit code: ${exitCode}` });
            appendLines(lines);
        } catch (e) {
            appendLines([{ kind: 'stderr', text: String(e) }]);
        } finally {
            setBusy(false);
        }
    }, [busy, sendRequest, appendLines]);

    const handleClear = useCallback(() => {
        setOutput([]);
    }, []);

    // --- Render ---
    return (
        <div style={{ height: '100%', display: 'flex', flexDirection: 'column', background: theme.bg, color: theme.text, fontFamily: "'Consolas', 'Courier New', monospace", fontSize: 12 }}>
            {/* Toolbar */}
            <div style={{ display: 'flex', gap: 6, padding: '4px 8px', borderBottom: `1px solid ${theme.border}`, flexShrink: 0, background: theme.bgPanel }}>
                <button
                    onClick={() => fileInputRef.current?.click()}
                    disabled={busy}
                    style={{ padding: '2px 10px', background: theme.accent, color: '#fff', border: 'none', borderRadius: 3, cursor: busy ? 'not-allowed' : 'pointer', fontSize: 12 }}
                >
                    Run File
                </button>
                <button
                    onClick={handleClear}
                    style={{ padding: '2px 10px', background: theme.bgInput, color: theme.text, border: `1px solid ${theme.border}`, borderRadius: 3, cursor: 'pointer', fontSize: 12 }}
                >
                    Clear
                </button>
                {busy && <span style={{ color: theme.textMuted, alignSelf: 'center' }}>Running…</span>}
                {/* Hidden file input */}
                <input
                    ref={fileInputRef}
                    type="file"
                    accept=".py"
                    style={{ display: 'none' }}
                    onChange={handleRunFile}
                />
            </div>

            {/* Output area */}
            <div style={{ flex: 1, overflowY: 'auto', padding: '6px 10px', display: 'flex', flexDirection: 'column', gap: 1 }}>
                {output.map((line, i) => (
                    <div key={i} style={{ color: kindColor[line.kind], whiteSpace: 'pre-wrap', wordBreak: 'break-all' }}>
                        {line.text}
                    </div>
                ))}
                <div ref={outputEndRef} />
            </div>

            {/* Input field */}
            <div style={{ display: 'flex', borderTop: `1px solid ${theme.border}`, flexShrink: 0, background: theme.bgPanel }}>
                <span style={{ color: theme.textMuted, padding: '6px 8px', alignSelf: 'center', flexShrink: 0 }}>{'>>>'}</span>
                <input
                    type="text"
                    value={input}
                    onChange={e => setInput(e.target.value)}
                    onKeyDown={handleKeyDown}
                    disabled={busy}
                    placeholder="Enter Python code…"
                    style={{
                        flex: 1,
                        background: 'transparent',
                        border: 'none',
                        outline: 'none',
                        color: theme.text,
                        fontSize: 12,
                        fontFamily: "'Consolas', 'Courier New', monospace",
                        padding: '6px 0',
                    }}
                    autoFocus
                />
                <button
                    onClick={handleExecute}
                    disabled={busy || !input.trim()}
                    style={{ padding: '0 12px', background: theme.accent, color: '#fff', border: 'none', cursor: busy ? 'not-allowed' : 'pointer', flexShrink: 0, fontSize: 12 }}
                >
                    Run
                </button>
            </div>
        </div>
    );
};

export default App;
