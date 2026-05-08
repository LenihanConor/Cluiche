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

const withRunningStage: PipelineState = {
    ...initialPipelineState,
    runInProgress: true,
    stages: [{
        name: 'compile-code',
        status: 'running',
        durationMs: 0,
        startTimestamp: 1000,
        logLines: [],
        steps: [],
        expanded: false,
    }],
};

describe('pipelineReducer step events', () => {
    it('OnStepStarted creates step with running status', () => {
        const result = pipelineReducer(withRunningStage, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStepStarted', stage: 'compile-code', step: 'msbuild', ts: 1001 })],
        });
        expect(result.stages[0].steps).toHaveLength(1);
        expect(result.stages[0].steps[0].name).toBe('msbuild');
        expect(result.stages[0].steps[0].status).toBe('running');
        expect(result.stages[0].steps[0].startTimestamp).toBe(1001);
    });

    it('OnStepCompleted marks step as passed with durationMs', () => {
        const withStep: PipelineState = {
            ...withRunningStage,
            stages: [{
                ...withRunningStage.stages[0],
                steps: [{ name: 'msbuild', status: 'running', durationMs: 0, startTimestamp: 1000 }],
            }],
        };
        const result = pipelineReducer(withStep, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStepCompleted', stage: 'compile-code', step: 'msbuild', durationMs: 4200 })],
        });
        expect(result.stages[0].steps[0].status).toBe('passed');
        expect(result.stages[0].steps[0].durationMs).toBe(4200);
    });

    it('OnStepFailed marks step as failed', () => {
        const withStep: PipelineState = {
            ...withRunningStage,
            stages: [{
                ...withRunningStage.stages[0],
                steps: [{ name: 'msbuild', status: 'running', durationMs: 0, startTimestamp: 1000 }],
            }],
        };
        const result = pipelineReducer(withStep, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStepFailed', stage: 'compile-code', step: 'msbuild', durationMs: 2000 })],
        });
        expect(result.stages[0].steps[0].status).toBe('failed');
        expect(result.stages[0].steps[0].durationMs).toBe(2000);
    });

    it('OnStepStarted for nonexistent stage creates the stage first', () => {
        const state: PipelineState = { ...initialPipelineState, runInProgress: true };
        const result = pipelineReducer(state, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStepStarted', stage: 'compile-code', step: 'protobuf', ts: 1002 })],
        });
        expect(result.stages).toHaveLength(1);
        expect(result.stages[0].name).toBe('compile-code');
        expect(result.stages[0].steps[0].name).toBe('protobuf');
    });

    it('multiple steps within same stage accumulate', () => {
        const result = pipelineReducer(withRunningStage, {
            type: 'PROCESS_EVENTS',
            events: [
                mkEvent({ event: 'OnStepStarted', stage: 'compile-code', step: 'protobuf', ts: 1001 }),
                mkEvent({ event: 'OnStepCompleted', stage: 'compile-code', step: 'protobuf', durationMs: 500 }),
                mkEvent({ event: 'OnStepStarted', stage: 'compile-code', step: 'msbuild', ts: 1002 }),
            ],
        });
        expect(result.stages[0].steps).toHaveLength(2);
        expect(result.stages[0].steps[0].name).toBe('protobuf');
        expect(result.stages[0].steps[0].status).toBe('passed');
        expect(result.stages[0].steps[1].name).toBe('msbuild');
        expect(result.stages[0].steps[1].status).toBe('running');
    });

    it('step failure does not change stage status', () => {
        const result = pipelineReducer(withRunningStage, {
            type: 'PROCESS_EVENTS',
            events: [
                mkEvent({ event: 'OnStepStarted', stage: 'compile-code', step: 'msbuild', ts: 1001 }),
                mkEvent({ event: 'OnStepFailed', stage: 'compile-code', step: 'msbuild', durationMs: 800 }),
            ],
        });
        // Stage is still 'running' — only OnStageFailed changes stage status
        expect(result.stages[0].status).toBe('running');
        expect(result.stages[0].steps[0].status).toBe('failed');
    });

    it('OnStepCompleted with durationMs=-1 preserves existing durationMs', () => {
        const withStep: PipelineState = {
            ...withRunningStage,
            stages: [{
                ...withRunningStage.stages[0],
                steps: [{ name: 'msbuild', status: 'running', durationMs: 999, startTimestamp: 1000 }],
            }],
        };
        const result = pipelineReducer(withStep, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnStepCompleted', stage: 'compile-code', step: 'msbuild', durationMs: -1 })],
        });
        expect(result.stages[0].steps[0].durationMs).toBe(999);
    });

    it('UPDATE_SUMMARY interrupted marks running steps as interrupted', () => {
        const withRunningStep: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            stages: [{
                name: 'compile-code',
                status: 'running',
                durationMs: 0,
                startTimestamp: 1000,
                logLines: [],
                steps: [
                    { name: 'msbuild', status: 'running', durationMs: 0, startTimestamp: 1001 },
                    { name: 'protobuf', status: 'passed', durationMs: 500, startTimestamp: 1000 },
                ],
                expanded: false,
            }],
        };
        const result = pipelineReducer(withRunningStep, {
            type: 'UPDATE_SUMMARY',
            summary: { target: 'googletest', config: 'Debug', passCount: 0, failCount: 0, totalDurationMs: 2000, interrupted: true, runInProgress: false },
        });
        expect(result.stages[0].status).toBe('interrupted');
        expect(result.stages[0].steps[0].status).toBe('interrupted');
        expect(result.stages[0].steps[1].status).toBe('passed');
    });

    it('OnRunStarted clears steps from previous run', () => {
        const withStep: PipelineState = {
            ...initialPipelineState,
            stages: [{
                name: 'compile-code',
                status: 'passed',
                durationMs: 4000,
                startTimestamp: 0,
                logLines: [],
                steps: [{ name: 'msbuild', status: 'passed', durationMs: 4000, startTimestamp: 0 }],
                expanded: false,
            }],
        };
        const result = pipelineReducer(withStep, {
            type: 'PROCESS_EVENTS',
            events: [mkEvent({ event: 'OnRunStarted' })],
        });
        expect(result.stages).toHaveLength(0);
    });
});
