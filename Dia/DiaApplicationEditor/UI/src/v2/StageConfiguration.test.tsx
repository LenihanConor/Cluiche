import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { StageConfiguration } from './StageConfiguration';
import { useManifestStoreV2 } from './useManifestStoreV2';
import type { ManifestV2 } from './types';

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn().mockResolvedValue({ ok: true }),
    bridgeEvent: vi.fn(),
}));

import { bridgeRequest } from './bridge';

const mockManifest: ManifestV2 = {
    version: 1,
    stages: [
        { name: 'Boot', manifestPath: 'boot.json' },
        { name: 'Main', manifestPath: 'main.json' },
        { name: 'GameOver', manifestPath: 'gameover.json' },
    ],
    initialStage: 'Boot',
    autoStages: ['Main'],
    streams: [],
    processingUnits: [],
};

beforeEach(() => {
    vi.clearAllMocks();
    useManifestStoreV2.setState({ manifest: mockManifest });
});

describe('StageConfiguration', () => {
    it('renders stage rows (count matches manifest.stages.length)', () => {
        render(<StageConfiguration />);
        const rows = screen.getAllByTestId('stage-row');
        expect(rows.length).toBe(mockManifest.stages.length);
    });

    it('clicking "+" and entering a name calls AddStage', () => {
        render(<StageConfiguration />);
        const addBtn = screen.getByTestId('add-stage-btn');
        fireEvent.click(addBtn);

        const input = screen.getByPlaceholderText('stage name');
        fireEvent.change(input, { target: { value: 'Credits' } });

        const confirmBtn = screen.getByText('Add');
        fireEvent.click(confirmBtn);

        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
            commandType: 'AddStage',
            name: 'Credits',
            manifestPath: '',
        });
    });

    it('clicking remove calls RemoveStage with stage name', () => {
        render(<StageConfiguration />);
        const removeBtns = screen.getAllByTestId('remove-stage-btn');
        // Remove the first stage (Boot)
        fireEvent.click(removeBtns[0]);
        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
            commandType: 'RemoveStage',
            name: 'Boot',
        });
    });

    it('initial indicator is marked for manifest.initialStage', () => {
        render(<StageConfiguration />);
        const rows = screen.getAllByTestId('stage-row');
        const bootRow = rows.find(r => r.getAttribute('data-stage-name') === 'Boot')!;
        const indicator = bootRow.querySelector('[data-testid="initial-indicator"]')!;
        expect(indicator.getAttribute('data-is-initial')).toBe('true');

        const mainRow = rows.find(r => r.getAttribute('data-stage-name') === 'Main')!;
        const mainIndicator = mainRow.querySelector('[data-testid="initial-indicator"]')!;
        expect(mainIndicator.getAttribute('data-is-initial')).toBe('false');
    });

    it('clicking initial indicator on another stage calls SetInitialStage', () => {
        render(<StageConfiguration />);
        const rows = screen.getAllByTestId('stage-row');
        const mainRow = rows.find(r => r.getAttribute('data-stage-name') === 'Main')!;
        const mainIndicator = mainRow.querySelector('[data-testid="initial-indicator"]')!;
        fireEvent.click(mainIndicator);
        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
            commandType: 'SetInitialStage',
            name: 'Main',
        });
    });

    it('auto indicator toggles and calls SetStageTrigger', () => {
        render(<StageConfiguration />);
        const rows = screen.getAllByTestId('stage-row');

        // Main is in autoStages — clicking should disable
        const mainRow = rows.find(r => r.getAttribute('data-stage-name') === 'Main')!;
        const mainAutoIndicator = mainRow.querySelector('[data-testid="auto-indicator"]')!;
        expect(mainAutoIndicator.getAttribute('data-is-auto')).toBe('true');
        fireEvent.click(mainAutoIndicator);
        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
            commandType: 'SetStageTrigger',
            name: 'Main',
            isAuto: false,
        });

        vi.clearAllMocks();

        // Boot is not in autoStages — clicking should enable
        const bootRow = rows.find(r => r.getAttribute('data-stage-name') === 'Boot')!;
        const bootAutoIndicator = bootRow.querySelector('[data-testid="auto-indicator"]')!;
        expect(bootAutoIndicator.getAttribute('data-is-auto')).toBe('false');
        fireEvent.click(bootAutoIndicator);
        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
            commandType: 'SetStageTrigger',
            name: 'Boot',
            isAuto: true,
        });
    });
});
