import { render, screen } from '@testing-library/react';
import { act } from 'react';
import AppInspector from './AppInspector';
import { useLiveStoreV2 } from './useLiveStoreV2';

// Reset store between tests
beforeEach(() => {
  useLiveStoreV2.getState().clearLiveState();
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
});
