import { create } from 'zustand';
import { queryWindow } from './bridge';
import { applyViewState, buildLanes, newestTime, unwrapWindowReply } from './model/laneModel';
import type { Lane, PerfWindowReply, TripRange, ViewResult, ViewState } from './model/laneModel';
import { tripRangesFromRecords } from './model/trips';
import type { Marker, TripRecord } from './types';

/** Always fetch the full retained window; the 10/30/60 choice slices client-side. */
export const kFetchSeconds = 60;

interface WindowState {
    reply: PerfWindowReply | null;
    lanes: Lane[];
    view: ViewState;
    /** Trip ranges for shading. Empty until the trip wire format lands (plan row 3b). */
    tripRanges: TripRange[];
    /** Trip records and markers: empty until the game has topics for them (plan row 3b). */
    trips: TripRecord[];
    markers: Marker[];
    loading: boolean;
    error: string | null;
    setTrips: (records: TripRecord[]) => void;
    setWindowSeconds: (seconds: ViewState['windowSeconds']) => void;
    togglePause: () => void;
    refresh: () => Promise<void>;
    reset: () => void;
}

const initialView: ViewState = { windowSeconds: 10, pausedAt: null };

export const useWindowStore = create<WindowState>((set, get) => ({
    reply: null,
    lanes: [],
    view: initialView,
    tripRanges: [],
    trips: [],
    markers: [],
    loading: false,
    error: null,

    setTrips: (trips) => set({ trips, tripRanges: tripRangesFromRecords(trips) }),

    setWindowSeconds: (windowSeconds) => set((s) => ({ view: { ...s.view, windowSeconds } })),

    togglePause: () => {
        const { view, lanes } = get();
        if (view.pausedAt !== null) {
            set({ view: { ...view, pausedAt: null } });
            void get().refresh();
        } else {
            set({ view: { ...view, pausedAt: newestTime(lanes) } });
        }
    },

    refresh: async () => {
        if (get().loading) return;
        set({ loading: true });
        try {
            const raw = await queryWindow(kFetchSeconds);
            const reply = unwrapWindowReply(raw);
            if (reply) {
                set({ reply, lanes: buildLanes(reply), error: null, loading: false });
            } else {
                const r = (raw as { result?: { error?: unknown } } | null)?.result;
                const msg = typeof r?.error === 'string' ? r.error : 'unusable perf.window reply';
                set({ error: msg, loading: false });
            }
        } catch (e) {
            set({ error: e instanceof Error ? e.message : String(e), loading: false });
        }
    },

    reset: () => set({ reply: null, lanes: [], view: initialView, tripRanges: [], trips: [], markers: [], loading: false, error: null }),
}));

/** The visible slice for the current view state. */
export function selectView(s: Pick<WindowState, 'lanes' | 'view'>): ViewResult {
    return applyViewState(s.lanes, s.view);
}
