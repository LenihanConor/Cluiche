import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { StagesTab } from './StagesTab';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import type { ManifestV2 } from './types';

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn().mockResolvedValue({ ok: true }),
    bridgeEvent: vi.fn(),
}));

import { bridgeRequest } from './bridge';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function setManifest(manifest: ManifestV2 | null) {
    useManifestStoreV2.setState({ manifest, hasManifest: manifest !== null });
}

function setLive(connected: boolean, activeStage: string | null = null) {
    useLiveStoreV2.setState({
        connectionState: connected ? 'connected' : 'disconnected',
        activeStage,
    });
}

const makeManifest = (stages: ManifestV2['stages']): ManifestV2 => ({
    version: 3,
    stages,
    initialStage: stages[0]?.name ?? '',
    streams: [],
    processingUnits: [],
});

beforeEach(() => {
    vi.clearAllMocks();
    setManifest(null);
    setLive(false, null);
});

// ---------------------------------------------------------------------------
// Empty / single stage
// ---------------------------------------------------------------------------

describe('StagesTab — empty', () => {
    it('renders placeholder when stages is empty', () => {
        setManifest(makeManifest([]));
        render(<StagesTab />);
        expect(screen.getByTestId('stages-empty')).toBeTruthy();
    });

    it('renders placeholder when manifest is null', () => {
        setManifest(null);
        render(<StagesTab />);
        expect(screen.getByTestId('stages-empty')).toBeTruthy();
    });
});

describe('StagesTab — single stage', () => {
    it('renders one node, no edges', () => {
        setManifest(makeManifest([
            { name: 'Boot', manifestPath: '', transitions: [], autoAdvance: false },
        ]));
        render(<StagesTab />);
        const nodes = screen.getAllByTestId('stage-node');
        expect(nodes.length).toBe(1);
        expect(screen.queryAllByTestId('stage-edge').length).toBe(0);
    });
});

// ---------------------------------------------------------------------------
// Node rendering (static)
// ---------------------------------------------------------------------------

describe('StagesTab — static nodes', () => {
    beforeEach(() => {
        setManifest(makeManifest([
            { name: 'Boot',     manifestPath: '', transitions: ['Main'], autoAdvance: false },
            { name: 'Main',     manifestPath: '', transitions: [],       autoAdvance: true  },
            { name: 'GameOver', manifestPath: '', transitions: [],       autoAdvance: false },
        ]));
    });

    it('renders correct number of nodes', () => {
        render(<StagesTab />);
        expect(screen.getAllByTestId('stage-node').length).toBe(3);
    });

    it('initial node has data-is-initial=true', () => {
        render(<StagesTab />);
        const nodes = screen.getAllByTestId('stage-node');
        const boot = nodes.find(n => n.getAttribute('data-stage-name') === 'Boot')!;
        expect(boot.getAttribute('data-is-initial')).toBe('true');
    });

    it('auto-advance node has data-is-auto=true', () => {
        render(<StagesTab />);
        const nodes = screen.getAllByTestId('stage-node');
        const main = nodes.find(n => n.getAttribute('data-stage-name') === 'Main')!;
        expect(main.getAttribute('data-is-auto')).toBe('true');
    });

    it('manual node has data-is-auto=false', () => {
        render(<StagesTab />);
        const nodes = screen.getAllByTestId('stage-node');
        const go = nodes.find(n => n.getAttribute('data-stage-name') === 'GameOver')!;
        expect(go.getAttribute('data-is-auto')).toBe('false');
    });
});

// ---------------------------------------------------------------------------
// Edge rendering
// ---------------------------------------------------------------------------

describe('StagesTab — edges', () => {
    it('renders one edge for one transition', () => {
        setManifest(makeManifest([
            { name: 'Boot', manifestPath: '', transitions: ['Main'], autoAdvance: false },
            { name: 'Main', manifestPath: '', transitions: [],       autoAdvance: false },
        ]));
        render(<StagesTab />);
        const edges = screen.getAllByTestId('stage-edge');
        expect(edges.length).toBe(1);
        expect(edges[0].getAttribute('data-edge-from')).toBe('Boot');
        expect(edges[0].getAttribute('data-edge-to')).toBe('Main');
    });

    it('renders zero edges when all transitions are empty', () => {
        setManifest(makeManifest([
            { name: 'A', manifestPath: '', transitions: [], autoAdvance: false },
            { name: 'B', manifestPath: '', transitions: [], autoAdvance: false },
        ]));
        render(<StagesTab />);
        expect(screen.queryAllByTestId('stage-edge').length).toBe(0);
    });

    it('renders multiple edges from one stage', () => {
        setManifest(makeManifest([
            { name: 'Boot', manifestPath: '', transitions: ['Main', 'Error'], autoAdvance: false },
            { name: 'Main',  manifestPath: '', transitions: [], autoAdvance: false },
            { name: 'Error', manifestPath: '', transitions: [], autoAdvance: false },
        ]));
        render(<StagesTab />);
        const edges = screen.getAllByTestId('stage-edge');
        expect(edges.length).toBe(2);
    });

    it('skips edges to unknown targets (no crash)', () => {
        setManifest(makeManifest([
            { name: 'Boot', manifestPath: '', transitions: ['Unknown'], autoAdvance: false },
        ]));
        render(<StagesTab />);
        expect(screen.queryAllByTestId('stage-edge').length).toBe(0);
    });
});

// ---------------------------------------------------------------------------
// Live overlay
// ---------------------------------------------------------------------------

describe('StagesTab — live overlay', () => {
    beforeEach(() => {
        setManifest(makeManifest([
            { name: 'Boot', manifestPath: '', transitions: ['Main'], autoAdvance: true },
            { name: 'Main', manifestPath: '', transitions: [],       autoAdvance: false },
        ]));
    });

    it('active node has data-active=true in live mode', () => {
        setLive(true, 'Boot');
        render(<StagesTab />);
        const nodes = screen.getAllByTestId('stage-node');
        const boot = nodes.find(n => n.getAttribute('data-stage-name') === 'Boot')!;
        expect(boot.getAttribute('data-active')).toBe('true');
    });

    it('non-active nodes have data-active=false in live mode', () => {
        setLive(true, 'Boot');
        render(<StagesTab />);
        const nodes = screen.getAllByTestId('stage-node');
        const main = nodes.find(n => n.getAttribute('data-stage-name') === 'Main')!;
        expect(main.getAttribute('data-active')).toBe('false');
    });

    it('active stage with autoAdvance=true sets data-live-edge=true on outgoing edge', () => {
        setLive(true, 'Boot');
        render(<StagesTab />);
        const edges = screen.getAllByTestId('stage-edge');
        const bootEdge = edges.find(e =>
            e.getAttribute('data-edge-from') === 'Boot' && e.getAttribute('data-edge-to') === 'Main')!;
        expect(bootEdge.getAttribute('data-live-edge')).toBe('true');
    });

    it('active stage with autoAdvance=false does NOT set data-live-edge=true', () => {
        setManifest(makeManifest([
            { name: 'Boot', manifestPath: '', transitions: ['Main'], autoAdvance: false },
            { name: 'Main', manifestPath: '', transitions: [],       autoAdvance: false },
        ]));
        setLive(true, 'Boot');
        render(<StagesTab />);
        const edges = screen.getAllByTestId('stage-edge');
        expect(edges[0].getAttribute('data-live-edge')).toBe('false');
    });

    it('static mode: no node is active', () => {
        setLive(false);
        render(<StagesTab />);
        const nodes = screen.getAllByTestId('stage-node');
        nodes.forEach(n => expect(n.getAttribute('data-active')).toBe('false'));
    });
});

// ---------------------------------------------------------------------------
// Live click-to-transition
// ---------------------------------------------------------------------------

describe('StagesTab — click-to-transition', () => {
    beforeEach(() => {
        setManifest(makeManifest([
            { name: 'Boot', manifestPath: '', transitions: ['Main'], autoAdvance: false },
            { name: 'Main', manifestPath: '', transitions: [],       autoAdvance: false },
        ]));
    });

    it('clicking non-active node in live mode dispatches app.transitionTo', () => {
        setLive(true, 'Boot');
        render(<StagesTab />);
        const nodes = screen.getAllByTestId('stage-node');
        const main = nodes.find(n => n.getAttribute('data-stage-name') === 'Main')!;
        fireEvent.click(main);
        expect(bridgeRequest).toHaveBeenCalledWith('app.transitionTo', { stage: 'Main' });
    });

    it('clicking active node in live mode is a no-op', () => {
        setLive(true, 'Boot');
        render(<StagesTab />);
        const nodes = screen.getAllByTestId('stage-node');
        const boot = nodes.find(n => n.getAttribute('data-stage-name') === 'Boot')!;
        fireEvent.click(boot);
        expect(bridgeRequest).not.toHaveBeenCalled();
    });

    it('clicking any node in static mode is a no-op', () => {
        setLive(false);
        render(<StagesTab />);
        const nodes = screen.getAllByTestId('stage-node');
        fireEvent.click(nodes[0]);
        expect(bridgeRequest).not.toHaveBeenCalled();
    });
});
