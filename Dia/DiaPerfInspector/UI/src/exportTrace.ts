// Export trace to file. Guarded for jsdom (no DOM in tests).

import type { ChromeTraceJSON } from './model/chromeTrace';

export function exportTraceFile(json: ChromeTraceJSON, filename: string): void {
    // Guard: jsdom has no window.URL or document
    if (typeof window === 'undefined' || !window.URL || !document) {
        return;
    }

    const str = JSON.stringify(json, null, 2);
    const blob = new Blob([str], { type: 'application/json' });
    const url = window.URL.createObjectURL(blob);
    try {
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
    } finally {
        window.URL.revokeObjectURL(url);
    }
}
