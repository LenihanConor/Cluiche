import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { GraphView } from './GraphView';
import type { ManifestV2 } from './types';

// Mock the manifest store
const mockManifest: ManifestV2 = {
    version: 1,
    stages: [{ name: 'Main', manifestPath: 'main.json' }],
    initialStage: 'Main',
    autoStages: [],
    streams: [
        {
            id: 'stream-a',
            kind: 'SPSC',
            payloadType: 'int',
            fromPU: 'PU_Render',
            toPU: 'PU_Physics',
            capacity: 64,
            maxReaders: 1,
        },
        {
            id: 'stream-b',
            kind: 'SPMC',
            payloadType: 'float',
            fromPU: 'PU_Physics',
            toPU: 'PU_Audio',
            capacity: 32,
            maxReaders: 4,
        },
    ],
    processingUnits: [
        {
            instanceId: 'PU_Render',
            frequencyHz: 60,
            dedicatedThread: true,
            modules: [
                {
                    instanceId: 'RenderMod',
                    typeId: 'RenderModule',
                    stages: ['Main'],
                    dependencies: [],
                    reads: [],
                    writes: [],
                    startTimeoutMs: 1000,
                    stopTimeoutMs: 1000,
                },
            ],
        },
        {
            instanceId: 'PU_Physics',
            frequencyHz: 120,
            dedicatedThread: false,
            modules: [
                {
                    instanceId: 'PhysicsMod',
                    typeId: 'PhysicsModule',
                    stages: ['Main'],
                    dependencies: [],
                    reads: [],
                    writes: [],
                    startTimeoutMs: 500,
                    stopTimeoutMs: 500,
                },
                {
                    instanceId: 'CollisionMod',
                    typeId: 'CollisionModule',
                    stages: ['Main'],
                    dependencies: [],
                    reads: [],
                    writes: [],
                    startTimeoutMs: 500,
                    stopTimeoutMs: 500,
                },
            ],
        },
        {
            instanceId: 'PU_Audio',
            frequencyHz: 44,
            dedicatedThread: true,
            modules: [],
        },
    ],
};

vi.mock('./useManifestStoreV2', () => ({
    useManifestStoreV2: vi.fn(() => ({ manifest: null })),
}));

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn(() => Promise.resolve({})),
    bridgeEvent: vi.fn(),
}));

vi.mock('./useLiveStoreV2', () => ({
    useLiveStoreV2: vi.fn(() => ({
        connectionState: 'disconnected',
        modules: [],
        streams: [],
        activeStage: null,
    })),
}));

import { useManifestStoreV2 } from './useManifestStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import { bridgeRequest } from './bridge';

const mockUseManifestStore = vi.mocked(useManifestStoreV2);
const mockUseLiveStore = vi.mocked(useLiveStoreV2);
const mockBridgeRequest = vi.mocked(bridgeRequest);

describe('GraphView', () => {
    beforeEach(() => {
        vi.clearAllMocks();
        mockUseManifestStore.mockReturnValue({ manifest: null } as any);
    });

    it('renders SVG with data-testid="graph-view" when manifest is null', () => {
        render(<GraphView />);
        const svg = screen.getByTestId('graph-view');
        expect(svg).toBeTruthy();
        expect(svg.tagName.toLowerCase()).toBe('svg');
    });

    it('renders correct number of pu-node elements for N PUs', () => {
        mockUseManifestStore.mockReturnValue({ manifest: mockManifest } as any);
        render(<GraphView />);
        const nodes = screen.getAllByTestId('pu-node');
        expect(nodes).toHaveLength(3);
    });

    it('renders correct number of stream-edge elements for M streams', () => {
        mockUseManifestStore.mockReturnValue({ manifest: mockManifest } as any);
        render(<GraphView />);
        const edges = screen.getAllByTestId('stream-edge');
        expect(edges).toHaveLength(2);
    });

    it('clicking a PU node updates selection', () => {
        mockUseManifestStore.mockReturnValue({ manifest: mockManifest } as any);
        render(<GraphView />);
        const nodes = screen.getAllByTestId('pu-node');

        // Click the first node
        fireEvent.click(nodes[0]);

        // Verify selection via data-selected attribute
        expect(nodes[0].getAttribute('data-selected')).toBe('true');
        // Others should not be selected
        expect(nodes[1].getAttribute('data-selected')).toBeNull();
    });

    it('renders ghost node with data-testid="ghost-node"', () => {
        mockUseManifestStore.mockReturnValue({ manifest: mockManifest } as any);
        render(<GraphView />);
        const ghost = screen.getByTestId('ghost-node');
        expect(ghost).toBeTruthy();
    });

    it('renders ghost node even when manifest is null', () => {
        render(<GraphView />);
        const ghost = screen.getByTestId('ghost-node');
        expect(ghost).toBeTruthy();
    });

    it('clicking ghost node calls bridgeRequest with manifest.applyCommand', () => {
        mockUseManifestStore.mockReturnValue({ manifest: mockManifest } as any);
        render(<GraphView />);
        const ghost = screen.getByTestId('ghost-node');

        fireEvent.click(ghost);

        expect(mockBridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
            commandType: 'AddPU',
            instanceId: 'NewPU',
            frequencyHz: 60,
            dedicatedThread: false,
        });
    });

    it('renders PU nodes with correct data-pu-id attributes', () => {
        mockUseManifestStore.mockReturnValue({ manifest: mockManifest } as any);
        render(<GraphView />);
        const nodes = screen.getAllByTestId('pu-node');

        expect(nodes[0].getAttribute('data-pu-id')).toBe('PU_Render');
        expect(nodes[1].getAttribute('data-pu-id')).toBe('PU_Physics');
        expect(nodes[2].getAttribute('data-pu-id')).toBe('PU_Audio');
    });

    it('renders stream edges with correct data-stream-id attributes', () => {
        mockUseManifestStore.mockReturnValue({ manifest: mockManifest } as any);
        render(<GraphView />);
        const edges = screen.getAllByTestId('stream-edge');

        expect(edges[0].getAttribute('data-stream-id')).toBe('stream-a');
        expect(edges[1].getAttribute('data-stream-id')).toBe('stream-b');
    });

    it('calls onStreamLabelClick when stream label text is clicked', () => {
        mockUseManifestStore.mockReturnValue({ manifest: mockManifest } as any);
        const onStreamLabelClick = vi.fn();
        const { container } = render(<GraphView onStreamLabelClick={onStreamLabelClick} />);

        // Find the text element with the stream id
        const streamTexts = container.querySelectorAll('[data-testid="stream-edge"] text');
        fireEvent.click(streamTexts[0]);

        expect(onStreamLabelClick).toHaveBeenCalledWith('stream-a');
    });

    it('PU node traffic-light circle is green when PU has an active module in live state', () => {
        mockUseLiveStore.mockReturnValue({
            connectionState: 'connected',
            modules: [{ moduleId: 'ModA', puId: 'PU1', isActive: true, activeStage: 'Boot' }],
            streams: [],
            activeStage: null,
        } as any);
        mockUseManifestStore.mockReturnValue({
            manifest: {
                version: 1,
                stages: [],
                initialStage: '',
                autoStages: [],
                streams: [],
                processingUnits: [
                    {
                        instanceId: 'PU1',
                        frequencyHz: 60,
                        dedicatedThread: false,
                        modules: [
                            {
                                instanceId: 'ModA',
                                typeId: 'ModuleA',
                                stages: ['Boot'],
                                dependencies: [],
                                reads: [],
                                writes: [],
                                startTimeoutMs: 1000,
                                stopTimeoutMs: 1000,
                            },
                        ],
                    },
                ],
            },
        } as any);

        const { container } = render(<GraphView />);

        const puNode = container.querySelector('[data-pu-id="PU1"]');
        expect(puNode).toBeTruthy();
        const circle = puNode!.querySelector('circle');
        expect(circle).toBeTruthy();
        expect(circle!.getAttribute('fill')).toBe('#3cb370');
    });
});
