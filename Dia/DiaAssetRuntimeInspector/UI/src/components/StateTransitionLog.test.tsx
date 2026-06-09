import { render, screen, fireEvent, act } from '@testing-library/react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import { StateTransitionLog } from './StateTransitionLog';
import { useAssetRuntimeStore } from '../store';
import type { LogEntry } from '../types';

vi.mock('@dia/editor-ui', () => ({
    theme: {
        bg: '#1e1e1e',
        bgPanel: '#252526',
        bgInput: '#2d2d2d',
        border: '#3c3c3c',
        borderMuted: '#555',
        text: '#d4d4d4',
        textMuted: '#888',
        accent: '#0e639c',
        accentHover: '#007acc',
        success: '#89d185',
        warning: '#cca700',
        error: '#f48771',
        textDim: '#ccc',
    },
    injectThemeVars: vi.fn(),
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
    logEntries: [] as LogEntry[],
    logTotal: 0,
    logPaused: false,
    logMaxEntries: 100,
    activeTab: 'log' as const,
};

// Separate parent mock so window.parent !== window (needed for sendBridgeRequest guard)
const parentPostMessage = vi.fn();
const mockParent = { postMessage: parentPostMessage } as unknown as Window;

beforeEach(() => {
    useAssetRuntimeStore.setState(initialState);
    parentPostMessage.mockReset();
    Object.defineProperty(window, 'parent', { value: mockParent, writable: true, configurable: true });
});

// ─── Test 1: Renders transition entries from store ────────────────────────────

describe('StateTransitionLog — entry rendering', () => {
    it('renders transition entries from store', () => {
        const entries: LogEntry[] = [
            { timestamp: 1700000000000, type: 'transition', assetId: 'tex/hero.png', oldState: 'Staged', newState: 'Loading' },
            { timestamp: 1700000001000, type: 'transition', assetId: 'audio/sfx.wav', oldState: 'Loading', newState: 'Loaded' },
        ];
        act(() => {
            useAssetRuntimeStore.setState({ logEntries: entries });
        });

        render(<StateTransitionLog />);

        expect(screen.getByText(/tex\/hero\.png/)).toBeInTheDocument();
        expect(screen.getByText(/audio\/sfx\.wav/)).toBeInTheDocument();
    });
});

// ─── Test 2: Pause button fires log_pause and changes text to Resume ──────────

describe('StateTransitionLog — pause/resume', () => {
    it('clicking Pause fires log_pause bridge request and shows Resume', () => {
        act(() => {
            useAssetRuntimeStore.setState({ logPaused: false });
        });
        render(<StateTransitionLog />);

        const pauseBtn = screen.getByRole('button', { name: /Pause/i });
        expect(pauseBtn).toHaveTextContent('Pause');

        fireEvent.click(pauseBtn);

        expect(parentPostMessage).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: expect.objectContaining({
                    type: 'asset_runtime_inspector.log_pause',
                }),
            }),
            '*'
        );
    });

    // ─── Test 3: Resume button fires log_resume and changes text to Pause ────

    it('clicking Resume (when paused) fires log_resume bridge request', () => {
        act(() => {
            useAssetRuntimeStore.setState({ logPaused: true });
        });
        render(<StateTransitionLog />);

        const resumeBtn = screen.getByRole('button', { name: /Resume/i });
        expect(resumeBtn).toHaveTextContent('Resume');

        fireEvent.click(resumeBtn);

        expect(parentPostMessage).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: expect.objectContaining({
                    type: 'asset_runtime_inspector.log_resume',
                }),
            }),
            '*'
        );
    });
});

// ─── Test 4: Clear button fires log_clear ────────────────────────────────────

describe('StateTransitionLog — clear', () => {
    it('clicking Clear fires log_clear bridge request', () => {
        render(<StateTransitionLog />);

        const clearBtn = screen.getByRole('button', { name: /Clear/i });
        fireEvent.click(clearBtn);

        expect(parentPostMessage).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: expect.objectContaining({
                    type: 'asset_runtime_inspector.log_clear',
                }),
            }),
            '*'
        );
    });
});

// ─── Test 5: Asset ID filter (case-insensitive substring) ─────────────────────

describe('StateTransitionLog — asset ID filter', () => {
    it('hides non-matching entries and is case-insensitive', () => {
        const entries: LogEntry[] = [
            { timestamp: 1700000000000, type: 'transition', assetId: 'tex/Hero.png', oldState: 'Staged', newState: 'Loading' },
            { timestamp: 1700000001000, type: 'transition', assetId: 'audio/sfx.wav', oldState: 'Loading', newState: 'Loaded' },
        ];
        act(() => {
            useAssetRuntimeStore.setState({ logEntries: entries });
        });

        render(<StateTransitionLog />);

        // Both visible initially
        expect(screen.getByText(/tex\/Hero\.png/)).toBeInTheDocument();
        expect(screen.getByText(/audio\/sfx\.wav/)).toBeInTheDocument();

        // Type "hero" (lowercase) — should match tex/Hero.png (case-insensitive), not audio
        const filterInput = screen.getByRole('textbox', { name: /Filter by asset ID/i });
        fireEvent.change(filterInput, { target: { value: 'hero' } });

        expect(screen.getByText(/tex\/Hero\.png/)).toBeInTheDocument();
        expect(screen.queryByText(/audio\/sfx\.wav/)).toBeNull();
    });
});

// ─── Test 6: Transition filter 'Any→Failed' shows only failed + markers ───────

describe('StateTransitionLog — transition filter', () => {
    it("'Any→Failed' shows only failed transitions and marker entries", () => {
        const entries: LogEntry[] = [
            { timestamp: 1700000000000, type: 'transition', assetId: 'tex/a.png', oldState: 'Loading', newState: 'Failed' },
            { timestamp: 1700000001000, type: 'transition', assetId: 'tex/b.png', oldState: 'Loading', newState: 'Loaded' },
            { timestamp: 1700000002000, type: 'disconnect' },
        ];
        act(() => {
            useAssetRuntimeStore.setState({ logEntries: entries });
        });

        render(<StateTransitionLog />);

        // Select 'Any→Failed'
        const select = screen.getByRole('combobox', { name: /Transition filter/i });
        fireEvent.change(select, { target: { value: 'Any→Failed' } });

        // Failed transition and disconnect marker should be visible
        expect(screen.getByText(/tex\/a\.png/)).toBeInTheDocument();
        // Non-failed transition should be hidden
        expect(screen.queryByText(/tex\/b\.png/)).toBeNull();
        // Disconnect marker always visible
        expect(screen.getByText(/Disconnected at/i)).toBeInTheDocument();
    });
});

// ─── Test 7: Disconnect marker renders distinctly ─────────────────────────────

describe('StateTransitionLog — marker rendering', () => {
    it('disconnect marker renders in italic muted style', () => {
        const entries: LogEntry[] = [
            { timestamp: 1700000000000, type: 'disconnect' },
        ];
        act(() => {
            useAssetRuntimeStore.setState({ logEntries: entries });
        });

        render(<StateTransitionLog />);

        const disconnectEl = screen.getByText(/Disconnected at/i).closest('[data-entry-type="disconnect"]');
        expect(disconnectEl).toBeInTheDocument();
        expect(disconnectEl).toHaveStyle({ fontStyle: 'italic' });
    });

    it('reconnect marker renders in italic muted style', () => {
        const entries: LogEntry[] = [
            { timestamp: 1700000005000, type: 'reconnect' },
        ];
        act(() => {
            useAssetRuntimeStore.setState({ logEntries: entries });
        });

        render(<StateTransitionLog />);

        const reconnectEl = screen.getByText(/Reconnected at/i).closest('[data-entry-type="reconnect"]');
        expect(reconnectEl).toBeInTheDocument();
        expect(reconnectEl).toHaveStyle({ fontStyle: 'italic' });
    });
});

// ─── Test 8: PAUSED badge shown when logPaused === true ──────────────────────

describe('StateTransitionLog — PAUSED badge', () => {
    it('shows PAUSED badge when logPaused is true', () => {
        act(() => {
            useAssetRuntimeStore.setState({ logPaused: true });
        });
        render(<StateTransitionLog />);

        expect(screen.getByTestId('paused-badge')).toBeInTheDocument();
        expect(screen.getByTestId('paused-badge')).toHaveTextContent('PAUSED');
    });

    it('does not show PAUSED badge when logPaused is false', () => {
        act(() => {
            useAssetRuntimeStore.setState({ logPaused: false });
        });
        render(<StateTransitionLog />);

        expect(screen.queryByTestId('paused-badge')).toBeNull();
    });
});
