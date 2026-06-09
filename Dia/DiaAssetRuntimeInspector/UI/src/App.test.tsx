import { render, screen, fireEvent, act } from '@testing-library/react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import App from './App';
import { useAssetRuntimeStore } from './store';

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

const initialState = {
    connected: false,
    assets: [] as any[],
    total: 0,
    stateFilter: '',
    idSearch: '',
    treeNodes: [] as any[],
    globalAssets: [] as any[],
    selectedAssetId: '',
    stageChildren: {} as Record<string, string[]>,
    inspectorData: null,
    logEntries: [] as any[],
    logTotal: 0,
    logPaused: false,
    logMaxEntries: 0,
    activeTab: 'table' as const,
};

beforeEach(() => {
    useAssetRuntimeStore.setState(initialState);
});

// ─── Disconnect overlay ───────────────────────────────────────────────────────

describe('App — disconnect overlay', () => {
    it('shows disconnect overlay by default (not connected)', () => {
        render(<App />);
        expect(screen.getByTestId('disconnect-overlay')).toBeInTheDocument();
    });

    it('hides disconnect overlay when connected=true', () => {
        act(() => { useAssetRuntimeStore.setState({ connected: true }); });
        render(<App />);
        expect(screen.queryByTestId('disconnect-overlay')).toBeNull();
    });

    it('shows disconnect overlay when connected=false', () => {
        act(() => { useAssetRuntimeStore.setState({ connected: false }); });
        render(<App />);
        expect(screen.getByTestId('disconnect-overlay')).toBeInTheDocument();
    });
});

// ─── Bridge wiring via postMessage ───────────────────────────────────────────

describe('App — bridge wiring (postMessage)', () => {
    it('connection_state { connected: true } hides overlay', () => {
        render(<App />);
        expect(screen.getByTestId('disconnect-overlay')).toBeInTheDocument();

        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: {
                    __dia: true,
                    topic: 'asset_runtime_inspector.connection_state',
                    data: { connected: true },
                },
            }));
        });

        expect(screen.queryByTestId('disconnect-overlay')).toBeNull();
    });

    it('connection_state { connected: false } shows overlay', () => {
        act(() => { useAssetRuntimeStore.setState({ connected: true }); });
        render(<App />);
        expect(screen.queryByTestId('disconnect-overlay')).toBeNull();

        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: {
                    __dia: true,
                    topic: 'asset_runtime_inspector.connection_state',
                    data: { connected: false },
                },
            }));
        });

        expect(screen.getByTestId('disconnect-overlay')).toBeInTheDocument();
    });

    it('snapshot topic calls setSnapshot', () => {
        render(<App />);
        const assets = [
            { id: 'crc-001', state: 'Loaded', scope: 'Global', refCount: 2, deployPath: 'tex/hero.png' },
        ];
        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: {
                    __dia: true,
                    topic: 'asset_runtime_inspector.snapshot',
                    data: { assets, total: 1 },
                },
            }));
        });
        expect(useAssetRuntimeStore.getState().assets).toEqual(assets);
        expect(useAssetRuntimeStore.getState().total).toBe(1);
    });

    it('log_entry topic calls appendLogEntry', () => {
        render(<App />);
        const entry = { timestamp: 9999, type: 'disconnect' as const };
        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: {
                    __dia: true,
                    topic: 'asset_runtime_inspector.log_entry',
                    data: { entry, total: 1, paused: false },
                },
            }));
        });
        const state = useAssetRuntimeStore.getState();
        expect(state.logEntries[0]).toEqual(entry);
        expect(state.logTotal).toBe(1);
    });
});

// ─── DiaEditor_onDataChanged direct call ─────────────────────────────────────

describe('App — DiaEditor_onDataChanged direct call', () => {
    it('sets window.DiaEditor_onDataChanged on mount', () => {
        render(<App />);
        expect(typeof (window as any).DiaEditor_onDataChanged).toBe('function');
    });

    it('direct call dispatches connection_state correctly', () => {
        render(<App />);
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'asset_runtime_inspector.connection_state',
                data: { connected: true },
            });
        });
        expect(useAssetRuntimeStore.getState().connected).toBe(true);
    });

    it('direct call dispatches snapshot correctly', () => {
        render(<App />);
        const assets = [
            { id: 'crc-002', state: 'Loading', scope: 'Stage', refCount: 1, deployPath: 'audio/sfx.wav' },
        ];
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'asset_runtime_inspector.snapshot',
                data: { assets, total: 7 },
            });
        });
        expect(useAssetRuntimeStore.getState().assets).toEqual(assets);
        expect(useAssetRuntimeStore.getState().total).toBe(7);
    });

    it('direct call for table_filters sets stateFilter and idSearch', () => {
        render(<App />);
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'asset_runtime_inspector.table_filters',
                data: { stateFilter: 'Loaded', idSearch: 'hero' },
            });
        });
        expect(useAssetRuntimeStore.getState().stateFilter).toBe('Loaded');
        expect(useAssetRuntimeStore.getState().idSearch).toBe('hero');
    });

    it('direct call for tree_data updates treeNodes and globalAssets', () => {
        render(<App />);
        const stages = [{ stageId: 's1', assetCount: 3, expanded: false, childrenLoaded: false }];
        const globalAssets = [{ id: 'g1', state: 'Loaded' as const, refCount: 1 }];
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'asset_runtime_inspector.tree_data',
                data: { stages, globalAssets, selectedAssetId: 'g1' },
            });
        });
        expect(useAssetRuntimeStore.getState().treeNodes).toEqual(stages);
        expect(useAssetRuntimeStore.getState().globalAssets).toEqual(globalAssets);
        expect(useAssetRuntimeStore.getState().selectedAssetId).toBe('g1');
    });

    it('direct call for stage_children updates stageChildren', () => {
        render(<App />);
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'asset_runtime_inspector.stage_children',
                data: { stageId: 'stage-01', assets: ['crc-a', 'crc-b'] },
            });
        });
        expect(useAssetRuntimeStore.getState().stageChildren['stage-01']).toEqual(['crc-a', 'crc-b']);
    });

    it('direct call for log_data sets log entries', () => {
        render(<App />);
        const entries = [{ timestamp: 100, type: 'reconnect' as const }];
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'asset_runtime_inspector.log_data',
                data: { entries, total: 1, paused: true, maxEntries: 500 },
            });
        });
        const state = useAssetRuntimeStore.getState();
        expect(state.logEntries).toEqual(entries);
        expect(state.logPaused).toBe(true);
        expect(state.logMaxEntries).toBe(500);
    });
});

// ─── Tab switching ────────────────────────────────────────────────────────────

describe('App — tab switching', () => {
    it('default tab shows Asset State Table placeholder', () => {
        render(<App />);
        expect(screen.getByTestId('tab-table')).toBeInTheDocument();
    });

    it('clicking Stage Tree tab shows tree placeholder', () => {
        render(<App />);
        const treeTab = screen.getByRole('tab', { name: /Stage Tree/i });
        fireEvent.click(treeTab);
        expect(useAssetRuntimeStore.getState().activeTab).toBe('tree');
        expect(screen.getByTestId('tab-tree')).toBeInTheDocument();
    });

    it('clicking Ref Count tab shows inspector placeholder', () => {
        render(<App />);
        const inspectorTab = screen.getByRole('tab', { name: /Ref Count/i });
        fireEvent.click(inspectorTab);
        expect(useAssetRuntimeStore.getState().activeTab).toBe('inspector');
        expect(screen.getByTestId('tab-inspector')).toBeInTheDocument();
    });

    it('clicking Transition Log tab shows log placeholder', () => {
        render(<App />);
        const logTab = screen.getByRole('tab', { name: /Transition Log/i });
        fireEvent.click(logTab);
        expect(useAssetRuntimeStore.getState().activeTab).toBe('log');
        expect(screen.getByTestId('tab-log')).toBeInTheDocument();
    });

    it('setActiveTab via store also updates rendered tab', () => {
        render(<App />);
        act(() => { useAssetRuntimeStore.setState({ activeTab: 'inspector' }); });
        expect(screen.getByTestId('tab-inspector')).toBeInTheDocument();
    });
});
