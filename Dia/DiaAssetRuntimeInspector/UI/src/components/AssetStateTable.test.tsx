import { render, screen, fireEvent, act, within } from '@testing-library/react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import { AssetStateTable } from './AssetStateTable';
import { useAssetRuntimeStore } from '../store';
import type { AssetRow } from '../types';

// ─── Mock @dia/editor-ui ──────────────────────────────────────────────────────

vi.mock('@dia/editor-ui', () => ({
    theme: {
        bg: '#1e1e1e', bgPanel: '#252526', bgInput: '#2d2d2d', border: '#3c3c3c',
        borderMuted: '#555', text: '#d4d4d4', textMuted: '#888', accent: '#0e639c',
        accentHover: '#007acc', success: '#89d185', warning: '#cca700', error: '#f48771',
        textDim: '#ccc', bgHover: '#2a2d2e',
    },
    injectThemeVars: vi.fn(),
    ConnectionStatus: ({ state }: any) => <span data-testid="conn-status" data-state={state} />,
    TabBar: ({ tabs, activeTab, onTabChange }: any) => (
        <div role="tablist">
            {tabs.map((t: any) => (
                <button key={t.id} role="tab" aria-selected={activeTab === t.id} onClick={() => onTabChange(t.id)}>
                    {t.label}
                </button>
            ))}
        </div>
    ),
}));

// ─── Sample data ──────────────────────────────────────────────────────────────

const SAMPLE_ASSETS: AssetRow[] = [
    { id: 'crc-001', state: 'Loaded',   scope: 'Global', refCount: 3, deployPath: 'tex/hero.png' },
    { id: 'crc-002', state: 'Loading',  scope: 'Stage',  refCount: 1, deployPath: 'audio/sfx.wav' },
    { id: 'crc-003', state: 'Failed',   scope: 'Global', refCount: 0, deployPath: 'mesh/wall.obj' },
    { id: 'crc-004', state: 'Staged',   scope: 'Stage',  refCount: 0, deployPath: 'tex/sky.png' },
    { id: 'CRC-HERO', state: 'Loaded',  scope: 'Global', refCount: 5, deployPath: 'tex/hero2.png' },
];

// ─── Setup ────────────────────────────────────────────────────────────────────

beforeEach(() => {
    useAssetRuntimeStore.setState({
        assets: [],
        total: 0,
        stateFilter: '',
        idSearch: '',
    });

    Object.defineProperty(window, 'parent', { value: window, writable: true });
    vi.spyOn(window, 'postMessage').mockImplementation(() => {});
});

// ─── Tests ────────────────────────────────────────────────────────────────────

describe('AssetStateTable — table headers', () => {
    it('renders all column headers: id, state, scope, refCount, deployPath', () => {
        render(<AssetStateTable />);
        const header = screen.getByTestId('table-header');
        expect(within(header).getByText('id')).toBeInTheDocument();
        expect(within(header).getByText('state')).toBeInTheDocument();
        expect(within(header).getByText('scope')).toBeInTheDocument();
        expect(within(header).getByText('refCount')).toBeInTheDocument();
        expect(within(header).getByText('deployPath')).toBeInTheDocument();
    });
});

describe('AssetStateTable — row rendering', () => {
    it('renders asset rows from store', () => {
        act(() => {
            useAssetRuntimeStore.setState({ assets: SAMPLE_ASSETS, total: SAMPLE_ASSETS.length });
        });
        render(<AssetStateTable />);
        expect(screen.getByTestId('row-crc-001')).toBeInTheDocument();
        expect(screen.getByTestId('row-crc-002')).toBeInTheDocument();
        expect(screen.getByTestId('row-crc-003')).toBeInTheDocument();
        expect(screen.getByText('tex/hero.png')).toBeInTheDocument();
        expect(screen.getByText('audio/sfx.wav')).toBeInTheDocument();
    });
});

describe('AssetStateTable — state filter', () => {
    it("state filter 'Loaded' hides non-Loaded rows", () => {
        act(() => {
            useAssetRuntimeStore.setState({ assets: SAMPLE_ASSETS, total: SAMPLE_ASSETS.length });
        });
        render(<AssetStateTable />);

        const select = screen.getByTestId('state-filter');
        fireEvent.change(select, { target: { value: 'Loaded' } });

        expect(screen.queryByTestId('row-crc-002')).toBeNull(); // Loading
        expect(screen.queryByTestId('row-crc-003')).toBeNull(); // Failed
        expect(screen.queryByTestId('row-crc-004')).toBeNull(); // Staged
        expect(screen.getByTestId('row-crc-001')).toBeInTheDocument(); // Loaded
        expect(screen.getByTestId('row-CRC-HERO')).toBeInTheDocument(); // Loaded
    });

    it("state filter 'All' shows all rows", () => {
        act(() => {
            useAssetRuntimeStore.setState({ assets: SAMPLE_ASSETS, total: SAMPLE_ASSETS.length });
        });
        render(<AssetStateTable />);

        const select = screen.getByTestId('state-filter');
        // First set a filter, then switch back to All
        fireEvent.change(select, { target: { value: 'Failed' } });
        fireEvent.change(select, { target: { value: 'All' } });

        expect(screen.getByTestId('row-crc-001')).toBeInTheDocument();
        expect(screen.getByTestId('row-crc-002')).toBeInTheDocument();
        expect(screen.getByTestId('row-crc-003')).toBeInTheDocument();
        expect(screen.getByTestId('row-crc-004')).toBeInTheDocument();
    });
});

describe('AssetStateTable — ID search', () => {
    it('ID search filters case-insensitively', () => {
        act(() => {
            useAssetRuntimeStore.setState({ assets: SAMPLE_ASSETS, total: SAMPLE_ASSETS.length });
        });
        render(<AssetStateTable />);

        const input = screen.getByTestId('id-search');
        // "hero" should match crc-001 (no), CRC-HERO (yes via case-insensitive)
        // More specifically: 'hero' matches 'CRC-HERO' (contains 'hero' lowercase)
        fireEvent.change(input, { target: { value: 'hero' } });

        // crc-001 does not contain 'hero'
        expect(screen.queryByTestId('row-crc-001')).toBeNull();
        expect(screen.queryByTestId('row-crc-002')).toBeNull();
        // CRC-HERO contains 'hero' (case-insensitive)
        expect(screen.getByTestId('row-CRC-HERO')).toBeInTheDocument();
    });
});

describe('AssetStateTable — status line', () => {
    it('shows "X / Y assets" where X is filtered count and Y is store total', () => {
        act(() => {
            useAssetRuntimeStore.setState({ assets: SAMPLE_ASSETS, total: 10 });
        });
        render(<AssetStateTable />);

        // With no filter all 5 filtered, total=10
        const status = screen.getByTestId('asset-count');
        expect(status.textContent).toBe('5 / 10 assets');
    });

    it('updates X in status line when filter reduces visible rows', () => {
        act(() => {
            useAssetRuntimeStore.setState({ assets: SAMPLE_ASSETS, total: 10 });
        });
        render(<AssetStateTable />);

        const select = screen.getByTestId('state-filter');
        fireEvent.change(select, { target: { value: 'Loaded' } });

        const status = screen.getByTestId('asset-count');
        // 2 Loaded rows (crc-001 and CRC-HERO), total=10
        expect(status.textContent).toBe('2 / 10 assets');
    });
});

describe('AssetStateTable — row click', () => {
    it('row click fires window.parent.postMessage with select_asset type', () => {
        act(() => {
            useAssetRuntimeStore.setState({ assets: SAMPLE_ASSETS, total: SAMPLE_ASSETS.length });
        });
        render(<AssetStateTable />);

        const row = screen.getByTestId('row-crc-001');
        fireEvent.click(row);

        expect(window.postMessage).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: expect.objectContaining({
                    type: 'asset_runtime_inspector.select_asset',
                    data: { assetId: 'crc-001' },
                }),
            }),
            '*'
        );
    });
});

describe('AssetStateTable — sorting', () => {
    it('sort by id column changes row order', () => {
        act(() => {
            useAssetRuntimeStore.setState({ assets: SAMPLE_ASSETS, total: SAMPLE_ASSETS.length });
        });
        render(<AssetStateTable />);

        const header = screen.getByTestId('table-header');
        const idHeader = within(header).getByText('id');

        // Sort ascending by id
        fireEvent.click(idHeader);

        const rows = screen.getAllByRole('row').filter((r) => r.dataset.testid?.startsWith('row-'));
        const ids = rows.map((r) => r.dataset.testid?.replace('row-', '') ?? '');
        const sortedIds = [...ids].sort((a, b) => a.localeCompare(b));
        expect(ids).toEqual(sortedIds);
    });

    it('sort by id descending after second click', () => {
        act(() => {
            useAssetRuntimeStore.setState({ assets: SAMPLE_ASSETS, total: SAMPLE_ASSETS.length });
        });
        render(<AssetStateTable />);

        const header = screen.getByTestId('table-header');
        const idHeader = within(header).getByText('id');

        // Click twice for descending
        fireEvent.click(idHeader);
        fireEvent.click(idHeader);

        const rows = screen.getAllByRole('row').filter((r) => r.dataset.testid?.startsWith('row-'));
        const ids = rows.map((r) => r.dataset.testid?.replace('row-', '') ?? '');
        const sortedDesc = [...ids].sort((a, b) => b.localeCompare(a));
        expect(ids).toEqual(sortedDesc);
    });

    it('sort by refCount column changes row order', () => {
        act(() => {
            useAssetRuntimeStore.setState({ assets: SAMPLE_ASSETS, total: SAMPLE_ASSETS.length });
        });
        render(<AssetStateTable />);

        const header = screen.getByTestId('table-header');
        const refCountHeader = within(header).getByText('refCount');
        fireEvent.click(refCountHeader);

        const rows = screen.getAllByRole('row').filter((r) => r.dataset.testid?.startsWith('row-'));
        const ids = rows.map((r) => r.dataset.testid?.replace('row-', '') ?? '');

        // Ascending by refCount: 0,0,1,3,5 → crc-003(0), crc-004(0), crc-002(1), crc-001(3), CRC-HERO(5)
        // The exact order of equal refCounts (0) may vary — just verify the last two are highest
        const refCounts = ids.map((id) => SAMPLE_ASSETS.find((a) => a.id === id)?.refCount ?? 0);
        for (let i = 0; i < refCounts.length - 1; i++) {
            expect(refCounts[i]).toBeLessThanOrEqual(refCounts[i + 1]);
        }
    });
});

describe('AssetStateTable — refresh button', () => {
    it('refresh button fires window.parent.postMessage with force_refresh type', () => {
        render(<AssetStateTable />);

        const refreshBtn = screen.getByTestId('refresh-button');
        fireEvent.click(refreshBtn);

        expect(window.postMessage).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: expect.objectContaining({
                    type: 'asset_runtime_inspector.force_refresh',
                }),
            }),
            '*'
        );
    });
});
