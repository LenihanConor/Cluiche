import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import { PUInspector } from './PUInspector';
import { useManifestStoreV2 } from './useManifestStoreV2';
import type { ManifestV2 } from './types';

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn(),
    bridgeEvent: vi.fn(),
}));

import { bridgeRequest } from './bridge';

// Default: types.get returns a small module-types list, all other calls succeed.
const setupBridge = () => {
    (bridgeRequest as ReturnType<typeof vi.fn>).mockImplementation((type: string) => {
        if (type === 'types.get') {
            return Promise.resolve({
                ok: true,
                moduleTypes: [
                    { id: 'AudioModuleType', displayName: 'Audio' },
                    { id: 'NetworkModuleType', displayName: 'Network' },
                ],
                puTypes: [],
            });
        }
        return Promise.resolve({ ok: true });
    });
};

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
    setupBridge();
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

    it('module card × button calls RemoveModule with puId + instanceId', () => {
        render(<PUInspector puId="MainPU" />);
        const removeBtns = screen.getAllByTestId('remove-module-btn');
        const renderRemove = removeBtns.find(b => b.getAttribute('data-module-id') === 'RenderModule');
        expect(renderRemove).toBeTruthy();
        fireEvent.click(renderRemove!);
        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
            commandType: 'RemoveModule',
            puId: 'MainPU',
            instanceId: 'RenderModule',
        });
    });

    describe('Add Module', () => {
        it('+ Add Module button is visible by default', () => {
            render(<PUInspector puId="MainPU" />);
            expect(screen.getByTestId('add-module-btn')).toBeTruthy();
            expect(screen.queryByTestId('add-module-form')).toBeNull();
        });

        it('clicking + Add Module opens the inline form and fetches types', async () => {
            render(<PUInspector puId="MainPU" />);
            fireEvent.click(screen.getByTestId('add-module-btn'));
            expect(screen.getByTestId('add-module-form')).toBeTruthy();
            // bridge call for types.get fires on form open
            expect(bridgeRequest).toHaveBeenCalledWith('types.get');

            // Wait for types to populate the dropdown
            const select = screen.getByTestId('add-module-type-select') as HTMLSelectElement;
            // microtask flush so the .then callback runs
            await waitFor(() => {
                const sel = screen.getByTestId('add-module-type-select') as HTMLSelectElement;
                expect(Array.from(sel.querySelectorAll('option')).some(o => o.value === 'AudioModuleType')).toBe(true);
            });
            const options = Array.from(select.querySelectorAll('option')).map(o => o.value);
            expect(options).toContain('AudioModuleType');
            expect(options).toContain('NetworkModuleType');
        });

        it('Add button is disabled until both fields are valid', async () => {
            render(<PUInspector puId="MainPU" />);
            fireEvent.click(screen.getByTestId('add-module-btn'));

            const confirm = screen.getByTestId('add-module-confirm') as HTMLButtonElement;
            expect(confirm.disabled).toBe(true);

            const input = screen.getByTestId('add-module-instance-input') as HTMLInputElement;
            fireEvent.change(input, { target: { value: 'AudioModule' } });
            expect(confirm.disabled).toBe(true); // typeId still empty

            await waitFor(() => {
                const sel = screen.getByTestId('add-module-type-select') as HTMLSelectElement;
                expect(Array.from(sel.querySelectorAll('option')).some(o => o.value === 'AudioModuleType')).toBe(true);
            });

            const select = screen.getByTestId('add-module-type-select') as HTMLSelectElement;
            fireEvent.change(select, { target: { value: 'AudioModuleType' } });
            expect(confirm.disabled).toBe(false);
        });

        it('valid submit calls AddModule and resets the form', async () => {
            render(<PUInspector puId="MainPU" />);
            fireEvent.click(screen.getByTestId('add-module-btn'));
            await waitFor(() => {
                const sel = screen.getByTestId('add-module-type-select') as HTMLSelectElement;
                expect(Array.from(sel.querySelectorAll('option')).some(o => o.value === 'AudioModuleType')).toBe(true);
            });

            fireEvent.change(screen.getByTestId('add-module-instance-input'), { target: { value: 'AudioModule' } });
            fireEvent.change(screen.getByTestId('add-module-type-select'), { target: { value: 'AudioModuleType' } });
            fireEvent.click(screen.getByTestId('add-module-confirm'));

            expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
                commandType: 'AddModule',
                puId: 'MainPU',
                instanceId: 'AudioModule',
                typeId: 'AudioModuleType',
            });
            // Form closes after submit
            expect(screen.queryByTestId('add-module-form')).toBeNull();
            expect(screen.getByTestId('add-module-btn')).toBeTruthy();
        });

        it('duplicate instanceId shows inline error and disables Add', async () => {
            render(<PUInspector puId="MainPU" />);
            fireEvent.click(screen.getByTestId('add-module-btn'));
            await waitFor(() => {
                const sel = screen.getByTestId('add-module-type-select') as HTMLSelectElement;
                expect(Array.from(sel.querySelectorAll('option')).some(o => o.value === 'AudioModuleType')).toBe(true);
            });

            fireEvent.change(screen.getByTestId('add-module-instance-input'), { target: { value: 'RenderModule' } });
            fireEvent.change(screen.getByTestId('add-module-type-select'), { target: { value: 'AudioModuleType' } });

            expect(screen.getByTestId('add-module-error')).toBeTruthy();
            const confirm = screen.getByTestId('add-module-confirm') as HTMLButtonElement;
            expect(confirm.disabled).toBe(true);

            // Even if user clicks Add, no command should fire
            fireEvent.click(confirm);
            expect(bridgeRequest).not.toHaveBeenCalledWith(
                'manifest.applyCommand',
                expect.objectContaining({ commandType: 'AddModule' }),
            );
        });

        it('Cancel button closes the form without firing a command', () => {
            render(<PUInspector puId="MainPU" />);
            fireEvent.click(screen.getByTestId('add-module-btn'));
            fireEvent.change(screen.getByTestId('add-module-instance-input'), { target: { value: 'WillCancel' } });
            fireEvent.click(screen.getByTestId('add-module-cancel'));

            expect(screen.queryByTestId('add-module-form')).toBeNull();
            expect(bridgeRequest).not.toHaveBeenCalledWith(
                'manifest.applyCommand',
                expect.objectContaining({ commandType: 'AddModule' }),
            );
        });

        it('Escape on the input cancels the form', () => {
            render(<PUInspector puId="MainPU" />);
            fireEvent.click(screen.getByTestId('add-module-btn'));
            const input = screen.getByTestId('add-module-instance-input');
            fireEvent.keyDown(input, { key: 'Escape' });
            expect(screen.queryByTestId('add-module-form')).toBeNull();
        });

        it('Enter on the input submits when form is valid', async () => {
            render(<PUInspector puId="MainPU" />);
            fireEvent.click(screen.getByTestId('add-module-btn'));
            await waitFor(() => {
                const sel = screen.getByTestId('add-module-type-select') as HTMLSelectElement;
                expect(Array.from(sel.querySelectorAll('option')).some(o => o.value === 'AudioModuleType')).toBe(true);
            });

            fireEvent.change(screen.getByTestId('add-module-instance-input'), { target: { value: 'AudioModule' } });
            fireEvent.change(screen.getByTestId('add-module-type-select'), { target: { value: 'AudioModuleType' } });
            fireEvent.keyDown(screen.getByTestId('add-module-instance-input'), { key: 'Enter' });

            expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', expect.objectContaining({
                commandType: 'AddModule',
                instanceId: 'AudioModule',
                typeId: 'AudioModuleType',
            }));
        });
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
