import { create } from 'zustand';
import { queryFrame } from './bridge';
import { parseFrameReply } from './model/frameDetail';
import { reconcileSelection } from './model/selection';
import type { SelectedFrame } from './model/selection';
import type { Lane } from './model/laneModel';
import type { FrameDetailData } from './types';

export type FrameDetailState =
    | { status: 'idle' }
    | { status: 'loading' }
    | { status: 'ok'; data: FrameDetailData }
    | { status: 'error'; message: string };

interface SelectionState {
    /** Absolute entry index of a unit; survives polling refreshes. */
    selected: SelectedFrame | null;
    detail: FrameDetailState;
    /** Why the selection can no longer be trusted (unit gone, entry evicted); null when fine. */
    stale: string | null;
    select: (unit: string, index: number) => Promise<void>;
    reconcile: (lanes: Lane[]) => void;
    clear: () => void;
}

let token = 0;

export const useSelectionStore = create<SelectionState>((set, get) => ({
    selected: null,
    detail: { status: 'idle' },
    stale: null,

    select: async (unit, index) => {
        const mine = ++token;
        set({ selected: { unit, index }, detail: { status: 'loading' }, stale: null });
        let detail: FrameDetailState;
        try {
            const parsed = parseFrameReply(await queryFrame(unit, index));
            detail = parsed.kind === 'ok' ? { status: 'ok', data: parsed.data } : { status: 'error', message: parsed.message };
        } catch (e) {
            detail = { status: 'error', message: e instanceof Error ? e.message : String(e) };
        }
        if (mine === token) set({ detail });
    },

    reconcile: (lanes) => {
        const stale = reconcileSelection(get().selected, lanes);
        if (stale !== get().stale) set({ stale });
    },

    clear: () => {
        token++;
        set({ selected: null, detail: { status: 'idle' }, stale: null });
    },
}));
