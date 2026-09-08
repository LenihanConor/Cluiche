import { render, screen, act } from '@testing-library/react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import App from './App';
import { useBlackboardInspectorStore } from './store';
import type { BoardEntry } from './types';

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
    ConnectionStatus: ({ state, label }: any) => <span data-testid="conn-status" data-state={state}>{label}</span>,
    EmptyState: ({ message }: any) => <div data-testid="empty-state">{message}</div>,
}));

const initialState = {
    connected: false,
    boards: [] as BoardEntry[],
    filterText: '',
};

beforeEach(() => {
    useBlackboardInspectorStore.setState(initialState);
});

describe('App — disconnect overlay', () => {
    it('renders disconnect overlay when not connected', () => {
        render(<App />);
        expect(screen.getByTestId('disconnect-overlay')).toBeInTheDocument();
    });

    it('hides disconnect overlay when connected', () => {
        act(() => { useBlackboardInspectorStore.setState({ connected: true }); });
        render(<App />);
        expect(screen.queryByTestId('disconnect-overlay')).toBeNull();
    });
});

describe('App — bridge wiring', () => {
    it('sets window.DiaEditor_onDataChanged on mount', () => {
        render(<App />);
        expect(typeof (window as any).DiaEditor_onDataChanged).toBe('function');
    });

    it('connection_state dispatch sets connected=true', () => {
        render(<App />);
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'blackboard_inspector.connection_state',
                data: { connected: true },
            });
        });
        expect(useBlackboardInspectorStore.getState().connected).toBe(true);
    });

    it('blackboard_inspector.state dispatch populates boards', () => {
        render(<App />);
        const boards: BoardEntry[] = [
            { id: 'board-1', label: 'Combat Board', slots: [], observers: [] },
        ];
        act(() => {
            (window as any).DiaEditor_onDataChanged({
                topic: 'blackboard_inspector.state',
                data: { boards },
            });
        });
        expect(useBlackboardInspectorStore.getState().boards[0].id).toBe('board-1');
    });

    it('shows "No blackboards registered" when connected but no boards', () => {
        act(() => { useBlackboardInspectorStore.setState({ connected: true, boards: [] }); });
        render(<App />);
        expect(screen.getByTestId('empty-state')).toHaveTextContent('No blackboards registered');
    });
});
