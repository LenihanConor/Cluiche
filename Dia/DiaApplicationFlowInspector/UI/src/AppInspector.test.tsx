import { render, screen, fireEvent } from '@testing-library/react';
import { act } from 'react';
import { vi, beforeEach } from 'vitest';
import AppInspector from './AppInspector';
import { useLiveStoreV2 } from './useLiveStoreV2';
import { useInspectorStore } from './useInspectorStore';

vi.mock('./useLiveConnection', () => ({
    useLiveConnection: vi.fn(() => 'disconnected'),
}));

import { useLiveConnection } from './useLiveConnection';

const mockUseLiveConnection = useLiveConnection as ReturnType<typeof vi.fn>;

function setConnected(state: 'connected' | 'disconnected') {
    mockUseLiveConnection.mockReturnValue(state);
}

beforeEach(() => {
    useLiveStoreV2.getState().clearLiveState();
    useInspectorStore.getState().clearAll();
    mockUseLiveConnection.mockReturnValue('disconnected');
});

describe('AppInspector', () => {
    it('shows empty state when disconnected', () => {
        render(<AppInspector />);
        expect(screen.getByTestId('empty-state')).toBeInTheDocument();
        expect(screen.queryByTestId('connected-content')).toBeNull();
    });

    it('shows connected content when connected', () => {
        setConnected('connected');
        render(<AppInspector />);
        expect(screen.getByTestId('connected-content')).toBeInTheDocument();
        expect(screen.queryByTestId('empty-state')).toBeNull();
    });

    it('shows disconnect message in empty state', () => {
        render(<AppInspector />);
        expect(screen.getByText('No game connected')).toBeInTheDocument();
    });

    it('tab switching renders correct content', () => {
        setConnected('connected');
        render(<AppInspector />);
        fireEvent.click(screen.getByRole('tab', { name: 'Streams' }));
        expect(screen.getByTestId('tab-content-streams')).toBeInTheDocument();
        fireEvent.click(screen.getByRole('tab', { name: 'Timing' }));
        expect(screen.getByTestId('tab-content-timing')).toBeInTheDocument();
        fireEvent.click(screen.getByRole('tab', { name: 'Log' }));
        expect(screen.getByTestId('tab-content-log')).toBeInTheDocument();
    });

    it('footer shows module count', () => {
        setConnected('connected');
        act(() => {
            useInspectorStore.getState().updateModuleState({
                moduleId: 'mod1', puId: 'pu1', lifecycleState: 'Running',
                timeInStateMs: 0, timeoutMs: null, blockedByDep: null, errorMessage: null,
            });
        });
        render(<AppInspector />);
        expect(screen.getByTestId('footer-module-count').textContent).toBe('1 modules');
    });

    it('footer shows failed count when modules failed', () => {
        setConnected('connected');
        act(() => {
            useInspectorStore.getState().updateModuleState({
                moduleId: 'mod1', puId: 'pu1', lifecycleState: 'Failed',
                timeInStateMs: 0, timeoutMs: null, blockedByDep: null, errorMessage: 'crash',
            });
        });
        render(<AppInspector />);
        expect(screen.getByTestId('footer-failed-count').textContent).toContain('1 failed');
    });

    it('footer shows shutdown button when connected', () => {
        setConnected('connected');
        render(<AppInspector />);
        expect(screen.getByTestId('shutdown-btn')).toBeInTheDocument();
    });
});
