import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { FileConflictDialog } from './FileConflictDialog';

describe('FileConflictDialog', () => {
    const defaultProps = {
        isOpen: true,
        filePath: 'C:/projects/game/manifest.json',
        onReload: vi.fn(),
        onKeepEdits: vi.fn(),
    };

    it('renders nothing when isOpen=false', () => {
        render(<FileConflictDialog {...defaultProps} isOpen={false} />);
        expect(screen.queryByTestId('file-conflict-dialog')).toBeNull();
    });

    it('renders dialog when isOpen=true', () => {
        render(<FileConflictDialog {...defaultProps} />);
        expect(screen.getByTestId('file-conflict-dialog')).toBeTruthy();
    });

    it('shows filePath in message', () => {
        render(<FileConflictDialog {...defaultProps} />);
        expect(screen.getByText('C:/projects/game/manifest.json')).toBeTruthy();
    });

    it('clicking Reload calls onReload', () => {
        const onReload = vi.fn();
        render(<FileConflictDialog {...defaultProps} onReload={onReload} />);
        fireEvent.click(screen.getByTestId('reload-btn'));
        expect(onReload).toHaveBeenCalledOnce();
    });

    it('clicking Keep My Edits calls onKeepEdits', () => {
        const onKeepEdits = vi.fn();
        render(<FileConflictDialog {...defaultProps} onKeepEdits={onKeepEdits} />);
        fireEvent.click(screen.getByTestId('keep-edits-btn'));
        expect(onKeepEdits).toHaveBeenCalledOnce();
    });
});
