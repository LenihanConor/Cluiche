import { create } from 'zustand';

// ---------------------------------------------------------------------------
// Schema types
// ---------------------------------------------------------------------------

export interface SchemaResource {
    name: string;
    type: 'base' | 'derived';
    base_cap: number;
    income_rule: string;
}

export interface CostTableRow {
    action: string;
    [resource: string]: string | number;
}

// ---------------------------------------------------------------------------
// Instance / resource state types
// ---------------------------------------------------------------------------

export interface ResourceState {
    name: string;
    type: 'base' | 'derived';
    current: number;
    cap: number;
    net_rate: number;
    gross_income: number;
    gross_spend: number;
    capped_duration_s: number;
    starved_duration_s: number;
    history: number[];
}

export interface InstanceState {
    id: string;
    resources: ResourceState[];
}

// ---------------------------------------------------------------------------
// Modifier types
// ---------------------------------------------------------------------------

export interface ModifierEntry {
    type: string;
    value: number;
    source: string;
    condition: string | null;
    active: boolean;
}

export interface ModifierResource {
    name: string;
    modifiers: ModifierEntry[];
}

export interface ModifierInstance {
    id: string;
    resources: ModifierResource[];
}

// ---------------------------------------------------------------------------
// Event types
// ---------------------------------------------------------------------------

export interface EconomyEvent {
    frame: number;
    type: 'Earn' | 'Spend' | 'Clamped' | 'Transfer';
    instance: string;
    resource: string;
    amount?: number;
    attempted?: number;
    actual?: number;
    destination?: string;
}

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

const MAX_EVENTS = 500;

interface EconomyStore {
    schema: { resources: SchemaResource[]; cost_table: CostTableRow[] } | null;
    instances: InstanceState[];
    modifiers: ModifierInstance[];
    events: EconomyEvent[];

    setSchema: (schema: { resources: SchemaResource[]; cost_table: CostTableRow[] }) => void;
    setInstances: (instances: InstanceState[]) => void;
    setModifiers: (modifiers: ModifierInstance[]) => void;
    appendEvents: (payload: { full_ring: boolean; events: EconomyEvent[] }) => void;
    clearAll: () => void;
}

export const useEconomyStore = create<EconomyStore>((set) => ({
    schema: null,
    instances: [],
    modifiers: [],
    events: [],

    setSchema: (schema) => set({ schema }),

    setInstances: (instances) => set({ instances }),

    setModifiers: (modifiers) => set({ modifiers }),

    appendEvents: (payload) =>
        set((state) => {
            if (payload.full_ring) {
                // Full ring: replace all events, capped at MAX_EVENTS (take the last N)
                const next = payload.events.slice(-MAX_EVENTS);
                return { events: next };
            } else {
                // Delta: append new events and trim to ring size
                const combined = [...state.events, ...payload.events];
                return { events: combined.slice(-MAX_EVENTS) };
            }
        }),

    clearAll: () => set({ schema: null, instances: [], modifiers: [], events: [] }),
}));
