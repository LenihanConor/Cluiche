import { renderHook, act } from '@testing-library/react';
import { encodeU16 } from './model/__fixtures__/makeFixture';
import type { PerfWindowReply } from './model/laneModel';

vi.mock('./bridge', () => ({ queryWindow: vi.fn() }));

import { queryWindow } from './bridge';
import { selectView, useWindowStore } from './useWindowStore';
import { useWindowPolling } from './useWindowPolling';

const mockQuery = vi.mocked(queryWindow);

/** hz 10, 600 entries (60 s) ending at t = endT. */
function window60(endT = 60): PerfWindowReply {
    const n = 600;
    return {
        v: 1, seconds: 60, markers: [], trips: [], truncated: false,
        units: [{
            unit: 'Main', unitCrc: 1, hz: 10, firstEntry: Math.round(endT * 10) - n + 1, count: n,
            modules: [{ crc: 5, name: 'A' }],
            unitMs: encodeU16(new Array(n).fill(500)), moduleMs: encodeU16(new Array(n).fill(200)),
        }],
    };
}
const ok = (w: PerfWindowReply) => ({ success: true, result: w });

beforeEach(() => {
    mockQuery.mockReset();
    mockQuery.mockResolvedValue(ok(window60()) as never);
    useWindowStore.getState().reset();
});

describe('useWindowStore', () => {
    it('refresh always asks for 60 s and stores the unwrapped reply and lanes', async () => {
        await useWindowStore.getState().refresh();
        expect(mockQuery).toHaveBeenCalledWith(60);
        const s = useWindowStore.getState();
        expect(s.reply?.v).toBe(1);
        expect(s.lanes).toHaveLength(1);
        expect(s.error).toBeNull();
        expect(s.loading).toBe(false);
        expect(s.tripRanges).toEqual([]);
    });

    it('window seconds slice client-side without refetching', async () => {
        await useWindowStore.getState().refresh();
        useWindowStore.getState().setWindowSeconds(60);
        expect(selectView(useWindowStore.getState()).lanes[0].count).toBe(600);
        useWindowStore.getState().setWindowSeconds(10);
        expect(selectView(useWindowStore.getState()).lanes[0].count).toBe(100);
        useWindowStore.getState().setWindowSeconds(30);
        expect(selectView(useWindowStore.getState()).lanes[0].count).toBe(300);
        expect(mockQuery).toHaveBeenCalledTimes(1);
    });

    it('pause freezes the view on the newest time; newer replies do not move it', async () => {
        await useWindowStore.getState().refresh();
        useWindowStore.getState().togglePause();
        const paused = useWindowStore.getState().view.pausedAt;
        expect(paused).toBeCloseTo(60, 6);
        mockQuery.mockResolvedValue(ok(window60(70)) as never);
        await useWindowStore.getState().refresh();
        const v = selectView(useWindowStore.getState());
        expect(v.paused).toBe(true);
        expect(v.anchorT).toBeCloseTo(60, 6);
    });

    it('resume clears pausedAt and refetches', async () => {
        await useWindowStore.getState().refresh();
        useWindowStore.getState().togglePause();
        mockQuery.mockClear();
        mockQuery.mockResolvedValue(ok(window60(70)) as never);
        useWindowStore.getState().togglePause();
        expect(useWindowStore.getState().view.pausedAt).toBeNull();
        await vi.waitFor(() => expect(mockQuery).toHaveBeenCalledTimes(1));
        await vi.waitFor(() => expect(selectView(useWindowStore.getState()).anchorT).toBeCloseTo(70, 6));
    });

    it('sets an error on a failed reply and keeps the previous data', async () => {
        await useWindowStore.getState().refresh();
        mockQuery.mockResolvedValue({ success: false, result: { error: 'boom' } } as never);
        await useWindowStore.getState().refresh();
        const s = useWindowStore.getState();
        expect(s.error).toBe('boom');
        expect(s.lanes).toHaveLength(1);
        expect(s.loading).toBe(false);
    });

    it('sets an error when the bridge rejects', async () => {
        mockQuery.mockRejectedValue(new Error('down'));
        await useWindowStore.getState().refresh();
        expect(useWindowStore.getState().error).toBe('down');
    });

    it('does not start a second query while one is in flight', async () => {
        let release!: (v: unknown) => void;
        mockQuery.mockReturnValue(new Promise((r) => { release = r; }) as never);
        const a = useWindowStore.getState().refresh();
        const b = useWindowStore.getState().refresh();
        release(ok(window60()));
        await Promise.all([a, b]);
        expect(mockQuery).toHaveBeenCalledTimes(1);
    });
});

describe('useWindowPolling', () => {
    beforeEach(() => { vi.useFakeTimers(); });
    afterEach(() => { vi.useRealTimers(); });

    it('refreshes at once, then once per second, while connected', async () => {
        renderHook(() => useWindowPolling('connected'));
        await vi.advanceTimersByTimeAsync(0);
        expect(mockQuery).toHaveBeenCalledTimes(1);
        await vi.advanceTimersByTimeAsync(3000);
        expect(mockQuery).toHaveBeenCalledTimes(4);
    });

    it('never polls faster than 1 Hz', async () => {
        renderHook(() => useWindowPolling('connected'));
        await vi.advanceTimersByTimeAsync(999);
        expect(mockQuery).toHaveBeenCalledTimes(1);
    });

    it('does not poll while disconnected', async () => {
        renderHook(() => useWindowPolling('disconnected'));
        await vi.advanceTimersByTimeAsync(5000);
        expect(mockQuery).not.toHaveBeenCalled();
    });

    it('stops when the connection drops and when unmounted', async () => {
        type C = 'connected' | 'disconnected';
        const { rerender, unmount } = renderHook(({ c }: { c: C }) => useWindowPolling(c), {
            initialProps: { c: 'connected' as C },
        });
        await vi.advanceTimersByTimeAsync(1000);
        const n = mockQuery.mock.calls.length;
        rerender({ c: 'disconnected' });
        await vi.advanceTimersByTimeAsync(5000);
        expect(mockQuery).toHaveBeenCalledTimes(n);
        rerender({ c: 'connected' });
        await vi.advanceTimersByTimeAsync(0);
        const m = mockQuery.mock.calls.length;
        unmount();
        await vi.advanceTimersByTimeAsync(5000);
        expect(mockQuery).toHaveBeenCalledTimes(m);
    });

    it('stops polling while paused and resumes (with one refetch) on resume', async () => {
        renderHook(() => useWindowPolling('connected'));
        await vi.advanceTimersByTimeAsync(0);
        await act(async () => { useWindowStore.getState().togglePause(); });
        mockQuery.mockClear();
        await vi.advanceTimersByTimeAsync(5000);
        expect(mockQuery).not.toHaveBeenCalled();
        await act(async () => { useWindowStore.getState().togglePause(); });
        await vi.advanceTimersByTimeAsync(0);
        expect(mockQuery).toHaveBeenCalledTimes(1);
        await vi.advanceTimersByTimeAsync(2000);
        expect(mockQuery).toHaveBeenCalledTimes(3);
    });
});
