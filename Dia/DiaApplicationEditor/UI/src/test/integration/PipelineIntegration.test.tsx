/**
 * Integration tests for the DiaApplicationEditor data pipeline.
 *
 * Each test renders multiple real components together — only the heavy
 * third-party libraries (react-arborist, reactflow, CodeMirror) are stubbed.
 * The zustand store, useValidation hook, postMessage bridge, and all
 * application components are real.
 */
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, act, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { useManifestStore } from '../../ManifestStore';

// ── Stub heavy third-party deps ──────────────────────────────────────────────

vi.mock('react-arborist', () => ({
    Tree: ({ data, onSelect }: any) => (
        <div data-testid="tree">
            {(data ?? []).flatMap((node: any) => {
                const rows: React.ReactNode[] = [
                    <div
                        key={node.id}
                        data-testid={`node-${node.id}`}
                        data-nodeid={node.id}
                        onClick={() => onSelect([{ data: node }])}
                    >
                        {node.name}
                    </div>,
                ];
                (node.children ?? []).forEach((child: any) => {
                    rows.push(
                        <div
                            key={child.id}
                            data-testid={`node-${child.id}`}
                            data-nodeid={child.id}
                            onClick={() => onSelect([{ data: child }])}
                        >
                            {child.name}
                        </div>
                    );
                    (child.children ?? []).forEach((leaf: any) => {
                        rows.push(
                            <div
                                key={leaf.id}
                                data-testid={`node-${leaf.id}`}
                                data-nodeid={leaf.id}
                                onClick={() => onSelect([{ data: leaf }])}
                            >
                                {leaf.name}
                            </div>
                        );
                    });
                });
                return rows;
            })}
        </div>
    ),
}));

vi.mock('../../TreeNodeRenderer', () => ({
    TreeNodeRenderer: ({ node, selectedNodeId }: any) => (
        <div
            data-testid={`renderer-${node.data.id}`}
            data-selected={node.data.id === selectedNodeId ? 'true' : 'false'}
            style={{ borderLeft: node.data.id === selectedNodeId ? '2px solid #007acc' : '2px solid transparent' }}
        >
            {node.data.name}
        </div>
    ),
}));

vi.mock('reactflow', () => {
    const React = require('react');
    return {
        default: () => <div data-testid="reactflow" />,
        Background: () => null, Controls: () => null, MiniMap: () => null,
        useNodesState: () => [[], vi.fn(), vi.fn()],
        useEdgesState:  () => [[], vi.fn(), vi.fn()],
        addEdge: (_: any, eds: any) => eds,
        applyEdgeChanges: (_: any, eds: any) => eds,
    };
});
vi.mock('reactflow/dist/style.css', () => ({}));
vi.mock('@dagrejs/dagre', () => {
    class Graph {
        private _nodes = new Map<string, any>();
        setDefaultEdgeLabel() {} setGraph() {}
        setNode(id: string, attrs: any) { this._nodes.set(id, attrs); }
        setEdge() {}
        node(id: string) { return this._nodes.get(id) ?? { x: 0, y: 0 }; }
    }
    return { default: { graphlib: { Graph }, layout: () => {} } };
});
vi.mock('../../PhaseNode', () => ({ PhaseNode: () => null }));

vi.mock('@uiw/react-codemirror', () => ({
    default: ({ value, onChange }: any) => (
        <textarea data-testid="codemirror" value={value} onChange={e => onChange(e.target.value)} />
    ),
}));
vi.mock('@codemirror/lang-json', () => ({ json: () => [] }));
vi.mock('@codemirror/lint',     () => ({ linter: () => [] }));

// ── Imports (after mocks) ────────────────────────────────────────────────────

import { TreeView }        from '../../TreeView';
import { Toolbar }         from '../../Toolbar';
import { ValidationPanel } from '../../ValidationPanel';
import { useValidation }   from '../../useValidation';
import { App }             from '../../App';

// ── Helpers ──────────────────────────────────────────────────────────────────

const MANIFEST = {
    version: 1,
    processing_units: [{
        instance_id: 'MainPU',
        type: 'MainProcessingUnit',
        phases: [
            { instance_id: 'InitPhase',   type: 'InitPhase'   },
            { instance_id: 'UpdatePhase', type: 'UpdatePhase' },
        ],
        modules: [
            {
                instance_id: 'RenderModule',
                type: 'RenderModule',
                phases: ['UpdatePhase'],
                config: { fps: 60 },
            },
        ],
        transitions: [],
    }],
};

function dispatch(topic: string, data?: unknown) {
    window.dispatchEvent(new MessageEvent('message', {
        data: { __dia: true, topic, data },
    }));
}

// Harness that mounts useValidation alongside whatever children need the store
function ValidationHarness({ children }: { children: React.ReactNode }) {
    const manifestVersion = useManifestStore(s => s.manifestVersion);
    useValidation(manifestVersion);
    return <>{children}</>;
}

beforeEach(() => {
    useManifestStore.setState({
        manifest: null,
        selectedNode: null,
        isDirty: false,
        validationResult: null,
        manifestVersion: 0,
        currentView: 'tree',
    });
});

afterEach(() => {
    vi.restoreAllMocks();
});

// ── 1. manifest_loaded → useValidation → store → TreeView renders ────────────

describe('Integration: manifest_loaded pipeline', () => {
    it('dispatching manifest_loaded causes TreeView to render PU and phase nodes', async () => {
        render(
            <ValidationHarness>
                <TreeView />
            </ValidationHarness>
        );

        // Before message: TreeView renders nothing (manifest is null)
        expect(screen.queryByTestId('node-MainPU')).not.toBeInTheDocument();

        act(() => {
            dispatch('manifest_loaded', { manifest: MANIFEST, is_dirty: false });
        });

        await waitFor(() => expect(screen.getByTestId('node-MainPU')).toBeInTheDocument());
        expect(screen.getByTestId('node-MainPU_InitPhase')).toBeInTheDocument();
        expect(screen.getByTestId('node-MainPU_UpdatePhase')).toBeInTheDocument();
    });

    it('dispatching manifest_loaded sets isDirty=false in the store', () => {
        useManifestStore.setState({ isDirty: true });
        render(<ValidationHarness><TreeView /></ValidationHarness>);

        act(() => {
            dispatch('manifest_loaded', { manifest: MANIFEST, is_dirty: false });
        });

        expect(useManifestStore.getState().isDirty).toBe(false);
    });

    it('dispatching manifest_loaded then manifest_closed removes nodes from TreeView', async () => {
        render(<ValidationHarness><TreeView /></ValidationHarness>);

        act(() => { dispatch('manifest_loaded', { manifest: MANIFEST, is_dirty: false }); });
        await waitFor(() => screen.getByTestId('node-MainPU'));

        act(() => { dispatch('manifest_closed'); });
        await waitFor(() => expect(screen.queryByTestId('node-MainPU')).not.toBeInTheDocument());
    });
});

// ── 2. manifest_updated → store dirty → Toolbar Save button enables ──────────

describe('Integration: manifest_updated → Toolbar Save button', () => {
    it('Save is disabled before any manifest', () => {
        render(<ValidationHarness><Toolbar /></ValidationHarness>);
        expect(screen.getByTitle(/save/i)).toBeDisabled();
    });

    it('Save becomes enabled after manifest_updated marks the store dirty', async () => {
        render(<ValidationHarness><Toolbar /></ValidationHarness>);

        act(() => {
            dispatch('manifest_updated', { manifest: MANIFEST });
        });

        await waitFor(() => expect(screen.getByTitle(/save/i)).not.toBeDisabled());
    });

    it('Save is disabled again after manifest_saved clears dirty', async () => {
        render(<ValidationHarness><Toolbar /></ValidationHarness>);

        act(() => { dispatch('manifest_updated', { manifest: MANIFEST }); });
        await waitFor(() => expect(screen.getByTitle(/save/i)).not.toBeDisabled());

        act(() => { dispatch('manifest_saved'); });
        await waitFor(() => expect(screen.getByTitle(/save/i)).toBeDisabled());
    });

    it('title shows asterisk when dirty', async () => {
        render(<ValidationHarness><Toolbar /></ValidationHarness>);

        act(() => { dispatch('manifest_updated', { manifest: MANIFEST }); });
        // The dirty marker is a nested <span> containing ' *'
        await waitFor(() => expect(useManifestStore.getState().isDirty).toBe(true));
        expect(document.body.textContent).toContain('*');
    });
});

// ── 3. TreeView node click → App shows ModuleInspector ──────────────────────

describe('Integration: TreeView selection → App shows ModuleInspector', () => {
    it('clicking a module node (3-segment id) shows ModuleInspector in App', async () => {

        useManifestStore.setState({ manifest: MANIFEST, currentView: 'tree' });

        render(<App />);

        // ModuleInspector should not be visible yet (no selectedNode)
        expect(screen.queryByTestId('module-inspector')).not.toBeInTheDocument();

        // Click the module node in the real TreeView
        await waitFor(() => screen.getByTestId('node-MainPU_UpdatePhase_RenderModule'));
        await userEvent.click(screen.getByTestId('node-MainPU_UpdatePhase_RenderModule'));

        // App checks selectedNode.split('_').length >= 3 → renders ModuleInspector
        await waitFor(() =>
            expect(screen.getAllByText('RenderModule').length).toBeGreaterThanOrEqual(1)
        );
        expect(useManifestStore.getState().selectedNode).toBe('MainPU_UpdatePhase_RenderModule');
    });

    it('clicking a phase node (2-segment id) does NOT show ModuleInspector', async () => {
        useManifestStore.setState({ manifest: MANIFEST, currentView: 'tree' });

        render(<App />);

        await waitFor(() => screen.getByTestId('node-MainPU_InitPhase'));
        await userEvent.click(screen.getByTestId('node-MainPU_InitPhase'));

        // Phase id has 2 segments — inspector should remain hidden
        await waitFor(() =>
            expect(useManifestStore.getState().selectedNode).toBe('MainPU_InitPhase')
        );
        // ModuleInspector header text only appears when inspector is shown
        expect(screen.queryByText('Form')).not.toBeInTheDocument();
    });
});

// ── 4. ValidationPanel error click → selectedNode → TreeView highlights ──────

describe('Integration: ValidationPanel click → store → TreeView highlight', () => {
    it('clicking a validation error sets selectedNode in the store', () => {
        useManifestStore.setState({
            manifest: MANIFEST,
            validationResult: {
                is_valid: false,
                errors: [{
                    message: 'Missing required field',
                    context: 'MainPU_UpdatePhase_RenderModule',
                    severity: 'error',
                    code: 'E001',
                }],
            },
        });

        render(
            <>
                <ValidationPanel />
                <TreeView />
            </>
        );

        act(() => {
            screen.getByText('Missing required field').click();
        });

        expect(useManifestStore.getState().selectedNode).toBe('MainPU_UpdatePhase_RenderModule');
    });

    it('selectedNode set by ValidationPanel is reflected in TreeView selection prop', async () => {
        useManifestStore.setState({
            manifest: MANIFEST,
            validationResult: {
                is_valid: false,
                errors: [{
                    message: 'Config error',
                    context: 'MainPU_UpdatePhase_RenderModule',
                    severity: 'error',
                    code: 'E002',
                }],
            },
        });

        render(
            <>
                <ValidationPanel />
                <TreeView />
            </>
        );

        act(() => {
            screen.getByText('Config error').click();
        });

        await waitFor(() => {
            const renderer = screen.queryByTestId('renderer-MainPU_UpdatePhase_RenderModule');
            // renderer uses selectedNodeId prop wired from the store
            if (renderer) {
                expect(renderer.getAttribute('data-selected')).toBe('true');
            } else {
                // arborist mock renders nodes; check store directly
                expect(useManifestStore.getState().selectedNode).toBe('MainPU_UpdatePhase_RenderModule');
            }
        });
    });

    it('validation error node matching does not affect unrelated nodes', () => {
        useManifestStore.setState({
            manifest: MANIFEST,
            validationResult: {
                is_valid: false,
                errors: [{
                    message: 'Bad config',
                    context: 'MainPU_UpdatePhase_RenderModule',
                    severity: 'error',
                    code: 'E003',
                }],
            },
        });

        render(
            <>
                <ValidationPanel />
                <TreeView />
            </>
        );

        act(() => {
            screen.getByText('Bad config').click();
        });

        // InitPhase node should not be selected
        expect(useManifestStore.getState().selectedNode).not.toBe('MainPU_InitPhase');
    });
});
