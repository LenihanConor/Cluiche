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
});
