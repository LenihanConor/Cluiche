import { describe, it, expect, beforeEach, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import { ModulePresenceGrid } from './ModulePresenceGrid';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import type { ManifestV2, ModuleV2 } from './types';

vi.mock('./useLiveStoreV2', () => ({
    useLiveStoreV2: vi.fn(() => ({
        connectionState: 'disconnected',
        activeStage: null,
        modules: [],
        streams: [],
    })),
}));

function makeModule(instanceId: string, stages: string[]): ModuleV2 {
    return {
        instanceId,
        typeId: instanceId + 'Type',
        stages,
        dependencies: [],
        reads: [],
        writes: [],
        startTimeoutMs: 1000,
        stopTimeoutMs: 1000,
    };
}

function makeManifest(overrides?: Partial<ManifestV2>): ManifestV2 {
    return {
        version: 3,
        stages: [
            { name: 'Boot', manifestPath: 'boot.json', transitions: [], autoAdvance: false },
            { name: 'Game', manifestPath: 'game.json', transitions: [], autoAdvance: false },
            { name: 'Menu', manifestPath: 'menu.json', transitions: [], autoAdvance: false },
        ],
        initialStage: 'Boot',
        streams: [],
        processingUnits: [
            {
                instanceId: 'MainPU',
                frequencyHz: 60,
                dedicatedThread: false,
                modules: [
                    makeModule('RenderModule', ['Boot', 'Game', 'Menu']),
                    makeModule('PhysicsModule', ['Game']),
                ],
            },
            {
                instanceId: 'AudioPU',
                frequencyHz: 30,
                dedicatedThread: true,
                modules: [
                    makeModule('AudioModule', ['Game', 'Menu']),
                ],
            },
        ],
        ...overrides,
    };
}

beforeEach(() => {
    useManifestStoreV2.setState({ manifest: null, filePath: null, isDirty: false, hasManifest: false });
    vi.mocked(useLiveStoreV2).mockReturnValue({
        connectionState: 'disconnected',
        activeStage: null,
        modules: [],
        streams: [],
    } as ReturnType<typeof useLiveStoreV2>);
});

describe('ModulePresenceGrid', () => {
    it('renders presence-grid when manifest is null (no crash)', () => {
        render(<ModulePresenceGrid />);
        expect(screen.getByTestId('presence-grid')).toBeTruthy();
    });

    it('renders correct stage column headers for N stages', () => {
        useManifestStoreV2.setState({ manifest: makeManifest(), hasManifest: true });
        render(<ModulePresenceGrid />);
        const headers = screen.getAllByTestId('stage-col-header');
        // "All" + 3 stage headers = 4 total
        expect(headers.length).toBe(4);
        expect(headers[0].getAttribute('data-colname')).toBe('All');
        expect(headers[1].getAttribute('data-colname')).toBe('Boot');
        expect(headers[2].getAttribute('data-colname')).toBe('Game');
        expect(headers[3].getAttribute('data-colname')).toBe('Menu');
    });

    it('renders PU group headers (one per PU)', () => {
        useManifestStoreV2.setState({ manifest: makeManifest(), hasManifest: true });
        render(<ModulePresenceGrid />);
        const puHeaders = screen.getAllByTestId('pu-group-header');
        expect(puHeaders.length).toBe(2);
        expect(puHeaders[0].getAttribute('data-pu-id')).toBe('MainPU');
        expect(puHeaders[1].getAttribute('data-pu-id')).toBe('AudioPU');
    });

    it('renders presence cells: filled dot when module.stages includes stage, empty otherwise', () => {
        useManifestStoreV2.setState({ manifest: makeManifest(), hasManifest: true });
        render(<ModulePresenceGrid />);

        // PhysicsModule is only in 'Game'
        const physicsCells = screen.getAllByTestId('presence-cell')
            .filter(el => el.getAttribute('data-module') === 'PhysicsModule');

        expect(physicsCells.length).toBe(3); // Boot, Game, Menu

        const bootCell = physicsCells.find(el => el.getAttribute('data-stage') === 'Boot')!;
        const gameCell = physicsCells.find(el => el.getAttribute('data-stage') === 'Game')!;
        const menuCell = physicsCells.find(el => el.getAttribute('data-stage') === 'Menu')!;

        // Game cell should have a dot inside
        expect(gameCell.querySelector('[data-testid="traffic-light-dot"]')).toBeTruthy();
        // Boot cell should be empty
        expect(bootCell.querySelector('[data-testid="traffic-light-dot"]')).toBeNull();
        // Menu cell should be empty
        expect(menuCell.querySelector('[data-testid="traffic-light-dot"]')).toBeNull();
    });

    it('"All" badge appears when a module is in all stages', () => {
        useManifestStoreV2.setState({ manifest: makeManifest(), hasManifest: true });
        render(<ModulePresenceGrid />);

        // RenderModule is in Boot, Game, Menu (all stages) -> should have all-badge
        const badges = screen.getAllByTestId('all-badge');
        expect(badges.length).toBe(1);
        expect(badges[0].textContent).toBe('all');
    });

    it('virtual scroll: renders only a window of rows when there are many modules', () => {
        // Create a manifest with 50 modules in one PU
        const manyModules: ModuleV2[] = [];
        for (let i = 0; i < 50; i++) {
            manyModules.push(makeModule(`Mod${i}`, ['Boot']));
        }
        const bigManifest = makeManifest({
            stages: [{ name: 'Boot', manifestPath: 'boot.json' }],
            processingUnits: [{
                instanceId: 'BigPU',
                frequencyHz: 60,
                dedicatedThread: false,
                modules: manyModules,
            }],
        });

        useManifestStoreV2.setState({ manifest: bigManifest, hasManifest: true });
        const { container } = render(<ModulePresenceGrid containerHeight={400} />);

        // Total rows = 1 PU header + 50 modules = 51
        // Visible window at 400px height / 28px = ~14.3 rows, plus overscan of 3 each side = ~20 rows
        // Should NOT render all 51 rows
        const puHeaders = container.querySelectorAll('[data-testid="pu-group-header"]');
        const presenceCells = container.querySelectorAll('[data-testid="presence-cell"]');

        // Count rendered module rows by counting unique sets of cells per module
        // Each module row has 1 presence cell (1 stage). So presence cells = rendered module rows.
        const renderedModuleRows = presenceCells.length;

        // Should be significantly less than 50
        expect(renderedModuleRows).toBeLessThan(50);
        // Should be at least a few rows
        expect(renderedModuleRows).toBeGreaterThan(0);

        // Total rendered rows (PU headers + module rows) should be less than 51
        const totalRendered = puHeaders.length + renderedModuleRows;
        expect(totalRendered).toBeLessThan(51);
    });

    it('Active stage column header has data-active=true when live', () => {
        vi.mocked(useLiveStoreV2).mockReturnValue({
            connectionState: 'connected',
            activeStage: 'Boot',
            modules: [],
            streams: [],
        } as ReturnType<typeof useLiveStoreV2>);

        const manifest = makeManifest({
            stages: [
                { name: 'Boot', manifestPath: 'boot.json' },
                { name: 'Play', manifestPath: 'play.json' },
            ],
            processingUnits: [{
                instanceId: 'MainPU',
                frequencyHz: 60,
                dedicatedThread: false,
                modules: [makeModule('RenderModule', ['Boot', 'Play'])],
            }],
        });
        useManifestStoreV2.setState({ manifest, hasManifest: true });
        render(<ModulePresenceGrid />);

        const headers = screen.getAllByTestId('stage-col-header');
        const bootHeader = headers.find(h => h.getAttribute('data-colname') === 'Boot')!;
        const playHeader = headers.find(h => h.getAttribute('data-colname') === 'Play')!;

        expect(bootHeader.getAttribute('data-active')).toBe('true');
        expect(playHeader.getAttribute('data-active')).toBe('false');
    });

    it('No active stage highlighted when disconnected', () => {
        vi.mocked(useLiveStoreV2).mockReturnValue({
            connectionState: 'disconnected',
            activeStage: null,
            modules: [],
            streams: [],
        } as ReturnType<typeof useLiveStoreV2>);

        useManifestStoreV2.setState({ manifest: makeManifest(), hasManifest: true });
        render(<ModulePresenceGrid />);

        const stageHeaders = screen.getAllByTestId('stage-col-header')
            .filter(h => h.getAttribute('data-colname') !== 'All');

        for (const header of stageHeaders) {
            expect(header.getAttribute('data-active')).toBe('false');
        }
    });
});
