import { render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';

vi.mock('../bridge', () => ({ sendPerfCommand: vi.fn(), queryWindow: vi.fn(), queryFrame: vi.fn() }));

import { sendPerfCommand } from '../bridge';
import { ModuleTable } from './ModuleTable';
import { usePerfTimingStore } from '../usePerfTimingStore';
import { usePauseStore } from '../usePauseStore';
import { useWindowStore } from '../useWindowStore';

const mockSend = vi.mocked(sendPerfCommand);

beforeEach(() => {
    mockSend.mockReset();
    usePauseStore.getState().reset();
    useWindowStore.getState().reset();
    usePerfTimingStore.setState({
        connectionState: 'connected',
        timing: {
            scale: 0.01, windowSec: 10, unavailable: [],
            units: [
                { unit: 'Main', hz: 30, samples: 300, tick: [2000, 3000], modules: ['A', 'B'], ewma: [500, 1000], p95: [900, 1500] },
                { unit: 'Sim', hz: 60, samples: 600, tick: [800, 2000], modules: ['X'], ewma: [400], p95: [1800] },
            ],
        },
    });
});

describe('ModuleTable', () => {
    it('shows honest column labels and a row per module, sorted by p95 desc', () => {
        render(<ModuleTable />);
        const header = screen.getAllByTestId('module-table-header')[0];
        expect(header).toHaveTextContent('avg (EWMA)');
        expect(header).toHaveTextContent('p95');
        expect(header).not.toHaveTextContent('p50');
        const rows = screen.getAllByTestId('module-row');
        expect(rows.map((r) => r.getAttribute('data-module'))).toEqual(['B', 'A', 'X']);
        expect(rows[0]).toHaveTextContent('15.00');
    });

    it('shows an empty message with no timing', () => {
        usePerfTimingStore.setState({ timing: null });
        render(<ModuleTable />);
        expect(screen.getByTestId('module-table')).toHaveTextContent(/no timing data/i);
    });

    it('clicking a switch sends perf.pause with the module name and shows paused', async () => {
        mockSend.mockResolvedValue({ success: true, result: { kind: 'module', id: 'A', paused: true } } as never);
        render(<ModuleTable />);
        await userEvent.click(screen.getByTestId('pause-Main-A'));
        expect(mockSend).toHaveBeenCalledWith('perf.pause', { kind: 'module', id: 'A' });
        await waitFor(() => expect(screen.getByTestId('pause-Main-A')).toHaveAttribute('aria-pressed', 'true'));
    });

    it('clicking a paused switch sends perf.resume', async () => {
        usePauseStore.setState({ paused: { 'Main/A': true } });
        mockSend.mockResolvedValue({ success: true, result: { paused: false } } as never);
        render(<ModuleTable />);
        await userEvent.click(screen.getByTestId('pause-Main-A'));
        expect(mockSend).toHaveBeenCalledWith('perf.resume', { kind: 'module', id: 'A' });
        await waitFor(() => expect(screen.getByTestId('pause-Main-A')).toHaveAttribute('aria-pressed', 'false'));
    });

    it('disables the switches with the reason after an unknown-command reply', async () => {
        mockSend.mockResolvedValue({ success: false, result: { success: false, message: 'Unknown command' } } as never);
        render(<ModuleTable />);
        await userEvent.click(screen.getByTestId('pause-Main-A'));
        await waitFor(() => expect(screen.getByTestId('pause-Main-A')).toBeDisabled());
        expect(screen.getByTestId('pause-unsupported')).toHaveTextContent(/Debug/);
        expect(screen.getByTestId('resume-all')).toBeDisabled();
    });

    it('Resume all sends perf.resume_all', async () => {
        usePauseStore.setState({ paused: { 'Main/A': true } });
        mockSend.mockResolvedValue({ success: true, result: {} } as never);
        render(<ModuleTable />);
        await userEvent.click(screen.getByTestId('resume-all'));
        expect(mockSend).toHaveBeenCalledWith('perf.resume_all', {});
        await waitFor(() => expect(screen.getByTestId('pause-Main-A')).toHaveAttribute('aria-pressed', 'false'));
    });

    it('shows a game error inline on its row', async () => {
        mockSend.mockResolvedValue({ success: true, result: { error: 'refused' } } as never);
        render(<ModuleTable />);
        await userEvent.click(screen.getByTestId('pause-Main-A'));
        await waitFor(() => expect(screen.getByTestId('pause-error-Main-A')).toHaveTextContent('refused'));
    });

    it('sort by name button reorders rows', async () => {
        render(<ModuleTable />);
        await userEvent.click(screen.getByRole('button', { name: /sort by name/i }));
        expect(screen.getAllByTestId('module-row').map((r) => r.getAttribute('data-module'))).toEqual(['A', 'B', 'X']);
    });
});
