import { render, screen, fireEvent } from '@testing-library/react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import { WatchTab } from './WatchTab';
import type { EntityEntry, WatchItem } from '../types';

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
}));

const ENTITIES: EntityEntry[] = [
    {
        i: 0, g: 1, n: 'Player', t: ['T'], d: 0,
        components: [
            {
                name: 'Transform',
                fields: [
                    { n: 'x', v: '0', t: 'float' },
                    { n: 'y', v: '0', t: 'float' },
                ],
            },
        ],
    },
];

const WATCH_ITEMS: WatchItem[] = [
    { e: 'Player', c: 'Transform', f: 'x', v: '5.0', d: 'up' },
    { e: 'Player', c: 'Transform', f: 'y', v: '3.0', d: 'eq' },
];

function mkProps(overrides: Partial<Parameters<typeof WatchTab>[0]> = {}) {
    return {
        watchItems: WATCH_ITEMS,
        entities: ENTITIES,
        onWatchAdd: vi.fn(),
        onWatchRemove: vi.fn(),
        ...overrides,
    };
}

beforeEach(() => {
    vi.clearAllMocks();
});

describe('WatchTab', () => {
    it('"+ Watch" button is disabled when selects are empty', () => {
        render(<WatchTab {...mkProps()} />);
        const btn = screen.getByTestId('watch-add-btn');
        expect(btn).toBeDisabled();
    });

    it('"+ Watch" button is enabled when all 3 selects have values', () => {
        render(<WatchTab {...mkProps()} />);
        fireEvent.change(screen.getByTestId('watch-entity-select'), { target: { value: 'Player' } });
        fireEvent.change(screen.getByTestId('watch-comp-select'), { target: { value: 'Transform' } });
        fireEvent.change(screen.getByTestId('watch-field-select'), { target: { value: 'x' } });
        const btn = screen.getByTestId('watch-add-btn');
        expect(btn).not.toBeDisabled();
    });

    it('clicking × calls onWatchRemove with correct index', () => {
        const onWatchRemove = vi.fn();
        render(<WatchTab {...mkProps({ onWatchRemove })} />);
        fireEvent.click(screen.getByTestId('watch-remove-1'));
        expect(onWatchRemove).toHaveBeenCalledWith(1);
    });

    it('clicking × on first item calls onWatchRemove with index 0', () => {
        const onWatchRemove = vi.fn();
        render(<WatchTab {...mkProps({ onWatchRemove })} />);
        fireEvent.click(screen.getByTestId('watch-remove-0'));
        expect(onWatchRemove).toHaveBeenCalledWith(0);
    });

    it('watch rows are rendered with correct entity names', () => {
        render(<WatchTab {...mkProps()} />);
        const rows = screen.getAllByText('Player');
        expect(rows.length).toBeGreaterThanOrEqual(1);
    });

    it('selecting entity resets component and field selects', () => {
        render(<WatchTab {...mkProps()} />);
        fireEvent.change(screen.getByTestId('watch-entity-select'), { target: { value: 'Player' } });
        fireEvent.change(screen.getByTestId('watch-comp-select'), { target: { value: 'Transform' } });
        fireEvent.change(screen.getByTestId('watch-field-select'), { target: { value: 'x' } });
        // Change entity — comp and field should reset
        fireEvent.change(screen.getByTestId('watch-entity-select'), { target: { value: '' } });
        expect((screen.getByTestId('watch-comp-select') as HTMLSelectElement).value).toBe('');
        expect((screen.getByTestId('watch-field-select') as HTMLSelectElement).value).toBe('');
    });

    it('selecting component resets field select', () => {
        render(<WatchTab {...mkProps()} />);
        fireEvent.change(screen.getByTestId('watch-entity-select'), { target: { value: 'Player' } });
        fireEvent.change(screen.getByTestId('watch-comp-select'), { target: { value: 'Transform' } });
        fireEvent.change(screen.getByTestId('watch-field-select'), { target: { value: 'x' } });
        // Change comp — only field should reset
        fireEvent.change(screen.getByTestId('watch-comp-select'), { target: { value: '' } });
        expect((screen.getByTestId('watch-field-select') as HTMLSelectElement).value).toBe('');
    });

    it('successful add clears form state', () => {
        const onWatchAdd = vi.fn();
        render(<WatchTab {...mkProps({ onWatchAdd })} />);
        fireEvent.change(screen.getByTestId('watch-entity-select'), { target: { value: 'Player' } });
        fireEvent.change(screen.getByTestId('watch-comp-select'), { target: { value: 'Transform' } });
        fireEvent.change(screen.getByTestId('watch-field-select'), { target: { value: 'x' } });
        fireEvent.click(screen.getByTestId('watch-add-btn'));
        expect(onWatchAdd).toHaveBeenCalledWith('Player', 'Transform', 'x');
        expect((screen.getByTestId('watch-entity-select') as HTMLSelectElement).value).toBe('');
    });

    it('renders delta symbols correctly', () => {
        render(<WatchTab {...mkProps()} />);
        expect(screen.getByText('▲')).toBeInTheDocument();
        expect(screen.getByText('—')).toBeInTheDocument();
    });

    it('handles watch items with null value and undefined delta', () => {
        const items: WatchItem[] = [{ e: 'X', c: 'Y', f: 'z', v: null }];
        render(<WatchTab {...mkProps({ watchItems: items })} />);
        expect(screen.getByTestId('watch-row-0')).toBeInTheDocument();
    });

    it('shows empty table when no watch items', () => {
        render(<WatchTab {...mkProps({ watchItems: [] })} />);
        expect(screen.queryByTestId('watch-row-0')).toBeNull();
    });
});
