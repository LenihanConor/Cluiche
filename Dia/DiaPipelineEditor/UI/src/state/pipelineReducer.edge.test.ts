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

describe('pipelineReducer edge cases', () => {
    it('OnLogLine for nonexistent stage creates it', () => {
        const state: PipelineState = { ...initialPipelineState, runInProgress: true };

        const result = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnLogLine', stage: 'new-stage', detail: 'hello', level: 'info' })],
        });

        expect(result.stages).toHaveLength(1);
        expect(result.stages[0].name).toBe('new-stage');
        expect(result.stages[0].status).toBe('not-started');
        expect(result.stages[0].logLines).toHaveLength(1);
        expect(result.stages[0].logLines[0].message).toBe('hello');
    });

    it('OnStageCompleted for nonexistent stage creates it', () => {
        const state: PipelineState = { ...initialPipelineState, runInProgress: true };

        const result = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStageCompleted', stage: 'phantom', durationMs: 200 })],
        });

        expect(result.stages).toHaveLength(1);
        expect(result.stages[0].name).toBe('phantom');
        expect(result.stages[0].status).toBe('passed');
        expect(result.stages[0].durationMs).toBe(200);
    });

    it('multiple PROCESS_EVENTS calls accumulate stages', () => {
        let state = initialPipelineState;

        state = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnRunStarted', detail: 'test' })],
        });

        state = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStageStarted', stage: 'stage-a', ts: 1001 })],
        });

        state = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStageStarted', stage: 'stage-b', ts: 1002 })],
        });

        expect(state.stages).toHaveLength(2);
        expect(state.stages[0].name).toBe('stage-a');
        expect(state.stages[1].name).toBe('stage-b');
    });

    it('UPDATE_SUMMARY with empty target preserves existing target', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            target: 'original-target',
            config: 'Debug',
            runInProgress: true,
        };

        const result = pipelineReducer(state, {
            type: 'UPDATE_SUMMARY',
            summary: { target: '', config: '', passCount: 1, failCount: 0, totalDurationMs: 500, interrupted: false, runInProgress: true },
        });

        expect(result.target).toBe('original-target');
        expect(result.config).toBe('Debug');
        expect(result.passCount).toBe(1);
    });

    it('UPDATE_SUMMARY with non-empty target overwrites', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            target: 'original-target',
            runInProgress: true,
        };

        const result = pipelineReducer(state, {
            type: 'UPDATE_SUMMARY',
            summary: { target: 'new-target', config: 'Release', passCount: 0, failCount: 0, totalDurationMs: 0, interrupted: false, runInProgress: true },
        });

        expect(result.target).toBe('new-target');
        expect(result.config).toBe('Release');
    });

    it('OnLogLine uses error field as message when detail is absent', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            stages: [{ name: 'compile', status: 'running', durationMs: 0, startTimestamp: 1000, logLines: [], steps: [], expanded: false }],
        };

        const result = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnLogLine', stage: 'compile', error: 'build failed', level: 'error' })],
        });

        expect(result.stages[0].logLines[0].message).toBe('build failed');
    });

    it('OnLogLine defaults level to info when absent', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            stages: [{ name: 'compile', status: 'running', durationMs: 0, startTimestamp: 1000, logLines: [], steps: [], expanded: false }],
        };

        const result = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnLogLine', stage: 'compile', detail: 'some output' })],
        });

        expect(result.stages[0].logLines[0].level).toBe('info');
    });

    it('OnStageCompleted with durationMs=-1 preserves existing durationMs', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            stages: [{ name: 'compile', status: 'running', durationMs: 999, startTimestamp: 1000, logLines: [], steps: [], expanded: false }],
        };

        const result = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStageCompleted', stage: 'compile', durationMs: -1 })],
        });

        expect(result.stages[0].durationMs).toBe(999);
    });

    it('empty events array is a no-op', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            target: 'unchanged',
            stages: [{ name: 'x', status: 'passed', durationMs: 100, startTimestamp: 0, logLines: [], steps: [], expanded: false }],
        };

        const result = pipelineReducer(state, { type: 'PROCESS_EVENTS', events: [] });
        expect(result).toEqual(state);
    });
});
