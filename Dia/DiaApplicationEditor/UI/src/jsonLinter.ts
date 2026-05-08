import type { Diagnostic } from '@codemirror/lint';
import type { EditorView } from '@codemirror/view';

export function jsonLinter(view: EditorView): Diagnostic[] {
    const text = view.state.doc.toString();
    if (!text.trim()) return [];
    try {
        JSON.parse(text);
        return [];
    } catch (e: unknown) {
        const msg = e instanceof Error ? e.message : String(e);
        // Try to extract character position from message like "at position 42"
        const match = msg.match(/position (\d+)/);
        const pos = match ? Math.min(parseInt(match[1], 10), text.length - 1) : 0;
        return [{ from: pos, to: Math.min(pos + 1, text.length), severity: 'error', message: msg }];
    }
}
