import { render, screen, fireEvent } from '@testing-library/react';
import { act } from 'react';
import { vi } from 'vitest';
import { EventLog } from './EventLog';
import { useInspectorStore } from './useInspectorStore';

beforeEach(() => {
    useInspectorStore.getState().clearAll();
    vi.clearAllMocks();
});

describe('EventLog', () => {
    it('renders no entries when empty', () => {
        render(<EventLog />);
        expect(screen.queryAllByTestId('log-entry')).toHaveLength(0);
    });

    it('renders entries pushed to store', () => {
        act(() => {
            useInspectorStore.getState().pushEventLogEntry('info', 'hello world', 1000);
            useInspectorStore.getState().pushEventLogEntry('warn', 'watch out', 2000);
        });
        render(<EventLog />);
        const entries = screen.queryAllByTestId('log-entry');
        expect(entries).toHaveLength(2);
        expect(entries[0].textContent).toContain('hello world');
        expect(entries[1].textContent).toContain('watch out');
    });

    it('severity filter hides non-matching entries', () => {
        act(() => {
            useInspectorStore.getState().pushEventLogEntry('info', 'info msg', 0);
            useInspectorStore.getState().pushEventLogEntry('error', 'error msg', 1);
        });
        render(<EventLog />);
        const select = screen.getByTestId('severity-filter') as HTMLSelectElement;
        fireEvent.change(select, { target: { value: 'error' } });
        const entries = screen.queryAllByTestId('log-entry');
        expect(entries).toHaveLength(1);
        expect(entries[0].getAttribute('data-severity')).toBe('error');
    });

    it('Copy button calls clipboard.writeText', () => {
        const writeText = vi.fn().mockResolvedValue(undefined);
        Object.defineProperty(navigator, 'clipboard', {
            value: { writeText },
            writable: true,
            configurable: true,
        });
        act(() => useInspectorStore.getState().pushEventLogEntry('info', 'copy me', 500));
        render(<EventLog />);
        fireEvent.click(screen.getByTestId('copy-btn'));
        expect(writeText).toHaveBeenCalledWith(expect.stringContaining('copy me'));
    });

    it('entry has correct data-severity attribute', () => {
        act(() => useInspectorStore.getState().pushEventLogEntry('transition', 'stage changed', 0));
        render(<EventLog />);
        const entry = screen.getByTestId('log-entry');
        expect(entry.getAttribute('data-severity')).toBe('transition');
    });
});
