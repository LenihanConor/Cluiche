import { useReducer, useEffect, useRef } from 'react';
import { pipelineReducer } from '../state/pipelineReducer';
import { initialPipelineState } from '../state/types';
import type { PipelinePayload } from '../state/types';
import { useBridgeRequest } from './useBridgeRequest';

interface BridgeMessage {
    __dia?: boolean;
    topic?: string;
    data?: unknown;
}

export function usePipelineEvents() {
    const [state, dispatch] = useReducer(pipelineReducer, initialPipelineState);
    const { request } = useBridgeRequest();
    const stateRef = useRef(state);

    useEffect(() => {
        stateRef.current = state;
    }, [state]);

    useEffect(() => {
        const handler = (event: MessageEvent<BridgeMessage>) => {
            if (!event.data?.__dia) return;
            const { topic, data } = event.data;

            if (topic === 'pipeline.event') {
                const payload = data as PipelinePayload;
                if (payload.events?.length > 0) {
                    dispatch({ type: 'PROCESS_EVENTS', events: payload.events });

                    // Check for run completion events to record into session history
                    for (const evt of payload.events) {
                        if (evt.event === 'OnRunCompleted' || evt.event === 'OnRunFailed') {
                            const stageDurationsMs: Record<string, number> = {};
                            for (const stage of stateRef.current.stages) {
                                if (stage.durationMs > 0) {
                                    stageDurationsMs[stage.name] = stage.durationMs;
                                }
                            }
                            if (payload.summary) {
                                dispatch({
                                    type: 'RECORD_RUN',
                                    run: { ...payload.summary, stageDurationsMs },
                                });
                            }
                        }
                    }
                }
                if (payload.summary) {
                    dispatch({ type: 'UPDATE_SUMMARY', summary: payload.summary });

                    // Also record run when summary arrives with runInProgress === false
                    if (payload.summary.runInProgress === false && !payload.events?.some(
                        e => e.event === 'OnRunCompleted' || e.event === 'OnRunFailed'
                    )) {
                        const stageDurationsMs: Record<string, number> = {};
                        for (const stage of stateRef.current.stages) {
                            if (stage.durationMs > 0) {
                                stageDurationsMs[stage.name] = stage.durationMs;
                            }
                        }
                        dispatch({
                            type: 'RECORD_RUN',
                            run: { ...payload.summary, stageDurationsMs },
                        });
                    }
                }
            }

            if (topic === 'pipeline.project_changed') {
                const payload = data as { isValid: boolean; diagameName: string; target?: string };
                dispatch({ type: 'SET_PROJECT_STATE', isValid: payload.isValid, diagameName: payload.diagameName });
                if (payload.isValid) {
                    request('pipeline.get_target_stages').then((result) => {
                        const stagesData = result as { stages: string[] } | null;
                        if (stagesData?.stages) {
                            dispatch({ type: 'SET_STAGE_MANIFEST', stages: stagesData.stages });
                        }
                    });
                }
            }
        };
        window.addEventListener('message', handler);
        return () => window.removeEventListener('message', handler);
    }, [request]);

    useEffect(() => {
        request('pipeline.get_project_state').then((result) => {
            const data = result as { isValid: boolean; diagameName: string } | null;
            if (data) {
                dispatch({ type: 'SET_PROJECT_STATE', isValid: data.isValid, diagameName: data.diagameName });
                if (data.isValid) {
                    request('pipeline.get_target_stages').then((stagesResult) => {
                        const stagesData = stagesResult as { stages: string[] } | null;
                        if (stagesData?.stages) {
                            dispatch({ type: 'SET_STAGE_MANIFEST', stages: stagesData.stages });
                        }
                    });
                }
            }
        });
    }, [request]);

    return { state, dispatch };
}
