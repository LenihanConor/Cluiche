import { create } from 'zustand';
import type { AssetRow, StageNode, GlobalAsset, InspectorData, LogEntry } from './types';

export type ActiveTab = 'table' | 'tree' | 'inspector' | 'log';

interface AssetRuntimeState {
    connected: boolean;
    assets: AssetRow[];
    total: number;
    stateFilter: string;
    idSearch: string;
    treeNodes: StageNode[];
    globalAssets: GlobalAsset[];
    selectedAssetId: string;
    stageChildren: Record<string, string[]>;
    inspectorData: InspectorData | null;
    logEntries: LogEntry[];
    logTotal: number;
    logPaused: boolean;
    logMaxEntries: number;
    activeTab: ActiveTab;

    setConnected: (v: boolean) => void;
    setSnapshot: (assets: AssetRow[], total: number) => void;
    setTableFilters: (stateFilter: string, idSearch: string) => void;
    setTreeData: (nodes: StageNode[], globalAssets: GlobalAsset[], selectedAssetId: string) => void;
    setStageChildren: (stageId: string, assets: string[]) => void;
    setInspectorData: (data: InspectorData | null) => void;
    setLogData: (entries: LogEntry[], total: number, paused: boolean, maxEntries: number) => void;
    appendLogEntry: (entry: LogEntry, total: number, paused: boolean) => void;
    setActiveTab: (tab: ActiveTab) => void;
}

export const useAssetRuntimeStore = create<AssetRuntimeState>((set) => ({
    connected: false,
    assets: [],
    total: 0,
    stateFilter: '',
    idSearch: '',
    treeNodes: [],
    globalAssets: [],
    selectedAssetId: '',
    stageChildren: {},
    inspectorData: null,
    logEntries: [],
    logTotal: 0,
    logPaused: false,
    logMaxEntries: 0,
    activeTab: 'table',

    setConnected: (v) => set({ connected: v }),
    setSnapshot: (assets, total) => set({ assets, total }),
    setTableFilters: (stateFilter, idSearch) => set({ stateFilter, idSearch }),
    setTreeData: (nodes, globalAssets, selectedAssetId) =>
        set({ treeNodes: nodes, globalAssets, selectedAssetId }),
    setStageChildren: (stageId, assets) =>
        set((state) => ({
            stageChildren: { ...state.stageChildren, [stageId]: assets },
        })),
    setInspectorData: (data) => set({ inspectorData: data }),
    setLogData: (entries, total, paused, maxEntries) =>
        set({ logEntries: entries, logTotal: total, logPaused: paused, logMaxEntries: maxEntries }),
    appendLogEntry: (entry, total, paused) =>
        set((state) => ({
            logEntries: [entry, ...state.logEntries],
            logTotal: total,
            logPaused: paused,
        })),
    setActiveTab: (tab) => set({ activeTab: tab }),
}));
