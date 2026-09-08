// useBridge.ts — unified bridge hooks for Dia editor plugins
// Works in two execution contexts:
//   Main frame (CluicheEditor plugins): window.CluicheEditor.subscribe / .request
//   Iframe (Dia plugin iframes): window.addEventListener('message', ...) for events;
//                                window.parent.postMessage relay for requests

import { useEffect, useRef, useCallback } from 'react';

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

export interface BridgeSubscription {
    topic: string
    handler: (data: unknown) => void
}

/**
 * Subscribe to one or more bridge topics; auto-unsubscribes on unmount.
 *
 * Binding: DEUI-003 — subscriptions are torn down when the component unmounts.
 *
 * Safe to call with an empty array.
 */
export function useBridgeSubscribe(subscriptions: BridgeSubscription[]): void {
    // Keep a stable ref to the latest handlers so the effect does not need to
    // re-run when only a callback reference changes (e.g. an inline arrow).
    const handlersRef = useRef<BridgeSubscription[]>(subscriptions);
    handlersRef.current = subscriptions;

    // Build a stable dep key from topic names only — re-subscribe only when
    // the set of topics changes, not on every render.
    const topicKey = subscriptions.map((s) => s.topic).join('\0');

    useEffect(() => {
        const subs = handlersRef.current;
        if (subs.length === 0) return;

        const mainFrame = (window as any).CluicheEditor;

        if (mainFrame?.subscribe) {
            // ----------------------------------------------------------------
            // Main-frame path: CluicheEditor.subscribe returns an unsubscribe fn
            // ----------------------------------------------------------------
            const unsubscribers = subs.map((sub, idx) =>
                mainFrame.subscribe(sub.topic, (data: unknown) => {
                    // Delegate to the latest handler via ref (avoids stale closure).
                    // Use the original index so that duplicate topics each invoke
                    // their own handler independently.
                    const current = handlersRef.current[idx];
                    if (current) current.handler(data);
                })
            );
            return () => {
                unsubscribers.forEach((unsub: () => void) => unsub());
            };
        } else {
            // ----------------------------------------------------------------
            // Iframe path: host page broadcasts { __dia: true, topic, data }
            // ----------------------------------------------------------------
            const onMessage = (e: MessageEvent) => {
                const env = e.data;
                if (env && env.__dia === true && typeof env.topic === 'string') {
                    // Fire all handlers whose topic matches (supports duplicate topics).
                    handlersRef.current
                        .filter((s) => s.topic === env.topic)
                        .forEach((s) => s.handler(env.data));
                }
            };
            window.addEventListener('message', onMessage);
            return () => window.removeEventListener('message', onMessage);
        }
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [topicKey]);
}

// ---------------------------------------------------------------------------

const REQUEST_TIMEOUT_MS = 5000;

/**
 * Returns a stable async function that sends a request to the C++ bridge and
 * resolves with a typed response.
 *
 * Main frame: delegates to window.CluicheEditor.request<T>()
 * Iframe:     posts { __diaFromFrame, payload: { type, reqId, data } } to
 *             window.parent and awaits { __diaResponse: true, reqId, result }.
 */
export function useBridgeRequest<T = unknown>(): (topic: string, payload?: unknown) => Promise<T> {
    return useCallback(
        (topic: string, payload?: unknown): Promise<T> => {
            const mainFrame = (window as any).CluicheEditor;

            if (mainFrame?.request) {
                // Main-frame path: delegate directly
                return (mainFrame.request(topic, payload) as Promise<T>);
            }

            // Iframe path: postMessage relay
            return new Promise<T>((resolve, reject) => {
                const reqId = `req_${Date.now()}_${Math.random().toString(36).slice(2)}`;

                const handler = (e: MessageEvent) => {
                    const p = e.data?.payload ?? e.data;
                    // Support both the existing DiaApplicationFlowInspector
                    // response envelope and a minimal { __diaResponse, reqId, result } envelope.
                    const matchesLegacy =
                        p?.type === 'DiaEditor_onResponse' && p?.reqId === reqId;
                    const matchesNew =
                        e.data?.__diaResponse === true && e.data?.reqId === reqId;

                    if (matchesLegacy || matchesNew) {
                        clearTimeout(timer);
                        window.removeEventListener('message', handler);
                        resolve((p?.result ?? e.data?.result) as T);
                    }
                };

                const timer = setTimeout(() => {
                    window.removeEventListener('message', handler);
                    reject(new Error(`useBridgeRequest: '${topic}' timed out after ${REQUEST_TIMEOUT_MS}ms`));
                }, REQUEST_TIMEOUT_MS);

                window.addEventListener('message', handler);
                window.parent.postMessage(
                    { __diaFromFrame: true, payload: { type: topic, reqId, data: payload ?? {} } },
                    '*'
                );
            });
        },
        [] // stable — no deps; bridge detection is done at call time
    );
}
