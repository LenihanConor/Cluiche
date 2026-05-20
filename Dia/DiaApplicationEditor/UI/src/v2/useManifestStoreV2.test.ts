import { describe, it, expect, vi, beforeEach } from 'vitest';

// Mock bridge before importing store
vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn(),
    bridgeEvent: vi.fn(),
}));

import { useManifestStoreV2 } from './useManifestStoreV2';
import { bridgeRequest } from './bridge';

const mockBridgeRequest = bridgeRequest as ReturnType<typeof vi.fn>;

beforeEach(() => {
    // Reset store state
    useManifestStoreV2.setState({
        filePath: null, isDirty: false, hasManifest: false, manifest: null,
    });
    mockBridgeRequest.mockReset();
});

describe('useManifestStoreV2', () => {
    it('initial state has no manifest', () => {
        const s = useManifestStoreV2.getState();
        expect(s.hasManifest).toBe(false);
        expect(s.manifest).toBeNull();
    });

    it('loadManifest on success populates state', async () => {
        mockBridgeRequest.mockResolvedValue({
            ok: true,
            state: { filePath: '/test.diaapp', isDirty: false, hasManifest: true, manifest: { version: 2, stages: [], initialStage: '', autoStages: [], streams: [], processingUnits: [] } },
        });
        const result = await useManifestStoreV2.getState().loadManifest('/test.diaapp');
        expect(result.ok).toBe(true);
        expect(useManifestStoreV2.getState().hasManifest).toBe(true);
        expect(useManifestStoreV2.getState().filePath).toBe('/test.diaapp');
    });

    it('loadManifest on failure returns error', async () => {
        mockBridgeRequest.mockResolvedValue({ ok: false, error: 'file not found' });
        const result = await useManifestStoreV2.getState().loadManifest('/bad.diaapp');
        expect(result.ok).toBe(false);
        expect(result.error).toBe('file not found');
        expect(useManifestStoreV2.getState().hasManifest).toBe(false);
    });

    it('saveManifest on success clears dirty', async () => {
        useManifestStoreV2.setState({ isDirty: true });
        mockBridgeRequest.mockResolvedValue({ ok: true });
        const result = await useManifestStoreV2.getState().saveManifest();
        expect(result.ok).toBe(true);
        expect(useManifestStoreV2.getState().isDirty).toBe(false);
    });

    it('applyStateSnapshot updates all fields', () => {
        useManifestStoreV2.getState().applyStateSnapshot({
            filePath: '/x.diaapp', isDirty: true, hasManifest: true,
            manifest: { version: 2, stages: [], initialStage: 'Boot', autoStages: [], streams: [], processingUnits: [] },
        });
        const s = useManifestStoreV2.getState();
        expect(s.filePath).toBe('/x.diaapp');
        expect(s.isDirty).toBe(true);
        expect(s.manifest?.initialStage).toBe('Boot');
    });
});
