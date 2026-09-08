import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { RiskyChangeDialog } from './RiskyChangeDialog';

describe('RiskyChangeDialog', () => {
    const defaultProps = {
        isOpen: true,
        riskCondition: 'RemovePU',
        commandDescription: 'Removing MainPU will disconnect all modules.',
        onProceed: vi.fn(),
        onCancel: vi.fn(),
    };

    it('renders nothing when isOpen=false', () => {
        render(<RiskyChangeDialog {...defaultProps} isOpen={false} />);
        expect(screen.queryByTestId('risky-dialog')).toBeNull();
    });

    it('renders dialog when isOpen=true', () => {
        render(<RiskyChangeDialog {...defaultProps} />);
        expect(screen.getByTestId('risky-dialog')).toBeTruthy();
    });

    it('shows correct riskCondition in badge', () => {
        render(<RiskyChangeDialog {...defaultProps} />);
        const badge = screen.getByTestId('risk-condition-badge');
        expect(badge.textContent).toBe('RemovePU');
    });

    it('shows commandDescription in warning text', () => {
        render(<RiskyChangeDialog {...defaultProps} />);
        expect(screen.getByText(/Removing MainPU will disconnect all modules/)).toBeTruthy();
    });

    it('clicking "Proceed Anyway" calls onProceed', () => {
        const onProceed = vi.fn();
        render(<RiskyChangeDialog {...defaultProps} onProceed={onProceed} />);
        fireEvent.click(screen.getByTestId('proceed-btn'));
        expect(onProceed).toHaveBeenCalledOnce();
    });

    it('clicking "Cancel" calls onCancel', () => {
        const onCancel = vi.fn();
        render(<RiskyChangeDialog {...defaultProps} onCancel={onCancel} />);
        fireEvent.click(screen.getByTestId('cancel-btn'));
        expect(onCancel).toHaveBeenCalledOnce();
    });
});
