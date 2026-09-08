// CEF bridge utilities
// Communicates with the C++ backend via window.parent.postMessage (matching CEF iframe pattern).

const REQUEST_TIMEOUT_MS = 5000;

export function bridgeRequest(type: string, data: object = {}): Promise<unknown> {
    return new Promise((resolve) => {
        const reqId = `req_${Date.now()}_${Math.random().toString(36).slice(2)}`;

        const handler = (e: MessageEvent) => {
            const p = e.data?.payload ?? e.data;
            if (p?.type === 'DiaEditor_onResponse' && p?.reqId === reqId) {
                clearTimeout(timer);
                window.removeEventListener('message', handler);
                resolve(p.result);
            }
        };

        // If the C++ side has no handler registered, the request will hang forever.
        // Time it out and resolve with an error so the UI surfaces the failure
        // instead of silently sitting in a "loading" state.
        const timer = setTimeout(() => {
            window.removeEventListener('message', handler);
            // eslint-disable-next-line no-console
            console.error(
                `[DiaEconomyInspector] bridgeRequest('${type}') timed out after ${REQUEST_TIMEOUT_MS}ms ` +
                `(reqId='${reqId}'). C++ handler is likely not registered — check editor session logs ` +
                `for 'No request handler' from WebUIBridge.`
            );
            resolve({ ok: false, error: `bridgeRequest timeout: '${type}'` });
        }, REQUEST_TIMEOUT_MS);

        window.addEventListener('message', handler);
        window.parent.postMessage({ __diaFromFrame: true, payload: { type, reqId, data } }, '*');
    });
}

export function bridgeEvent(type: string, data: object = {}): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}
