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
});
