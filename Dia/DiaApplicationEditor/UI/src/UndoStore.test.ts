import { describe, it, expect, beforeEach } from 'vitest';
import { useUndoStore } from './UndoStore';
import type { ManifestData } from './types';

function makeManifest(id: string): ManifestData {
    return {
        version: 1,
        imports: [],
        processing_units: [
            {
                instance_id: id,
                type: id,
                frequency_hz: 30,
                dedicated_thread: false,
                initial_phase: 'Init',
                phases: [{ instance_id: 'Init', type: 'Init', config: {} }],
                transitions: [],
                modules: [],
                config: {},
            },
        ],
    };
}

beforeEach(() => {
    useUndoStore.getState().clearHistory();
});

describe('UndoStore – pushSnapshot', () => {
    it('enables canUndo after push', () => {
        const m = makeManifest('A');
        useUndoStore.getState().pushSnapshot(m, 'edit');
        expect(useUndoStore.getState().canUndo).toBe(true);
    });

    it('clears redo stack on push', () => {
        const m1 = makeManifest('A');
        const m2 = makeManifest('B');
        useUndoStore.getState().pushSnapshot(m1, 'edit 1');
        // Simulate undo to populate redo
        useUndoStore.getState().undo(m2);
        expect(useUndoStore.getState().canRedo).toBe(true);
        // Push new snapshot clears redo
        useUndoStore.getState().pushSnapshot(m2, 'edit 2');
        expect(useUndoStore.getState().canRedo).toBe(false);
    });

    it('caps history at 8 entries', () => {
        for (let i = 0; i < 12; i++) {
            useUndoStore.getState().pushSnapshot(makeManifest(`PU${i}`), `edit ${i}`);
        }
        expect(useUndoStore.getState().undoStack.length).toBe(8);
    });
});

describe('UndoStore – undo', () => {
    it('returns previous snapshot', () => {
        const before = makeManifest('Before');
        const after = makeManifest('After');
        useUndoStore.getState().pushSnapshot(before, 'create');
        const result = useUndoStore.getState().undo(after);
        expect(result).toEqual(before);
    });

    it('pushes current manifest to redo stack', () => {
        const before = makeManifest('Before');
        const current = makeManifest('Current');
        useUndoStore.getState().pushSnapshot(before, 'create');
        useUndoStore.getState().undo(current);
        expect(useUndoStore.getState().canRedo).toBe(true);
        // Redo should restore 'current'
        const redone = useUndoStore.getState().redo(before);
        expect(redone).toEqual(current);
    });

    it('returns null when stack is empty', () => {
        const m = makeManifest('X');
        const result = useUndoStore.getState().undo(m);
        expect(result).toBeNull();
    });

    it('disables canUndo when stack depleted', () => {
        const m = makeManifest('A');
        useUndoStore.getState().pushSnapshot(m, 'edit');
        useUndoStore.getState().undo(makeManifest('B'));
        expect(useUndoStore.getState().canUndo).toBe(false);
    });
});

describe('UndoStore – redo', () => {
    it('returns null when redo stack is empty', () => {
        const result = useUndoStore.getState().redo(makeManifest('X'));
        expect(result).toBeNull();
    });

    it('restores undone state', () => {
        const before = makeManifest('Before');
        const after = makeManifest('After');
        useUndoStore.getState().pushSnapshot(before, 'create');
        useUndoStore.getState().undo(after);
        const result = useUndoStore.getState().redo(before);
        expect(result).toEqual(after);
    });
});

describe('UndoStore – clearHistory', () => {
    it('resets both stacks', () => {
        useUndoStore.getState().pushSnapshot(makeManifest('A'), 'edit');
        useUndoStore.getState().clearHistory();
        expect(useUndoStore.getState().canUndo).toBe(false);
        expect(useUndoStore.getState().canRedo).toBe(false);
        expect(useUndoStore.getState().undoStack.length).toBe(0);
        expect(useUndoStore.getState().redoStack.length).toBe(0);
    });
});

describe('UndoStore – dirty tracking', () => {
    it('isDirtyVsSaved returns false at start', () => {
        expect(useUndoStore.getState().isDirtyVsSaved()).toBe(false);
    });

    it('isDirtyVsSaved returns true after push', () => {
        useUndoStore.getState().pushSnapshot(makeManifest('A'), 'edit');
        expect(useUndoStore.getState().isDirtyVsSaved()).toBe(true);
    });

    it('markSaved + no change = not dirty', () => {
        useUndoStore.getState().pushSnapshot(makeManifest('A'), 'edit');
        useUndoStore.getState().markSaved();
        expect(useUndoStore.getState().isDirtyVsSaved()).toBe(false);
    });

    it('markSaved + further edit = dirty again', () => {
        useUndoStore.getState().pushSnapshot(makeManifest('A'), 'edit');
        useUndoStore.getState().markSaved();
        useUndoStore.getState().pushSnapshot(makeManifest('B'), 'edit 2');
        expect(useUndoStore.getState().isDirtyVsSaved()).toBe(true);
    });
});
