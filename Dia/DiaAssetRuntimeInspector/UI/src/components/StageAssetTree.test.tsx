import { render, screen, fireEvent } from '@testing-library/react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import { act } from 'react';
import { StageAssetTree } from './StageAssetTree';
import { useAssetRuntimeStore } from '../store';

// ─── Mock @dia/editor-ui (not used directly but imported transitively) ────────

vi.mock('@dia/editor-ui', () => ({
    theme: {
        bg: '#1e1e1e', bgPanel: '#252526', bgInput: '#2d2d2d', border: '#3c3c3c',
        borderMuted: '#555', text: '#d4d4d4', textMuted: '#888', accent: '#0e639c',
        accentHover: '#007acc', success: '#89d185', warning: '#cca700', error: '#f48771',
        textDim: '#ccc',
    },
    injectThemeVars: vi.fn(),
    ConnectionStatus: ({ state }: any) => (
        <span data-testid="conn-status" data-state={state} />
    ),
    TabBar: ({ tabs, activeTab, onTabChange }: any) => (
        <div role="tablist">
            {tabs.map((t: any) => (
                <button
                    key={t.id}
                    role="tab"
                    aria-selected={activeTab === t.id}
                    data-tab-id={t.id}
                    onClick={() => onTabChange(t.id)}
                >
                    {t.label}
                </button>
            ))}
        </div>
    ),
}));

// ─── postMessage mock ─────────────────────────────────────────────────────────

beforeEach(() => {
    // Reset store to clean state
    useAssetRuntimeStore.setState({
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
    });

    // Mock window.parent postMessage
    Object.defineProperty(window, 'parent', { value: window, writable: true });
    vi.spyOn(window, 'postMessage').mockImplementation(() => {});
});

// ─── Tests ────────────────────────────────────────────────────────────────────

describe('StageAssetTree', () => {
    // Test 1: Renders stage nodes from treeNodes store
    it('renders stage nodes from treeNodes store', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                treeNodes: [
                    { stageId: 'Stage01', assetCount: 3, expanded: false, childrenLoaded: false },
                    { stageId: 'Stage02', assetCount: 7, expanded: false, childrenLoaded: false },
                ],
            });
        });

        render(<StageAssetTree />);

        expect(screen.getByTestId('stage-node-Stage01')).toBeInTheDocument();
        expect(screen.getByTestId('stage-node-Stage02')).toBeInTheDocument();
        expect(screen.getByText('Stage01')).toBeInTheDocument();
        expect(screen.getByText('Stage02')).toBeInTheDocument();
        expect(screen.getByText('(3)')).toBeInTheDocument();
        expect(screen.getByText('(7)')).toBeInTheDocument();
    });

    // Test 2: [Global] node renders global assets when expanded=true
    it('[Global] node renders global assets when expanded=true', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                treeNodes: [
                    { stageId: '[Global]', assetCount: 2, expanded: true, childrenLoaded: true },
                ],
                globalAssets: [
                    { id: 'global-asset-1', state: 'Loaded', refCount: 2 },
                    { id: 'global-asset-2', state: 'Loading', refCount: 1 },
                ],
            });
        });

        render(<StageAssetTree />);

        expect(screen.getByTestId('asset-row-global-asset-1')).toBeInTheDocument();
        expect(screen.getByTestId('asset-row-global-asset-2')).toBeInTheDocument();
        expect(screen.getByText('global-asset-1')).toBeInTheDocument();
        expect(screen.getByText('global-asset-2')).toBeInTheDocument();
    });

    // Test 3: State dot renders (check element exists per asset)
    it('renders a state dot for each global asset', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                treeNodes: [
                    { stageId: '[Global]', assetCount: 3, expanded: true, childrenLoaded: true },
                ],
                globalAssets: [
                    { id: 'g1', state: 'Loaded',   refCount: 1 },
                    { id: 'g2', state: 'Failed',   refCount: 0 },
                    { id: 'g3', state: 'Unloaded', refCount: 0 },
                ],
            });
        });

        render(<StageAssetTree />);

        const dots = screen.getAllByTestId('state-dot');
        expect(dots).toHaveLength(3);
    });

    // Test 4: Expand toggle click fires expand_stage bridge request with correct stageId
    it('expand toggle fires expand_stage bridge request', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                treeNodes: [
                    { stageId: 'MyStage', assetCount: 2, expanded: false, childrenLoaded: false },
                ],
            });
        });

        render(<StageAssetTree />);

        fireEvent.click(screen.getByTestId('stage-header-MyStage'));

        expect(window.postMessage).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: expect.objectContaining({
                    type: 'asset_runtime_inspector.expand_stage',
                    data: { stageId: 'MyStage' },
                }),
            }),
            '*',
        );
    });

    // Test 5: Collapse toggle click fires collapse_stage bridge request
    it('collapse toggle fires collapse_stage bridge request', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                treeNodes: [
                    { stageId: 'MyStage', assetCount: 2, expanded: true, childrenLoaded: true },
                ],
                stageChildren: { MyStage: [] },
            });
        });

        render(<StageAssetTree />);

        fireEvent.click(screen.getByTestId('stage-header-MyStage'));

        expect(window.postMessage).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: expect.objectContaining({
                    type: 'asset_runtime_inspector.collapse_stage',
                    data: { stageId: 'MyStage' },
                }),
            }),
            '*',
        );
    });

    // Test 6: Asset click fires tree_select_asset bridge request
    it('asset click fires tree_select_asset bridge request', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                treeNodes: [
                    { stageId: '[Global]', assetCount: 1, expanded: true, childrenLoaded: true },
                ],
                globalAssets: [
                    { id: 'clickable-asset', state: 'Loaded', refCount: 1 },
                ],
            });
        });

        render(<StageAssetTree />);

        fireEvent.click(screen.getByTestId('asset-row-clickable-asset'));

        expect(window.postMessage).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: expect.objectContaining({
                    type: 'asset_runtime_inspector.tree_select_asset',
                    data: { assetId: 'clickable-asset' },
                }),
            }),
            '*',
        );
    });

    // Test 7: Ref count badge shown on global assets
    it('ref count badge shown on global assets', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                treeNodes: [
                    { stageId: '[Global]', assetCount: 2, expanded: true, childrenLoaded: true },
                ],
                globalAssets: [
                    { id: 'g-ref-1', state: 'Loaded', refCount: 5 },
                    { id: 'g-ref-2', state: 'Staged', refCount: 0 },
                ],
            });
        });

        render(<StageAssetTree />);

        expect(screen.getByTestId('ref-badge-g-ref-1')).toHaveTextContent('ref:5');
        expect(screen.getByTestId('ref-badge-g-ref-2')).toHaveTextContent('ref:0');
    });

    // Test 8: [Global] node absent when globalAssets is empty AND no [Global] treeNode exists
    it('[Global] node absent when no globalAssets and no [Global] treeNode', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                treeNodes: [
                    { stageId: 'Stage01', assetCount: 3, expanded: false, childrenLoaded: false },
                ],
                globalAssets: [],
            });
        });

        render(<StageAssetTree />);

        expect(screen.queryByTestId('stage-node-[Global]')).toBeNull();
        expect(screen.queryByText('[Global]')).toBeNull();
    });

    // Test 9: Non-global stage renders children from stageChildren store slice
    it('non-global stage renders asset ids from stageChildren when expanded', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                treeNodes: [
                    { stageId: 'GameStage', assetCount: 2, expanded: true, childrenLoaded: true },
                ],
                stageChildren: {
                    GameStage: ['asset-crc-001', 'asset-crc-002'],
                },
            });
        });

        render(<StageAssetTree />);

        expect(screen.getByTestId('asset-row-asset-crc-001')).toBeInTheDocument();
        expect(screen.getByTestId('asset-row-asset-crc-002')).toBeInTheDocument();
        expect(screen.getByText('asset-crc-001')).toBeInTheDocument();
    });

    // Test 10: Selected asset gets highlighted background
    it('selected asset has highlighted background', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                treeNodes: [
                    { stageId: '[Global]', assetCount: 2, expanded: true, childrenLoaded: true },
                ],
                globalAssets: [
                    { id: 'selected-id', state: 'Loaded', refCount: 1 },
                    { id: 'other-id',    state: 'Loaded', refCount: 1 },
                ],
                selectedAssetId: 'selected-id',
            });
        });

        render(<StageAssetTree />);

        const selectedRow = screen.getByTestId('asset-row-selected-id');
        const otherRow    = screen.getByTestId('asset-row-other-id');

        // Selected row has non-empty background; other row does not
        expect(selectedRow.style.background).not.toBe('');
        expect(otherRow.style.background).toBe('');
    });
});
