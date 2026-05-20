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
    version: 1,
    stages: [
        { name: 'Boot', manifestPath: 'boot.json' },
        { name: 'Main', manifestPath: 'main.json' },
        { name: 'GameOver', manifestPath: 'gameover.json' },
    ],
    initialStage: 'Boot',
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
                    dependencies: ['PhysicsModule'],
                    reads: ['PositionStream'],
                    writes: ['RenderStream'],
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
});
