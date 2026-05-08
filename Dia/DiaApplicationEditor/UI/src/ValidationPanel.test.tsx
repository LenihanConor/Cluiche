import { describe, it, expect, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { ValidationPanel, resolveContextToNodeId } from './ValidationPanel';
import { useManifestStore } from './ManifestStore';
import type { ManifestData } from './types';

beforeEach(() => {
    useManifestStore.setState({ validationResult: null, selectedNode: null });
});

describe('ValidationPanel – no result', () => {
    it('renders nothing when validationResult is null', () => {
        const { container } = render(<ValidationPanel />);
        expect(container.firstChild).toBeNull();
    });
});

describe('ValidationPanel – valid manifest', () => {
    it('shows success message when there are no errors', () => {
        useManifestStore.setState({ validationResult: { is_valid: true, errors: [] } });
        render(<ValidationPanel />);
        expect(screen.getByText(/no validation errors/i)).toBeInTheDocument();
    });
});

describe('ValidationPanel – errors and warnings', () => {
    const errors = [
        { message: 'Missing required field', context: 'pu_init_renderer', severity: 'error' as const, code: 'E001' },
        { message: 'Deprecated config key', context: 'pu_update_physics', severity: 'warning' as const, code: 'W001' },
    ];

    beforeEach(() => {
        useManifestStore.setState({ validationResult: { is_valid: false, errors } });
    });

    it('shows "Validation Issues" header', () => {
        render(<ValidationPanel />);
        expect(screen.getByText('Validation Issues')).toBeInTheDocument();
    });

    it('shows correct error count badge', () => {
        render(<ValidationPanel />);
        expect(screen.getByText(/1 error/)).toBeInTheDocument();
    });

    it('shows correct warning count badge', () => {
        render(<ValidationPanel />);
        expect(screen.getByText(/1 warning/)).toBeInTheDocument();
    });

    it('renders each error message', () => {
        render(<ValidationPanel />);
        expect(screen.getByText('Missing required field')).toBeInTheDocument();
        expect(screen.getByText('Deprecated config key')).toBeInTheDocument();
    });

    it('renders context paths', () => {
        render(<ValidationPanel />);
        expect(screen.getByText('pu_init_renderer')).toBeInTheDocument();
        expect(screen.getByText('pu_update_physics')).toBeInTheDocument();
    });

    it('clicking an error sets selectedNode to its context', async () => {
        render(<ValidationPanel />);
        await userEvent.click(screen.getByText('Missing required field'));
        expect(useManifestStore.getState().selectedNode).toBe('pu_init_renderer');
    });

    it('error badge is pluralised correctly for multiple errors', () => {
        const multiErrors = [
            { message: 'Err1', context: 'a', severity: 'error' as const, code: 'E1' },
            { message: 'Err2', context: 'b', severity: 'error' as const, code: 'E2' },
        ];
        useManifestStore.setState({ validationResult: { is_valid: false, errors: multiErrors } });
        render(<ValidationPanel />);
        expect(screen.getByText(/2 errors/)).toBeInTheDocument();
    });
});

// ==============================================================================
// resolveContextToNodeId unit tests
// ==============================================================================

describe('resolveContextToNodeId', () => {
    const manifest: ManifestData = {
        version: 1,
        imports: [],
        processing_units: [
            {
                instance_id: 'MainPU',
                type: 'MainPU',
                frequency_hz: 60,
                dedicated_thread: false,
                initial_phase: 'Init',
                phases: [
                    { instance_id: 'Init', type: 'InitPhase', config: {} },
                    { instance_id: 'Update', type: 'UpdatePhase', config: {} },
                ],
                transitions: [],
                modules: [
                    { instance_id: 'Renderer', type: 'RenderModule', phases: ['Update'], dependencies: [], config: {} },
                ],
                config: {},
            },
        ],
    };

    it('returns null for empty context', () => {
        expect(resolveContextToNodeId(manifest, '')).toBeNull();
    });

    it('matches PU by instance_id (plain string context)', () => {
        expect(resolveContextToNodeId(manifest, 'MainPU')).toBe('MainPU');
    });

    it('resolves processing_units[0] to PU node ID', () => {
        expect(resolveContextToNodeId(manifest, 'processing_units[0]')).toBe('MainPU');
    });

    it('resolves processing_units[0].phases[1] to PU_Phase node ID', () => {
        expect(resolveContextToNodeId(manifest, 'processing_units[0].phases[1]')).toBe('MainPU_Update');
    });

    it('resolves processing_units[0].modules[0] to PU_Phase_Module node ID', () => {
        const result = resolveContextToNodeId(manifest, 'processing_units[0].phases[0].modules[0]');
        expect(result).toBe('MainPU_Update_Renderer');
    });

    it('returns null for unknown plain string context', () => {
        expect(resolveContextToNodeId(manifest, 'NonExistentPU')).toBeNull();
    });

    it('returns PU node ID for out-of-range phase index', () => {
        expect(resolveContextToNodeId(manifest, 'processing_units[0].phases[99]')).toBe('MainPU');
    });
});
