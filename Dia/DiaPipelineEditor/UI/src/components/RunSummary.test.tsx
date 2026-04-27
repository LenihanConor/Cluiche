import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { RunSummary } from './RunSummary';
import { initialPipelineState } from '../state/types';
import type { PipelineState } from '../state/types';

function mkState(overrides: Partial<PipelineState>): PipelineState {
    return { ...initialPipelineState, ...overrides };
}

describe('RunSummary', () => {
    it('shows "Pipeline" heading', () => {
        render(<RunSummary state={mkState({})} />);
        expect(screen.getByText('Pipeline')).toBeInTheDocument();
    });

    it('shows target and config', () => {
        render(<RunSummary state={mkState({ target: 'googletest', config: 'Debug' })} />);
        expect(screen.getByText(/googletest/)).toBeInTheDocument();
        expect(screen.getByText(/Debug/)).toBeInTheDocument();
    });

    it('shows pass count', () => {
        render(<RunSummary state={mkState({ passCount: 3 })} />);
        expect(screen.getByText(/3 passed/)).toBeInTheDocument();
    });

    it('shows fail count', () => {
        render(<RunSummary state={mkState({ failCount: 2 })} />);
        expect(screen.getByText(/2 failed/)).toBeInTheDocument();
    });

    it('shows both pass and fail counts', () => {
        render(<RunSummary state={mkState({ passCount: 5, failCount: 1 })} />);
        expect(screen.getByText(/5 passed/)).toBeInTheDocument();
        expect(screen.getByText(/1 failed/)).toBeInTheDocument();
    });

    it('shows "running" when in progress with no pass/fail', () => {
        render(<RunSummary state={mkState({ runInProgress: true })} />);
        expect(screen.getByText(/running/)).toBeInTheDocument();
    });

    it('shows "no stages" when not running and no counts', () => {
        render(<RunSummary state={mkState({ runInProgress: false, passCount: 0, failCount: 0 })} />);
        expect(screen.getByText(/no stages/)).toBeInTheDocument();
    });

    it('shows interrupted label', () => {
        render(<RunSummary state={mkState({ interrupted: true, target: 'test' })} />);
        expect(screen.getByText('interrupted')).toBeInTheDocument();
    });

    it('formats sub-minute duration', () => {
        render(<RunSummary state={mkState({ totalDurationMs: 1500 })} />);
        expect(screen.getByText('1.5s')).toBeInTheDocument();
    });

    it('formats multi-minute duration', () => {
        render(<RunSummary state={mkState({ totalDurationMs: 65000 })} />);
        expect(screen.getByText('1:05.0')).toBeInTheDocument();
    });

    it('shows dash for zero duration', () => {
        render(<RunSummary state={mkState({ totalDurationMs: 0 })} />);
        expect(screen.getByText('—')).toBeInTheDocument();
    });

    it('shows dash for negative duration', () => {
        render(<RunSummary state={mkState({ totalDurationMs: -1 })} />);
        expect(screen.getByText('—')).toBeInTheDocument();
    });
});
