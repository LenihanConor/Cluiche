import type { PipelineState, PipelineEventData, StageState, StepState, LogLine, HistoryRun } from './types';
import { initialPipelineState } from './types';

export type PipelineAction =
    | { type: 'PROCESS_EVENTS'; events: PipelineEventData[] }
    | { type: 'UPDATE_SUMMARY'; summary: { target: string; config: string; passCount: number; failCount: number; totalDurationMs: number; interrupted: boolean; runInProgress: boolean } }
    | { type: 'TOGGLE_STAGE'; stageName: string }
    | { type: 'SET_HISTORY'; runs: HistoryRun[] }
    | { type: 'VIEW_HISTORY'; index: number }
    | { type: 'CLEAR_HISTORY_VIEW' }
    | { type: 'SET_STAGE_MANIFEST'; stages: string[] }
    | { type: 'SET_PROJECT_STATE'; isValid: boolean; diagameName: string }
    | { type: 'RECORD_RUN'; run: HistoryRun & { stageDurationsMs: Record<string, number> } };

function findOrCreateStage(stages: StageState[], name: string): StageState[] {
    if (stages.some(s => s.name === name)) return stages;
    return [...stages, {
        name,
        status: 'not-started',
        durationMs: 0,
        startTimestamp: 0,
        logLines: [],
        steps: [],
        expanded: false,
    }];
}

function updateStage(stages: StageState[], name: string, updater: (s: StageState) => StageState): StageState[] {
    return stages.map(s => s.name === name ? updater(s) : s);
}

function findOrCreateStep(steps: StepState[], name: string): StepState[] {
    if (steps.some(s => s.name === name)) return steps;
    return [...steps, { name, status: 'not-started', durationMs: 0, startTimestamp: 0 }];
}

function updateStep(steps: StepState[], name: string, updater: (s: StepState) => StepState): StepState[] {
    return steps.map(s => s.name === name ? updater(s) : s);
}

function processEvent(state: PipelineState, evt: PipelineEventData): PipelineState {
    switch (evt.event) {
        case 'OnRunStarted':
            return {
                ...initialPipelineState,
                historyRuns: state.historyRuns,
                viewingHistoryIndex: null,
                runInProgress: true,
                target: evt.detail ?? '',
                config: '',
                stageManifest: state.stageManifest,
                stageDurationsMs: state.stageDurationsMs,
                lastSuccessTimestamp: null,
                isProjectLoaded: state.isProjectLoaded,
                diagameName: state.diagameName,
                canLaunch: false,
                stages: state.stageManifest.map(name => ({
                    name,
                    status: 'not-started' as const,
                    durationMs: 0,
                    startTimestamp: 0,
                    logLines: [],
                    steps: [],
                    expanded: false,
                })),
            };

        case 'OnStageStarted': {
            const stages = findOrCreateStage(state.stages, evt.stage);
            return {
                ...state,
                stages: updateStage(stages, evt.stage, s => ({
                    ...s,
                    status: 'running',
                    startTimestamp: evt.ts,
                    expanded: true,
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
                    expanded: false,
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
                    expanded: true,
                })),
            };
        }

        case 'OnStepStarted': {
            const stages = findOrCreateStage(state.stages, evt.stage);
            return {
                ...state,
                stages: updateStage(stages, evt.stage, s => ({
                    ...s,
                    steps: updateStep(findOrCreateStep(s.steps, evt.step), evt.step, st => ({
                        ...st,
                        status: 'running',
                        startTimestamp: evt.ts,
                    })),
                })),
            };
        }

        case 'OnStepCompleted': {
            const stages = findOrCreateStage(state.stages, evt.stage);
            return {
                ...state,
                stages: updateStage(stages, evt.stage, s => ({
                    ...s,
                    steps: updateStep(findOrCreateStep(s.steps, evt.step), evt.step, st => ({
                        ...st,
                        status: 'passed',
                        durationMs: evt.durationMs >= 0 ? evt.durationMs : st.durationMs,
                    })),
                })),
            };
        }

        case 'OnStepFailed': {
            const stages = findOrCreateStage(state.stages, evt.stage);
            return {
                ...state,
                stages: updateStage(stages, evt.stage, s => ({
                    ...s,
                    expanded: true,
                    steps: updateStep(findOrCreateStep(s.steps, evt.step), evt.step, st => ({
                        ...st,
                        status: 'failed',
                        durationMs: evt.durationMs >= 0 ? evt.durationMs : st.durationMs,
                        inlineError: evt.error ?? null,
                    })),
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
            // Track last error line per step for inline display
            const updatedStages = updateStage(stages, evt.stage, s => {
                const updatedSteps = (evt.level === 'error' && evt.step)
                    ? updateStep(s.steps, evt.step, st => ({
                        ...st,
                        inlineError: evt.detail ?? evt.error ?? null,
                    }))
                    : s.steps;
                return {
                    ...s,
                    logLines: [...s.logLines, line],
                    steps: updatedSteps,
                };
            });
            return { ...state, stages: updatedStages };
        }

        case 'OnRunCompleted':
            return {
                ...state,
                runInProgress: false,
                totalDurationMs: evt.durationMs >= 0 ? evt.durationMs : state.totalDurationMs,
                canLaunch: true,
                lastSuccessTimestamp: Date.now(),
            };

        case 'OnRunFailed':
            return {
                ...state,
                runInProgress: false,
                totalDurationMs: evt.durationMs >= 0 ? evt.durationMs : state.totalDurationMs,
                canLaunch: false,
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
                    st.status === 'running'
                        ? {
                            ...st,
                            status: 'interrupted' as const,
                            steps: st.steps.map(sp =>
                                sp.status === 'running' ? { ...sp, status: 'interrupted' as const } : sp
                            ),
                        }
                        : st
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

        case 'SET_HISTORY':
            return {
                ...state,
                historyRuns: action.runs,
            };

        case 'VIEW_HISTORY':
            return {
                ...state,
                viewingHistoryIndex: action.index,
            };

        case 'CLEAR_HISTORY_VIEW':
            return {
                ...state,
                viewingHistoryIndex: null,
            };

        case 'SET_STAGE_MANIFEST':
            return {
                ...state,
                stageManifest: action.stages,
                stages: action.stages.map(name => ({
                    name,
                    status: 'not-started' as const,
                    durationMs: 0,
                    startTimestamp: 0,
                    logLines: [],
                    steps: [],
                    expanded: false,
                })),
            };

        case 'SET_PROJECT_STATE':
            return {
                ...state,
                isProjectLoaded: action.isValid,
                diagameName: action.diagameName,
                canLaunch: false,
                lastSuccessTimestamp: null,
                stageManifest: action.isValid ? state.stageManifest : [],
                stages: action.isValid ? state.stages : [],
            };

        case 'RECORD_RUN': {
            const newRun: HistoryRun = {
                target: action.run.target,
                config: action.run.config,
                passCount: action.run.passCount,
                failCount: action.run.failCount,
                totalDurationMs: action.run.totalDurationMs,
                startTimestamp: action.run.startTimestamp,
                interrupted: action.run.interrupted,
                stageDurationsMs: action.run.stageDurationsMs,
            };
            const runs = [newRun, ...state.historyRuns].slice(0, 5);
            // Build new stageDurationsMs from this latest successful run
            const stageDurationsMs = action.run.failCount === 0 && !action.run.interrupted
                ? { ...action.run.stageDurationsMs }
                : state.stageDurationsMs;
            return {
                ...state,
                historyRuns: runs,
                stageDurationsMs,
            };
        }

        default:
            return state;
    }
}
