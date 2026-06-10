// ChatPanel.test.tsx — behaviour tests for ChatPanel
// Uses @testing-library/react + vitest

import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { ChatPanel } from './ChatPanel';

// ---------------------------------------------------------------------------
// Mock window.parent.postMessage so sendEvent calls can be captured
// ---------------------------------------------------------------------------

const postMessageMock = vi.fn();
Object.defineProperty(window, 'parent', {
    value: { postMessage: postMessageMock },
    writable: true,
});

// jsdom does not implement scrollIntoView — stub it out globally
window.HTMLElement.prototype.scrollIntoView = vi.fn();

// ---------------------------------------------------------------------------
// Helper: extract the payload from the most recent postMessage call
// ---------------------------------------------------------------------------

function lastPayload() {
    const calls = postMessageMock.mock.calls;
    if (calls.length === 0) throw new Error('No postMessage calls recorded');
    return calls[calls.length - 1][0].payload;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

describe('ChatPanel', () => {
    beforeEach(() => {
        postMessageMock.mockClear();
    });

    // (a) Empty state renders when no messages
    it('shows empty state text when no messages', () => {
        render(<ChatPanel />);
        expect(screen.getByText('Hello! Ask me anything about Dia.')).toBeTruthy();
    });

    // (b) Send message adds user bubble
    it('adds a user bubble after sending a message', () => {
        render(<ChatPanel />);
        const textarea = screen.getByPlaceholderText(/Ask anything/i);
        fireEvent.change(textarea, { target: { value: 'Hello world' } });
        fireEvent.keyDown(textarea, { key: 'Enter', shiftKey: false });
        expect(screen.getByText('Hello world')).toBeTruthy();
    });

    // (c) Context mode toggle
    it('sends chat.set_context_mode when Tools only is clicked', () => {
        render(<ChatPanel />);
        const button = screen.getByText('Tools only');
        fireEvent.click(button);
        const payload = lastPayload();
        expect(payload.type).toBe('chat.set_context_mode');
        expect(payload.data).toEqual({ mode: 'tools_only' });
    });

    // (d) @file injection
    it('sends extra_files when message contains @filename.md', () => {
        render(<ChatPanel />);
        const textarea = screen.getByPlaceholderText(/Ask anything/i);
        fireEvent.change(textarea, { target: { value: '@engine_overview.md explain this' } });
        fireEvent.keyDown(textarea, { key: 'Enter', shiftKey: false });

        // Find the chat.send_message call (may not be the last one if context_mode fired too)
        const sendCall = postMessageMock.mock.calls.find(
            c => c[0]?.payload?.type === 'chat.send_message'
        );
        expect(sendCall).toBeTruthy();
        const data = sendCall![0].payload.data;
        expect(data.extra_files).toEqual(['engine_overview.md']);
    });

    // (e) Clear history
    it('clears messages after clicking New chat', () => {
        render(<ChatPanel />);

        // First add a message
        const textarea = screen.getByPlaceholderText(/Ask anything/i);
        fireEvent.change(textarea, { target: { value: 'Hi there' } });
        fireEvent.keyDown(textarea, { key: 'Enter', shiftKey: false });
        expect(screen.queryByText('Hello! Ask me anything about Dia.')).toBeNull();

        // Now clear
        const clearBtn = screen.getByRole('button', { name: /new chat/i });
        fireEvent.click(clearBtn);

        expect(screen.getByText('Hello! Ask me anything about Dia.')).toBeTruthy();
    });
});
