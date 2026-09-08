import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { ModuleInspector } from './ModuleInspector';
import { useManifestStoreV2 } from './useManifestStoreV2';
import type { ManifestV2 } from './types';

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn().mockResolvedValue({ ok: true }),
    bridgeEvent: vi.fn(),
}));

import { bridgeRequest } from './bridge';

const mockManifest: ManifestV2 = {
    version: 3,
    stages: [
        { name: 'Boot',     manifestPath: 'boot.json',     transitions: [], autoAdvance: false },
        { name: 'Main',     manifestPath: 'main.json',     transitions: [], autoAdvance: false },
        { name: 'GameOver', manifestPath: 'gameover.json', transitions: [], autoAdvance: false },
    ],
    initialStage: 'Boot',
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
                    dependencies: ['PhysicsModule'],
                    channels: [
                        { id: 'PositionStream', role: 'reads' as const },
                        { id: 'RenderStream',   role: 'writes' as const },
                    ],
                    startTimeoutMs: 1000,
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

describe('ModuleInspector', () => {
    it('renders "No module selected" when moduleId is null', () => {
        render(<ModuleInspector moduleId={null} puId={null} />);
        expect(screen.getByText('No module selected')).toBeTruthy();
    });

    it('renders module instanceId', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        expect(screen.getByTestId('module-inspector')).toBeTruthy();
        // instanceId appears in properties
        expect(screen.getAllByText('RenderModule').length).toBeGreaterThan(0);
    });

    it('stage dots render (one per stage in manifest)', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        const dots = screen.getAllByTestId('stage-dot');
        expect(dots.length).toBe(mockManifest.stages.length);
    });

    it('clicking grey dot calls bridgeRequest to add stage', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        // 'Boot' is not in RenderModule.stages, so its dot is grey
        const bootDotEl = screen.getAllByTestId('stage-dot').find(el =>
            el.getAttribute('data-stage-name') === 'Boot'
        )!;
        fireEvent.click(bootDotEl);
        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', expect.objectContaining({
            commandType: 'SetModuleStages',
            instanceId: 'RenderModule',
            puId: 'MainPU',
        }));
        const call = (bridgeRequest as ReturnType<typeof vi.fn>).mock.calls[0][1] as { stages: string[] };
        expect(call.stages).toContain('Boot');
        expect(call.stages).toContain('Main');
    });

    it('dependencies section renders dep chips', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        expect(screen.getByTestId('deps-section')).toBeTruthy();
        expect(screen.getByText('PhysicsModule')).toBeTruthy();
    });

    it('add dep button shows input and calls bridgeRequest on confirm', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        const addBtn = screen.getByTestId('add-dep-btn');
        fireEvent.click(addBtn);

        // Input should now be visible
        const input = screen.getByPlaceholderText('dependency id') as HTMLInputElement;
        expect(input).toBeTruthy();

        fireEvent.change(input, { target: { value: 'AudioModule' } });
        const confirmBtn = screen.getByText('Add');
        fireEvent.click(confirmBtn);

        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', expect.objectContaining({
            commandType: 'AddModuleDep',
            instanceId: 'RenderModule',
            dependency: 'AudioModule',
        }));
    });

    it('clicking × on a dep chip calls RemoveModuleDep', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        const removeBtn = screen.getByTitle('Remove PhysicsModule');
        fireEvent.click(removeBtn);
        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', expect.objectContaining({
            commandType: 'RemoveModuleDep',
            instanceId: 'RenderModule',
            puId: 'MainPU',
            dependency: 'PhysicsModule',
        }));
    });

    it('Escape key cancels add-dep without calling bridgeRequest', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        fireEvent.click(screen.getByTestId('add-dep-btn'));

        const input = screen.getByPlaceholderText('dependency id') as HTMLInputElement;
        fireEvent.change(input, { target: { value: 'WillBeCancelled' } });
        fireEvent.keyDown(input, { key: 'Escape' });

        expect(bridgeRequest).not.toHaveBeenCalled();
        // Input should be gone (reverted to + button)
        expect(screen.queryByPlaceholderText('dependency id')).toBeNull();
    });

    it('empty value on add-dep does not call bridgeRequest', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        fireEvent.click(screen.getByTestId('add-dep-btn'));

        const input = screen.getByPlaceholderText('dependency id') as HTMLInputElement;
        fireEvent.change(input, { target: { value: '   ' } });
        fireEvent.click(screen.getByText('Add'));

        expect(bridgeRequest).not.toHaveBeenCalled();
    });

    it('Enter key on add-dep input confirms and calls AddModuleDep', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        fireEvent.click(screen.getByTestId('add-dep-btn'));

        const input = screen.getByPlaceholderText('dependency id') as HTMLInputElement;
        fireEvent.change(input, { target: { value: 'AudioModule' } });
        fireEvent.keyDown(input, { key: 'Enter' });

        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', expect.objectContaining({
            commandType: 'AddModuleDep',
            dependency: 'AudioModule',
        }));
    });

    it('reads chip renders and × calls RemoveModuleChannel with role reads', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        expect(screen.getByText('PositionStream')).toBeTruthy();
        fireEvent.click(screen.getByTitle('Remove PositionStream'));
        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', expect.objectContaining({
            commandType: 'RemoveModuleChannel',
            instanceId: 'RenderModule',
            puId: 'MainPU',
            streamId: 'PositionStream',
            role: 'reads',
        }));
    });

    it('add-channel input calls AddModuleChannel', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        fireEvent.click(screen.getByTestId('add-channel-btn'));

        const input = screen.getByPlaceholderText('stream id') as HTMLInputElement;
        fireEvent.change(input, { target: { value: 'NewReadStream' } });
        fireEvent.keyDown(input, { key: 'Enter' });

        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', expect.objectContaining({
            commandType: 'AddModuleChannel',
            instanceId: 'RenderModule',
            streamId: 'NewReadStream',
            role: 'reads',
        }));
    });

    it('writes chip renders and × calls RemoveModuleChannel with role writes', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        expect(screen.getByText('RenderStream')).toBeTruthy();
        fireEvent.click(screen.getByTitle('Remove RenderStream'));
        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', expect.objectContaining({
            commandType: 'RemoveModuleChannel',
            instanceId: 'RenderModule',
            puId: 'MainPU',
            streamId: 'RenderStream',
            role: 'writes',
        }));
    });

    it('add-channel form submits AddModuleChannel for writes role', () => {
        render(<ModuleInspector moduleId="RenderModule" puId="MainPU" />);
        fireEvent.click(screen.getByTestId('add-channel-btn'));

        // Change role to writes
        const select = screen.getByTestId('channel-role-select') as HTMLSelectElement;
        fireEvent.change(select, { target: { value: 'writes' } });

        const input = screen.getByPlaceholderText('stream id') as HTMLInputElement;
        fireEvent.change(input, { target: { value: 'NewWriteStream' } });
        fireEvent.keyDown(input, { key: 'Enter' });

        expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', expect.objectContaining({
            commandType: 'AddModuleChannel',
            instanceId: 'RenderModule',
            streamId: 'NewWriteStream',
            role: 'writes',
        }));
    });
});
