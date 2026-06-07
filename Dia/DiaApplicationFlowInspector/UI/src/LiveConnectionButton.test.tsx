import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/react';

vi.mock('./useLiveStoreV2', () => ({
    useLiveStoreV2: vi.fn(),
}));

import { LiveConnectionButton } from './LiveConnectionButton';
import { useLiveStoreV2 } from './useLiveStoreV2';

const mockUseLive = useLiveStoreV2 as unknown as ReturnType<typeof vi.fn>;

function makeStore(overrides: Record<string, unknown> = {}) {
    return {
        connectionState: 'disconnected',
        connect: vi.fn(),
        disconnect: vi.fn(),
        ...overrides,
    };
}

function setupMock(store: ReturnType<typeof makeStore>) {
    mockUseLive.mockImplementation((selector: (s: typeof store) => unknown) => selector(store));
}

beforeEach(() => {
    vi.clearAllMocks();
});

describe('LiveConnectionButton (indicator)', () => {
    it('renders disconnected label', () => {
        setupMock(makeStore({ connectionState: 'disconnected' }));
        render(<LiveConnectionButton />);
        expect(screen.getByTestId('live-connection-indicator')).toBeTruthy();
        expect(screen.getByText('Disconnected')).toBeTruthy();
    });

    it('renders connecting label', () => {
        setupMock(makeStore({ connectionState: 'connecting' }));
        render(<LiveConnectionButton />);
        expect(screen.getByText('Connecting...')).toBeTruthy();
    });

    it('renders connected label', () => {
        setupMock(makeStore({ connectionState: 'connected' }));
        render(<LiveConnectionButton />);
        expect(screen.getByText('Connected')).toBeTruthy();
    });

    it('exposes connection state via data attribute', () => {
        setupMock(makeStore({ connectionState: 'connected' }));
        render(<LiveConnectionButton />);
        const el = screen.getByTestId('live-connection-indicator');
        expect(el.getAttribute('data-connection-state')).toBe('connected');
    });

    it('does not render a button or call connect/disconnect on click', () => {
        const store = makeStore({ connectionState: 'connected' });
        setupMock(store);
        const { container } = render(<LiveConnectionButton />);
        expect(container.querySelector('button')).toBeNull();
        // No popover ever; no actions wired.
        expect(store.disconnect).not.toHaveBeenCalled();
    });
});
