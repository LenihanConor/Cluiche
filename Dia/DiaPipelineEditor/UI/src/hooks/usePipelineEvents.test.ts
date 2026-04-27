import { describe, it, expect, vi, afterEach } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { usePipelineEvents } from './usePipelineEvents';
import type { PipelinePayload } from '../state/types';

function sendBridgeMessage(payload: PipelinePayload) {
    window.dispatchEvent(new MessageEvent('message', {
        data: { __dia: true, topic: 'pipeline.event', data: payload },
    }));
}

describe('usePipelineEvents', () => {
    afterEach(() => {
        vi.restoreAllMocks();
    });

    it('starts with initial empty state', () => {
        const { result } = renderHook(() => usePipelineEvents());
        expect(result.current.state.runInProgress).toBe(false);
        expect(result.current.state.stages).toHaveLength(0);
        expect(result.current.state.target).toBe('');
    });

    it('processes pipeline.event messages from bridge', () => {
        const { result } = renderHook(() => usePipelineEvents());

        act(() => {
            sendBridgeMessage({
                events: [
                    { event: 'OnRunStarted', system: 'pipeline', stage: '', step: '', ts: 1000, durationMs: -1, detail: 'googletest' },
                    { event: 'OnStageStarted', system: 'pipeline', stage: 'compile-code', step: '', ts: 1001, durationMs: -1 },
                ],
                summary: { target: 'googletest', config: 'Debug', passCount: 0, failCount: 0, totalDurationMs: 0, startTimestamp: 1000, interrupted: false, runInProgress: true },
            });
        });

        expect(result.current.state.runInProgress).toBe(true);
        expect(result.current.state.target).toBe('googletest');
        expect(result.current.state.stages).toHaveLength(1);
        expect(result.current.state.stages[0].name).toBe('compile-code');
        expect(result.current.state.stages[0].status).toBe('running');
    });

    it('ignores messages without __dia flag', () => {
        const { result } = renderHook(() => usePipelineEvents());

        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { topic: 'pipeline.event', data: { events: [{ event: 'OnRunStarted', system: 'pipeline', stage: '', step: '', ts: 1000, durationMs: -1 }], summary: {} } },
            }));
        });

        expect(result.current.state.runInProgress).toBe(false);
        expect(result.current.state.stages).toHaveLength(0);
    });

    it('ignores messages with wrong topic', () => {
        const { result } = renderHook(() => usePipelineEvents());

        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { __dia: true, topic: 'manifest_loaded', data: {} },
            }));
        });

        expect(result.current.state.runInProgress).toBe(false);
    });

    it('handles incremental event batches', () => {
        const { result } = renderHook(() => usePipelineEvents());

        act(() => {
            sendBridgeMessage({
                events: [
                    { event: 'OnRunStarted', system: 'pipeline', stage: '', step: '', ts: 1000, durationMs: -1, detail: 'test' },
                    { event: 'OnStageStarted', system: 'pipeline', stage: 'compile', step: '', ts: 1001, durationMs: -1 },
                ],
                summary: { target: 'test', config: 'Debug', passCount: 0, failCount: 0, totalDurationMs: 0, startTimestamp: 1000, interrupted: false, runInProgress: true },
            });
        });

        expect(result.current.state.stages).toHaveLength(1);
        expect(result.current.state.stages[0].status).toBe('running');

        act(() => {
            sendBridgeMessage({
                events: [
                    { event: 'OnStageCompleted', system: 'pipeline', stage: 'compile', step: '', ts: 1005, durationMs: 4000 },
                ],
                summary: { target: 'test', config: 'Debug', passCount: 1, failCount: 0, totalDurationMs: 0, startTimestamp: 1000, interrupted: false, runInProgress: true },
            });
        });

        expect(result.current.state.stages[0].status).toBe('passed');
        expect(result.current.state.stages[0].durationMs).toBe(4000);
    });

    it('applies UPDATE_SUMMARY for interrupted state', () => {
        const { result } = renderHook(() => usePipelineEvents());

        act(() => {
            sendBridgeMessage({
                events: [
                    { event: 'OnRunStarted', system: 'pipeline', stage: '', step: '', ts: 1000, durationMs: -1, detail: 'test' },
                    { event: 'OnStageStarted', system: 'pipeline', stage: 'compile', step: '', ts: 1001, durationMs: -1 },
                ],
                summary: { target: 'test', config: 'Debug', passCount: 0, failCount: 0, totalDurationMs: 0, startTimestamp: 1000, interrupted: false, runInProgress: true },
            });
        });

        act(() => {
            sendBridgeMessage({
                events: [],
                summary: { target: 'test', config: 'Debug', passCount: 0, failCount: 0, totalDurationMs: 2000, startTimestamp: 1000, interrupted: true, runInProgress: false },
            });
        });

        expect(result.current.state.interrupted).toBe(true);
        expect(result.current.state.stages[0].status).toBe('interrupted');
    });

    it('cleans up message listener on unmount', () => {
        const removeSpy = vi.spyOn(window, 'removeEventListener');
        const { unmount } = renderHook(() => usePipelineEvents());
        unmount();
        expect(removeSpy).toHaveBeenCalledWith('message', expect.any(Function));
    });
});
