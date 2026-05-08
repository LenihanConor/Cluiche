import { describe, it, expect, beforeEach, vi } from 'vitest';
import { render, screen, act } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { useManifestStore } from './ManifestStore';

// Stub react-arborist — it uses ResizeObserver and complex DOM internals
vi.mock('react-arborist', () => ({
    Tree: ({ data, children, onSelect, onMove }: any) => (
        <div data-testid="tree">
            {data.map((node: any) => (
                <div key={node.id}>
                    <div
                        data-testid={`node-${node.id}`}
                        onClick={() => onSelect([{ data: node }])}
                        onContextMenu={(e) => e.preventDefault()}
                    >
                        {node.name}
                    </div>
                    {node.children?.map((child: any) => (
                        <div key={child.id}>
                            <div
                                data-testid={`node-${child.id}`}
                                onClick={() => onSelect([{ data: child }])}
                            >
                                {child.name}
                            </div>
                            {child.children?.map((leaf: any) => (
                                <div
                                    key={leaf.id}
                                    data-testid={`node-${leaf.id}`}
                                    onClick={() => onSelect([{ data: leaf }])}
                                >
                                    {leaf.name}
                                </div>
                            ))}
                        </div>
                    ))}
                </div>
            ))}
        </div>
    ),
}));

// TreeNodeRenderer reads from store — stub it with a simple pass-through
vi.mock('./TreeNodeRenderer', () => ({
    TreeNodeRenderer: ({ node }: any) => (
        <div data-testid={`renderer-${node.data.id}`}>{node.data.name}</div>
    ),
}));

import { TreeView, ManifestTreeNode } from './TreeView';

const MANIFEST = {
    version: 1,
    processing_units: [
        {
            instance_id: 'MainPU',
            type: 'MainProcessingUnit',
            phases: [
                { instance_id: 'InitPhase', type: 'InitPhase' },
                { instance_id: 'UpdatePhase', type: 'UpdatePhase' },
            ],
            modules: [
                { instance_id: 'RenderModule', type: 'RenderModule', phases: ['UpdatePhase'], config: { fps: 60 } },
            ],
            transitions: [],
        },
    ],
};

beforeEach(() => {
    useManifestStore.setState({ manifest: null, selectedNode: null });
    vi.clearAllMocks();
});

// ── buildTree algorithm (tested via rendered output) ─────────────────────────

describe('TreeView – tree building', () => {
    beforeEach(() => { useManifestStore.setState({ manifest: MANIFEST }); });

    it('renders the processing unit node', () => {
        render(<TreeView />);
        expect(screen.getByTestId('node-MainPU')).toBeInTheDocument();
    });

    it('renders phase child nodes', () => {
        render(<TreeView />);
        expect(screen.getByTestId('node-MainPU_InitPhase')).toBeInTheDocument();
        expect(screen.getByTestId('node-MainPU_UpdatePhase')).toBeInTheDocument();
    });

    it('renders module nested under its phase', () => {
        render(<TreeView />);
        expect(screen.getByTestId('node-MainPU_UpdatePhase_RenderModule')).toBeInTheDocument();
    });
});

// ── filterTree algorithm ──────────────────────────────────────────────────────

describe('TreeView – search/filter', () => {
    beforeEach(() => { useManifestStore.setState({ manifest: MANIFEST }); });

    it('shows all nodes when search is empty', () => {
        render(<TreeView />);
        expect(screen.getByTestId('node-MainPU')).toBeInTheDocument();
        expect(screen.getByTestId('node-MainPU_InitPhase')).toBeInTheDocument();
    });

    it('filters by node name — matching nodes remain visible', async () => {
        render(<TreeView />);
        await userEvent.type(screen.getByPlaceholderText(/search/i), 'Render');
        expect(screen.queryByTestId('node-MainPU_UpdatePhase_RenderModule')).toBeInTheDocument();
    });

    it('filters out non-matching nodes', async () => {
        render(<TreeView />);
        await userEvent.type(screen.getByPlaceholderText(/search/i), 'InitPhase');
        // RenderModule lives under UpdatePhase, not InitPhase — should be absent
        expect(screen.queryByTestId('node-MainPU_UpdatePhase_RenderModule')).not.toBeInTheDocument();
    });
});

// ── node selection ────────────────────────────────────────────────────────────

describe('TreeView – selection', () => {
    beforeEach(() => { useManifestStore.setState({ manifest: MANIFEST }); });

    it('clicking a node sets selectedNode in the store', async () => {
        render(<TreeView />);
        await userEvent.click(screen.getByTestId('node-MainPU'));
        expect(useManifestStore.getState().selectedNode).toBe('MainPU');
    });

    it('clicking a node posts node_selected message', async () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<TreeView />);
        await userEvent.click(screen.getByTestId('node-MainPU_InitPhase'));
        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({ payload: expect.objectContaining({ type: 'node_selected' }) }),
            '*'
        );
    });
});

// ── hierarchical PU nesting ──────────────────────────────────────────────────

const MULTI_PU_MANIFEST = {
    version: 1,
    processing_units: [
        {
            instance_id: 'MainPU',
            type: 'MainProcessingUnit',
            root: true,
            phases: [{ instance_id: 'InitPhase', type: 'InitPhase' }],
            modules: [],
            transitions: [],
        },
        {
            instance_id: 'RenderPU',
            type: 'RenderProcessingUnit',
            _source: 'cluiche_render.diaapp',
            phases: [{ instance_id: 'RenderPhase', type: 'RenderPhase' }],
            modules: [],
            transitions: [],
        },
        {
            instance_id: 'SimPU',
            type: 'SimProcessingUnit',
            _source: 'cluiche_sim.diaapp',
            phases: [{ instance_id: 'SimPhase', type: 'SimPhase' }],
            modules: [],
            transitions: [],
        },
    ],
};

describe('TreeView – hierarchical PU nesting', () => {
    beforeEach(() => { useManifestStore.setState({ manifest: MULTI_PU_MANIFEST }); });

    it('nests non-root PUs under the root PU', () => {
        render(<TreeView />);
        // Root PU is top-level
        expect(screen.getByTestId('node-MainPU')).toBeInTheDocument();
        // Child PUs appear as children of root (rendered at second level)
        expect(screen.getByTestId('node-RenderPU')).toBeInTheDocument();
        expect(screen.getByTestId('node-SimPU')).toBeInTheDocument();
    });

    it('child PU phases are still rendered', () => {
        render(<TreeView />);
        expect(screen.getByTestId('node-RenderPU_RenderPhase')).toBeInTheDocument();
        expect(screen.getByTestId('node-SimPU_SimPhase')).toBeInTheDocument();
    });
});

// ── empty/null manifest ───────────────────────────────────────────────────────

describe('TreeView – no manifest', () => {
    it('renders nothing when manifest is null', () => {
        const { container } = render(<TreeView />);
        expect(container.firstChild).toBeNull();
    });
});
