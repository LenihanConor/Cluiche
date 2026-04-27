import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { HistoryDrawer } from './HistoryDrawer';
import type { HistoryRun } from '../state/types';

const sampleRuns: HistoryRun[] = [
    { target: 'googletest', config: 'Debug', passCount: 5, failCount: 0, totalDurationMs: 3000, startTimestamp: 1714123456, interrupted: false },
    { target: 'cluichetest', config: 'Release', passCount: 2, failCount: 1, totalDurationMs: 5000, startTimestamp: 1714120000, interrupted: false },
    { target: 'editortest', config: 'Debug', passCount: 0, failCount: 3, totalDurationMs: 1000, startTimestamp: 1714116000, interrupted: true },
];

describe('HistoryDrawer', () => {
    // AC5: shows list of past runs with target, config, pass/fail, time
    it('renders history entries with target and config', () => {
        render(<HistoryDrawer runs={sampleRuns} viewingIndex={null} dispatch={vi.fn()} />);
        expect(screen.getByText('googletest')).toBeInTheDocument();
        expect(screen.getByText('cluichetest')).toBeInTheDocument();
        expect(screen.getByText('editortest')).toBeInTheDocument();
    });

    it('shows "No history yet" when runs are empty', () => {
        render(<HistoryDrawer runs={[]} viewingIndex={null} dispatch={vi.fn()} />);
        expect(screen.getByText('No history yet')).toBeInTheDocument();
    });

    // AC6: clicking entry dispatches VIEW_HISTORY
    it('dispatches VIEW_HISTORY on entry click', () => {
        const dispatch = vi.fn();
        render(<HistoryDrawer runs={sampleRuns} viewingIndex={null} dispatch={dispatch} />);
        fireEvent.click(screen.getByText('cluichetest'));
        expect(dispatch).toHaveBeenCalledWith({ type: 'VIEW_HISTORY', index: 1 });
    });

    // AC7: "Back to current" button dispatches CLEAR_HISTORY_VIEW
    it('shows "Back to current" button when viewing history', () => {
        render(<HistoryDrawer runs={sampleRuns} viewingIndex={1} dispatch={vi.fn()} />);
        expect(screen.getByText('Back to current')).toBeInTheDocument();
    });

    it('dispatches CLEAR_HISTORY_VIEW on "Back to current" click', () => {
        const dispatch = vi.fn();
        render(<HistoryDrawer runs={sampleRuns} viewingIndex={1} dispatch={dispatch} />);
        fireEvent.click(screen.getByText('Back to current'));
        expect(dispatch).toHaveBeenCalledWith({ type: 'CLEAR_HISTORY_VIEW' });
    });

    it('does not show "Back to current" when not viewing history', () => {
        render(<HistoryDrawer runs={sampleRuns} viewingIndex={null} dispatch={vi.fn()} />);
        expect(screen.queryByText('Back to current')).not.toBeInTheDocument();
    });

    it('shows History header', () => {
        render(<HistoryDrawer runs={sampleRuns} viewingIndex={null} dispatch={vi.fn()} />);
        expect(screen.getByText('History')).toBeInTheDocument();
    });
});
