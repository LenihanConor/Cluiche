import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { PipelineToolbar } from './PipelineToolbar';
import type { PipelineAction } from '../state/pipelineReducer';
import type { Dispatch } from 'react';

const noop: Dispatch<PipelineAction> = () => {};

function defaultProps(overrides: Partial<{
    buildRunning: boolean;
    diagameName: string;
    canLaunch: boolean;
    lastSuccessTimestamp: number | null;
}> = {}) {
    return {
        buildRunning: false,
        diagameName: 'cluichetest',
        canLaunch: false,
        lastSuccessTimestamp: null,
        dispatch: noop,
        ...overrides,
    };
}

describe('PipelineToolbar', () => {
    let postSpy: ReturnType<typeof vi.spyOn>;

    afterEach(() => {
        vi.restoreAllMocks();
    });

    // T12: shows diagame name instead of target dropdown
    it('shows diagame name as a label, not a dropdown', () => {
        render(<PipelineToolbar {...defaultProps({ diagameName: 'cluichetest' })} />);
        expect(screen.getByText('cluichetest')).toBeInTheDocument();
        expect(screen.queryByText('Target:')).not.toBeInTheDocument();
        // No select for the target
        const selects = screen.getAllByRole('combobox');
        selects.forEach(s => expect(s).not.toHaveDisplayValue('cluichetest'));
    });

    // T12: shows em-dash when diagameName is empty
    it('shows em-dash when diagameName is empty', () => {
        render(<PipelineToolbar {...defaultProps({ diagameName: '' })} />);
        expect(screen.getByText('—')).toBeInTheDocument();
    });

    // T12: config dropdown is retained
    it('renders Config dropdown with Debug and Release options', () => {
        render(<PipelineToolbar {...defaultProps()} />);
        expect(screen.getByText('Config:')).toBeInTheDocument();
        expect(screen.getByText('Debug')).toBeInTheDocument();
        expect(screen.getByText('Release')).toBeInTheDocument();
    });

    // T12: force checkbox retained
    it('renders Force checkbox, unchecked by default', () => {
        render(<PipelineToolbar {...defaultProps()} />);
        const checkbox = screen.getByRole('checkbox');
        expect(checkbox).not.toBeChecked();
    });

    it('force checkbox can be toggled', () => {
        render(<PipelineToolbar {...defaultProps()} />);
        const checkbox = screen.getByRole('checkbox');
        fireEvent.click(checkbox);
        expect(checkbox).toBeChecked();
    });

    // T13: Build/Launch split-button
    it('renders Build split-button when not running', () => {
        render(<PipelineToolbar {...defaultProps()} />);
        expect(screen.getByTitle('Build')).toBeInTheDocument();
    });

    it('Build button is disabled when diagameName is empty', () => {
        render(<PipelineToolbar {...defaultProps({ diagameName: '' })} />);
        expect(screen.getByTitle('Build')).toBeDisabled();
    });

    it('Build button sends pipeline.start postMessage', () => {
        postSpy = vi.spyOn(window.parent, 'postMessage');
        render(<PipelineToolbar {...defaultProps({ diagameName: 'cluichetest' })} />);
        fireEvent.click(screen.getByTitle('Build'));
        const call = postSpy.mock.calls.find(c => {
            const msg = c[0] as { __diaFromFrame?: boolean; payload?: { type?: string } };
            return msg?.__diaFromFrame && msg?.payload?.type === 'pipeline.start';
        });
        expect(call).toBeDefined();
    });

    it('dropdown arrow toggles Build & Launch option', () => {
        render(<PipelineToolbar {...defaultProps({ diagameName: 'cluichetest' })} />);
        fireEvent.click(screen.getByTitle('More build options'));
        expect(screen.getByText('Build & Launch')).toBeInTheDocument();
    });

    // T13: shows Cancel when build running, hides split-button
    it('shows Cancel button when build is running', () => {
        render(<PipelineToolbar {...defaultProps({ buildRunning: true })} />);
        expect(screen.getByText('Cancel')).toBeInTheDocument();
        expect(screen.queryByTitle('Build')).not.toBeInTheDocument();
    });

    it('disables Config dropdown when build is running', () => {
        render(<PipelineToolbar {...defaultProps({ buildRunning: true })} />);
        const selects = screen.getAllByRole('combobox');
        selects.forEach(s => expect(s).toBeDisabled());
    });

    it('Cancel button sends pipeline.cancel postMessage', () => {
        postSpy = vi.spyOn(window.parent, 'postMessage');
        render(<PipelineToolbar {...defaultProps({ buildRunning: true })} />);
        fireEvent.click(screen.getByText('Cancel'));
        const call = postSpy.mock.calls.find(c => {
            const msg = c[0] as { __diaFromFrame?: boolean; payload?: { type?: string } };
            return msg?.__diaFromFrame && msg?.payload?.type === 'pipeline.cancel';
        });
        expect(call).toBeDefined();
    });

    // T13: Launch button
    it('renders Launch button, disabled when canLaunch is false', () => {
        render(<PipelineToolbar {...defaultProps({ canLaunch: false })} />);
        expect(screen.getByTitle('Launch')).toBeDisabled();
    });

    it('Launch button is enabled when canLaunch is true and not running', () => {
        render(<PipelineToolbar {...defaultProps({ canLaunch: true, buildRunning: false })} />);
        expect(screen.getByTitle('Launch')).not.toBeDisabled();
    });

    it('Launch button sends pipeline.launch postMessage', () => {
        postSpy = vi.spyOn(window.parent, 'postMessage');
        render(<PipelineToolbar {...defaultProps({ canLaunch: true })} />);
        fireEvent.click(screen.getByTitle('Launch'));
        const call = postSpy.mock.calls.find(c => {
            const msg = c[0] as { __diaFromFrame?: boolean; payload?: { type?: string } };
            return msg?.__diaFromFrame && msg?.payload?.type === 'pipeline.launch';
        });
        expect(call).toBeDefined();
    });

    // T14: "Built X ago" label
    it('shows no elapsed label when lastSuccessTimestamp is null', () => {
        render(<PipelineToolbar {...defaultProps({ lastSuccessTimestamp: null })} />);
        expect(screen.queryByText(/Built/)).not.toBeInTheDocument();
    });

    it('shows "Built Xs ago" when lastSuccessTimestamp is recent', () => {
        const now = Date.now();
        render(<PipelineToolbar {...defaultProps({ lastSuccessTimestamp: now - 10_000 })} />);
        expect(screen.getByText(/Built \d+s ago/)).toBeInTheDocument();
    });

    it('shows "Built Xm ago" when lastSuccessTimestamp is minutes ago', () => {
        const now = Date.now();
        render(<PipelineToolbar {...defaultProps({ lastSuccessTimestamp: now - 5 * 60 * 1000 })} />);
        expect(screen.getByText(/Built \d+m ago/)).toBeInTheDocument();
    });

    it('shows "Built Xh ago" when lastSuccessTimestamp is hours ago', () => {
        const now = Date.now();
        render(<PipelineToolbar {...defaultProps({ lastSuccessTimestamp: now - 2 * 3600 * 1000 })} />);
        expect(screen.getByText(/Built \d+h ago/)).toBeInTheDocument();
    });

    // T14: Open logs link
    it('renders logs link', () => {
        render(<PipelineToolbar {...defaultProps()} />);
        expect(screen.getByText('↗ logs')).toBeInTheDocument();
    });

    it('logs link sends pipeline.open-logs-folder when diagameName set', () => {
        postSpy = vi.spyOn(window.parent, 'postMessage');
        render(<PipelineToolbar {...defaultProps({ diagameName: 'cluichetest' })} />);
        fireEvent.click(screen.getByText('↗ logs'));
        const call = postSpy.mock.calls.find(c => {
            const msg = c[0] as { __diaFromFrame?: boolean; payload?: { type?: string } };
            return msg?.__diaFromFrame && msg?.payload?.type === 'pipeline.open-logs-folder';
        });
        expect(call).toBeDefined();
    });

    it('logs link does nothing when diagameName is empty', () => {
        postSpy = vi.spyOn(window.parent, 'postMessage');
        render(<PipelineToolbar {...defaultProps({ diagameName: '' })} />);
        fireEvent.click(screen.getByText('↗ logs'));
        const call = postSpy.mock.calls.find(c => {
            const msg = c[0] as { __diaFromFrame?: boolean; payload?: { type?: string } };
            return msg?.__diaFromFrame && msg?.payload?.type === 'pipeline.open-logs-folder';
        });
        expect(call).toBeUndefined();
    });
});
