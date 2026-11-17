import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { vi } from 'vitest';
import { ExportTraceButton } from './ExportTraceButton';

vi.mock('../exportTrace', () => ({
    exportTraceFile: vi.fn(),
}));

describe('ExportTraceButton', () => {
    it('renders the button', () => {
        render(<ExportTraceButton />);
        const button = screen.getByRole('button', { name: /export perfetto/i });
        expect(button).toBeInTheDocument();
    });

    it('calls exportTraceFile when clicked', async () => {
        const { exportTraceFile } = await import('../exportTrace');
        const user = userEvent.setup();
        render(<ExportTraceButton />);
        const button = screen.getByRole('button', { name: /export perfetto/i });
        await user.click(button);
        expect(exportTraceFile).toHaveBeenCalled();
    });

    it('generates a filename with ISO timestamp', async () => {
        const { exportTraceFile } = await import('../exportTrace');
        const user = userEvent.setup();
        render(<ExportTraceButton />);
        const button = screen.getByRole('button', { name: /export perfetto/i });
        await user.click(button);
        const call = vi.mocked(exportTraceFile).mock.calls[0];
        expect(call).toBeDefined();
        const filename = call[1];
        expect(filename).toMatch(/^perf-window-\d{4}-\d{2}-\d{2}T\d{2}-\d{2}-\d{2}.*\.json$/);
    });
});
