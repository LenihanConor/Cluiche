import { act, renderHook } from '@testing-library/react';

vi.mock('./bridge', () => ({ queryLatency: vi.fn() }));

import { queryLatency } from './bridge';
import { useLatencyStore } from './useLatencyStore';
import { kLatencyPollIntervalMs, useLatencyPolling } from './useLatencyPolling';
import type { LiveConnectionState } from './types';

const mockQuery = vi.mocked(queryLatency);
const reply = { success: true, result: { success: true, data: { actions: { 'D.c': { count: 2, p50Ms: 1, p95Ms: 2 } } } } };

describe('useLatencyStore', () => {
    beforeEach(() => { mockQuery.mockReset(); useLatencyStore.getState().reset(); });
    it('starts null with no reason', () => {
        expect(useLatencyStore.getState().actions).toBeNull();
        expect(useLatencyStore.getState().reason).toBeNull();
    });
    it('holds and clears actions', () => {
        useLatencyStore.getState().setActions([{ action: 'a', targetMs: 100, p50Ms: 1, p95Ms: 2, samples: 1 }]);
        expect(useLatencyStore.getState().actions).toHaveLength(1);
        useLatencyStore.getState().setActions(null);
        expect(useLatencyStore.getState().actions).toBeNull();
    });
    it('refresh fills actions from the reply', async () => {
        mockQuery.mockResolvedValue(reply as never);
        await useLatencyStore.getState().refresh();
        expect(useLatencyStore.getState().actions?.[0].action).toBe('D.c');
    });
    it('refresh leaves null and a Debug-build reason for an error reply', async () => {
        mockQuery.mockResolvedValue({ success: false, result: { error: 'unknown command' } } as never);
        await useLatencyStore.getState().refresh();
        expect(useLatencyStore.getState().actions).toBeNull();
        expect(useLatencyStore.getState().reason).toBe('latency probe needs a Debug build');
    });
    it('refresh survives a rejected query', async () => {
        mockQuery.mockRejectedValue(new Error('x'));
        await useLatencyStore.getState().refresh();
        expect(useLatencyStore.getState().actions).toBeNull();
        expect(useLatencyStore.getState().reason).toBeTruthy();
    });
});

describe('useLatencyPolling', () => {
    beforeEach(() => { vi.useFakeTimers(); mockQuery.mockReset(); mockQuery.mockResolvedValue(reply as never); useLatencyStore.getState().reset(); });
    afterEach(() => { vi.useRealTimers(); });
    const flush = () => act(async () => { await vi.advanceTimersByTimeAsync(0); });

    it('fetches immediately then every 2 s while connected and visible', async () => {
        renderHook(() => useLatencyPolling('connected', true));
        await flush();
        expect(mockQuery).toHaveBeenCalledTimes(1);
        await act(async () => { await vi.advanceTimersByTimeAsync(kLatencyPollIntervalMs); });
        expect(mockQuery).toHaveBeenCalledTimes(2);
    });
    it('does not poll while disconnected or hidden', async () => {
        renderHook(() => useLatencyPolling('disconnected', true));
        renderHook(() => useLatencyPolling('connected', false));
        await act(async () => { await vi.advanceTimersByTimeAsync(kLatencyPollIntervalMs * 3); });
        expect(mockQuery).not.toHaveBeenCalled();
    });
    it('clears the store on disconnect', async () => {
        const { rerender } = renderHook(({ c }: { c: LiveConnectionState }) => useLatencyPolling(c, true), { initialProps: { c: 'connected' as LiveConnectionState } });
        await flush();
        expect(useLatencyStore.getState().actions).not.toBeNull();
        rerender({ c: 'disconnected' });
        expect(useLatencyStore.getState().actions).toBeNull();
    });
});
