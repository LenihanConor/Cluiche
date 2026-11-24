// jobs.fixture.json is SYNTHETIC (see model/__fixtures__/makeJobsFixture.ts).
import fixture from './model/__fixtures__/jobs.fixture.json';
import { vi } from 'vitest';
import type { PerfWindowReply } from './model/laneModel';
import type { PerfTimingPush } from './types';

const queryJobs = vi.fn();
vi.mock('./bridge', async () => {
    const real = await vi.importActual<typeof import('./bridge')>('./bridge');
    return { ...real, queryJobs: (...a: unknown[]) => queryJobs(...a) };
});

import { selectedStepOf, useJobsStore } from './useJobsStore';
import { stepKey } from './model/jobs';

const windowReply = { v: 1, seconds: 60, units: [], markers: [], trips: [], truncated: false, jobs: fixture.window } as unknown as PerfWindowReply;
const timing = { scale: 0.01, windowSec: 10, units: [], jobs: fixture.timing.jobs, unavailable: fixture.timing.unavailable } as unknown as PerfTimingPush;
const ok = (result: unknown) => Promise.resolve({ success: true, result });

beforeEach(() => {
    queryJobs.mockReset();
    useJobsStore.getState().reset();
});

describe('useJobsStore derivation (no extra polling)', () => {
    it('derives tier 2 from the window reply and tier 1 from perf.timing without calling the game', () => {
        useJobsStore.getState().applyWindowReply(windowReply);
        useJobsStore.getState().applyTiming(timing);
        const s = useJobsStore.getState();
        expect(s.window?.units).toHaveLength(2);
        expect(s.window?.steps).toHaveLength(3);
        expect(s.tier1[0].unit).toBe('Sim');
        expect(s.unavailable).toEqual(['cap_use', 'peak_queue_depth', 'time_at_cap']);
        expect(s.ownerNames.get(0x5ea71002)).toBe('ecology');
        expect(queryJobs).not.toHaveBeenCalled();
    });
    it('a reply without a jobs key clears tier 2; a push without jobs clears tier 1', () => {
        useJobsStore.getState().applyWindowReply(windowReply);
        useJobsStore.getState().applyTiming(timing);
        useJobsStore.getState().applyWindowReply({ ...windowReply, jobs: undefined });
        useJobsStore.getState().applyTiming({ ...timing, jobs: undefined, unavailable: [] });
        expect(useJobsStore.getState().window).toBeNull();
        expect(useJobsStore.getState().tier1).toEqual([]);
        useJobsStore.getState().applyWindowReply(null);
        useJobsStore.getState().applyTiming(null);
        expect(queryJobs).not.toHaveBeenCalled();
    });
});

describe('fetchRaw gating (tier 3)', () => {
    it('does not fetch while zoomed out with no step selected', async () => {
        useJobsStore.getState().applyWindowReply(windowReply);
        await useJobsStore.getState().setZoom(10000);
        await useJobsStore.getState().selectStep(null);
        expect(queryJobs).not.toHaveBeenCalled();
        expect(useJobsStore.getState().raw).toBeNull();
    });

    it('selecting a step fetches kick .. max(apply, ready) for the step unit', async () => {
        queryJobs.mockReturnValue(ok(fixture.raw));
        useJobsStore.getState().applyWindowReply(windowReply);
        const eco = useJobsStore.getState().window!.steps[1];
        await useJobsStore.getState().selectStep(stepKey(eco));
        expect(queryJobs).toHaveBeenCalledTimes(1);
        expect(queryJobs).toHaveBeenCalledWith('Sim', eco.kick, Math.max(eco.apply, eco.ready));
        expect(useJobsStore.getState().raw?.slices).toHaveLength(17);
        expect(selectedStepOf(useJobsStore.getState())?.channel).toBe('ecology');
    });

    it('deselecting while zoomed out drops the raw records without another request', async () => {
        queryJobs.mockReturnValue(ok(fixture.raw));
        useJobsStore.getState().applyWindowReply(windowReply);
        await useJobsStore.getState().selectStep(stepKey(useJobsStore.getState().window!.steps[0]));
        await useJobsStore.getState().selectStep(null);
        expect(queryJobs).toHaveBeenCalledTimes(1);
        expect(useJobsStore.getState().raw).toBeNull();
    });

    it('zooming to 2 s finds the newest record time then fetches the 2 s ending there', async () => {
        queryJobs.mockReturnValue(ok(fixture.raw));
        useJobsStore.getState().applyWindowReply(windowReply);
        await useJobsStore.getState().setZoom(2000);
        const newest = Math.max(...useJobsStore.getState().raw!.slices.map((s) => s.endNs));
        expect(queryJobs).toHaveBeenCalledTimes(2);
        expect(queryJobs.mock.calls[0]).toEqual(['Sim', 0, Number.MAX_SAFE_INTEGER]);
        expect(queryJobs.mock.calls[1]).toEqual(['Sim', newest - 2e9, newest]);
    });

    it('zoom 200 ms also fetches; going back to 10 s clears raw and stops fetching', async () => {
        queryJobs.mockReturnValue(ok(fixture.raw));
        useJobsStore.getState().applyWindowReply(windowReply);
        await useJobsStore.getState().setZoom(200);
        const calls = queryJobs.mock.calls.length;
        expect(calls).toBeGreaterThan(0);
        await useJobsStore.getState().setZoom(10000);
        expect(queryJobs.mock.calls.length).toBe(calls);
        expect(useJobsStore.getState().raw).toBeNull();
    });

    it('keeps raw records when zooming out while a step is selected', async () => {
        queryJobs.mockReturnValue(ok(fixture.raw));
        useJobsStore.getState().applyWindowReply(windowReply);
        await useJobsStore.getState().selectStep(stepKey(useJobsStore.getState().window!.steps[1]));
        await useJobsStore.getState().setZoom(10000);
        expect(useJobsStore.getState().raw).not.toBeNull();
    });

    it('keeps probing from the newest end while replies are truncated (bounded)', async () => {
        queryJobs.mockReturnValue(ok({ ...fixture.raw, truncated: true }));
        useJobsStore.getState().applyWindowReply(windowReply);
        await useJobsStore.getState().setZoom(2000);
        // 4 probes + 1 window fetch
        expect(queryJobs).toHaveBeenCalledTimes(5);
        expect(useJobsStore.getState().raw?.truncated).toBe(true);
    });

    it('surfaces the truncation flag from a step fetch', async () => {
        queryJobs.mockReturnValue(ok({ ...fixture.raw, truncated: true }));
        useJobsStore.getState().applyWindowReply(windowReply);
        await useJobsStore.getState().selectStep(stepKey(useJobsStore.getState().window!.steps[0]));
        expect(useJobsStore.getState().raw?.truncated).toBe(true);
    });

    it('an error reply sets rawError and no raw records', async () => {
        queryJobs.mockReturnValue(ok({ error: 'unknown unit' }));
        useJobsStore.getState().applyWindowReply(windowReply);
        await useJobsStore.getState().selectStep(stepKey(useJobsStore.getState().window!.steps[0]));
        expect(useJobsStore.getState().rawError).toBe('unknown unit');
        expect(useJobsStore.getState().raw).toBeNull();
        expect(useJobsStore.getState().rawLoading).toBe(false);
    });

    it('perJobRecording false is kept on the raw result, with no slices', async () => {
        queryJobs.mockReturnValue(ok({ v: 1, unit: 'Sim', perJobRecording: false, truncated: false, count: 0 }));
        useJobsStore.getState().applyWindowReply(windowReply);
        await useJobsStore.getState().selectStep(stepKey(useJobsStore.getState().window!.steps[0]));
        expect(useJobsStore.getState().raw?.perJobRecording).toBe(false);
        expect(useJobsStore.getState().raw?.slices).toEqual([]);
    });

    it('zoom with no unit known does nothing', async () => {
        await useJobsStore.getState().setZoom(2000);
        expect(queryJobs).not.toHaveBeenCalled();
    });
});
