import { describe, it, expect, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { ValidationPanel } from './ValidationPanel';
import { useManifestStore } from './ManifestStore';

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
