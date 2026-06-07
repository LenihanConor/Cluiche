import { create } from 'zustand';

export type ModuleLifecycleState = 'Running' | 'Loading' | 'Stopped' | 'Failed';

export interface ModuleState {
    moduleId: string;
    puId: string;
    lifecycleState: ModuleLifecycleState;
    timeInStateMs: number;
    timeoutMs: number | null;
    blockedByDep: string | null;
    errorMessage: string | null;
}

export interface StreamState {
    streamId: string;
    msgPerSec: number;
    kbPerSec: number;
    fillPercent: number;
    dropsTotal: number;
}

export interface PUTimingState {
    puId: string;
    lastTickMs: number;
    targetPeriodMs: number;
}

export type EventSeverity = 'info' | 'warn' | 'error' | 'transition';

export interface EventLogEntry {
    id: number;
    severity: EventSeverity;
    message: string;
    timestampMs: number;
}

export interface StageHistoryEntry {
    stageName: string;
    enteredAtMs: number;
}

const TIMELINE_MAX = 1024;
const EVENT_LOG_MAX = 1024;
const MODULE_MAX = 64;
const STREAM_MAX = 16;

let nextEventId = 0;

interface InspectorStoreState {
    // Timeline ring buffer (max 1024)
    timeline: StageHistoryEntry[];

    // Module states (map by moduleId, max 64)
    modules: Record<string, ModuleState>;

    // Stream states (map by streamId, max 16)
    streams: Record<string, StreamState>;

    // PU timing states (map by puId)
    puTimings: Record<string, PUTimingState>;

    // Event log ring buffer (max 1024)
    eventLog: EventLogEntry[];

    // Severity filter (null = show all)
    severityFilter: EventSeverity | null;

    // Actions
    pushStageEntry: (entry: StageHistoryEntry) => void;
    updateModuleState: (state: ModuleState) => void;
    removeModule: (moduleId: string) => void;
    updateStreamState: (state: StreamState) => void;
    updatePUTiming: (state: PUTimingState) => void;
    pushEventLogEntry: (severity: EventSeverity, message: string, timestampMs: number) => void;
    setSeverityFilter: (filter: EventSeverity | null) => void;
    clearAll: () => void;
}

export const useInspectorStore = create<InspectorStoreState>((set) => ({
    timeline: [],
    modules: {},
    streams: {},
    puTimings: {},
    eventLog: [],
    severityFilter: null,

    pushStageEntry: (entry) => set((state) => {
        const next = [...state.timeline, entry];
        return { timeline: next.length > TIMELINE_MAX ? next.slice(next.length - TIMELINE_MAX) : next };
    }),

    updateModuleState: (moduleState) => set((state) => {
        const keys = Object.keys(state.modules);
        if (!state.modules[moduleState.moduleId] && keys.length >= MODULE_MAX) {
            return state; // cap reached, drop oldest would require ordered map — skip for now
        }
        return { modules: { ...state.modules, [moduleState.moduleId]: moduleState } };
    }),

    removeModule: (moduleId) => set((state) => {
        const next = { ...state.modules };
        delete next[moduleId];
        return { modules: next };
    }),

    updateStreamState: (streamState) => set((state) => {
        const keys = Object.keys(state.streams);
        if (!state.streams[streamState.streamId] && keys.length >= STREAM_MAX) {
            return state;
        }
        return { streams: { ...state.streams, [streamState.streamId]: streamState } };
    }),

    updatePUTiming: (timing) => set((state) => ({
        puTimings: { ...state.puTimings, [timing.puId]: timing },
    })),

    pushEventLogEntry: (severity, message, timestampMs) => set((state) => {
        const entry: EventLogEntry = { id: nextEventId++, severity, message, timestampMs };
        const next = [...state.eventLog, entry];
        return { eventLog: next.length > EVENT_LOG_MAX ? next.slice(next.length - EVENT_LOG_MAX) : next };
    }),

    setSeverityFilter: (filter) => set({ severityFilter: filter }),

    clearAll: () => set({
        timeline: [],
        modules: {},
        streams: {},
        puTimings: {},
        eventLog: [],
        severityFilter: null,
    }),
}));
