// CEF bridge utilities
// Communicates with the C++ backend via window.parent.postMessage (matching CEF iframe pattern).

export function bridgeRequest(type: string, data: object = {}): Promise<unknown> {
    return new Promise((resolve) => {
        const reqId = `req_${Date.now()}_${Math.random().toString(36).slice(2)}`;
        const handler = (e: MessageEvent) => {
            const p = e.data?.payload ?? e.data;
            if (p?.type === 'DiaEditor_onResponse' && p?.reqId === reqId) {
                window.removeEventListener('message', handler);
                resolve(p.result);
            }
        };
        window.addEventListener('message', handler);
        window.parent.postMessage({ __diaFromFrame: true, payload: { type, reqId, data } }, '*');
    });
}

export function bridgeEvent(type: string, data: object = {}): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}
