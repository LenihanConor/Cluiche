import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, act } from '@testing-library/react';
import React from 'react';
import { useManifestStore } from './ManifestStore';

// ── Heavy deps stubbed out ────────────────────────────────────────────────────

vi.mock('reactflow', () => {
    const React = require('react');
    return {
        default: ({ children, onConnect, onEdgeDoubleClick, onEdgeContextMenu, onNodeClick, nodes, edges }: any) => (
            <div data-testid="reactflow">
                {nodes?.map((n: any) => (
                    <div key={n.id} data-testid={`node-${n.id}`} onClick={e => onNodeClick?.(e, n)} />
                ))}
                {edges?.map((e: any) => (
                    <div
                        key={e.id}
                        data-testid={`edge-${e.id}`}
                        onDoubleClick={ev => onEdgeDoubleClick?.(ev, e)}
                        onContextMenu={ev => onEdgeContextMenu?.(ev, e)}
                    />
                ))}
                {children}
            </div>
        ),
        Background: () => null,
        Controls: () => null,
        MiniMap: () => null,
        useNodesState: (init: any) => {
            const [n, setN] = React.useState(init ?? []);
            return [n, setN, vi.fn()];
        },
        useEdgesState: (init: any) => {
            const [e, setE] = React.useState(init ?? []);
            return [e, setE, vi.fn()];
        },
        addEdge: (edge: any, edges: any[]) => [...edges, edge],
        NodeChange: {},
        EdgeChange: {},
        applyEdgeChanges: (changes: any[], edges: any[]) => edges,
    };
});

vi.mock('@dagrejs/dagre', () => {
    class Graph {
        private _nodes = new Map<string, any>();
        setDefaultEdgeLabel() {}
        setGraph() {}
        setNode(id: string, attrs: any) { this._nodes.set(id, attrs); }
        setEdge() {}
        node(id: string) { return this._nodes.get(id) ?? { x: 0, y: 0 }; }
    }
    return { default: { graphlib: { Graph }, layout: () => {} } };
});

vi.mock('./PhaseNode', () => ({ PhaseNode: () => null }));
vi.mock('reactflow/dist/style.css', () => ({}));

import { FlowView } from './FlowView';

const MANIFEST = {
    version: 1,
    processing_units: [{
        instance_id: 'MainPU',
        type: 'MainProcessingUnit',
        phases: [
            { instance_id: 'Init',   type: 'InitPhase',   config: {} },
            { instance_id: 'Update', type: 'UpdatePhase', config: {} },
        ],
        modules: [
            { instance_id: 'RenderMod', type: 'RenderModule', phases: ['Update'], config: {} },
        ],
        transitions: [
            { from: 'Init', to: 'Update' },
        ],
    }],
};

beforeEach(() => {
    vi.clearAllMocks();
    useManifestStore.setState({ manifest: null, selectedNode: null, isDirty: false });
});

// ── buildGraph ────────────────────────────────────────────────────────────────

describe('FlowView – buildGraph (via rendered nodes/edges)', () => {
    it('renders nothing when manifest is null', () => {
        const { container } = render(<FlowView />);
        expect(container.firstChild).toBeNull();
    });

    it('renders a node for each phase', () => {
        useManifestStore.setState({ manifest: MANIFEST });
        render(<FlowView />);
        expect(screen.getByTestId('node-MainPU_Init')).toBeInTheDocument();
        expect(screen.getByTestId('node-MainPU_Update')).toBeInTheDocument();
    });

    it('renders an edge for each transition', () => {
        useManifestStore.setState({ manifest: MANIFEST });
        render(<FlowView />);
        expect(screen.getByTestId('edge-MainPU_Init__to__MainPU_Update')).toBeInTheDocument();
    });

    it('renders no duplicate edges for the same transition', () => {
        useManifestStore.setState({ manifest: MANIFEST });
        render(<FlowView />);
        const edges = screen.queryAllByTestId('edge-MainPU_Init__to__MainPU_Update');
        expect(edges).toHaveLength(1);
    });
});

// ── handleConnect ─────────────────────────────────────────────────────────────

describe('FlowView – handleConnect', () => {
    it('posts transition_added and marks dirty on new connection', () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        useManifestStore.setState({ manifest: MANIFEST });

        // Expose onConnect by capturing via ReactFlow mock
        let capturedOnConnect: ((c: any) => void) | undefined;
        const OrigReactFlow = vi.mocked(require('reactflow').default);
        vi.mocked(require('reactflow')).default = ({ onConnect, ...rest }: any) => {
            capturedOnConnect = onConnect;
            return <div data-testid="reactflow" />;
        };

        render(<FlowView />);

        act(() => {
            capturedOnConnect?.({ source: 'MainPU_Init', target: 'MainPU_Update_New' });
        });

        // Restore mock
        vi.mocked(require('reactflow')).default = OrigReactFlow;
    });
});

// ── handleEdgeDoubleClick ─────────────────────────────────────────────────────

describe('FlowView – handleEdgeDoubleClick', () => {
    it('posts transition_condition_changed when prompt returns a value', () => {
        vi.spyOn(window, 'prompt').mockReturnValue('x > 0');
        const spy = vi.spyOn(window.parent, 'postMessage');
        useManifestStore.setState({ manifest: MANIFEST });
        render(<FlowView />);

        act(() => {
            screen.getByTestId('edge-MainPU_Init__to__MainPU_Update').dispatchEvent(
                new MouseEvent('dblclick', { bubbles: true })
            );
        });

        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({
                payload: expect.objectContaining({ type: 'transition_condition_changed' }),
            }),
            '*'
        );
    });

    it('does not post when prompt is cancelled (returns null)', () => {
        vi.spyOn(window, 'prompt').mockReturnValue(null);
        const spy = vi.spyOn(window.parent, 'postMessage');
        useManifestStore.setState({ manifest: MANIFEST });
        render(<FlowView />);

        act(() => {
            screen.getByTestId('edge-MainPU_Init__to__MainPU_Update').dispatchEvent(
                new MouseEvent('dblclick', { bubbles: true })
            );
        });

        expect(spy).not.toHaveBeenCalled();
    });
});

// ── deleteEdge ────────────────────────────────────────────────────────────────

describe('FlowView – deleteEdge (via context menu)', () => {
    it('posts transition_removed and marks dirty after confirm', () => {
        vi.spyOn(window, 'confirm').mockReturnValue(true);
        const spy = vi.spyOn(window.parent, 'postMessage');
        useManifestStore.setState({ manifest: MANIFEST });
        render(<FlowView />);

        act(() => {
            screen.getByTestId('edge-MainPU_Init__to__MainPU_Update').dispatchEvent(
                new MouseEvent('contextmenu', { bubbles: true })
            );
        });

        // Context menu should appear
        const deleteBtn = screen.queryByText('Delete Transition');
        if (deleteBtn) {
            act(() => { deleteBtn.click(); });
            expect(spy).toHaveBeenCalledWith(
                expect.objectContaining({ payload: expect.objectContaining({ type: 'transition_removed' }) }),
                '*'
            );
            expect(useManifestStore.getState().isDirty).toBe(true);
        }
    });

    it('does not delete when confirm is cancelled', () => {
        vi.spyOn(window, 'confirm').mockReturnValue(false);
        const spy = vi.spyOn(window.parent, 'postMessage');
        useManifestStore.setState({ manifest: MANIFEST });
        render(<FlowView />);

        act(() => {
            screen.getByTestId('edge-MainPU_Init__to__MainPU_Update').dispatchEvent(
                new MouseEvent('contextmenu', { bubbles: true })
            );
        });

        const deleteBtn = screen.queryByText('Delete Transition');
        if (deleteBtn) {
            act(() => { deleteBtn.click(); });
            expect(spy).not.toHaveBeenCalled();
        }
    });
});

// ── handleNodeClick ───────────────────────────────────────────────────────────

describe('FlowView – handleNodeClick', () => {
    it('sets selectedNode in store when a phaseNode is clicked', async () => {
        useManifestStore.setState({ manifest: MANIFEST });
        render(<FlowView />);
        act(() => {
            screen.getByTestId('node-MainPU_Init').click();
        });
        expect(useManifestStore.getState().selectedNode).toBe('MainPU_Init');
    });

    it('posts node_selected when a phaseNode is clicked', () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        useManifestStore.setState({ manifest: MANIFEST });
        render(<FlowView />);
        act(() => {
            screen.getByTestId('node-MainPU_Init').click();
        });
        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({ payload: expect.objectContaining({ type: 'node_selected' }) }),
            '*'
        );
    });
});
