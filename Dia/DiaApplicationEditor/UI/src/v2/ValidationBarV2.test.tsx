import { describe, it, expect, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { ValidationBarV2 } from './ValidationBarV2';
import { useValidationStoreV2 } from './useValidationStoreV2';

beforeEach(() => {
    useValidationStoreV2.setState({ result: null, isExpanded: false });
});

describe('ValidationBarV2', () => {
    it('shows no-manifest message when result is null', () => {
        render(<ValidationBarV2 />);
        expect(screen.getByText('No manifest loaded')).toBeTruthy();
    });

    it('shows error count when errors exist', () => {
        useValidationStoreV2.setState({
            result: { errorCount: 2, warningCount: 0, issues: [] },
        });
        render(<ValidationBarV2 />);
        expect(screen.getByTestId('error-count').textContent).toContain('2');
    });

    it('shows warning count when warnings exist', () => {
        useValidationStoreV2.setState({
            result: { errorCount: 0, warningCount: 3, issues: [] },
        });
        render(<ValidationBarV2 />);
        expect(screen.getByTestId('warning-count').textContent).toContain('3');
    });

    it('clicking bar with issues toggles expanded panel', () => {
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [{ ruleId: 0, severity: 'error', message: 'Cycle detected' }] },
            isExpanded: false,
        });
        const { getByTestId } = render(<ValidationBarV2 />);
        fireEvent.click(getByTestId('validation-bar'));
        expect(screen.getByTestId('validation-issues')).toBeTruthy();
    });

    it('expanded panel shows issue messages', () => {
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [{ ruleId: 0, severity: 'error', message: 'Dep cycle in MainPU' }] },
            isExpanded: true,
        });
        render(<ValidationBarV2 />);
        expect(screen.getByText(/Dep cycle in MainPU/)).toBeTruthy();
    });
});
