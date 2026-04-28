import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { PipelineToolbar } from './PipelineToolbar';

function simulateTargetsResponse(targets: string[]) {
    // The toolbar sends a pipeline.get-targets request on mount via postMessage.
    // Simulate the response arriving via the DiaEditor_onResponse pattern.
    // Since useBridgeRequest listens for __dia messages with reqId, we need to
    // intercept the outgoing reqId and respond with it.
    // Simpler approach: directly send the targets via the build-status topic pattern.
    // Actually, let's just test the UI rendering with targets pre-populated by
    // dispatching a message response.

    // The useBridgeRequest hook stores pending callbacks by reqId. We need to capture
    // the reqId from the outgoing postMessage and respond with it.
    return targets;
}

describe('PipelineToolbar', () => {
    let postSpy: ReturnType<typeof vi.spyOn>;

    afterEach(() => {
        vi.restoreAllMocks();
    });

    // AC6: toolbar has target dropdown, config dropdown, force checkbox, Run button
    it('renders target dropdown, config dropdown, force checkbox, and Run button', () => {
        render(<PipelineToolbar buildRunning={false} />);
        expect(screen.getByText('Target:')).toBeInTheDocument();
        expect(screen.getByText('Config:')).toBeInTheDocument();
        expect(screen.getByText('Force')).toBeInTheDocument();
        expect(screen.getByText('Run')).toBeInTheDocument();
    });

    // AC8: shows Cancel when build running
    it('shows Cancel button when build is running', () => {
        render(<PipelineToolbar buildRunning={true} />);
        expect(screen.getByText('Cancel')).toBeInTheDocument();
        expect(screen.queryByText('Run')).not.toBeInTheDocument();
    });

    // AC8: dropdowns disabled when running
    it('disables dropdowns when build is running', () => {
        render(<PipelineToolbar buildRunning={true} />);
        const selects = screen.getAllByRole('combobox');
        selects.forEach(s => expect(s).toBeDisabled());
    });

    it('config dropdown has Debug and Release options', () => {
        render(<PipelineToolbar buildRunning={false} />);
        expect(screen.getByText('Debug')).toBeInTheDocument();
        expect(screen.getByText('Release')).toBeInTheDocument();
    });

    it('force checkbox is unchecked by default', () => {
        render(<PipelineToolbar buildRunning={false} />);
        const checkbox = screen.getByRole('checkbox');
        expect(checkbox).not.toBeChecked();
    });

    it('force checkbox can be toggled', () => {
        render(<PipelineToolbar buildRunning={false} />);
        const checkbox = screen.getByRole('checkbox');
        fireEvent.click(checkbox);
        expect(checkbox).toBeChecked();
    });

    // AC6: Run button is disabled when no targets are loaded
    it('Run button is disabled when no targets loaded', () => {
        render(<PipelineToolbar buildRunning={false} />);
        const button = screen.getByText('Run');
        expect(button).toBeDisabled();
    });

    // AC6: sends pipeline.get-targets request on mount
    it('sends pipeline.get-targets request on mount', () => {
        postSpy = vi.spyOn(window.parent, 'postMessage');
        render(<PipelineToolbar buildRunning={false} />);
        const call = postSpy.mock.calls.find(c => {
            const msg = c[0] as { __diaFromFrame?: boolean; payload?: { type?: string } };
            return msg?.__diaFromFrame && msg?.payload?.type === 'pipeline.get-targets';
        });
        expect(call).toBeDefined();
    });

    it('Cancel button sends pipeline.cancel postMessage', () => {
        postSpy = vi.spyOn(window.parent, 'postMessage');
        render(<PipelineToolbar buildRunning={true} />);
        fireEvent.click(screen.getByText('Cancel'));
        const call = postSpy.mock.calls.find(c => {
            const msg = c[0] as { __diaFromFrame?: boolean; payload?: { type?: string } };
            return msg?.__diaFromFrame && msg?.payload?.type === 'pipeline.cancel';
        });
        expect(call).toBeDefined();
    });

    // Unused helper reference
    void simulateTargetsResponse;
});
