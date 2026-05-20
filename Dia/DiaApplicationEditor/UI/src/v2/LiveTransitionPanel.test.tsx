import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import React from 'react';

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn(),
    bridgeEvent: vi.fn(),
}));

vi.mock('./useLiveStoreV2', () => ({
    useLiveStoreV2: vi.fn(),
}));

import { LiveTransitionPanel } from './LiveTransitionPanel';
import { bridgeRequest } from './bridge';
import { useLiveStoreV2 } from './useLiveStoreV2';

const mockBridgeRequest = bridgeRequest as ReturnType<typeof vi.fn>;
const mockUseLive = useLiveStoreV2 as unknown as ReturnType<typeof vi.fn>;

const mockSetActiveStage = vi.fn();

function setupLiveMock(connectionState = 'connected') {
    const store = { connectionState, setActiveStage: mockSetActiveStage };
    mockUseLive.mockImplementation((selector: (s: typeof store) => unknown) => selector(store));
}

beforeEach(() => {
    vi.clearAllMocks();
    setupLiveMock('connected');
});

const STAGES = ['Boot', 'Gameplay', 'Menu'];

describe('LiveTransitionPanel', () => {
    it('renders stage dropdown with provided stages', () => {
        render(<LiveTransitionPanel stages={STAGES} />);
        const select = screen.getByTestId('stage-select') as HTMLSelectElement;
        expect(select).toBeTruthy();
        // Should have an empty option + 3 stage options
        const options = Array.from(select.options).map((o) => o.value);
        expect(options).toContain('Boot');
        expect(options).toContain('Gameplay');
        expect(options).toContain('Menu');
    });

    it('trigger button disabled when no stage selected', () => {
        render(<LiveTransitionPanel stages={STAGES} />);
        const btn = screen.getByTestId('trigger-btn') as HTMLButtonElement;
        expect(btn.disabled).toBe(true);
    });

    it('trigger button enabled when stage selected', () => {
        render(<LiveTransitionPanel stages={STAGES} />);
        fireEvent.change(screen.getByTestId('stage-select'), { target: { value: 'Boot' } });
        const btn = screen.getByTestId('trigger-btn') as HTMLButtonElement;
        expect(btn.disabled).toBe(false);
    });

    it('clicking Trigger calls bridgeRequest with live.transitionTo and selected stage name', async () => {
        mockBridgeRequest.mockResolvedValue({ ok: true });
        render(<LiveTransitionPanel stages={STAGES} />);
        fireEvent.change(screen.getByTestId('stage-select'), { target: { value: 'Gameplay' } });
        fireEvent.click(screen.getByTestId('trigger-btn'));
        expect(mockBridgeRequest).toHaveBeenCalledWith('live.transitionTo', { stageName: 'Gameplay' });
        await waitFor(() => screen.getByTestId('transition-feedback'));
    });

    it('success response shows feedback message', async () => {
        mockBridgeRequest.mockResolvedValue({ ok: true });
        render(<LiveTransitionPanel stages={STAGES} />);
        fireEvent.change(screen.getByTestId('stage-select'), { target: { value: 'Menu' } });
        fireEvent.click(screen.getByTestId('trigger-btn'));
        await waitFor(() => {
            expect(screen.getByTestId('transition-feedback').textContent).toBe('Transition to Menu complete');
        });
    });

    it('successful transition updates LiveStore activeStage', async () => {
        mockBridgeRequest.mockResolvedValue({ ok: true });
        render(<LiveTransitionPanel stages={STAGES} />);
        fireEvent.change(screen.getByTestId('stage-select'), { target: { value: 'Gameplay' } });
        fireEvent.click(screen.getByTestId('trigger-btn'));
        await waitFor(() => screen.getByTestId('transition-feedback'));
        expect(mockSetActiveStage).toHaveBeenCalledWith('Gameplay');
    });
});
