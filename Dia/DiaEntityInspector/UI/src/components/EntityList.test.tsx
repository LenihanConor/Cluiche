import { render, screen, fireEvent } from '@testing-library/react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import { EntityList } from './EntityList';
import type { EntityEntry } from '../types';

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

const ENTITIES: EntityEntry[] = [
    { i: 0, g: 1, n: 'Player', t: ['T', 'P'], d: 0 },
    { i: 1, g: 2, n: 'Enemy',  t: ['P', 'H'], d: 0 },
    { i: 2, g: 1, n: 'Camera', t: ['T'],      d: 0 },
];

function mkProps(overrides: Partial<Parameters<typeof EntityList>[0]> = {}) {
    return {
        entities: ENTITIES,
        selectedIdx: null,
        onSelect: vi.fn(),
        searchText: '',
        onSearchChange: vi.fn(),
        activeFilter: 'All',
        onFilterChange: vi.fn(),
        ...overrides,
    };
}

beforeEach(() => {
    vi.clearAllMocks();
});

describe('EntityList', () => {
    it('renders entity rows with names', () => {
        render(<EntityList {...mkProps()} />);
        expect(screen.getByText('Player')).toBeInTheDocument();
        expect(screen.getByText('Enemy')).toBeInTheDocument();
        expect(screen.getByText('Camera')).toBeInTheDocument();
    });

    it('search filter hides non-matching entities', () => {
        render(<EntityList {...mkProps({ searchText: 'play' })} />);
        expect(screen.getByText('Player')).toBeInTheDocument();
        expect(screen.queryByText('Enemy')).toBeNull();
        expect(screen.queryByText('Camera')).toBeNull();
    });

    it('chip filter "T" shows only entities with tag T', () => {
        render(<EntityList {...mkProps({ activeFilter: 'T' })} />);
        expect(screen.getByText('Player')).toBeInTheDocument();
        expect(screen.getByText('Camera')).toBeInTheDocument();
        expect(screen.queryByText('Enemy')).toBeNull();
    });

    it('clicking entity row calls onSelect', () => {
        const onSelect = vi.fn();
        render(<EntityList {...mkProps({ onSelect })} />);
        fireEvent.click(screen.getByTestId('entity-row-1'));
        expect(onSelect).toHaveBeenCalledWith(1);
    });

    it('selected entity row has data-selected="true"', () => {
        render(<EntityList {...mkProps({ selectedIdx: 0 })} />);
        const row = screen.getByTestId('entity-row-0');
        expect(row).toHaveAttribute('data-selected', 'true');
    });

    it('non-selected rows have data-selected="false"', () => {
        render(<EntityList {...mkProps({ selectedIdx: 0 })} />);
        const row1 = screen.getByTestId('entity-row-1');
        expect(row1).toHaveAttribute('data-selected', 'false');
    });
});
