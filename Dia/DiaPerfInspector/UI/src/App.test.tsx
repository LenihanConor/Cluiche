import { describe, it, expect, beforeEach, vi } from 'vitest';
import { render, screen, act } from '@testing-library/react';
import App from './App';
import { subscribePerfTiming, usePerfTimingStore } from './usePerfTimingStore';

function postDia(topic: string, data: unknown) {
    window.dispatchEvent(new MessageEvent('message', { data: { __dia: true, topic, data } }));
}

beforeEach(() => {
    usePerfTimingStore.getState().clear();
    vi.spyOn(window.parent, 'postMessage').mockImplementation(() => {});
});

describe('perf.timing store', () => {
    it('applies a perf.timing push', () => {
        const off = subscribePerfTiming();
        postDia('perf_inspector.timing', {
            scale: 1, windowSec: 10, unavailable: [],
            units: [{ unit: 'Main', hz: 60, samples: 600, tick: [1, 2], modules: ['A'], ewma: [0.5], p95: [0.9] }],
        });
        const t = usePerfTimingStore.getState().timing;
        expect(t?.units[0].unit).toBe('Main');
        expect(t?.units[0].tick[1]).toBe(2);
        off();
    });
});

describe('App shell', () => {
    it('renders the disconnected banner and empty lanes area', () => {
        render(<App />);
        expect(screen.getByTestId('connection-banner')).toHaveAttribute('data-state', 'disconnected');
        expect(screen.getByTestId('lanes')).toBeEmptyDOMElement();
    });

    it('renders the connected banner after a connection_state push', async () => {
        render(<App />);
        await act(async () => { postDia('perf_inspector.connection_state', { connected: true }); });
        expect(screen.getByTestId('connection-banner')).toHaveAttribute('data-state', 'connected');
        expect(usePerfTimingStore.getState().connectionState).toBe('connected');
    });
});
