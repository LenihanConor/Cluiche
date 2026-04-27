import { useCallback, useEffect, useRef } from 'react';

let reqCounter = 0;

function sendRequest(type: string, data: object = {}): string {
    const reqId = `req-${++reqCounter}-${Date.now()}`;
    window.parent.postMessage({
        __diaFromFrame: true,
        payload: { type, data, reqId },
    }, '*');
    return reqId;
}

type ResponseCallback = (result: unknown) => void;

export function useBridgeRequest() {
    const pendingRef = useRef<Map<string, ResponseCallback>>(new Map());

    useEffect(() => {
        const handler = (event: MessageEvent) => {
            const data = event.data;
            if (!data?.__dia) return;
            if (data.topic !== 'response' && !data.reqId) return;

            const reqId = data.reqId as string;
            const callback = pendingRef.current.get(reqId);
            if (callback) {
                pendingRef.current.delete(reqId);
                callback(data.result);
            }
        };
        window.addEventListener('message', handler);
        return () => window.removeEventListener('message', handler);
    }, []);

    const request = useCallback((type: string, data: object = {}): Promise<unknown> => {
        return new Promise(resolve => {
            const reqId = sendRequest(type, data);
            pendingRef.current.set(reqId, resolve);
            setTimeout(() => {
                if (pendingRef.current.has(reqId)) {
                    pendingRef.current.delete(reqId);
                    resolve(null);
                }
            }, 10000);
        });
    }, []);

    return { request };
}

export function sendToPlugin(type: string, data: object = {}): void {
    window.parent.postMessage({
        __diaFromFrame: true,
        payload: { type, data },
    }, '*');
}
