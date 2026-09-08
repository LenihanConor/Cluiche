import { render, screen, fireEvent } from '@testing-library/react';
import { vi, describe, it, expect } from 'vitest';
import { QueriesTab } from './QueriesTab';
import type { QueryEntry } from '../types';

vi.mock('@dia/editor-ui', () => ({
    theme: {
        bg: '#1e1e1e', bgPanel: '#252526', bgInput: '#2d2d2d', border: '#3c3c3c',
        borderMuted: '#555', text: '#d4d4d4', textMuted: '#888', accent: '#0e639c',
        accentHover: '#007acc', success: '#89d185', warning: '#cca700', error: '#f48771',
        textDim: '#ccc',
    },
    injectThemeVars: vi.fn(),
}));

const QUERIES: QueryEntry[] = [
    { sig: 'Transform,Physics', count: 3, members: ['Player', 'Enemy', 'Bullet'] },
    { sig: 'Render,Animation', count: 2, members: ['Player', 'NPC'] },
    { sig: 'AI,Pathfinding', count: 1, members: ['Enemy'] },
];

describe('QueriesTab', () => {
    it('shows placeholder when selectedEntityName is null', () => {
        render(<QueriesTab queries={QUERIES} selectedEntityName={null} />);
        expect(screen.getByText(/select an entity/i)).toBeInTheDocument();
    });

    it('renders all query cards', () => {
        render(<QueriesTab queries={QUERIES} selectedEntityName="Player" />);
        expect(screen.getByTestId('query-card-Transform,Physics')).toBeInTheDocument();
        expect(screen.getByTestId('query-card-Render,Animation')).toBeInTheDocument();
        expect(screen.getByTestId('query-card-AI,Pathfinding')).toBeInTheDocument();
    });

    it('member cards start expanded, non-member cards start collapsed', () => {
        render(<QueriesTab queries={QUERIES} selectedEntityName="Player" />);
        // Player is member of Transform,Physics and Render,Animation — those should be open
        expect(screen.getByText('Enemy')).toBeInTheDocument(); // visible inside Transform,Physics members
        // AI,Pathfinding — Player is NOT a member, should be collapsed
        // "Pathfinding" sig text is visible in header, but members list is not
        const aiCard = screen.getByTestId('query-card-AI,Pathfinding');
        expect(aiCard.querySelectorAll('[style]').length).toBeGreaterThan(0);
        // Enemy chip should not appear inside AI card since it's collapsed
        const allEnemyChips = screen.getAllByText('Enemy');
        // One in Transform,Physics members (open), none from AI card (closed)
        expect(allEnemyChips.length).toBe(1);
    });

    it('clicking a card header toggles its open/closed state', () => {
        render(<QueriesTab queries={QUERIES} selectedEntityName="Player" />);
        // AI,Pathfinding starts closed — click to open
        const aiHeader = screen.getByText('AI,Pathfinding').closest('[style]')!;
        fireEvent.click(aiHeader);
        // Now Enemy should appear inside AI card members
        expect(screen.getAllByText('Enemy').length).toBe(2);
        // Click again to close
        fireEvent.click(aiHeader);
        expect(screen.getAllByText('Enemy').length).toBe(1);
    });

    it('shows "✓ member" badge only on cards where entity is a member', () => {
        render(<QueriesTab queries={QUERIES} selectedEntityName="Player" />);
        const badges = screen.getAllByText('✓ member');
        expect(badges.length).toBe(2); // Transform,Physics + Render,Animation
    });

    it('highlights selected entity chip in member list', () => {
        render(<QueriesTab queries={QUERIES} selectedEntityName="Player" />);
        const playerChips = screen.getAllByText('Player');
        // Both member cards are open, so 2 Player chips
        expect(playerChips.length).toBe(2);
    });

    it('handles query with null/undefined members gracefully', () => {
        const broken: QueryEntry[] = [
            { sig: 'Broken', count: 0, members: undefined as unknown as string[] },
        ];
        render(<QueriesTab queries={broken} selectedEntityName="X" />);
        expect(screen.getByTestId('query-card-Broken')).toBeInTheDocument();
    });

    it('handles empty queries array', () => {
        render(<QueriesTab queries={[]} selectedEntityName="Player" />);
        expect(screen.getByTestId('queries-tab')).toBeInTheDocument();
    });

    it('shows count badge with correct value', () => {
        render(<QueriesTab queries={QUERIES} selectedEntityName="Player" />);
        expect(screen.getByText('3')).toBeInTheDocument();
        expect(screen.getByText('2')).toBeInTheDocument();
        expect(screen.getByText('1')).toBeInTheDocument();
    });
});
