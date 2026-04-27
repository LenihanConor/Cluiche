import { useReducer, useEffect } from 'react';
import { pipelineReducer } from '../state/pipelineReducer';
import { initialPipelineState } from '../state/types';
import type { PipelinePayload } from '../state/types';

interface BridgeMessage {
    __dia?: boolean;
    topic?: string;
    data?: unknown;
}

export function usePipelineEvents() {
    const [state, dispatch] = useReducer(pipelineReducer, initialPipelineState);

    useEffect(() => {
        const handler = (event: MessageEvent<BridgeMessage>) => {
            if (!event.data?.__dia) return;
            const { topic, data } = event.data;

            if (topic === 'pipeline.event') {
                const payload = data as PipelinePayload;
                if (payload.events?.length > 0) {
                    dispatch({ type: 'PROCESS_EVENTS', events: payload.events });
                }
                if (payload.summary) {
                    dispatch({ type: 'UPDATE_SUMMARY', summary: payload.summary });
                }
            }
        };
        window.addEventListener('message', handler);
        return () => window.removeEventListener('message', handler);
    }, []);

    return { state, dispatch };
}
