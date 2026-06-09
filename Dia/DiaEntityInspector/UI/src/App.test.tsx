import { render, screen, fireEvent, act } from '@testing-library/react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import App from './App';
import { useInspectorStore } from './store';
import type { EntityEntry, QueryEntry, MailboxEntry, WatchItem } from './types';

vi.mock('@dia/editor-ui', () => ({
    theme: {
        bg: '#1e1e1e', bgPanel: '#252526', bgInput: '#2d2d2d', border: '#3c3c3c',
        borderMuted: '#555', text: '#d4d4d4', textMuted: '#888', accent: '#0e639c',
        accentHover: '#007acc', success: '#89d185', warning: '#cca700', error: '#f48771',
        textDim: '#ccc',
    },
    injectThemeVars: vi.fn(),
    inputStyle: () => ({}),
    buttonStyle: () => ({}),
    TrafficLightDot: ({ state }: any) => <span data-testid="dot" data-state={state} />,
    ConnectionStatus: ({ state, label }: any) => <span data-testid="conn-status" data-state={state}>{label}</span>,
    TabBar: ({ tabs, activeTab, onTabChange }: any) => (
        <div role="tablist">
            {tabs.map((t: any) => (
                <button key={t.id} role="tab" aria-selected={activeTab === t.id}
                        data-tab-id={t.id} onClick={() => onTabChange(t.id)}>
                    {t.label}{t.count != null ? ` (${t.count})` : ''}
                </button>
            ))}
        </div>
    ),
    EmptyState: ({ message }: any) => <div data-testid="empty-state">{message}</div>,
    useBridgeSubscribe: vi.fn(),
    useBridgeRequest: () => vi.fn().mockResolvedValue({}),
}));

const initialState = {
    connected: false,
    entities: [] as EntityEntry[],
    queries: [] as QueryEntry[],
    mailbox: [] as MailboxEntry[],
    watchItems: [] as WatchItem[],
    selectedIdx: null as number | null,
    activeTab: 'fields' as const,
    frame: 0,
    entityCount: 0,
};

beforeEach(() => {
    useInspectorStore.setState(initialState);
});

describe('App — disconnect overlay', () => {
    it('shows disconnect overlay when not connected', () => {
        render(<App />);
        expect(screen.getByTestId('disconnect-overlay')).toBeInTheDocument();
    });

    it('hides disconnect overlay when connected', () => {
        act(() => { useInspectorStore.setState({ connected: true }); });
        render(<App />);
        expect(screen.queryByTestId('disconnect-overlay')).toBeNull();
    });
});

describe('App — entity list reflects store', () => {
    it('EntityList renders entity names from store', () => {
        const entities: EntityEntry[] = [
            { i: 0, g: 1, n: 'Player', t: ['T'], d: 0 },
            { i: 1, g: 1, n: 'Enemy',  t: ['P'], d: 0 },
        ];
        act(() => { useInspectorStore.setState({ connected: true, entities }); });
        render(<App />);
        expect(screen.getByText('Player')).toBeInTheDocument();
        expect(screen.getByText('Enemy')).toBeInTheDocument();
    });
});

describe('App — bridge wiring', () => {
    it('sets window.DiaEditor_onDataChanged on mount', () => {
        render(<App />);
        expect(typeof (window as any).DiaEditor_onDataChanged).toBe('function');
    });

    it('entity_inspector.connection_state dispatch sets connected=true', () => {
        render(<App />);
        act(() => {
            (window as any).DiaEditor_onDataChanged({ topic: 'entity_inspector.connection_state', data: { connected: true } });
        });
        expect(useInspectorStore.getState().connected).toBe(true);
    });

    it('entity_inspector.inspect_data dispatch updates entity list', () => {
        render(<App />);
        const entities: EntityEntry[] = [{ i: 0, g: 1, n: 'Hero', t: ['T'], d: 0 }];
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'entity_inspector.inspect_data',
                data: { frame: 42, entityCount: 1, entities },
            });
        });
        expect(useInspectorStore.getState().entities[0].n).toBe('Hero');
        expect(useInspectorStore.getState().frame).toBe(42);
    });

    it('entity_inspector.query_data dispatch updates queries', () => {
        render(<App />);
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'entity_inspector.query_data',
                data: { queries: [{ sig: 'Transform+Physics', count: 3, members: [] }] },
            });
        });
        expect(useInspectorStore.getState().queries[0].sig).toBe('Transform+Physics');
    });

    it('entity_inspector.mailbox_data dispatch updates mailbox (key is log)', () => {
        render(<App />);
        const log: MailboxEntry[] = [{ f: 1, s: 'Player', a: 'Enemy', type: 'Damage', tc: 'mtdmg' }];
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'entity_inspector.mailbox_data',
                data: { log },
            });
        });
        expect(useInspectorStore.getState().mailbox.length).toBe(1);
        expect(useInspectorStore.getState().mailbox[0].type).toBe('Damage');
    });

    it('entity_inspector.watch_data dispatch updates watchItems', () => {
        render(<App />);
        const items: WatchItem[] = [{ e: 'Player', c: 'Transform', f: 'x', v: '10', d: 'up' }];
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'entity_inspector.watch_data',
                data: { items },
            });
        });
        expect(useInspectorStore.getState().watchItems[0].f).toBe('x');
    });

    it('postMessage bridge also dispatches topics', () => {
        render(<App />);
        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { __dia: true, topic: 'entity_inspector.connection_state', data: { connected: true } },
            }));
        });
        expect(useInspectorStore.getState().connected).toBe(true);
    });
});

describe('App — tab switching', () => {
    it('tab switching via TabBar changes active tab', () => {
        const entities: EntityEntry[] = [{ i: 0, g: 1, n: 'Hero', t: ['T'], d: 0, components: [] }];
        act(() => { useInspectorStore.setState({ connected: true, entities, selectedIdx: 0 }); });
        render(<App />);
        const queriesTab = screen.getByRole('tab', { name: /Queries/i });
        fireEvent.click(queriesTab);
        expect(useInspectorStore.getState().activeTab).toBe('queries');
    });
});
