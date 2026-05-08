/**
 * Pure unit tests for the tree-manipulation algorithms in DockingManager.tsx.
 * Because the functions are module-private, we duplicate the minimal logic here
 * and keep the tests in sync with the source.  Any change to the algorithm must
 * be reflected here — the tests act as a specification.
 */
import { describe, it, expect } from 'vitest';
import type { MosaicNode } from 'react-mosaic-component';

type PanelId = string;

// ── Copied algorithm implementations ─────────────────────────────────────────

function buildTree(panelNames: PanelId[]): MosaicNode<PanelId> | null {
    if (panelNames.length === 0) return null;
    if (panelNames.length === 1) return panelNames[0];
    const mid = Math.ceil(panelNames.length / 2);
    const left  = buildTree(panelNames.slice(0, mid));
    const right = buildTree(panelNames.slice(mid));
    if (left == null)  return right;
    if (right == null) return left;
    return {
        direction: panelNames.length > 2 ? 'column' : 'row',
        first: left,
        second: right,
        splitPercentage: 50,
    };
}

function collectLeaves(node: MosaicNode<PanelId> | null): PanelId[] {
    if (node == null) return [];
    if (typeof node === 'string') return [node];
    return [...collectLeaves(node.first), ...collectLeaves(node.second)];
}

function removeFromLayout(node: MosaicNode<PanelId> | null, id: PanelId): MosaicNode<PanelId> | null {
    if (node == null) return null;
    if (typeof node === 'string') return node === id ? null : node;
    const first  = removeFromLayout(node.first,  id);
    const second = removeFromLayout(node.second, id);
    if (first  == null) return second;
    if (second == null) return first;
    return { ...node, first, second };
}

function addToLayout(node: MosaicNode<PanelId> | null, id: PanelId): MosaicNode<PanelId> {
    if (node == null) return id;
    return { direction: 'row', first: node, second: id, splitPercentage: 70 };
}

// ── buildTree ─────────────────────────────────────────────────────────────────

describe('buildTree', () => {
    it('returns null for empty array', () => {
        expect(buildTree([])).toBeNull();
    });

    it('returns the panel name string for a single panel', () => {
        expect(buildTree(['Console'])).toBe('Console');
    });

    it('returns a row split for two panels', () => {
        const tree = buildTree(['A', 'B']);
        expect(tree).toMatchObject({ direction: 'row', first: 'A', second: 'B', splitPercentage: 50 });
    });

    it('returns a column split for three panels', () => {
        const tree = buildTree(['A', 'B', 'C']) as any;
        expect(tree.direction).toBe('column');
        // Left half: ceil(3/2)=2 → ['A','B'] tree; right: ['C']
        expect(collectLeaves(tree)).toEqual(['A', 'B', 'C']);
    });

    it('produces a balanced tree for four panels', () => {
        const tree = buildTree(['A', 'B', 'C', 'D']) as any;
        expect(collectLeaves(tree)).toEqual(['A', 'B', 'C', 'D']);
    });

    it('all panels appear exactly once as leaves', () => {
        const panels = ['P1', 'P2', 'P3', 'P4', 'P5'];
        const leaves = collectLeaves(buildTree(panels));
        expect(leaves.sort()).toEqual([...panels].sort());
    });
});

// ── collectLeaves ─────────────────────────────────────────────────────────────

describe('collectLeaves', () => {
    it('returns [] for null', () => {
        expect(collectLeaves(null)).toEqual([]);
    });

    it('returns [id] for a leaf string', () => {
        expect(collectLeaves('Console')).toEqual(['Console']);
    });

    it('collects all leaves from a nested tree', () => {
        const tree: MosaicNode<PanelId> = {
            direction: 'row',
            first: { direction: 'column', first: 'A', second: 'B', splitPercentage: 50 },
            second: 'C',
            splitPercentage: 50,
        };
        expect(collectLeaves(tree)).toEqual(['A', 'B', 'C']);
    });
});

// ── removeFromLayout ─────────────────────────────────────────────────────────

describe('removeFromLayout', () => {
    it('returns null for null input', () => {
        expect(removeFromLayout(null, 'X')).toBeNull();
    });

    it('returns null when removing the only leaf', () => {
        expect(removeFromLayout('A', 'A')).toBeNull();
    });

    it('returns the other leaf when removing one side of a two-panel tree', () => {
        const tree: MosaicNode<PanelId> = { direction: 'row', first: 'A', second: 'B', splitPercentage: 50 };
        expect(removeFromLayout(tree, 'A')).toBe('B');
        expect(removeFromLayout(tree, 'B')).toBe('A');
    });

    it('correctly removes a deeply nested leaf', () => {
        const tree = buildTree(['A', 'B', 'C'])!;
        const result = removeFromLayout(tree, 'B');
        const leaves = collectLeaves(result);
        expect(leaves).toContain('A');
        expect(leaves).toContain('C');
        expect(leaves).not.toContain('B');
    });

    it('is a no-op when the id is not in the tree', () => {
        const tree = buildTree(['A', 'B'])!;
        expect(removeFromLayout(tree, 'Z')).toEqual(tree);
    });
});

// ── addToLayout ───────────────────────────────────────────────────────────────

describe('addToLayout', () => {
    it('returns the id string when adding to null', () => {
        expect(addToLayout(null, 'X')).toBe('X');
    });

    it('wraps existing tree in a row split with 70/30', () => {
        const result = addToLayout('A', 'B') as any;
        expect(result).toMatchObject({ direction: 'row', first: 'A', second: 'B', splitPercentage: 70 });
    });

    it('preserves existing tree structure as the first branch', () => {
        const existing = buildTree(['A', 'B'])!;
        const result = addToLayout(existing, 'C') as any;
        expect(result.first).toEqual(existing);
        expect(result.second).toBe('C');
    });

    it('new panel appears in collectLeaves output', () => {
        const tree = buildTree(['A', 'B'])!;
        const result = addToLayout(tree, 'C');
        expect(collectLeaves(result)).toContain('C');
    });
});
