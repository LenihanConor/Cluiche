import { renderHook, act } from '@testing-library/react';
import type { LiveConnectionState } from './types';

vi.mock('./bridge', () => ({ queryChain: vi.fn() }));

import { queryChain } from './bridge';
import { useChainStore } from './useChainStore';
import { kChainPollIntervalMs, useChainPolling } from './useChainPolling';

const mockQuery = vi.mocked(queryChain);
const available = { success: true, result: { success: true, data: { available: true, name: 'c', stages: [{ id: '1', unit: 'Sim', name: 'A', startMs: 0, endMs: 1 }], edges: [] } } };
const unavailable = { success: true, result: { success: true, data: { available: false, reason: 'tracing is off' } } };

beforeEach(() => {
    vi.useFakeTimers();
    mockQuery.mockReset();
    mockQuery.mockResolvedValue(available as never);
    useChainStore.getState().reset();
});
afterEach(() => { vi.useRealTimers(); });

async function flush() { await act(async () => { await vi.advanceTimersByTimeAsync(0); }); }

describe('useChainPolling', () => {
    it('fetches immediately when connected and visible, then every 2 s', async () => {
        renderHook(() => useChainPolling('connected', true));
        await flush();
        expect(mockQuery).toHaveBeenCalledTimes(1);
        expect(useChainStore.getState().chain?.stages).toHaveLength(1);
        await act(async () => { await vi.advanceTimersByTimeAsync(kChainPollIntervalMs); });
        expect(mockQuery).toHaveBeenCalledTimes(2);
        await act(async () => { await vi.advanceTimersByTimeAsync(kChainPollIntervalMs); });
        expect(mockQuery).toHaveBeenCalledTimes(3);
    });

    it('does not poll while disconnected or while the Frames tab is hidden', async () => {
        renderHook(() => useChainPolling('disconnected', true));
        renderHook(() => useChainPolling('connected', false));
        await act(async () => { await vi.advanceTimersByTimeAsync(kChainPollIntervalMs * 3); });
        expect(mockQuery).not.toHaveBeenCalled();
    });

    it('stops polling and clears the store on disconnect', async () => {
        const { rerender } = renderHook(({ c }: { c: LiveConnectionState }) => useChainPolling(c, true), { initialProps: { c: 'connected' as LiveConnectionState } });
        await flush();
        expect(useChainStore.getState().chain).not.toBeNull();
        rerender({ c: 'disconnected' });
        expect(useChainStore.getState().chain).toBeNull();
        mockQuery.mockClear();
        await act(async () => { await vi.advanceTimersByTimeAsync(kChainPollIntervalMs * 3); });
        expect(mockQuery).not.toHaveBeenCalled();
    });

    it('stops polling when the tab is hidden but keeps the last chain', async () => {
        const { rerender } = renderHook(({ a }: { a: boolean }) => useChainPolling('connected', a), { initialProps: { a: true } });
        await flush();
        rerender({ a: false });
        mockQuery.mockClear();
        await act(async () => { await vi.advanceTimersByTimeAsync(kChainPollIntervalMs * 3); });
        expect(mockQuery).not.toHaveBeenCalled();
        expect(useChainStore.getState().chain).not.toBeNull();
    });
});

describe('useChainStore.refresh', () => {
    it('an unavailable reply leaves the store null with the reason, replacing an earlier chain', async () => {
        await useChainStore.getState().refresh();
        expect(useChainStore.getState().chain).not.toBeNull();
        mockQuery.mockResolvedValue(unavailable as never);
        await useChainStore.getState().refresh();
        expect(useChainStore.getState().chain).toBeNull();
        expect(useChainStore.getState().reason).toBe('tracing is off');
    });

    it('does not throw when the query rejects or returns junk', async () => {
        mockQuery.mockRejectedValue(new Error('boom'));
        await expect(useChainStore.getState().refresh()).resolves.toBeUndefined();
        expect(useChainStore.getState().chain).toBeNull();
        mockQuery.mockResolvedValue(undefined as never);
        await expect(useChainStore.getState().refresh()).resolves.toBeUndefined();
        expect(useChainStore.getState().reason).toBeTruthy();
    });
});
