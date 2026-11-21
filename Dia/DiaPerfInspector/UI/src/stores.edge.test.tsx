// Perf Inspector task 10: store and App-level edge tests (garbage replies, reconnect, stale selection, polling).

import { act, render } from '@testing-library/react';
import { renderHook } from '@testing-library/react';
import { encodeU16 } from './model/__fixtures__/makeFixture';
import type { PerfWindowReply } from './model/laneModel';
import type { PerfTimingPush } from './types';

vi.mock('./bridge', async (importOriginal) => {
    const actual = await importOriginal<typeof import('./bridge')>();
    return {
        ...actual,
        bridgeRequest: vi.fn(),
        queryWindow: vi.fn(),
        queryFrame: vi.fn(),
        queryJobs: vi.fn(),
        sendPerfCommand: vi.fn(),
    };
});

import { bridgeRequest, queryFrame, queryJobs, queryWindow, sendPerfCommand } from './bridge';
import App from './App';
import { useJobsStore } from './useJobsStore';
import { usePauseStore } from './usePauseStore';
import { usePerfTimingStore } from './usePerfTimingStore';
import { useSelectionStore } from './useSelectionStore';
import { useWindowStore } from './useWindowStore';
import { useWindowPolling } from './useWindowPolling';

const mockWindow = vi.mocked(queryWindow);
const mockFrame = vi.mocked(queryFrame);
const mockSend = vi.mocked(sendPerfCommand);
const mockBridge = vi.mocked(bridgeRequest);
const mockJobs = vi.mocked(queryJobs);

function window10(first = 0): PerfWindowReply {
    const n = 100;
    return {
        v: 1, seconds: 60, markers: [], trips: [], truncated: false,
        units: [{
            unit: 'Main', unitCrc: 1, hz: 10, firstEntry: first, count: n, modules: [{ crc: 5, name: 'A' }],
            unitMs: encodeU16(new Array(n).fill(500)), moduleMs: encodeU16(new Array(n).fill(200)),
        }],
    };
}
const okWindow = (w: PerfWindowReply) => ({ success: true, result: w }) as never;
const frameReply = (sections: unknown) => ({ success: true, result: { v: 1, unit: 'Main', frame: 5, unitMs: 5, modules: [{ crc: 5, name: 'A', ms: 2 }], sections } }) as never;
const push = (topic: string, data: unknown) => window.dispatchEvent(new MessageEvent('message', { data: { __dia: true, topic, data } }));
const connection = (connected: boolean) => act(() => { push('perf_inspector.connection_state', { connected }); });
const flush = () => act(async () => { await vi.advanceTimersByTimeAsync(0); });

beforeEach(() => {
    for (const m of [mockWindow, mockFrame, mockSend, mockBridge, mockJobs]) m.mockReset();
    mockWindow.mockResolvedValue(okWindow(window10()));
    mockBridge.mockResolvedValue({ connected: false } as never);
    mockJobs.mockResolvedValue({ success: true, result: { error: 'no' } } as never);
    useWindowStore.getState().reset();
    useSelectionStore.getState().clear();
    usePauseStore.getState().reset();
    useJobsStore.getState().reset();
    usePerfTimingStore.getState().clear();
});

describe('window store with garbage replies', () => {
    const garbage: unknown[] = [null, undefined, 5, 'x', {}, { success: false }, { success: true }, { success: true, result: null },
        { success: true, result: { v: 2, units: [] } }, { success: true, result: { v: 1, units: 'x' } },
        { success: true, result: { error: 'unknown unit' } }, { success: false, result: { error: 'query timeout: perf_inspector.query_window' } }];

    it('every garbage reply gives an error state, keeps the old data and never throws', async () => {
        await useWindowStore.getState().refresh();
        const before = useWindowStore.getState().lanes;
        for (const g of garbage) {
            mockWindow.mockResolvedValueOnce(g as never);
            await expect(useWindowStore.getState().refresh()).resolves.toBeUndefined();
            const s = useWindowStore.getState();
            expect(s.error).toEqual(expect.any(String));
            expect(s.loading).toBe(false);
            expect(s.lanes).toBe(before);
        }
        mockWindow.mockResolvedValueOnce(okWindow(window10(50)));
        await useWindowStore.getState().refresh();
        expect(useWindowStore.getState().error).toBeNull();
    });

    it('the game error text is shown rather than a generic message', async () => {
        mockWindow.mockResolvedValueOnce({ success: true, result: { error: 'internal error: packed reply did not parse' } } as never);
        await useWindowStore.getState().refresh();
        expect(useWindowStore.getState().error).toBe('internal error: packed reply did not parse');
    });

    it('a reply whose units are junk still yields a usable state (no lanes for junk)', async () => {
        mockWindow.mockResolvedValueOnce({ success: true, result: { v: 1, units: [null, 3, 'x'], markers: [], trips: [], truncated: false, seconds: 60 } } as never);
        await useWindowStore.getState().refresh();
        const s = useWindowStore.getState();
        expect(s.error).toBeNull();
        expect(s.lanes).toEqual([]);
    });

    it('paused view stays frozen across a game restart reply (firstEntry goes backwards)', async () => {
        mockWindow.mockResolvedValueOnce(okWindow(window10(5000)));
        await useWindowStore.getState().refresh();
        act(() => useWindowStore.getState().togglePause());
        const at = useWindowStore.getState().view.pausedAt;
        expect(at).not.toBeNull();
        mockWindow.mockResolvedValueOnce(okWindow(window10(0)));
        await useWindowStore.getState().refresh();
        expect(useWindowStore.getState().view.pausedAt).toBe(at);
    });

    it('trips arriving while paused update the ranges and do not move the freeze', async () => {
        await useWindowStore.getState().refresh();
        act(() => useWindowStore.getState().togglePause());
        const at = useWindowStore.getState().view.pausedAt;
        act(() => useWindowStore.getState().setTrips([{
            unit: 'Main', buildConfig: 'Debug', firstFrame: 10, lastFrame: 20, budgetMs: 1, peakMs: 3, suppressedSinceLastTrip: 0,
            topModules: [], sections: [], worstFrame: { index: 10, unitMs: 3, modules: [] }, recentUnitMs: [],
        }]));
        expect(useWindowStore.getState().tripRanges).toEqual([{ unit: 'Main', firstEntry: 10, lastEntry: 20 }]);
        expect(useWindowStore.getState().view.pausedAt).toBe(at);
    });
});

describe('jobs store with garbage pushes', () => {
    it('applyTiming and applyWindowReply tolerate junk', () => {
        const junk: unknown[] = [null, {}, { jobs: null }, { jobs: {} }, { jobs: { units: 'x' } }, { jobs: { units: [null, 3] } },
            { jobs: { units: [{ unit: 'u' }] } }, { jobs: { units: [{ unit: 'u', owners: [null, 4, { crc: 1, name: 'n' }] }] } }, { jobs: { units: [{ unit: 'u', owners: 'x' }] } }];
        for (const j of junk) {
            expect(() => useJobsStore.getState().applyTiming(j as PerfTimingPush)).not.toThrow();
            expect(Array.isArray(useJobsStore.getState().tier1)).toBe(true);
        }
        for (const j of [null, {}, { jobs: 5 }, { jobs: { units: [null] } }]) {
            expect(() => useJobsStore.getState().applyWindowReply(j as PerfWindowReply)).not.toThrow();
        }
    });

    it('raw fetch with an error reply keeps an honest error state', async () => {
        mockJobs.mockResolvedValueOnce({ success: false, result: { error: 'query timeout' } } as never);
        await useJobsStore.getState().fetchRaw('Sim', 0, 10);
        const s = useJobsStore.getState();
        expect(s.rawError).toBe('query timeout');
        expect(s.raw).toBeNull();
        expect(s.rawLoading).toBe(false);
        mockJobs.mockResolvedValueOnce(null as never);
        await useJobsStore.getState().fetchRaw('Sim', 0, 10);
        expect(useJobsStore.getState().rawError).toBe('unusable perf.jobs reply');
    });
});

describe('pause store', () => {
    it('unsupported is detected again after a reset (reconnect to a Release game, then a Debug game)', async () => {
        mockSend.mockResolvedValueOnce({ success: false, result: { error: 'unknown command: perf.pause' } } as never);
        await usePauseStore.getState().toggle('Main', 'A');
        expect(usePauseStore.getState().unsupported).toEqual(expect.any(String));
        mockSend.mockClear();
        await usePauseStore.getState().toggle('Main', 'A');
        expect(mockSend).not.toHaveBeenCalled();

        usePauseStore.getState().reset();                 // disconnect
        expect(usePauseStore.getState().unsupported).toBeNull();
        mockSend.mockResolvedValueOnce({ success: true, result: { kind: 'module', id: 'A', paused: true } } as never);
        await usePauseStore.getState().toggle('Main', 'A');   // reconnected to a game that has the commands
        expect(mockSend).toHaveBeenCalledTimes(1);
        expect(usePauseStore.getState().paused['Main/A']).toBe(true);
        expect(usePauseStore.getState().unsupported).toBeNull();
    });

    it('junk replies become per-row errors and do not throw', async () => {
        for (const g of [null, undefined, 7, {}, { success: true }, { success: true, result: null }]) {
            mockSend.mockResolvedValueOnce(g as never);
            await expect(usePauseStore.getState().toggle('Main', 'A')).resolves.toBeUndefined();
        }
        expect(usePauseStore.getState().paused['Main/A']).toBeUndefined();
        expect(usePauseStore.getState().errors['Main/A']).toEqual(expect.any(String));
    });
});

describe('selection store', () => {
    it('re-selecting the same frame refreshes the detail when profiling sections toggle', async () => {
        mockFrame.mockResolvedValueOnce(frameReply({ available: true, sections: [{ name: 's', ms: 1 }] }));
        await useSelectionStore.getState().select('Main', 5);
        let d = useSelectionStore.getState().detail;
        expect(d.status === 'ok' && d.data.sections).toHaveLength(1);
        mockFrame.mockResolvedValueOnce(frameReply({ available: false, reason: 'profiling off' }));
        await useSelectionStore.getState().select('Main', 5);
        d = useSelectionStore.getState().detail;
        expect(d.status === 'ok' && d.data.sectionsUnavailableReason).toBe('profiling off');
        mockFrame.mockResolvedValueOnce(frameReply({ available: true, sections: [{ name: 't', ms: 2 }] }));
        await useSelectionStore.getState().select('Main', 5);
        d = useSelectionStore.getState().detail;
        expect(d.status === 'ok' && d.data.sections[0].name).toBe('t');
    });

    it('a reply that arrives after clear (disconnect) is dropped', async () => {
        let release: (v: never) => void = () => {};
        mockFrame.mockReturnValueOnce(new Promise<never>((r) => { release = r; }));
        const p = useSelectionStore.getState().select('Main', 5);
        useSelectionStore.getState().clear();
        release(frameReply({ available: false }));
        await p;
        const s = useSelectionStore.getState();
        expect(s.selected).toBeNull();
        expect(s.detail.status).toBe('idle');
    });

    it('junk frame replies are an error detail, not an exception', async () => {
        for (const g of [null, undefined, 3, {}, { success: true, result: { v: 1 } }]) {
            mockFrame.mockResolvedValueOnce(g as never);
            await useSelectionStore.getState().select('Main', 5);
            expect(useSelectionStore.getState().detail.status).toBe('error');
        }
    });

    it('selecting a frame outside the retained window shows the game error and marks it stale on reconcile', async () => {
        await useWindowStore.getState().refresh();
        mockFrame.mockResolvedValueOnce({ success: true, result: { error: 'frame outside retained range' } } as never);
        await useSelectionStore.getState().select('Main', 0);   // lanes retain 0..99, so not stale here
        useSelectionStore.getState().reconcile(useWindowStore.getState().lanes);
        expect(useSelectionStore.getState().stale).toBeNull();
        expect(useSelectionStore.getState().detail).toEqual({ status: 'error', message: 'frame outside retained range' });
        mockWindow.mockResolvedValueOnce(okWindow(window10(500)));
        await useWindowStore.getState().refresh();
        useSelectionStore.getState().reconcile(useWindowStore.getState().lanes);
        expect(useSelectionStore.getState().stale).toMatch(/no longer retained/);
    });
});

describe('polling across reconnects', () => {
    beforeEach(() => { vi.useFakeTimers(); });
    afterEach(() => { vi.useRealTimers(); });

    it('reconnect restarts polling exactly once: one immediate refresh then one per second', async () => {
        type C = 'connected' | 'disconnected';
        const { rerender } = renderHook(({ c }: { c: C }) => useWindowPolling(c), { initialProps: { c: 'connected' as C } });
        await flush();
        expect(mockWindow).toHaveBeenCalledTimes(1);
        rerender({ c: 'disconnected' });
        rerender({ c: 'connected' });
        rerender({ c: 'disconnected' });
        rerender({ c: 'connected' });                       // flapping inside one tick
        await flush();
        const afterFlap = mockWindow.mock.calls.length;
        await act(async () => { await vi.advanceTimersByTimeAsync(3000); });
        expect(mockWindow.mock.calls.length - afterFlap).toBe(3);   // not 6: a single interval is alive
    });

    it('a refresh still in flight at reconnect is not doubled', async () => {
        let release: (v: never) => void = () => {};
        mockWindow.mockReset();
        mockWindow.mockReturnValueOnce(new Promise<never>((r) => { release = r; }));
        mockWindow.mockResolvedValue(okWindow(window10()));
        type C = 'connected' | 'disconnected';
        const { rerender } = renderHook(({ c }: { c: C }) => useWindowPolling(c), { initialProps: { c: 'connected' as C } });
        await flush();
        rerender({ c: 'disconnected' });
        rerender({ c: 'connected' });
        await flush();
        expect(mockWindow).toHaveBeenCalledTimes(1);        // the in-flight request is the only one
        await act(async () => { release(okWindow(window10())); await vi.advanceTimersByTimeAsync(0); });
        expect(useWindowStore.getState().loading).toBe(false);
        await act(async () => { await vi.advanceTimersByTimeAsync(1000); });
        expect(mockWindow).toHaveBeenCalledTimes(2);
    });
});

describe('App reconnect', () => {
    beforeEach(() => { vi.useFakeTimers(); });
    afterEach(() => { vi.useRealTimers(); });

    const timing = (name: string): PerfTimingPush => ({
        scale: 0.01, windowSec: 10, unavailable: [],
        units: [{ unit: name, hz: 60, samples: 1, tick: [100, 200], modules: ['A'], ewma: [50], p95: [60] }],
    });

    it('disconnect releases pauses and the selection, reconnect restores polling and the perf.timing subscription', async () => {
        render(<App />);
        await flush();
        await connection(true);
        await flush();
        expect(mockWindow).toHaveBeenCalledTimes(1);

        act(() => push('perf_inspector.timing', timing('Main')));
        expect(usePerfTimingStore.getState().timing?.units[0].unit).toBe('Main');
        usePauseStore.setState({ paused: { 'Main/A': true }, unsupported: 'old reason' });
        mockFrame.mockResolvedValueOnce(frameReply({ available: false }));
        await act(async () => { await useSelectionStore.getState().select('Main', 5); });
        expect(useSelectionStore.getState().selected).not.toBeNull();

        await connection(false);
        await flush();
        expect(usePauseStore.getState().paused).toEqual({});
        expect(usePauseStore.getState().unsupported).toBeNull();
        expect(useSelectionStore.getState().selected).toBeNull();
        expect(usePerfTimingStore.getState().timing).toBeNull();
        const calls = mockWindow.mock.calls.length;
        await act(async () => { await vi.advanceTimersByTimeAsync(5000); });
        expect(mockWindow.mock.calls.length).toBe(calls);   // no polling while disconnected

        await connection(true);
        await flush();
        expect(mockWindow.mock.calls.length).toBe(calls + 1);
        await act(async () => { await vi.advanceTimersByTimeAsync(2000); });
        expect(mockWindow.mock.calls.length).toBe(calls + 3);   // exactly one poller

        act(() => push('perf_inspector.timing', timing('Sim')));
        expect(usePerfTimingStore.getState().timing?.units[0].unit).toBe('Sim');   // subscription survived the drop
    });

    it('a junk perf.timing push does not break the App', async () => {
        render(<App />);
        await flush();
        await connection(true);
        for (const junk of [{}, { units: 'x' }, { units: [null] }, { units: [{ unit: 'M', modules: [null] }] }, { jobs: { units: 'x' } }, { jobs: { units: [null] } }]) {
            expect(() => act(() => push('perf_inspector.timing', junk))).not.toThrow();
        }
    });
});
