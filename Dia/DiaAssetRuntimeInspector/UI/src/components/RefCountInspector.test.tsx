import { render, screen } from '@testing-library/react';
import { act } from 'react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import { RefCountInspector } from './RefCountInspector';
import { useAssetRuntimeStore } from '../store';

vi.mock('@dia/editor-ui', () => ({
    theme: {
        bg: '#1e1e1e', bgPanel: '#252526', bgInput: '#2d2d2d', border: '#3c3c3c',
        borderMuted: '#555', text: '#d4d4d4', textMuted: '#888', accent: '#0e639c',
        accentHover: '#007acc', success: '#89d185', warning: '#cca700', error: '#f48771',
        textDim: '#ccc',
    },
    EmptyState: ({ message }: { message: string }) => (
        <div data-testid="empty-state">{message}</div>
    ),
}));

beforeEach(() => {
    useAssetRuntimeStore.setState({ inspectorData: null });
});

// ─── 1. inspectorData === null → EmptyState ────────────────────────────────────

describe('RefCountInspector — null inspectorData', () => {
    it('renders empty state with default message when inspectorData is null', () => {
        render(<RefCountInspector />);
        const el = screen.getByTestId('empty-state');
        expect(el).toBeInTheDocument();
        expect(el).toHaveTextContent('Select an asset to inspect ref counts.');
    });
});

// ─── 2. hasSelection === false → EmptyState ───────────────────────────────────

describe('RefCountInspector — hasSelection false', () => {
    it('renders empty state with the data message when hasSelection is false', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                inspectorData: { hasSelection: false, message: 'No asset selected yet.' },
            });
        });
        render(<RefCountInspector />);
        const el = screen.getByTestId('empty-state');
        expect(el).toBeInTheDocument();
        expect(el).toHaveTextContent('No asset selected yet.');
    });
});

// ─── 3. missing === true ──────────────────────────────────────────────────────

describe('RefCountInspector — missing asset', () => {
    it('renders asset ID and "Asset no longer present in runtime." when missing is true', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                inspectorData: {
                    hasSelection: true,
                    missing: true,
                    assetId: 'crc-missing-99',
                    message: 'Asset was removed.',
                },
            });
        });
        render(<RefCountInspector />);
        expect(screen.getByTestId('ref-count-inspector')).toBeInTheDocument();
        expect(screen.getByTestId('asset-id')).toHaveTextContent('crc-missing-99');
        expect(screen.getByTestId('missing-message')).toHaveTextContent(
            'Asset no longer present in runtime.'
        );
        expect(screen.queryByTestId('stage-refs-list')).toBeNull();
        expect(screen.queryByTestId('stage-scoped-message')).toBeNull();
    });
});

// ─── 4. Stage-scoped asset ────────────────────────────────────────────────────

describe('RefCountInspector — stage-scoped asset', () => {
    it('renders single-reference message and no stage refs list for stage-scoped asset', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                inspectorData: {
                    hasSelection: true,
                    missing: false,
                    assetId: 'crc-stage-01',
                    state: 'Loaded',
                    scope: 'Stage',
                    refCount: 1,
                    stageScoped: true,
                    message: 'Owned by stage.',
                },
            });
        });
        render(<RefCountInspector />);
        expect(screen.getByTestId('asset-id')).toHaveTextContent('crc-stage-01');
        expect(screen.getByTestId('asset-state')).toHaveTextContent('Loaded');
        expect(screen.getByTestId('asset-scope')).toHaveTextContent('Stage');
        expect(screen.getByTestId('ref-count')).toHaveTextContent('1');
        expect(screen.getByTestId('stage-scoped-message')).toHaveTextContent(
            'Stage-scoped asset — single reference from owning stage.'
        );
        expect(screen.queryByTestId('stage-refs-list')).toBeNull();
        expect(screen.queryByTestId('no-stage-refs')).toBeNull();
    });
});

// ─── 5. Global asset with stageRefs ──────────────────────────────────────────

describe('RefCountInspector — global asset with stage refs', () => {
    it('renders stage refs list with each stageId', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                inspectorData: {
                    hasSelection: true,
                    missing: false,
                    assetId: 'crc-global-42',
                    state: 'Loaded',
                    scope: 'Global',
                    refCount: 3,
                    stageScoped: false,
                    stageRefs: [
                        { stageId: 'stage-alpha' },
                        { stageId: 'stage-beta' },
                        { stageId: 'stage-gamma' },
                    ],
                },
            });
        });
        render(<RefCountInspector />);
        expect(screen.getByTestId('asset-id')).toHaveTextContent('crc-global-42');
        expect(screen.getByTestId('ref-count')).toHaveTextContent('3');
        expect(screen.queryByTestId('no-stage-refs')).toBeNull();
        const items = screen.getAllByTestId('stage-ref-item');
        expect(items).toHaveLength(3);
        expect(items[0]).toHaveTextContent('stage-alpha');
        expect(items[1]).toHaveTextContent('stage-beta');
        expect(items[2]).toHaveTextContent('stage-gamma');
    });
});

// ─── 6. Global asset with empty stageRefs ────────────────────────────────────

describe('RefCountInspector — global asset with empty stage refs', () => {
    it('renders "No stage references found." when stageRefs is empty', () => {
        act(() => {
            useAssetRuntimeStore.setState({
                inspectorData: {
                    hasSelection: true,
                    missing: false,
                    assetId: 'crc-global-orphan',
                    state: 'Staged',
                    scope: 'Global',
                    refCount: 0,
                    stageScoped: false,
                    stageRefs: [],
                },
            });
        });
        render(<RefCountInspector />);
        expect(screen.getByTestId('no-stage-refs')).toHaveTextContent(
            'No stage references found.'
        );
        expect(screen.queryByTestId('stage-refs-list')).toBeNull();
        expect(screen.queryByTestId('stage-ref-item')).toBeNull();
    });
});
