import type { PipelineState, PipelineEventData, StageState, LogLine } from './types';
import { initialPipelineState } from './types';

export type PipelineAction =
    | { type: 'PROCESS_EVENTS'; events: PipelineEventData[] }
    | { type: 'UPDATE_SUMMARY'; summary: { target: string; config: string; passCount: number; failCount: number; totalDurationMs: number; interrupted: boolean; runInProgress: boolean } }
    | { type: 'TOGGLE_STAGE'; stageName: string };

function findOrCreateStage(stages: StageState[], name: string): StageState[] {
    if (stages.some(s => s.name === name)) return stages;
    return [...stages, {
        name,
        status: 'not-started',
        durationMs: 0,
        startTimestamp: 0,
        logLines: [],
        expanded: false,
    }];
}

function updateStage(stages: StageState[], name: string, updater: (s: StageState) => StageState): StageState[] {
    return stages.map(s => s.name === name ? updater(s) : s);
}

function processEvent(state: PipelineState, evt: PipelineEventData): PipelineState {
    switch (evt.event) {
        case 'OnRunStarted':
            return {
                ...initialPipelineState,
                runInProgress: true,
                target: evt.detail ?? '',
                config: '',
                stages: [],
            };

        case 'OnStageStarted': {
            const stages = findOrCreateStage(state.stages, evt.stage);
            return {
                ...state,
                stages: updateStage(stages, evt.stage, s => ({
                    ...s,
                    status: 'running',
                    startTimestamp: evt.ts,
                })),
            };
        }

        case 'OnStageCompleted': {
            const stages = findOrCreateStage(state.stages, evt.stage);
            return {
                ...state,
                stages: updateStage(stages, evt.stage, s => ({
                    ...s,
                    status: 'passed',
                    durationMs: evt.durationMs >= 0 ? evt.durationMs : s.durationMs,
                })),
            };
        }

        case 'OnStageFailed': {
            const stages = findOrCreateStage(state.stages, evt.stage);
            return {
                ...state,
                stages: updateStage(stages, evt.stage, s => ({
                    ...s,
                    status: 'failed',
                    durationMs: evt.durationMs >= 0 ? evt.durationMs : s.durationMs,
                })),
            };
        }

        case 'OnLogLine': {
            const stages = findOrCreateStage(state.stages, evt.stage);
            const line: LogLine = {
                level: (evt.level as LogLine['level']) ?? 'info',
                message: evt.detail ?? evt.error ?? '',
                timestamp: evt.ts,
            };
            return {
                ...state,
                stages: updateStage(stages, evt.stage, s => ({
                    ...s,
                    logLines: [...s.logLines, line],
                })),
            };
        }

        case 'OnRunCompleted':
            return {
                ...state,
                runInProgress: false,
                totalDurationMs: evt.durationMs >= 0 ? evt.durationMs : state.totalDurationMs,
            };

        case 'OnRunFailed':
            return {
                ...state,
                runInProgress: false,
                totalDurationMs: evt.durationMs >= 0 ? evt.durationMs : state.totalDurationMs,
            };

        default:
            return state;
    }
}

export function pipelineReducer(state: PipelineState, action: PipelineAction): PipelineState {
    switch (action.type) {
        case 'PROCESS_EVENTS': {
            let next = state;
            for (const evt of action.events) {
                next = processEvent(next, evt);
            }
            return next;
        }

        case 'UPDATE_SUMMARY': {
            const s = action.summary;
            let stages = state.stages;
            if (s.interrupted) {
                stages = stages.map(st =>
                    st.status === 'running' ? { ...st, status: 'interrupted' as const } : st
                );
            }
            return {
                ...state,
                target: s.target || state.target,
                config: s.config || state.config,
                passCount: s.passCount,
                failCount: s.failCount,
                totalDurationMs: s.totalDurationMs,
                interrupted: s.interrupted,
                runInProgress: s.runInProgress,
                stages,
            };
        }

        case 'TOGGLE_STAGE':
            return {
                ...state,
                stages: state.stages.map(s =>
                    s.name === action.stageName ? { ...s, expanded: !s.expanded } : s
                ),
            };

        default:
            return state;
    }
}
