import { describe, it, expect } from 'vitest';
import { pipelineReducer } from './pipelineReducer';
import { initialPipelineState } from './types';
import type { HistoryRun } from './types';

const sampleHistory: HistoryRun[] = [
    { target: 'googletest', config: 'Debug', passCount: 5, failCount: 0, totalDurationMs: 3000, startTimestamp: 1000, interrupted: false },
    { target: 'cluichetest', config: 'Release', passCount: 2, failCount: 1, totalDurationMs: 5000, startTimestamp: 900, interrupted: false },
];

describe('pipelineReducer — history actions', () => {
    it('SET_HISTORY populates historyRuns', () => {
        const state = pipelineReducer(initialPipelineState, {
            type: 'SET_HISTORY',
            runs: sampleHistory,
        });
        expect(state.historyRuns).toHaveLength(2);
        expect(state.historyRuns[0].target).toBe('googletest');
    });

    it('VIEW_HISTORY sets viewingHistoryIndex', () => {
        const stateWithHistory = {
            ...initialPipelineState,
            historyRuns: sampleHistory,
        };
        const state = pipelineReducer(stateWithHistory, {
            type: 'VIEW_HISTORY',
            index: 1,
        });
        expect(state.viewingHistoryIndex).toBe(1);
    });

    it('CLEAR_HISTORY_VIEW resets viewingHistoryIndex to null', () => {
        const stateViewing = {
            ...initialPipelineState,
            historyRuns: sampleHistory,
            viewingHistoryIndex: 1 as number | null,
        };
        const state = pipelineReducer(stateViewing, {
            type: 'CLEAR_HISTORY_VIEW',
        });
        expect(state.viewingHistoryIndex).toBeNull();
    });

    it('OnRunStarted preserves historyRuns but clears viewingHistoryIndex', () => {
        const stateWithHistory = {
            ...initialPipelineState,
            historyRuns: sampleHistory,
            viewingHistoryIndex: 0 as number | null,
        };
        const state = pipelineReducer(stateWithHistory, {
            type: 'PROCESS_EVENTS',
            events: [{
                event: 'OnRunStarted',
                system: 'pipeline',
                stage: '',
                step: '',
                ts: 2000,
                durationMs: -1,
                detail: 'googletest',
            }],
        });
        expect(state.historyRuns).toHaveLength(2);
        expect(state.viewingHistoryIndex).toBeNull();
        expect(state.runInProgress).toBe(true);
    });

    it('SET_HISTORY replaces previous history', () => {
        const stateWithHistory = {
            ...initialPipelineState,
            historyRuns: sampleHistory,
        };
        const newHistory: HistoryRun[] = [
            { target: 'newtest', config: 'Debug', passCount: 1, failCount: 0, totalDurationMs: 100, startTimestamp: 2000, interrupted: false },
        ];
        const state = pipelineReducer(stateWithHistory, {
            type: 'SET_HISTORY',
            runs: newHistory,
        });
        expect(state.historyRuns).toHaveLength(1);
        expect(state.historyRuns[0].target).toBe('newtest');
    });
});
