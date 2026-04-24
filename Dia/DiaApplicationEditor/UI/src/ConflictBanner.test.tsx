import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, act } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { ConflictBanner, useConflictDetection } from './ConflictBanner';

const CONFLICT = {
    conflict: {
        diskVersion: { version: 1, processing_units: [] },
        localVersion: { version: 2, processing_units: [] },
    },
    onDismiss: vi.fn(),
};

beforeEach(() => { vi.clearAllMocks(); });

describe('ConflictBanner – rendering', () => {
    it('shows "File modified externally" message', () => {
        render(<ConflictBanner {...CONFLICT} />);
        expect(screen.getByText(/file modified externally/i)).toBeInTheDocument();
    });

    it('renders Keep My Changes and Reload from Disk buttons', () => {
        render(<ConflictBanner {...CONFLICT} />);
        expect(screen.getByText('Keep My Changes')).toBeInTheDocument();
        expect(screen.getByText('Reload from Disk')).toBeInTheDocument();
    });

    it('diff is hidden by default', () => {
        render(<ConflictBanner {...CONFLICT} />);
        expect(screen.queryByText('Disk')).not.toBeInTheDocument();
    });

    it('Show Diff button toggles diff panel', async () => {
        render(<ConflictBanner {...CONFLICT} />);
        await userEvent.click(screen.getByText('Show Diff'));
        expect(screen.getByText('Disk')).toBeInTheDocument();
        expect(screen.getByText('Your Changes')).toBeInTheDocument();
    });

    it('Show Diff button label toggles to Hide Diff', async () => {
        render(<ConflictBanner {...CONFLICT} />);
        await userEvent.click(screen.getByText('Show Diff'));
        expect(screen.getByText('Hide Diff')).toBeInTheDocument();
    });
});

describe('ConflictBanner – actions', () => {
    it('Keep My Changes posts resolve_conflict keep_local and calls onDismiss', async () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<ConflictBanner {...CONFLICT} />);
        await userEvent.click(screen.getByText('Keep My Changes'));
        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({ payload: expect.objectContaining({ type: 'resolve_conflict', data: { action: 'keep_local' } }) }),
            '*'
        );
        expect(CONFLICT.onDismiss).toHaveBeenCalledTimes(1);
    });

    it('Reload from Disk posts resolve_conflict reload_disk after confirmation', async () => {
        vi.spyOn(window, 'confirm').mockReturnValue(true);
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<ConflictBanner {...CONFLICT} />);
        await userEvent.click(screen.getByText('Reload from Disk'));
        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({ payload: expect.objectContaining({ type: 'resolve_conflict', data: { action: 'reload_disk' } }) }),
            '*'
        );
        expect(CONFLICT.onDismiss).toHaveBeenCalledTimes(1);
    });

    it('Reload from Disk does nothing if user cancels confirmation', async () => {
        vi.spyOn(window, 'confirm').mockReturnValue(false);
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<ConflictBanner {...CONFLICT} />);
        await userEvent.click(screen.getByText('Reload from Disk'));
        expect(spy).not.toHaveBeenCalled();
        expect(CONFLICT.onDismiss).not.toHaveBeenCalled();
    });
});

describe('useConflictDetection hook', () => {
    function Harness() {
        const { conflict, dismiss } = useConflictDetection();
        return (
            <div>
                <span data-testid="status">{conflict ? 'conflict' : 'none'}</span>
                <button onClick={dismiss}>dismiss</button>
            </div>
        );
    }

    it('starts with no conflict', () => {
        render(<Harness />);
        expect(screen.getByTestId('status')).toHaveTextContent('none');
    });

    it('sets conflict on file_conflict_detected message', () => {
        render(<Harness />);
        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: {
                    __dia: true,
                    topic: 'file_conflict_detected',
                    data: { disk_version: {}, local_version: {} },
                },
            }));
        });
        expect(screen.getByTestId('status')).toHaveTextContent('conflict');
    });

    it('clears conflict on conflict_resolved message', async () => {
        render(<Harness />);
        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { __dia: true, topic: 'file_conflict_detected', data: { disk_version: {}, local_version: {} } },
            }));
        });
        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { __dia: true, topic: 'conflict_resolved' },
            }));
        });
        expect(screen.getByTestId('status')).toHaveTextContent('none');
    });

    it('dismiss() clears conflict', async () => {
        render(<Harness />);
        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { __dia: true, topic: 'file_conflict_detected', data: { disk_version: {}, local_version: {} } },
            }));
        });
        await userEvent.click(screen.getByText('dismiss'));
        expect(screen.getByTestId('status')).toHaveTextContent('none');
    });
});
