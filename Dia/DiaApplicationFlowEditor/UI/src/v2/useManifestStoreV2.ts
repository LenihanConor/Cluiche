import { create } from 'zustand';
import { bridgeRequest } from './bridge';
import type { ManifestStateV2, ManifestV2 } from './types';

interface ManifestStoreV2State {
    filePath: string | null;
    isDirty: boolean;
    hasManifest: boolean;
    manifest: ManifestV2 | null;

    // Actions
    loadManifest: (path: string) => Promise<{ ok: boolean; error?: string }>;
    saveManifest: () => Promise<{ ok: boolean; error?: string }>;
    refreshState: () => Promise<void>;
    applyStateSnapshot: (snapshot: ManifestStateV2) => void;
    setDirty: (dirty: boolean) => void;
}

export const useManifestStoreV2 = create<ManifestStoreV2State>((set) => ({
    filePath: null,
    isDirty: false,
    hasManifest: false,
    manifest: null,

    loadManifest: async (path) => {
        const res = await bridgeRequest('manifest.load', { path }) as any;
        if (res?.ok && res?.state) {
            set({
                filePath: res.state.filePath ?? path,
                isDirty: res.state.isDirty ?? false,
                hasManifest: res.state.hasManifest ?? true,
                manifest: res.state.manifest ?? null,
            });
        }
        return { ok: res?.ok ?? false, error: res?.error };
    },

    saveManifest: async () => {
        const res = await bridgeRequest('manifest.save') as any;
        if (res?.ok) set({ isDirty: false });
        return { ok: res?.ok ?? false, error: res?.error };
    },

    refreshState: async () => {
        const res = await bridgeRequest('manifest.getState') as any;
        if (res?.ok && res?.state) {
            set({
                filePath: res.state.filePath,
                isDirty: res.state.isDirty,
                hasManifest: res.state.hasManifest,
                manifest: res.state.manifest,
            });
        }
    },

    applyStateSnapshot: (snapshot) => {
        set({
            filePath: snapshot.filePath,
            isDirty: snapshot.isDirty,
            hasManifest: snapshot.hasManifest,
            manifest: snapshot.manifest,
        });
    },

    setDirty: (dirty) => set({ isDirty: dirty }),
}));
