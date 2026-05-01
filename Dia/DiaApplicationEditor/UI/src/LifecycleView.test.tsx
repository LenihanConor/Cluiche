import { describe, it, expect, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { useManifestStore } from './ManifestStore';

beforeEach(() => {
    useManifestStore.setState({ manifest: null, selectedNode: null });
});

// ── Re-export helpers under test by duplicating them here ─────────────────────
// sortPhases and classifyCell are module-private. We test them via the rendered
// component output (column order, cell symbols) which is a stable public contract.

import { LifecycleView } from './LifecycleView';

const PU_LINEAR = {
    instance_id: 'MainPU',
    type: 'MainProcessingUnit',
    phases: [
        { instance_id: 'Init',   type: 'InitPhase' },
        { instance_id: 'Update', type: 'UpdatePhase' },
        { instance_id: 'Shutdown', type: 'ShutdownPhase' },
    ],
    modules: [
        { instance_id: 'RenderMod', type: 'RenderModule', phases: ['Update'], config: {} },
    ],
    transitions: [
        { from: 'Init', to: 'Update' },
        { from: 'Update', to: 'Shutdown' },
    ],
};

const MANIFEST_SIMPLE = { version: 1, processing_units: [PU_LINEAR] };

// ── sortPhases (via column order) ─────────────────────────────────────────────

describe('LifecycleView – sortPhases', () => {
    it('renders null when no manifest', () => {
        const { container } = render(<LifecycleView />);
        expect(container.firstChild).toBeNull();
    });

    it('columns appear in topological order (Init → Update → Shutdown)', () => {
        useManifestStore.setState({ manifest: MANIFEST_SIMPLE });
        render(<LifecycleView />);
        const headers = screen.getAllByRole('columnheader').map(h => h.textContent?.trim()).filter(Boolean);
        const init    = headers.indexOf('Init');
        const update  = headers.indexOf('Update');
        const shutdown = headers.indexOf('Shutdown');
        expect(init).toBeLessThan(update);
        expect(update).toBeLessThan(shutdown);
    });

    it('handles phases with no transitions — falls back to declaration order', () => {
        const manifest = {
            version: 1,
            processing_units: [{
                ...PU_LINEAR,
                transitions: [],
            }],
        };
        useManifestStore.setState({ manifest });
        render(<LifecycleView />);
        const headers = screen.getAllByRole('columnheader').map(h => h.textContent?.trim()).filter(Boolean);
        expect(headers).toContain('Init');
        expect(headers).toContain('Update');
        expect(headers).toContain('Shutdown');
    });

    it('handles a cycle gracefully — all phases still rendered', () => {
        const manifest = {
            version: 1,
            processing_units: [{
                ...PU_LINEAR,
                transitions: [
                    { from: 'Init', to: 'Update' },
                    { from: 'Update', to: 'Init' }, // cycle
                ],
            }],
        };
        useManifestStore.setState({ manifest });
        render(<LifecycleView />);
        const headers = screen.getAllByRole('columnheader').map(h => h.textContent?.trim()).filter(Boolean);
        expect(headers).toContain('Init');
        expect(headers).toContain('Update');
        expect(headers).toContain('Shutdown');
    });
});

// ── classifyCell (via cell symbols) ──────────────────────────────────────────

describe('LifecycleView – classifyCell symbols', () => {
    it('shows ● (active) when module is in a phase with no adjacent transitions', () => {
        const manifest = {
            version: 1,
            processing_units: [{
                instance_id: 'PU',
                type: 'T',
                phases: [{ instance_id: 'Lone', type: 'UpdatePhase' }],
                modules: [{ instance_id: 'Mod', type: 'T', phases: ['Lone'], config: {} }],
                transitions: [],
            }],
        };
        useManifestStore.setState({ manifest });
        render(<LifecycleView />);
        expect(screen.getByText('●')).toBeInTheDocument();
    });

    it('shows ↑ (acquired) when module is in a phase that has a predecessor but module was not in the predecessor', () => {
        // Module in Update only; transition Init→Update→Shutdown; module NOT in Init but IS in Update
        // classifyCell for Update column: here=true, prevPhase=Init, nextPhase=Shutdown
        // goingNext = hasTransition(Update,Shutdown) = true, but module not in Shutdown → activeNext=false
        // cameFromPrev = hasTransition(Init,Update) = true, but module not in Init → activePrev=false
        // → active (not acquired). Need: module in a middle phase connected from prev but not in prev.
        // Acquired = activeNext && !activePrev:
        //   module in [Update, Shutdown], transition Update→Shutdown; no transition into Update
        const manifest = {
            version: 1,
            processing_units: [{
                instance_id: 'PU',
                type: 'T',
                phases: [
                    { instance_id: 'Update',   type: 'UpdatePhase'   },
                    { instance_id: 'Shutdown', type: 'ShutdownPhase' },
                ],
                modules: [{ instance_id: 'Mod', type: 'T', phases: ['Update', 'Shutdown'], config: {} }],
                transitions: [{ from: 'Update', to: 'Shutdown' }],
            }],
        };
        useManifestStore.setState({ manifest });
        render(<LifecycleView />);
        // Update column: here=true, next=Shutdown (in phases, transition exists) → activeNext=true
        // prev=null → activePrev=false → acquired (↑)
        expect(screen.getByText('↑')).toBeInTheDocument();
    });

    it('shows ↓ (released) when module was in prev and here, but not in next — transition to next exists', () => {
        // For phase B: activePrev=true (A→B, mod in A), nextPhase=C, goingNext=true (B→C), but mod NOT in C
        // → activeNext=false, activePrev=true → released (↓)
        const manifest = {
            version: 1,
            processing_units: [{
                instance_id: 'PU',
                type: 'T',
                phases: [
                    { instance_id: 'A', type: 'InitPhase'     },
                    { instance_id: 'B', type: 'UpdatePhase'   },
                    { instance_id: 'C', type: 'ShutdownPhase' },
                ],
                modules: [{ instance_id: 'Mod', type: 'T', phases: ['A', 'B'], config: {} }],
                transitions: [{ from: 'A', to: 'B' }, { from: 'B', to: 'C' }],
            }],
        };
        useManifestStore.setState({ manifest });
        render(<LifecycleView />);
        expect(screen.getByText('↓')).toBeInTheDocument();
    });

    it('shows ↔ (retained) when module is in prev, here, and next — both transitions exist', () => {
        // For the middle phase B: activePrev=true (A→B, mod in A), activeNext=true (B→C, mod in C)
        const manifest = {
            version: 1,
            processing_units: [{
                instance_id: 'PU',
                type: 'T',
                phases: [
                    { instance_id: 'A', type: 'InitPhase'     },
                    { instance_id: 'B', type: 'UpdatePhase'   },
                    { instance_id: 'C', type: 'ShutdownPhase' },
                ],
                modules: [{ instance_id: 'Mod', type: 'T', phases: ['A', 'B', 'C'], config: {} }],
                transitions: [{ from: 'A', to: 'B' }, { from: 'B', to: 'C' }],
            }],
        };
        useManifestStore.setState({ manifest });
        render(<LifecycleView />);
        expect(screen.getByText('↔')).toBeInTheDocument();
    });
});

// ── Filter ────────────────────────────────────────────────────────────────────

describe('LifecycleView – module filter', () => {
    it('shows all modules with empty filter', () => {
        const manifest = {
            version: 1,
            processing_units: [{
                ...PU_LINEAR,
                modules: [
                    { instance_id: 'Alpha', type: 'AlphaModule', phases: ['Init'], config: {} },
                    { instance_id: 'Beta',  type: 'BetaModule',  phases: ['Update'], config: {} },
                ],
            }],
        };
        useManifestStore.setState({ manifest });
        render(<LifecycleView />);
        expect(screen.getByText('Alpha')).toBeInTheDocument();
        expect(screen.getByText('Beta')).toBeInTheDocument();
    });

    it('filters modules by instance_id', async () => {
        const manifest = {
            version: 1,
            processing_units: [{
                ...PU_LINEAR,
                modules: [
                    { instance_id: 'Alpha', type: 'AlphaModule', phases: ['Init'], config: {} },
                    { instance_id: 'Beta',  type: 'BetaModule',  phases: ['Update'], config: {} },
                ],
            }],
        };
        useManifestStore.setState({ manifest });
        render(<LifecycleView />);
        await userEvent.type(screen.getByPlaceholderText(/filter modules/i), 'Alpha');
        expect(screen.getByText('Alpha')).toBeInTheDocument();
        expect(screen.queryByText('Beta')).not.toBeInTheDocument();
    });

    it('filters modules by type', async () => {
        const manifest = {
            version: 1,
            processing_units: [{
                ...PU_LINEAR,
                modules: [
                    { instance_id: 'Alpha', type: 'AlphaModule', phases: ['Init'], config: {} },
                    { instance_id: 'Beta',  type: 'BetaModule',  phases: ['Update'], config: {} },
                ],
            }],
        };
        useManifestStore.setState({ manifest });
        render(<LifecycleView />);
        await userEvent.type(screen.getByPlaceholderText(/filter modules/i), 'BetaModule');
        expect(screen.queryByText('Alpha')).not.toBeInTheDocument();
        expect(screen.getByText('Beta')).toBeInTheDocument();
    });

    it('hides a PU entirely when all its modules are filtered out', async () => {
        const manifest = {
            version: 1,
            processing_units: [{
                ...PU_LINEAR,
                modules: [
                    { instance_id: 'Alpha', type: 'AlphaModule', phases: ['Init'], config: {} },
                ],
            }],
        };
        useManifestStore.setState({ manifest });
        render(<LifecycleView />);
        await userEvent.type(screen.getByPlaceholderText(/filter modules/i), 'ZZZ');
        expect(screen.queryByText('MainPU')).not.toBeInTheDocument();
    });
});

// ── Selection ──────────────────────────────────────────────────────────────────

describe('LifecycleView – module selection', () => {
    it('clicking a module row sets selectedNode in the store', async () => {
        useManifestStore.setState({ manifest: MANIFEST_SIMPLE });
        render(<LifecycleView />);
        await userEvent.click(screen.getByText('RenderMod'));
        expect(useManifestStore.getState().selectedNode).toMatch(/RenderMod/);
    });
});
