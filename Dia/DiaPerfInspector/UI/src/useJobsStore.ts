import { create } from 'zustand';
import { queryJobs } from './bridge';
import { parseJobsWindow, parseRawJobs, stepKey } from './model/jobs';
import type { JobsWindow, RawJobs } from './model/jobs';
import { readReply } from './model/reply';
import type { PerfWindowReply } from './model/laneModel';
import type { JobsTier1Unit, PerfTimingPush, StepSummary } from './types';

/** Zoom choices of the Workers tab. 10000 = the aggregate view (tier 2 only); 2000 and 200 fetch raw records (tier 3). */
export type WorkersZoomMs = 10000 | 2000 | 200;

/** Raw records are only requested when the view is zoomed to this span or less, or a step is selected. */
export const kRawZoomMaxMs = 2000;
/** The game caps a perf.jobs reply at 2000 records. */
export const kRawRecordCap = 2000;

const kMaxSafeNs = Number.MAX_SAFE_INTEGER;
const kAnchorProbeLimit = 4;

interface JobsState {
    tier1: JobsTier1Unit[];
    unavailable: string[];
    window: JobsWindow | null;
    selectedStep: string | null;
    zoomMs: WorkersZoomMs;
    raw: RawJobs | null;
    rawError: string | null;
    rawLoading: boolean;
    /** Owner crc to display name, collected from tier 1 and tier 2. */
    ownerNames: Map<number, string>;

    applyTiming: (push: PerfTimingPush | null) => void;
    applyWindowReply: (reply: PerfWindowReply | null) => void;
    selectStep: (key: string | null) => Promise<void>;
    setZoom: (ms: WorkersZoomMs) => Promise<void>;
    fetchRaw: (unit: string, fromNs: number, toNs: number) => Promise<void>;
    reset: () => void;
}

function collectNames(into: Map<number, string>, window: JobsWindow | null, tier1: JobsTier1Unit[]): Map<number, string> {
    const m = new Map(into);
    for (const u of tier1) for (const o of u.owners) m.set(o.crc, o.name);
    if (window) for (const u of window.units) for (const o of u.owners) m.set(o.crc, o.name);
    return m;
}

export function selectedStepOf(s: Pick<JobsState, 'window' | 'selectedStep'>): StepSummary | null {
    if (!s.window || s.selectedStep === null) return null;
    return s.window.steps.find((x) => stepKey(x) === s.selectedStep) ?? null;
}

/** The unit raw records are requested for: the selected step unit, else the first unit with per-job recording, else tier 1 first. */
export function rawUnitOf(s: Pick<JobsState, 'window' | 'tier1'>, step: StepSummary | null): string | null {
    return step?.unit ?? s.window?.units.find((u) => u.perJobRecording)?.unit ?? s.tier1[0]?.unit ?? null;
}

const initial = {
    tier1: [] as JobsTier1Unit[], unavailable: [] as string[], window: null as JobsWindow | null,
    selectedStep: null as string | null, zoomMs: 10000 as WorkersZoomMs, raw: null as RawJobs | null,
    rawError: null as string | null, rawLoading: false,
};

export const useJobsStore = create<JobsState>((set, get) => ({
    ...initial,
    ownerNames: new Map<number, string>(),

    applyTiming: (push) => {
        const tier1 = push?.jobs?.units ?? [];
        set((s) => ({ tier1, unavailable: push?.unavailable ?? [], ownerNames: collectNames(s.ownerNames, s.window, tier1) }));
    },

    applyWindowReply: (reply) => {
        const window = reply ? parseJobsWindow(reply.jobs) : null;
        set((s) => ({ window, ownerNames: collectNames(s.ownerNames, window, s.tier1) }));
    },

    fetchRaw: async (unit, fromNs, toNs) => {
        set({ rawLoading: true, rawError: null });
        try {
            const r = readReply(await queryJobs(unit, fromNs, toNs));
            const body = r?.body ?? null;
            if (!r || !r.success || body === null || body.error !== undefined) {
                const msg = body && typeof body.error === 'string' ? body.error : 'unusable perf.jobs reply';
                set({ rawLoading: false, rawError: msg, raw: null });
                return;
            }
            const raw = parseRawJobs(body, get().ownerNames);
            set(raw ? { raw, rawLoading: false, rawError: null } : { raw: null, rawLoading: false, rawError: 'unusable perf.jobs reply' });
        } catch (e) {
            set({ rawLoading: false, rawError: e instanceof Error ? e.message : String(e), raw: null });
        }
    },

    selectStep: async (key) => {
        set({ selectedStep: key });
        const st = get();
        const step = selectedStepOf(st);
        if (key === null || !step) {
            if (st.zoomMs > kRawZoomMaxMs) set({ raw: null, rawError: null });
            return;
        }
        const unit = rawUnitOf(st, step);
        if (unit === null) return;
        await get().fetchRaw(unit, step.kick, Math.max(step.apply, step.ready));
    },

    setZoom: async (zoomMs) => {
        set({ zoomMs });
        const st = get();
        if (zoomMs > kRawZoomMaxMs) {
            if (st.selectedStep === null) set({ raw: null, rawError: null });
            return;
        }
        const unit = rawUnitOf(st, selectedStepOf(st));
        if (unit === null) return;
        // The wire has no steady-clock reference for the lane axis, so find the newest record time first. perf.jobs keeps the
        // OLDEST records when it truncates, so keep asking from the newest end seen until a reply is not truncated.
        set({ rawLoading: true, rawError: null });
        let from = 0;
        let anchor = 0;
        for (let i = 0; i < kAnchorProbeLimit; i++) {
            const r = readReply(await queryJobs(unit, from, kMaxSafeNs));
            const body = r?.body ?? null;
            const probe = body && body.error === undefined ? parseRawJobs(body, get().ownerNames) : null;
            if (!r || !r.success || !probe) {
                set({ rawLoading: false, rawError: body && typeof body.error === 'string' ? body.error : 'unusable perf.jobs reply', raw: null });
                return;
            }
            if (!probe.perJobRecording) { set({ raw: probe, rawLoading: false, rawError: null }); return; }
            for (const s of probe.slices) anchor = Math.max(anchor, s.endNs);
            if (!probe.truncated || probe.slices.length === 0) break;
            from = anchor;
        }
        if (anchor === 0) { set({ raw: { unit, perJobRecording: true, truncated: false, slices: [] }, rawLoading: false, rawError: null }); return; }
        await get().fetchRaw(unit, Math.max(0, anchor - zoomMs * 1e6), anchor);
    },

    reset: () => set({ ...initial, ownerNames: new Map() }),
}));
