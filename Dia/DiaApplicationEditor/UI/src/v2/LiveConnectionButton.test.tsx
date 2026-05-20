import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

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

// useLiveStoreV2 is called with a selector in the component, so we need to
// return the selected value based on the selector function.
function setupMock(store: ReturnType<typeof makeStore>) {
    mockUseLive.mockImplementation((selector: (s: typeof store) => unknown) => selector(store));
}

beforeEach(() => {
    vi.clearAllMocks();
});

describe('LiveConnectionButton', () => {
    it('renders with disconnected state shows Disconnected text', () => {
        const store = makeStore({ connectionState: 'disconnected' });
        setupMock(store);
        render(<LiveConnectionButton />);
        expect(screen.getByTestId('live-connection-btn')).toBeTruthy();
        expect(screen.getByText('Disconnected')).toBeTruthy();
    });

    it('renders with connecting state shows Connecting... and is disabled', () => {
        const store = makeStore({ connectionState: 'connecting' });
        setupMock(store);
        render(<LiveConnectionButton />);
        const btn = screen.getByTestId('live-connection-btn');
        expect(btn.textContent).toContain('Connecting...');
        expect((btn as HTMLButtonElement).disabled).toBe(true);
    });

    it('renders with connected state shows Connected text', () => {
        const store = makeStore({ connectionState: 'connected' });
        setupMock(store);
        render(<LiveConnectionButton />);
        expect(screen.getByText('Connected')).toBeTruthy();
    });

    it('clicking when disconnected shows popover', () => {
        const store = makeStore({ connectionState: 'disconnected' });
        setupMock(store);
        render(<LiveConnectionButton />);
        const btn = screen.getByTestId('live-connection-btn');
        fireEvent.click(btn);
        expect(screen.getByTestId('live-popover')).toBeTruthy();
    });

    it('filling popover and clicking Connect calls store.connect with correct host/port', () => {
        const store = makeStore({ connectionState: 'disconnected' });
        setupMock(store);
        render(<LiveConnectionButton />);

        // Open popover
        fireEvent.click(screen.getByTestId('live-connection-btn'));

        // Change host and port
        fireEvent.change(screen.getByTestId('live-host-input'), { target: { value: '192.168.1.1' } });
        fireEvent.change(screen.getByTestId('live-port-input'), { target: { value: '9999' } });

        // Click submit
        fireEvent.click(screen.getByTestId('live-connect-submit'));

        expect(store.connect).toHaveBeenCalledWith('192.168.1.1', 9999);
    });
});
