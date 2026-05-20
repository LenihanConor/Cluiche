import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { PUInspector } from './PUInspector';
import { useManifestStoreV2 } from './useManifestStoreV2';
import type { ManifestV2 } from './types';

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn().mockResolvedValue({ ok: true }),
    bridgeEvent: vi.fn(),
}));

import { bridgeRequest } from './bridge';

const mockManifest: ManifestV2 = {
    version: 1,
    stages: [{ name: 'Main', manifestPath: 'main.json' }],
    initialStage: 'Main',
    autoStages: [],
    streams: [],
    processingUnits: [
        {
            instanceId: 'MainPU',
            frequencyHz: 60,
            dedicatedThread: false,
            modules: [
                {
                    instanceId: 'RenderModule',
                    typeId: 'Render',
                    stages: ['Main'],
                    dependencies: [],
                    reads: [],
                    writes: [],
                    startTimeoutMs: 1000,
                    stopTimeoutMs: 1000,
                },
                {
                    instanceId: 'PhysicsModule',
                    typeId: 'Physics',
                    stages: [],
                    dependencies: ['RenderModule'],
                    reads: [],
                    writes: [],
                    startTimeoutMs: 500,
                    stopTimeoutMs: 500,
                },
            ],
        },
    ],
};

beforeEach(() => {
    vi.clearAllMocks();
    useManifestStoreV2.setState({ manifest: mockManifest });
});

describe('PUInspector', () => {
    it('renders "No PU selected" when puId is null', () => {
        render(<PUInspector puId={null} />);
        expect(screen.getByText('No PU selected')).toBeTruthy();
    });

    it('renders PU instanceId when puId is set', () => {
        render(<PUInspector puId="MainPU" />);
        expect(screen.getByTestId('pu-inspector')).toBeTruthy();
        expect(screen.getByText('MainPU')).toBeTruthy();
    });

    it('frequencyHz input shows current value', () => {
        render(<PUInspector puId="MainPU" />);
        const input = screen.getByTestId('freq-input') as HTMLInputElement;
        expect(Number(input.value)).toBe(60);
    });

    it('changing frequencyHz calls bridgeRequest with SetPUFrequency', () => {
        render(<PUInspector puId="MainPU" />);
        const input = screen.getByTestId('freq-input');
        fireEvent.change(input, { target: { value: '120' } });
        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
            commandType: 'SetPUFrequency',
            instanceId: 'MainPU',
            frequencyHz: 120,
        });
    });

    it('module cards render (count matches pu.modules.length)', () => {
        render(<PUInspector puId="MainPU" />);
        const cards = screen.getAllByTestId('module-card');
        expect(cards.length).toBe(mockManifest.processingUnits[0].modules.length);
    });

    it('dep order section is collapsed by default, clicking header expands it', () => {
        render(<PUInspector puId="MainPU" />);
        const depSection = screen.getByTestId('dep-order-section');

        // Should not find the module list items in dep section initially (collapsed)
        // The section header is present but the content should be hidden
        // Check that "1." prefix text is not visible (collapsed)
        expect(screen.queryByText(/^1\./)).toBeNull();

        // Click the dep order section header (first child div)
        const header = depSection.querySelector('div');
        expect(header).not.toBeNull();
        fireEvent.click(header!);

        // After clicking, content should be visible
        expect(screen.getByText(/1\./)).toBeTruthy();
    });
});
