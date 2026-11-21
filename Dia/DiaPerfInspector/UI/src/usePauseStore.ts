import { create } from 'zustand';
import { sendPerfCommand } from './bridge';
import { interpretPauseReply } from './model/pauseReply';
import { rowKey } from './model/moduleTable';

interface PauseState {
    /** rowKey(unit, module) -> paused, from the game's replies. */
    paused: Record<string, boolean>;
    /** rowKey -> last error from the game for that row. */
    errors: Record<string, string>;
    /** Set when the game does not know the commands (Release): switches are disabled with this reason. */
    unsupported: string | null;
    toggle: (unit: string, module: string) => Promise<void>;
    resumeAll: () => Promise<void>;
    /** Pauses are owned by this client and released on disconnect. */
    reset: () => void;
}

export const usePauseStore = create<PauseState>((set, get) => ({
    paused: {},
    errors: {},
    unsupported: null,

    toggle: async (unit, module) => {
        if (get().unsupported) return;
        const key = rowKey(unit, module);
        const command = get().paused[key] ? 'perf.resume' : 'perf.pause';
        const reply = interpretPauseReply(await sendPerfCommand(command, { kind: 'module', id: module }));
        if (reply.kind === 'unsupported') { set({ unsupported: reply.reason }); return; }
        if (reply.kind === 'error') { set((s) => ({ errors: { ...s.errors, [key]: reply.message } })); return; }
        const { [key]: _drop, ...errors } = get().errors; // eslint-disable-line @typescript-eslint/no-unused-vars
        set((s) => ({ errors, paused: { ...s.paused, [key]: reply.paused ?? command === 'perf.pause' } }));
    },

    resumeAll: async () => {
        if (get().unsupported) return;
        const reply = interpretPauseReply(await sendPerfCommand('perf.resume_all', {}));
        if (reply.kind === 'unsupported') { set({ unsupported: reply.reason }); return; }
        if (reply.kind === 'error') { set((s) => ({ errors: { ...s.errors, '*': reply.message } })); return; }
        set({ paused: {}, errors: {} });
    },

    reset: () => set({ paused: {}, errors: {}, unsupported: null }),
}));
