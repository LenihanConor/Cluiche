import { useReducer, useEffect } from 'react';
import { pipelineReducer } from '../state/pipelineReducer';
import { initialPipelineState } from '../state/types';
import type { PipelinePayload, HistoryRun } from '../state/types';
import { useBridgeRequest } from './useBridgeRequest';

interface BridgeMessage {
    __dia?: boolean;
    topic?: string;
    data?: unknown;
}

export function usePipelineEvents() {
    const [state, dispatch] = useReducer(pipelineReducer, initialPipelineState);
    const { request } = useBridgeRequest();

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

            if (topic === 'pipeline.history') {
                const payload = data as { runs: HistoryRun[] };
                if (payload.runs) {
                    dispatch({ type: 'SET_HISTORY', runs: payload.runs });
                }
            }
        };
        window.addEventListener('message', handler);
        return () => window.removeEventListener('message', handler);
    }, []);

    useEffect(() => {
        request('pipeline.history').then((result) => {
            const data = result as { runs: HistoryRun[] } | null;
            if (data?.runs) {
                dispatch({ type: 'SET_HISTORY', runs: data.runs });
            }
        });
    }, [request]);

    return { state, dispatch };
}
