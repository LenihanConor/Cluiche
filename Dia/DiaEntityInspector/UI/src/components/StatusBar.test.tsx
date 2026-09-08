import { render, screen } from '@testing-library/react';
import { vi, describe, it, expect } from 'vitest';
import { StatusBar } from './StatusBar';

vi.mock('@dia/editor-ui', () => ({
    theme: {
        bg: '#1e1e1e', bgPanel: '#252526', bgInput: '#2d2d2d', border: '#3c3c3c',
        borderMuted: '#555', text: '#d4d4d4', textMuted: '#888', accent: '#0e639c',
        accentHover: '#007acc', success: '#89d185', warning: '#cca700', error: '#f48771',
        textDim: '#ccc',
    },
    injectThemeVars: vi.fn(),
}));

describe('StatusBar', () => {
    it('shows frame number when frame > 0', () => {
        render(<StatusBar frame={42} entityCount={10} selectedName={null} watchCount={0} />);
        expect(screen.getByText('#42')).toBeInTheDocument();
    });

    it('shows dash when frame is 0', () => {
        render(<StatusBar frame={0} entityCount={0} selectedName={null} watchCount={0} />);
        expect(screen.getByText('-')).toBeInTheDocument();
    });

    it('shows entity count when > 0', () => {
        render(<StatusBar frame={1} entityCount={99} selectedName={null} watchCount={0} />);
        expect(screen.getByText('99')).toBeInTheDocument();
    });

    it('hides entity count when entityCount is 0', () => {
        render(<StatusBar frame={1} entityCount={0} selectedName={null} watchCount={0} />);
        expect(screen.queryByText('entities')).toBeNull();
    });

    it('shows selected entity name', () => {
        render(<StatusBar frame={1} entityCount={5} selectedName="Dragon" watchCount={0} />);
        expect(screen.getByText('Dragon')).toBeInTheDocument();
    });

    it('shows dash for selected name when null', () => {
        render(<StatusBar frame={1} entityCount={5} selectedName={null} watchCount={0} />);
        expect(screen.getByText('—')).toBeInTheDocument();
    });

    it('shows watch count', () => {
        render(<StatusBar frame={1} entityCount={5} selectedName={null} watchCount={7} />);
        expect(screen.getByText('7')).toBeInTheDocument();
    });
});
