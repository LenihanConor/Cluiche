import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { LiveConnectionButton } from './LiveConnectionButton';

// LiveConnectionButton is now a forwarding shim for @dia/editor-ui ConnectionStatus.
// Tests verify the forwarded component renders correctly when given state as a prop.

describe('LiveConnectionButton (shim → ConnectionStatus)', () => {
    it('renders disconnected label', () => {
        render(<LiveConnectionButton state="disconnected" />);
        expect(screen.getByTestId('connection-status')).toBeTruthy();
        expect(screen.getByText('Disconnected')).toBeTruthy();
    });

    it('renders connecting label', () => {
        render(<LiveConnectionButton state="connecting" />);
        expect(screen.getByText('Connecting...')).toBeTruthy();
    });

    it('renders connected label', () => {
        render(<LiveConnectionButton state="connected" />);
        expect(screen.getByText('Connected')).toBeTruthy();
    });

    it('exposes connection state via data attribute', () => {
        render(<LiveConnectionButton state="connected" />);
        const el = screen.getByTestId('connection-status');
        expect(el.getAttribute('data-connection-state')).toBe('connected');
    });

    it('does not render an action button when no onConnect/onDisconnect provided', () => {
        const { container } = render(<LiveConnectionButton state="connected" />);
        expect(container.querySelector('[data-testid="disconnect-btn"]')).toBeNull();
    });
});
