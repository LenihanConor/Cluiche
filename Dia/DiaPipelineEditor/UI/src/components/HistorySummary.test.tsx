import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { HistorySummary } from './HistorySummary';
import type { HistoryRun } from '../state/types';

describe('HistorySummary', () => {
    it('renders target and config', () => {
        const run: HistoryRun = {
            target: 'googletest',
            config: 'Debug',
            passCount: 5,
            failCount: 1,
            totalDurationMs: 3000,
            startTimestamp: 1714123456,
            interrupted: false,
        };
        render(<HistorySummary run={run} />);
        expect(screen.getByText('googletest')).toBeInTheDocument();
        expect(screen.getByText('Past Run')).toBeInTheDocument();
    });

    it('shows pass and fail counts', () => {
        const run: HistoryRun = {
            target: 'test',
            config: 'Release',
            passCount: 3,
            failCount: 2,
            totalDurationMs: 5000,
            startTimestamp: 1714120000,
            interrupted: false,
        };
        render(<HistorySummary run={run} />);
        expect(screen.getByText('3')).toBeInTheDocument();
        expect(screen.getByText('2')).toBeInTheDocument();
    });

    it('shows interrupted indicator', () => {
        const run: HistoryRun = {
            target: 'test',
            config: 'Debug',
            passCount: 0,
            failCount: 0,
            totalDurationMs: 500,
            startTimestamp: 1714116000,
            interrupted: true,
        };
        render(<HistorySummary run={run} />);
        expect(screen.getByText('interrupted')).toBeInTheDocument();
    });

    it('shows formatted duration', () => {
        const run: HistoryRun = {
            target: 'test',
            config: 'Debug',
            passCount: 1,
            failCount: 0,
            totalDurationMs: 1500,
            startTimestamp: 1714123456,
            interrupted: false,
        };
        render(<HistorySummary run={run} />);
        expect(screen.getByText('1.5s')).toBeInTheDocument();
    });
});
