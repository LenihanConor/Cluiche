import { create } from 'zustand';

export type LiveConnectionState = 'disconnected' | 'connecting' | 'connected';

export interface LiveModuleState {
    moduleId: string;
    puId: string;
    isActive: boolean;
    activeStage: string | null;
}

export interface LiveStreamState {
    streamId: string;
    msgPerSec: number;
}

interface LiveStoreV2State {
    connectionState: LiveConnectionState;
    activeStage: string | null;
    modules: LiveModuleState[];
    streams: LiveStreamState[];

    // Actions
    setConnectionState: (state: LiveConnectionState) => void;
    setActiveStage: (stage: string | null) => void;
    updateModuleStates: (modules: LiveModuleState[]) => void;
    updateStreamStates: (streams: LiveStreamState[]) => void;
    clearLiveState: () => void;
}

export const useLiveStoreV2 = create<LiveStoreV2State>((set) => ({
    connectionState: 'disconnected',
    activeStage: null,
    modules: [],
    streams: [],

    setConnectionState: (state) => set({ connectionState: state }),

    setActiveStage: (stage) => set({ activeStage: stage }),

    updateModuleStates: (modules) => set({ modules }),

    updateStreamStates: (streams) => set({ streams }),

    clearLiveState: () => set({
        connectionState: 'disconnected',
        activeStage: null,
        modules: [],
        streams: [],
    }),
}));
