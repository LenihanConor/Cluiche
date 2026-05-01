import { describe, it, expect } from 'vitest';
import { pipelineReducer } from './pipelineReducer';
import { initialPipelineState } from './types';
import type { PipelineState, PipelineEventData } from './types';

function mkEvent(overrides: Partial<PipelineEventData>): PipelineEventData {
    return {
        event: 'OnLogLine',
        system: 'pipeline',
        stage: '',
        step: '',
        ts: 1000,
        durationMs: -1,
        ...overrides,
    };
}

describe('pipelineReducer', () => {
    it('returns initial state for unknown action', () => {
        const result = pipelineReducer(initialPipelineState, { type: 'PROCESS_EVENTS', events: [] });
        expect(result).toEqual(initialPipelineState);
    });

    // AC10: OnRunStarted resets state
    it('resets state on OnRunStarted', () => {
        const stateWithStages: PipelineState = {
            ...initialPipelineState,
            target: 'old-target',
            stages: [{ name: 'old-stage', status: 'passed', durationMs: 100, startTimestamp: 0, logLines: [], steps: [], expanded: false }],
        };

        const result = pipelineReducer(stateWithStages, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnRunStarted', detail: 'googletest' })],
        });

        expect(result.runInProgress).toBe(true);
        expect(result.target).toBe('googletest');
        expect(result.stages).toHaveLength(0);
    });

    // AC3: stage status icons
    it('sets stage to running on OnStageStarted', () => {
        const result = pipelineReducer(
            { ...initialPipelineState, runInProgress: true },
            { type: 'PROCESS_EVENTS', events: [mkEvent({ event: 'OnStageStarted', stage: 'compile-code', ts: 1001 })] }
        );

        expect(result.stages).toHaveLength(1);
        expect(result.stages[0].name).toBe('compile-code');
        expect(result.stages[0].status).toBe('running');
        expect(result.stages[0].startTimestamp).toBe(1001);
    });

    it('auto-expands stage on OnStageStarted', () => {
        const result = pipelineReducer(
            { ...initialPipelineState, runInProgress: true },
            { type: 'PROCESS_EVENTS', events: [mkEvent({ event: 'OnStageStarted', stage: 'compile-code', ts: 1001 })] }
        );

        expect(result.stages[0].expanded).toBe(true);
    });

    it('auto-collapses stage on OnStageCompleted', () => {
        const withExpanded: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            stages: [{ name: 'compile-code', status: 'running', durationMs: 0, startTimestamp: 1000, logLines: [], steps: [], expanded: true }],
        };

        const result = pipelineReducer(withExpanded, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStageCompleted', stage: 'compile-code', durationMs: 1500 })],
        });

        expect(result.stages[0].expanded).toBe(false);
    });

    it('keeps stage expanded on OnStageFailed', () => {
        const withExpanded: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            stages: [{ name: 'compile-code', status: 'running', durationMs: 0, startTimestamp: 1000, logLines: [], steps: [], expanded: true }],
        };

        const result = pipelineReducer(withExpanded, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStageFailed', stage: 'compile-code', durationMs: 800 })],
        });

        expect(result.stages[0].expanded).toBe(true);
    });

    it('sets stage to passed on OnStageCompleted', () => {
        const withRunning: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            stages: [{ name: 'compile-code', status: 'running', durationMs: 0, startTimestamp: 1000, logLines: [], steps: [], expanded: false }],
        };

        const result = pipelineReducer(withRunning, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStageCompleted', stage: 'compile-code', durationMs: 1500 })],
        });

        expect(result.stages[0].status).toBe('passed');
        expect(result.stages[0].durationMs).toBe(1500);
    });

    it('sets stage to failed on OnStageFailed', () => {
        const withRunning: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            stages: [{ name: 'compile-code', status: 'running', durationMs: 0, startTimestamp: 1000, logLines: [], steps: [], expanded: false }],
        };

        const result = pipelineReducer(withRunning, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStageFailed', stage: 'compile-code', durationMs: 800 })],
        });

        expect(result.stages[0].status).toBe('failed');
        expect(result.stages[0].durationMs).toBe(800);
    });

    // AC5/AC6: log lines with level colouring
    it('appends log lines to matching stage', () => {
        const withStage: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            stages: [{ name: 'compile-code', status: 'running', durationMs: 0, startTimestamp: 1000, logLines: [], steps: [], expanded: false }],
        };

        const result = pipelineReducer(withStage, {
            type: 'PROCESS_EVENTS',
            events: [
                mkEvent({ event: 'OnLogLine', stage: 'compile-code', detail: 'Building...', level: 'info' }),
                mkEvent({ event: 'OnLogLine', stage: 'compile-code', detail: 'Undeclared identifier', level: 'error' }),
                mkEvent({ event: 'OnLogLine', stage: 'compile-code', detail: 'Unused var', level: 'warn' }),
            ],
        });

        expect(result.stages[0].logLines).toHaveLength(3);
        expect(result.stages[0].logLines[0].level).toBe('info');
        expect(result.stages[0].logLines[0].message).toBe('Building...');
        expect(result.stages[0].logLines[1].level).toBe('error');
        expect(result.stages[0].logLines[2].level).toBe('warn');
    });

    // AC7: run summary data
    it('sets runInProgress=false and totalDurationMs on OnRunCompleted', () => {
        const withRun: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            target: 'googletest',
            config: 'Debug',
        };

        const result = pipelineReducer(withRun, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnRunCompleted', durationMs: 5000 })],
        });

        expect(result.runInProgress).toBe(false);
        expect(result.totalDurationMs).toBe(5000);
    });

    it('handles OnRunFailed same as OnRunCompleted', () => {
        const withRun: PipelineState = { ...initialPipelineState, runInProgress: true };

        const result = pipelineReducer(withRun, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnRunFailed', durationMs: 3000 })],
        });

        expect(result.runInProgress).toBe(false);
        expect(result.totalDurationMs).toBe(3000);
    });

    // AC9: interrupted run indicator
    it('marks running stages as interrupted via UPDATE_SUMMARY', () => {
        const withRunning: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            stages: [
                { name: 'compile-code', status: 'running', durationMs: 0, startTimestamp: 1000, logLines: [], steps: [], expanded: false },
                { name: 'proto-compile', status: 'passed', durationMs: 500, startTimestamp: 990, logLines: [], steps: [], expanded: false },
            ],
        };

        const result = pipelineReducer(withRunning, {
            type: 'UPDATE_SUMMARY',
            summary: { target: 'googletest', config: 'Debug', passCount: 1, failCount: 0, totalDurationMs: 2000, interrupted: true, runInProgress: false },
        });

        expect(result.interrupted).toBe(true);
        expect(result.stages[0].status).toBe('interrupted');
        expect(result.stages[1].status).toBe('passed');
    });

    // TOGGLE_STAGE
    it('toggles stage expanded state', () => {
        const withStage: PipelineState = {
            ...initialPipelineState,
            stages: [{ name: 'compile-code', status: 'passed', durationMs: 100, startTimestamp: 0, logLines: [], steps: [], expanded: false }],
        };

        const result = pipelineReducer(withStage, { type: 'TOGGLE_STAGE', stageName: 'compile-code' });
        expect(result.stages[0].expanded).toBe(true);

        const result2 = pipelineReducer(result, { type: 'TOGGLE_STAGE', stageName: 'compile-code' });
        expect(result2.stages[0].expanded).toBe(false);
    });

    // AC10: new run clears previous state
    it('processes two sequential runs correctly', () => {
        let state = initialPipelineState;
        state = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [
                mkEvent({ event: 'OnRunStarted', detail: 'run1' }),
                mkEvent({ event: 'OnStageStarted', stage: 'stage-a' }),
                mkEvent({ event: 'OnStageCompleted', stage: 'stage-a', durationMs: 100 }),
                mkEvent({ event: 'OnRunCompleted', durationMs: 200 }),
            ],
        });

        expect(state.stages).toHaveLength(1);
        expect(state.stages[0].name).toBe('stage-a');

        state = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [
                mkEvent({ event: 'OnRunStarted', detail: 'run2' }),
                mkEvent({ event: 'OnStageStarted', stage: 'stage-b' }),
            ],
        });

        expect(state.target).toBe('run2');
        expect(state.stages).toHaveLength(1);
        expect(state.stages[0].name).toBe('stage-b');
    });

    // AC8: empty state detection is in PipelinePanel component, not reducer
    // But we verify the initial state here
    it('has no stages in initial state', () => {
        expect(initialPipelineState.stages).toHaveLength(0);
        expect(initialPipelineState.runInProgress).toBe(false);
        expect(initialPipelineState.target).toBe('');
    });

    it('ignores unknown event types gracefully', () => {
        const state = { ...initialPipelineState, runInProgress: true };
        const result = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnUnknownEvent' })],
        });
        expect(result).toEqual(state);
    });
});
