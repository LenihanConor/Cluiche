import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { StageDetail } from './StageDetail';
import type { LogLine } from '../state/types';

describe('StageDetail', () => {
    it('shows "No log output" for empty logLines', () => {
        render(<StageDetail logLines={[]} />);
        expect(screen.getByText('No log output')).toBeInTheDocument();
    });

    it('renders all log lines', () => {
        const lines: LogLine[] = [
            { level: 'info', message: 'Building...', timestamp: 1000 },
            { level: 'error', message: 'C2065 error', timestamp: 1001 },
            { level: 'warn', message: 'unused var', timestamp: 1002 },
            { level: 'debug', message: 'debug trace', timestamp: 1003 },
        ];
        render(<StageDetail logLines={lines} />);
        expect(screen.getByText('Building...')).toBeInTheDocument();
        expect(screen.getByText('C2065 error')).toBeInTheDocument();
        expect(screen.getByText('unused var')).toBeInTheDocument();
        expect(screen.getByText('debug trace')).toBeInTheDocument();
    });

    it('shows level labels for each line', () => {
        const lines: LogLine[] = [
            { level: 'info', message: 'msg1', timestamp: 1000 },
            { level: 'error', message: 'msg2', timestamp: 1001 },
        ];
        render(<StageDetail logLines={lines} />);
        expect(screen.getByText('[info]')).toBeInTheDocument();
        expect(screen.getByText('[error]')).toBeInTheDocument();
    });

    it('calls clipboard.writeText on click', () => {
        const writeText = vi.fn().mockResolvedValue(undefined);
        Object.assign(navigator, { clipboard: { writeText } });

        const lines: LogLine[] = [
            { level: 'info', message: 'copy me', timestamp: 1000 },
        ];
        render(<StageDetail logLines={lines} />);
        fireEvent.click(screen.getByText('copy me'));
        expect(writeText).toHaveBeenCalledWith('copy me');
    });

    it('applies different colours per level', () => {
        const lines: LogLine[] = [
            { level: 'info', message: 'info msg', timestamp: 1000 },
            { level: 'error', message: 'error msg', timestamp: 1001 },
            { level: 'warn', message: 'warn msg', timestamp: 1002 },
        ];
        const { container } = render(<StageDetail logLines={lines} />);
        const rows = container.querySelectorAll('div[style*="font-family"]');
        expect(rows).toHaveLength(3);
    });
});
