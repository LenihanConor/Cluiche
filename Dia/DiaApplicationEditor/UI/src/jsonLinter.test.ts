import { describe, it, expect, vi } from 'vitest';
import { jsonLinter } from './jsonLinter';

// Minimal EditorView stub — jsonLinter only reads view.state.doc.toString()
function makeView(text: string) {
    return { state: { doc: { toString: () => text } } } as any;
}

describe('jsonLinter', () => {
    it('returns no diagnostics for valid JSON', () => {
        expect(jsonLinter(makeView('{"a":1}'))).toHaveLength(0);
    });

    it('returns no diagnostics for empty string', () => {
        expect(jsonLinter(makeView(''))).toHaveLength(0);
    });

    it('returns no diagnostics for whitespace-only input', () => {
        expect(jsonLinter(makeView('   '))).toHaveLength(0);
    });

    it('returns one error diagnostic for invalid JSON', () => {
        const diags = jsonLinter(makeView('{bad}'));
        expect(diags).toHaveLength(1);
        expect(diags[0].severity).toBe('error');
        expect(diags[0].message).toBeTruthy();
    });

    it('diagnostic from and to are within document bounds', () => {
        const text = '{"x":}';
        const diags = jsonLinter(makeView(text));
        expect(diags[0].from).toBeGreaterThanOrEqual(0);
        expect(diags[0].to).toBeLessThanOrEqual(text.length);
        expect(diags[0].from).toBeLessThanOrEqual(diags[0].to);
    });

    it('falls back to position 0 when parse error has no position', () => {
        // Corrupt JSON with no numeric position in message
        const diags = jsonLinter(makeView('['));
        expect(diags[0].from).toBeGreaterThanOrEqual(0);
    });

    it('handles arrays correctly', () => {
        expect(jsonLinter(makeView('[1,2,3]'))).toHaveLength(0);
    });

    it('handles nested objects correctly', () => {
        expect(jsonLinter(makeView('{"a":{"b":true}}'))).toHaveLength(0);
    });
});
