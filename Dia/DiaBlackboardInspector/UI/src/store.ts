import { create } from 'zustand';
import type { BoardEntry } from './types';

interface BlackboardInspectorState {
    connected: boolean;
    boards: BoardEntry[];
    filterText: string;
    setConnected: (v: boolean) => void;
    setBoards: (v: BoardEntry[]) => void;
    setFilterText: (v: string) => void;
}

export const useBlackboardInspectorStore = create<BlackboardInspectorState>((set) => ({
    connected: false,
    boards: [],
    filterText: '',
    setConnected: (v) => set({ connected: v }),
    setBoards: (v) => set({ boards: v }),
    setFilterText: (v) => set({ filterText: v }),
}));
