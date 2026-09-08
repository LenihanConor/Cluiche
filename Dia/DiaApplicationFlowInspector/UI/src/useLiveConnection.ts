import { useState, useEffect } from 'react';

export type LiveConnectionState = 'disconnected' | 'connecting' | 'connected';

/**
 * Tracks the connection state for a Dia editor panel.
 *
 * On mount, fires a `<prefix>.get_connection_state` request and seeds the state
 * from the response. Then listens for pushed `<prefix>.connection_state` messages
 * arriving via the postMessage bridge envelope `{ __dia: true, topic, data }`.
 *
 * @param prefix          The plugin's topic prefix, e.g. `"app_flow_inspector"`.
 * @param bridgeRequestFn The bridge request function (matches `bridge.ts` signature).
 */
export function useLiveConnection(
    prefix: string,
    bridgeRequestFn: (type: string, data: object) => Promise<unknown>,
): LiveConnectionState {
    const [state, setState] = useState<LiveConnectionState>('disconnected');

    // Seed state from a one-shot request on mount — handles the case where the
    // game was already connected before this panel was opened.
    useEffect(() => {
        bridgeRequestFn(`${prefix}.get_connection_state`, {}).then((result: unknown) => {
            const r = result as { connected?: boolean } | null;
            if (r?.connected === true) {
                setState('connected');
            }
        });
    }, [prefix, bridgeRequestFn]);

    // Listen for pushed connection-state changes via the __dia postMessage envelope.
    useEffect(() => {
        const onMessage = (e: MessageEvent) => {
            const env = e.data;
            if (env && env.__dia === true && env.topic === `${prefix}.connection_state`) {
                const d = env.data as { connected?: boolean } | null;
                setState(d?.connected === true ? 'connected' : 'disconnected');
            }
        };
        window.addEventListener('message', onMessage);
        return () => window.removeEventListener('message', onMessage);
    }, [prefix]);

    return state;
}
