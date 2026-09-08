import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect, vi } from 'vitest';
import { ConfirmDialog } from './ConfirmDialog';

vi.mock('@dia/editor-ui', () => ({
    theme: { bgPanel: '#252526', border: '#3c3c3c', text: '#d4d4d4', error: '#f48771' },
    buttonStyle: () => ({}),
}));

const baseProps = {
    isOpen: true,
    title: 'Test Title',
    message: 'Test message body',
    onConfirm: vi.fn(),
    onCancel: vi.fn(),
};

describe('ConfirmDialog', () => {
    it('returns null when isOpen is false', () => {
        const { container } = render(<ConfirmDialog {...baseProps} isOpen={false} />);
        expect(container.firstChild).toBeNull();
    });

    it('renders dialog when isOpen is true', () => {
        render(<ConfirmDialog {...baseProps} />);
        expect(screen.getByTestId('confirm-dialog')).toBeTruthy();
    });

    it('shows title text', () => {
        render(<ConfirmDialog {...baseProps} title="Are you sure?" />);
        expect(screen.getByTestId('confirm-title').textContent).toBe('Are you sure?');
    });

    it('shows message text', () => {
        render(<ConfirmDialog {...baseProps} message="This action cannot be undone." />);
        expect(screen.getByText('This action cannot be undone.')).toBeTruthy();
    });

    it('calls onConfirm when confirm button is clicked', () => {
        const onConfirm = vi.fn();
        render(<ConfirmDialog {...baseProps} onConfirm={onConfirm} />);
        fireEvent.click(screen.getByTestId('confirm-btn'));
        expect(onConfirm).toHaveBeenCalledTimes(1);
    });

    it('calls onCancel when cancel button is clicked', () => {
        const onCancel = vi.fn();
        render(<ConfirmDialog {...baseProps} onCancel={onCancel} />);
        fireEvent.click(screen.getByTestId('cancel-btn'));
        expect(onCancel).toHaveBeenCalledTimes(1);
    });

    it('shows custom confirmLabel on confirm button', () => {
        render(<ConfirmDialog {...baseProps} confirmLabel="Yes, add it" />);
        expect(screen.getByTestId('confirm-btn').textContent).toBe('Yes, add it');
    });

    it('shows custom cancelLabel on cancel button', () => {
        render(<ConfirmDialog {...baseProps} cancelLabel="No, go back" />);
        expect(screen.getByTestId('cancel-btn').textContent).toBe('No, go back');
    });

    it('has data-testid="confirm-dialog" when open', () => {
        render(<ConfirmDialog {...baseProps} />);
        expect(screen.getByTestId('confirm-dialog')).toBeTruthy();
    });

    it('Escape key calls onCancel', () => {
        const onCancel = vi.fn();
        render(<ConfirmDialog {...baseProps} onCancel={onCancel} />);
        fireEvent.keyDown(document, { key: 'Escape' });
        expect(onCancel).toHaveBeenCalledTimes(1);
    });

    it('Escape key does nothing when isOpen=false', () => {
        const onCancel = vi.fn();
        render(<ConfirmDialog {...baseProps} isOpen={false} onCancel={onCancel} />);
        fireEvent.keyDown(document, { key: 'Escape' });
        expect(onCancel).not.toHaveBeenCalled();
    });

    it('shows default "Confirm" label when confirmLabel not provided', () => {
        render(<ConfirmDialog {...baseProps} />);
        expect(screen.getByTestId('confirm-btn').textContent).toBe('Confirm');
    });

    it('shows default "Cancel" label when cancelLabel not provided', () => {
        render(<ConfirmDialog {...baseProps} />);
        expect(screen.getByTestId('cancel-btn').textContent).toBe('Cancel');
    });
});
