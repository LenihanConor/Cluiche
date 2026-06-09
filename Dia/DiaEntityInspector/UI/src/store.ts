import { create } from 'zustand';
import type { EntityEntry, QueryEntry, MailboxEntry, WatchItem } from './types';

export type ActiveTab = 'fields' | 'queries' | 'mailbox' | 'watch';

interface InspectorState {
    connected: boolean;
    entities: EntityEntry[];
    queries: QueryEntry[];
    mailbox: MailboxEntry[];
    watchItems: WatchItem[];
    selectedIdx: number | null;
    activeTab: ActiveTab;
    frame: number;
    entityCount: number;

    setConnected: (v: boolean) => void;
    setEntities: (v: EntityEntry[]) => void;
    setQueries: (v: QueryEntry[]) => void;
    setMailbox: (v: MailboxEntry[]) => void;
    setWatchItems: (v: WatchItem[]) => void;
    setSelectedIdx: (v: number | null) => void;
    setActiveTab: (v: ActiveTab) => void;
    setFrame: (v: number) => void;
    setEntityCount: (v: number) => void;
    clearMailbox: () => void;
}

export const useInspectorStore = create<InspectorState>((set) => ({
    connected: false,
    entities: [],
    queries: [],
    mailbox: [],
    watchItems: [],
    selectedIdx: null,
    activeTab: 'fields',
    frame: 0,
    entityCount: 0,

    setConnected: (v) => set({ connected: v }),
    setEntities: (v) => set({ entities: v }),
    setQueries: (v) => set({ queries: v }),
    setMailbox: (v) => set({ mailbox: v }),
    setWatchItems: (v) => set({ watchItems: v }),
    setSelectedIdx: (v) => set({ selectedIdx: v }),
    setActiveTab: (v) => set({ activeTab: v }),
    setFrame: (v) => set({ frame: v }),
    setEntityCount: (v) => set({ entityCount: v }),
    clearMailbox: () => set({ mailbox: [] }),
}));
