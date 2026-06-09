import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { ConnectionStatus } from './ConnectionStatus';

describe('ConnectionStatus', () => {
  it('renders data-connection-state="connected" when connected', () => {
    render(<ConnectionStatus state="connected" />);
    const el = screen.getByTestId('connection-status');
    expect(el.getAttribute('data-connection-state')).toBe('connected');
  });

  it('shows "Connected" text when no label provided', () => {
    render(<ConnectionStatus state="connected" />);
    expect(screen.getByText('Connected')).toBeTruthy();
  });

  it('shows "Disconnected" text when disconnected and no label', () => {
    render(<ConnectionStatus state="disconnected" />);
    expect(screen.getByText('Disconnected')).toBeTruthy();
  });

  it('shows "Connecting..." text when connecting and no label', () => {
    render(<ConnectionStatus state="connecting" />);
    expect(screen.getByText('Connecting...')).toBeTruthy();
  });

  it('shows "Error" text when error and no label', () => {
    render(<ConnectionStatus state="error" />);
    expect(screen.getByText('Error')).toBeTruthy();
  });

  it('shows custom label when provided', () => {
    render(<ConnectionStatus state="connected" label="localhost:7000" />);
    expect(screen.getByText('localhost:7000')).toBeTruthy();
  });

  it('compact mode renders only dot, no text', () => {
    render(<ConnectionStatus state="connected" compact />);
    const dot = screen.getByTestId('traffic-light-dot');
    expect(dot).toBeTruthy();
    expect(screen.queryByTestId('connection-status')).toBeNull();
    expect(screen.queryByText('Connected')).toBeNull();
  });

  it('onConnect button appears when disconnected and onConnect prop provided', () => {
    const onConnect = vi.fn();
    render(<ConnectionStatus state="disconnected" onConnect={onConnect} />);
    expect(screen.getByTestId('connect-btn')).toBeTruthy();
  });

  it('onConnect button does NOT appear when not disconnected', () => {
    render(<ConnectionStatus state="connected" onConnect={vi.fn()} />);
    expect(screen.queryByTestId('connect-btn')).toBeNull();
  });

  it('onDisconnect button appears when connected and onDisconnect prop provided', () => {
    const onDisconnect = vi.fn();
    render(<ConnectionStatus state="connected" onDisconnect={onDisconnect} />);
    expect(screen.getByTestId('disconnect-btn')).toBeTruthy();
  });

  it('onDisconnect button does NOT appear when not connected', () => {
    render(<ConnectionStatus state="disconnected" onDisconnect={vi.fn()} />);
    expect(screen.queryByTestId('disconnect-btn')).toBeNull();
  });

  it('clicking onConnect button calls the callback', async () => {
    const onConnect = vi.fn();
    render(<ConnectionStatus state="disconnected" onConnect={onConnect} />);
    await userEvent.click(screen.getByTestId('connect-btn'));
    expect(onConnect).toHaveBeenCalledOnce();
  });

  it('clicking onDisconnect button fires the callback', async () => {
    const onDisconnect = vi.fn();
    render(<ConnectionStatus state="connected" onDisconnect={onDisconnect} />);
    await userEvent.click(screen.getByTestId('disconnect-btn'));
    expect(onDisconnect).toHaveBeenCalledOnce();
  });

  it('compact={true} with onConnect provided: connect button does NOT render', () => {
    render(<ConnectionStatus state="disconnected" compact onConnect={vi.fn()} />);
    expect(screen.queryByTestId('connect-btn')).toBeNull();
  });

  it('compact={true} with onDisconnect provided: disconnect button does NOT render', () => {
    render(<ConnectionStatus state="connected" compact onDisconnect={vi.fn()} />);
    expect(screen.queryByTestId('disconnect-btn')).toBeNull();
  });

  it('connected state background is the dark green (#1a3a1a / rgb(26,58,26))', () => {
    render(<ConnectionStatus state="connected" />);
    const el = screen.getByTestId('connection-status') as HTMLElement;
    const bg = el.style.background;
    // jsdom may normalise hex to rgb — accept either form
    expect(bg === '#1a3a1a' || bg === 'rgb(26, 58, 26)').toBe(true);
  });

  it('error state background is the dark red (#3a1a1a / rgb(58,26,26))', () => {
    render(<ConnectionStatus state="error" />);
    const el = screen.getByTestId('connection-status') as HTMLElement;
    const bg = el.style.background;
    expect(bg === '#3a1a1a' || bg === 'rgb(58, 26, 26)').toBe(true);
  });
});
