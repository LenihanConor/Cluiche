import { describe, it, expect, vi, beforeEach } from 'vitest';

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn(),
    bridgeEvent: vi.fn(),
}));

import { useUndoStoreV2 } from './useUndoStoreV2';
import { bridgeRequest } from './bridge';

const mockBridgeRequest = bridgeRequest as ReturnType<typeof vi.fn>;

beforeEach(() => {
    useUndoStoreV2.setState({ canUndo: false, canRedo: false, count: 0, isDirty: false });
    mockBridgeRequest.mockReset();
});

describe('useUndoStoreV2', () => {
    it('initial state: cannot undo or redo', () => {
        const s = useUndoStoreV2.getState();
        expect(s.canUndo).toBe(false);
        expect(s.canRedo).toBe(false);
    });

    it('undo updates canUndo/canRedo/isDirty from response', async () => {
        mockBridgeRequest.mockResolvedValue({ ok: true, canUndo: false, canRedo: true, isDirty: true });
        await useUndoStoreV2.getState().undo();
        const s = useUndoStoreV2.getState();
        expect(s.canUndo).toBe(false);
        expect(s.canRedo).toBe(true);
        expect(s.isDirty).toBe(true);
    });

    it('redo updates state from response', async () => {
        mockBridgeRequest.mockResolvedValue({ ok: true, canUndo: true, canRedo: false, isDirty: false });
        await useUndoStoreV2.getState().redo();
        expect(useUndoStoreV2.getState().canUndo).toBe(true);
        expect(useUndoStoreV2.getState().canRedo).toBe(false);
    });

    it('syncFromBackend updates count', async () => {
        mockBridgeRequest.mockResolvedValue({ ok: true, canUndo: true, canRedo: false, count: 5, isDirty: true });
        await useUndoStoreV2.getState().syncFromBackend();
        expect(useUndoStoreV2.getState().count).toBe(5);
    });

    it('applyUndoResponse updates state synchronously', () => {
        useUndoStoreV2.getState().applyUndoResponse({ canUndo: true, canRedo: false, isDirty: true });
        expect(useUndoStoreV2.getState().canUndo).toBe(true);
        expect(useUndoStoreV2.getState().isDirty).toBe(true);
    });
});
