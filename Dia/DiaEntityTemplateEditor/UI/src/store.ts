import { create } from 'zustand';
import type { BlueprintGroup, BlueprintProperties, UsageEntry, AvailableComponent, NavigateFailedData } from './types';

interface TemplateEditorState {
    projectValid: boolean;
    groups: BlueprintGroup[];
    selectedId: string | null;
    selectedPath: string | null;
    properties: BlueprintProperties | null;
    usage: UsageEntry[];
    availableComponents: AvailableComponent[];
    navigateFailedData: NavigateFailedData | null;

    setProjectValid: (v: boolean) => void;
    setGroups: (v: BlueprintGroup[]) => void;
    setSelected: (id: string, path: string) => void;
    clearSelected: () => void;
    setProperties: (v: BlueprintProperties | null) => void;
    setUsage: (v: UsageEntry[]) => void;
    setAvailableComponents: (v: AvailableComponent[]) => void;
    setNavigateFailedData: (v: NavigateFailedData | null) => void;
}

export const useTemplateEditorStore = create<TemplateEditorState>((set) => ({
    projectValid: false,
    groups: [],
    selectedId: null,
    selectedPath: null,
    properties: null,
    usage: [],
    availableComponents: [],
    navigateFailedData: null,

    setProjectValid: (v) => set({ projectValid: v }),
    setGroups: (v) => set({ groups: v }),
    setSelected: (id, path) => set({ selectedId: id, selectedPath: path }),
    clearSelected: () => set({ selectedId: null, selectedPath: null, properties: null, usage: [], availableComponents: [] }),
    setProperties: (v) => set({ properties: v }),
    setUsage: (v) => set({ usage: v }),
    setAvailableComponents: (v) => set({ availableComponents: v }),
    setNavigateFailedData: (v) => set({ navigateFailedData: v }),
}));
