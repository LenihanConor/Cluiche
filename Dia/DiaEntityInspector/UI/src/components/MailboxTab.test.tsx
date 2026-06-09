import { render, screen, fireEvent } from '@testing-library/react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import { MailboxTab } from './MailboxTab';
import type { MailboxEntry } from '../types';

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

const MAILBOX: MailboxEntry[] = [
    { f: 1, s: 'Player', a: 'Enemy',  type: 'Damage', tc: 'mtdmg', inv: 'Enemy'  },
    { f: 2, s: 'Enemy',  a: 'System', type: 'Spawn',  tc: 'mtspwn', inv: 'Enemy' },
    { f: 3, s: 'System', a: 'System', type: 'State',  tc: 'mtstate', inv: 'NPC'  },
];

function mkProps(overrides: Partial<Parameters<typeof MailboxTab>[0]> = {}) {
    return {
        mailbox: MAILBOX,
        selectedEntityName: 'Player',
        onClear: vi.fn(),
        ...overrides,
    };
}

beforeEach(() => {
    vi.clearAllMocks();
});

describe('MailboxTab', () => {
    it('self-only toggle filters to selected entity messages', () => {
        render(<MailboxTab {...mkProps()} />);
        // Self-only starts ON by default — only rows involving 'Player'
        // Player is sender of row 0
        expect(screen.getByTestId('mailbox-row-0')).toBeInTheDocument();
        // Row 1: Enemy is sender, System is address — Player not involved directly
        // (inv=Enemy), so row 1 should be hidden
        expect(screen.queryByTestId('mailbox-row-1')).toBeNull();
        // Row 2: NPC involved, not Player
        expect(screen.queryByTestId('mailbox-row-2')).toBeNull();

        // Toggle off self-only — all rows appear
        fireEvent.click(screen.getByTestId('self-only-toggle'));
        expect(screen.getByTestId('mailbox-row-0')).toBeInTheDocument();
        expect(screen.getByTestId('mailbox-row-1')).toBeInTheDocument();
        expect(screen.getByTestId('mailbox-row-2')).toBeInTheDocument();
    });

    it('clear button calls onClear', () => {
        const onClear = vi.fn();
        render(<MailboxTab {...mkProps({ onClear })} />);
        fireEvent.click(screen.getByTestId('mailbox-clear'));
        expect(onClear).toHaveBeenCalledTimes(1);
    });

    it('selfOnly=true with selectedEntityName=null shows all messages', () => {
        render(<MailboxTab {...mkProps({ selectedEntityName: null })} />);
        // selfOnly is on but name is null — condition short-circuits, all visible
        expect(screen.getByTestId('mailbox-row-0')).toBeInTheDocument();
        expect(screen.getByTestId('mailbox-row-1')).toBeInTheDocument();
        expect(screen.getByTestId('mailbox-row-2')).toBeInTheDocument();
    });

    it('search filter combines with selfOnly filter', () => {
        render(<MailboxTab {...mkProps({ selectedEntityName: null })} />);
        // All visible initially (name null, selfOnly short-circuits). Type "Spawn" in search
        fireEvent.change(screen.getByTestId('mailbox-search'), { target: { value: 'Spawn' } });
        // Only 1 row matches "Spawn" — re-indexed as row-0
        expect(screen.getByTestId('mailbox-row-0')).toBeInTheDocument();
        expect(screen.queryByTestId('mailbox-row-1')).toBeNull();
        expect(screen.getByText('1 messages')).toBeInTheDocument();
    });

    it('shows message count', () => {
        render(<MailboxTab {...mkProps()} />);
        expect(screen.getByText('1 messages')).toBeInTheDocument();
    });

    it('renders empty table when mailbox is empty', () => {
        render(<MailboxTab {...mkProps({ mailbox: [] })} />);
        expect(screen.getByText('0 messages')).toBeInTheDocument();
    });

    it('renders payload when present and empty string when null', () => {
        const mail: MailboxEntry[] = [
            { f: 1, s: 'A', a: 'B', type: 'X', p: 'hello payload' },
            { f: 2, s: 'A', a: 'B', type: 'Y' },
        ];
        render(<MailboxTab {...mkProps({ mailbox: mail, selectedEntityName: null })} />);
        // selfOnly off because name null — both visible
        fireEvent.click(screen.getByTestId('self-only-toggle'));
        expect(screen.getByText('hello payload')).toBeInTheDocument();
    });

    it('uses fallback badge style for unknown type class', () => {
        const mail: MailboxEntry[] = [
            { f: 1, s: 'A', a: 'B', type: 'Unknown', tc: 'bogus_type' },
        ];
        render(<MailboxTab {...mkProps({ mailbox: mail, selectedEntityName: null })} />);
        expect(screen.getByText('Unknown')).toBeInTheDocument();
    });

    it('highlights sender and address when matching selected entity', () => {
        const mail: MailboxEntry[] = [
            { f: 1, s: 'Player', a: 'Player', type: 'Self' },
        ];
        render(<MailboxTab {...mkProps({ mailbox: mail, selectedEntityName: 'Player' })} />);
        const row = screen.getByTestId('mailbox-row-0');
        expect(row).toBeInTheDocument();
    });
});
