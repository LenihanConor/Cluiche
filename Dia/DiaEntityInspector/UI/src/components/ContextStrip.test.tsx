import { render, screen } from '@testing-library/react';
import { vi, describe, it, expect } from 'vitest';
import { ContextStrip } from './ContextStrip';
import type { EntityEntry } from '../types';

vi.mock('@dia/editor-ui', () => ({
    theme: {
        bg: '#1e1e1e', bgPanel: '#252526', bgInput: '#2d2d2d', border: '#3c3c3c',
        borderMuted: '#555', text: '#d4d4d4', textMuted: '#888', accent: '#0e639c',
        accentHover: '#007acc', success: '#89d185', warning: '#cca700', error: '#f48771',
        textDim: '#ccc',
    },
    injectThemeVars: vi.fn(),
}));

vi.mock('../tagStyles', () => ({
    tagChipStyle: () => ({ fontFamily: 'monospace', fontSize: 8.5, border: '1px solid #3c3c3c', borderRadius: 2, padding: '1px 2px', color: '#888', background: '#252526' }),
}));

const ENTITY: EntityEntry = { i: 7, g: 3, n: 'TestEntity', t: ['T', 'P'], d: 1 };

describe('ContextStrip', () => {
    it('shows placeholder when entity is null', () => {
        render(<ContextStrip entity={null} />);
        expect(screen.getByText(/select an entity/i)).toBeInTheDocument();
    });

    it('renders entity name and index', () => {
        render(<ContextStrip entity={ENTITY} />);
        expect(screen.getByText('TestEntity')).toBeInTheDocument();
        expect(screen.getByText('[007]')).toBeInTheDocument();
    });

    it('renders generation number', () => {
        render(<ContextStrip entity={ENTITY} />);
        expect(screen.getByText('g3')).toBeInTheDocument();
    });

    it('renders component count from tags', () => {
        render(<ContextStrip entity={ENTITY} />);
        expect(screen.getByText('2')).toBeInTheDocument();
    });

    it('renders tag chips for each tag', () => {
        render(<ContextStrip entity={ENTITY} />);
        expect(screen.getByText('T')).toBeInTheDocument();
        expect(screen.getByText('P')).toBeInTheDocument();
    });

    it('handles entity with zero tags', () => {
        const noTags: EntityEntry = { i: 0, g: 1, n: 'Empty', t: [], d: 0 };
        render(<ContextStrip entity={noTags} />);
        expect(screen.getByText('Empty')).toBeInTheDocument();
        expect(screen.getByText('0')).toBeInTheDocument();
    });
});
