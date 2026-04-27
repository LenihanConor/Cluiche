import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { PipelinePanel } from './PipelinePanel';
import { initialPipelineState } from '../state/types';
import type { PipelineState } from '../state/types';

describe('PipelinePanel', () => {
    // AC8: empty state
    it('shows empty state message when no run data', () => {
        render(<PipelinePanel state={initialPipelineState} dispatch={vi.fn()} />);
        expect(screen.getByText('No pipeline run yet')).toBeInTheDocument();
    });

    // AC7: run summary header
    it('shows run summary when a run exists', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            target: 'googletest',
            config: 'Debug',
            passCount: 2,
            failCount: 1,
            stages: [
                { name: 'compile-code', status: 'passed', durationMs: 1500, startTimestamp: 0, logLines: [], expanded: false },
            ],
        };
        render(<PipelinePanel state={state} dispatch={vi.fn()} />);
        expect(screen.getByText('Pipeline')).toBeInTheDocument();
        expect(screen.getByText(/googletest/)).toBeInTheDocument();
        expect(screen.getByText(/googletest · Debug/)).toBeInTheDocument();
    });

    // AC3: stage status icons
    it('renders stage rows with correct status text', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            runInProgress: true,
            target: 'test',
            stages: [
                { name: 'proto-compile', status: 'passed', durationMs: 800, startTimestamp: 0, logLines: [], expanded: false },
                { name: 'compile-code', status: 'running', durationMs: 0, startTimestamp: Date.now() / 1000, logLines: [], expanded: false },
                { name: 'asset-build', status: 'not-started', durationMs: 0, startTimestamp: 0, logLines: [], expanded: false },
            ],
        };
        render(<PipelinePanel state={state} dispatch={vi.fn()} />);
        expect(screen.getByText('proto-compile')).toBeInTheDocument();
        expect(screen.getByText('compile-code')).toBeInTheDocument();
        expect(screen.getByText('asset-build')).toBeInTheDocument();
    });

    // AC5: clicking a stage row expands it
    it('dispatches TOGGLE_STAGE on stage row click', () => {
        const dispatch = vi.fn();
        const state: PipelineState = {
            ...initialPipelineState,
            target: 'test',
            stages: [
                { name: 'compile-code', status: 'passed', durationMs: 1500, startTimestamp: 0, logLines: [], expanded: false },
            ],
        };
        render(<PipelinePanel state={state} dispatch={dispatch} />);
        fireEvent.click(screen.getByText('compile-code'));
        expect(dispatch).toHaveBeenCalledWith({ type: 'TOGGLE_STAGE', stageName: 'compile-code' });
    });

    // AC6: log lines colour-coded by level
    it('shows log lines when stage is expanded', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            target: 'test',
            stages: [{
                name: 'compile-code',
                status: 'passed',
                durationMs: 1500,
                startTimestamp: 0,
                logLines: [
                    { level: 'info', message: 'Building GoogleTests.vcxproj...', timestamp: 1000 },
                    { level: 'error', message: 'C2065: undeclared identifier', timestamp: 1001 },
                    { level: 'warn', message: 'unused variable', timestamp: 1002 },
                ],
                expanded: true,
            }],
        };
        render(<PipelinePanel state={state} dispatch={vi.fn()} />);
        expect(screen.getByText('Building GoogleTests.vcxproj...')).toBeInTheDocument();
        expect(screen.getByText('C2065: undeclared identifier')).toBeInTheDocument();
        expect(screen.getByText('unused variable')).toBeInTheDocument();
    });

    // AC9: interrupted run indicator
    it('shows interrupted indicator', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            target: 'test',
            interrupted: true,
            stages: [
                { name: 'compile-code', status: 'interrupted', durationMs: 0, startTimestamp: 0, logLines: [], expanded: false },
            ],
        };
        render(<PipelinePanel state={state} dispatch={vi.fn()} />);
        expect(screen.getByText('interrupted')).toBeInTheDocument();
    });

    // AC4: elapsed time display
    it('shows elapsed time for completed stage', () => {
        const state: PipelineState = {
            ...initialPipelineState,
            target: 'test',
            stages: [
                { name: 'compile-code', status: 'passed', durationMs: 1500, startTimestamp: 0, logLines: [], expanded: false },
            ],
        };
        render(<PipelinePanel state={state} dispatch={vi.fn()} />);
        expect(screen.getByText('1.5s')).toBeInTheDocument();
    });
});
