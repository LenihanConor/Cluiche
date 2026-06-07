import { render, screen, fireEvent } from '@testing-library/react';
import { act } from 'react';
import AppInspector from './AppInspector';
import { useLiveStoreV2 } from './useLiveStoreV2';
import { useInspectorStore } from './useInspectorStore';

beforeEach(() => {
    useLiveStoreV2.getState().clearLiveState();
    useInspectorStore.getState().clearAll();
});

describe('AppInspector', () => {
    it('shows empty state when disconnected', () => {
        render(<AppInspector />);
        expect(screen.getByTestId('empty-state')).toBeInTheDocument();
        expect(screen.queryByTestId('connected-content')).toBeNull();
    });

    it('shows connected content when connected', () => {
        act(() => useLiveStoreV2.getState().setConnectionState('connected'));
        render(<AppInspector />);
        expect(screen.getByTestId('connected-content')).toBeInTheDocument();
        expect(screen.queryByTestId('empty-state')).toBeNull();
    });

    it('renders LiveConnectionButton in header', () => {
        render(<AppInspector />);
        expect(screen.getByTestId('live-connection-indicator')).toBeInTheDocument();
    });

    it('tab switching renders correct content', () => {
        act(() => useLiveStoreV2.getState().setConnectionState('connected'));
        render(<AppInspector />);
        fireEvent.click(screen.getByTestId('tab-streams'));
        expect(screen.getByTestId('tab-content-streams')).toBeInTheDocument();
        fireEvent.click(screen.getByTestId('tab-timing'));
        expect(screen.getByTestId('tab-content-timing')).toBeInTheDocument();
        fireEvent.click(screen.getByTestId('tab-log'));
        expect(screen.getByTestId('tab-content-log')).toBeInTheDocument();
    });

    it('footer shows module count', () => {
        act(() => {
            useLiveStoreV2.getState().setConnectionState('connected');
            useInspectorStore.getState().updateModuleState({
                moduleId: 'mod1', puId: 'pu1', lifecycleState: 'Running',
                timeInStateMs: 0, timeoutMs: null, blockedByDep: null, errorMessage: null,
            });
        });
        render(<AppInspector />);
        expect(screen.getByTestId('footer-module-count').textContent).toBe('1 modules');
    });

    it('footer shows failed count when modules failed', () => {
        act(() => {
            useLiveStoreV2.getState().setConnectionState('connected');
            useInspectorStore.getState().updateModuleState({
                moduleId: 'mod1', puId: 'pu1', lifecycleState: 'Failed',
                timeInStateMs: 0, timeoutMs: null, blockedByDep: null, errorMessage: 'crash',
            });
        });
        render(<AppInspector />);
        expect(screen.getByTestId('footer-failed-count').textContent).toContain('1 failed');
    });

    it('footer shows shutdown button when connected', () => {
        act(() => useLiveStoreV2.getState().setConnectionState('connected'));
        render(<AppInspector />);
        expect(screen.getByTestId('shutdown-btn')).toBeInTheDocument();
    });
});
