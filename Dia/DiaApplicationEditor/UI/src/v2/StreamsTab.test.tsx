import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn(),
    bridgeEvent: vi.fn(),
}));

vi.mock('./useManifestStoreV2', () => ({
    useManifestStoreV2: vi.fn(),
}));

vi.mock('./useLiveStoreV2', () => ({
    useLiveStoreV2: vi.fn(),
}));

import { StreamsTab } from './StreamsTab';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import type { ManifestV2, StreamV2 } from './types';

const mockUseManifest = useManifestStoreV2 as unknown as ReturnType<typeof vi.fn>;
const mockUseLive = useLiveStoreV2 as unknown as ReturnType<typeof vi.fn>;

function makeStream(overrides: Partial<StreamV2> = {}): StreamV2 {
    return {
        id: 'stream1',
        kind: 'spsc',
        payloadType: 'MyPayload',
        fromPU: 'pu1',
        toPU: 'pu2',
        capacity: 64,
        maxReaders: 1,
        ...overrides,
    };
}

function makeManifest(streams: StreamV2[]): ManifestV2 {
    return {
        version: 2,
        stages: [],
        initialStage: '',
        autoStages: [],
        streams,
        processingUnits: [],
    };
}

function setupMocks(manifest: ManifestV2 | null, liveOverrides: Record<string, unknown> = {}) {
    const manifestStore = { manifest };
    mockUseManifest.mockImplementation((selector: (s: typeof manifestStore) => unknown) => selector(manifestStore));

    const liveStore = { connectionState: 'disconnected', streams: [], ...liveOverrides };
    mockUseLive.mockImplementation((selector: (s: typeof liveStore) => unknown) => selector(liveStore));
}

beforeEach(() => {
    vi.clearAllMocks();
});

describe('StreamsTab', () => {
    it('renders streams-tab with no manifest (empty state)', () => {
        setupMocks(null);
        render(<StreamsTab />);
        expect(screen.getByTestId('streams-tab')).toBeTruthy();
        expect(screen.queryAllByTestId('stream-row')).toHaveLength(0);
    });

    it('renders correct number of stream-row elements', () => {
        const streams = [makeStream({ id: 'a' }), makeStream({ id: 'b' }), makeStream({ id: 'c' })];
        setupMocks(makeManifest(streams));
        render(<StreamsTab />);
        expect(screen.getAllByTestId('stream-row')).toHaveLength(3);
    });

    it('clicking a row selects it and detail shows', () => {
        const streams = [makeStream({ id: 'stream1' })];
        setupMocks(makeManifest(streams));
        render(<StreamsTab />);

        // Before clicking: placeholder text
        expect(screen.getByTestId('stream-detail').textContent).toContain('Select a stream');

        fireEvent.click(screen.getByTestId('stream-row'));

        // After clicking: stream ID label should appear
        expect(screen.getByTestId('stream-id-label').textContent).toBe('stream1');
    });

    it('$-prefix stream row has read-only styling (opacity)', () => {
        const streams = [makeStream({ id: '$sys.stream' })];
        setupMocks(makeManifest(streams));
        render(<StreamsTab />);
        const row = screen.getByTestId('stream-row');
        const style = (row as HTMLElement).style;
        expect(style.opacity).toBe('0.6');
    });

    it('detail sidebar shows field inputs when stream is selected', () => {
        const streams = [makeStream({ id: 'stream1' })];
        setupMocks(makeManifest(streams));
        render(<StreamsTab />);
        fireEvent.click(screen.getByTestId('stream-row'));

        // Should have multiple inputs in the detail panel
        const detail = screen.getByTestId('stream-detail');
        const inputs = detail.querySelectorAll('input');
        expect(inputs.length).toBeGreaterThan(0);
    });

    it('$-prefix stream: sidebar inputs are disabled', () => {
        const streams = [makeStream({ id: '$sys.stream' })];
        setupMocks(makeManifest(streams));
        render(<StreamsTab />);
        fireEvent.click(screen.getByTestId('stream-row'));

        const detail = screen.getByTestId('stream-detail');
        const inputs = Array.from(detail.querySelectorAll('input')) as HTMLInputElement[];
        expect(inputs.length).toBeGreaterThan(0);
        inputs.forEach((input) => {
            expect(input.disabled).toBe(true);
        });
    });

    it('shows live throughput in detail when stream is selected and live', () => {
        const streams = [makeStream({ id: 'stream1' })];
        setupMocks(makeManifest(streams), {
            connectionState: 'connected',
            streams: [{ streamId: 'stream1', msgPerSec: 42 }],
        });
        render(<StreamsTab />);
        fireEvent.click(screen.getByTestId('stream-row'));

        const throughput = screen.getByTestId('live-throughput');
        expect(throughput.textContent).toContain('42 msg/s');
    });

    describe('kind-aware fields', () => {
        it('EventStream renders overflow select but no multi-writer checkbox', async () => {
            const { bridgeRequest } = await import('./bridge');
            (bridgeRequest as ReturnType<typeof vi.fn>).mockClear();

            const streams = [makeStream({ id: 'evt', kind: 'EventStream', overflow: 'drop-oldest' })];
            setupMocks(makeManifest(streams));
            render(<StreamsTab />);
            fireEvent.click(screen.getByTestId('stream-row'));

            expect(screen.getByTestId('stream-overflow-row')).toBeTruthy();
            expect(screen.queryByTestId('stream-multiwriter-row')).toBeNull();
            expect(screen.queryByTestId('stream-block-timeout-row')).toBeNull();
        });

        it('FrameStream renders multi-writer checkbox but no overflow select', () => {
            const streams = [makeStream({ id: 'frm', kind: 'FrameStream', multiWriter: true })];
            setupMocks(makeManifest(streams));
            render(<StreamsTab />);
            fireEvent.click(screen.getByTestId('stream-row'));

            expect(screen.getByTestId('stream-multiwriter-row')).toBeTruthy();
            expect(screen.queryByTestId('stream-overflow-row')).toBeNull();
            const cb = screen.getByTestId('stream-multiwriter-checkbox') as HTMLInputElement;
            expect(cb.checked).toBe(true);
        });

        it('EventStream with overflow=block renders block-timeout input', () => {
            const streams = [makeStream({ id: 'evt', kind: 'EventStream', overflow: 'block', blockTimeoutMs: 250 })];
            setupMocks(makeManifest(streams));
            render(<StreamsTab />);
            fireEvent.click(screen.getByTestId('stream-row'));

            expect(screen.getByTestId('stream-block-timeout-row')).toBeTruthy();
        });

        it('changing overflow select calls bridgeRequest with SetStreamOverflow', async () => {
            const { bridgeRequest } = await import('./bridge');
            (bridgeRequest as ReturnType<typeof vi.fn>).mockClear();

            const streams = [makeStream({ id: 'evt', kind: 'EventStream', overflow: 'drop-oldest' })];
            setupMocks(makeManifest(streams));
            render(<StreamsTab />);
            fireEvent.click(screen.getByTestId('stream-row'));

            const sel = screen.getByTestId('stream-overflow-select') as HTMLSelectElement;
            fireEvent.change(sel, { target: { value: 'fail-loud' } });

            expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
                commandType: 'SetStreamOverflow',
                streamId: 'evt',
                value: 'fail-loud',
            });
        });

        it('toggling multi-writer checkbox calls bridgeRequest with SetStreamMultiWriter', async () => {
            const { bridgeRequest } = await import('./bridge');
            (bridgeRequest as ReturnType<typeof vi.fn>).mockClear();

            const streams = [makeStream({ id: 'frm', kind: 'FrameStream', multiWriter: false })];
            setupMocks(makeManifest(streams));
            render(<StreamsTab />);
            fireEvent.click(screen.getByTestId('stream-row'));

            const cb = screen.getByTestId('stream-multiwriter-checkbox') as HTMLInputElement;
            fireEvent.click(cb);

            expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
                commandType: 'SetStreamMultiWriter',
                streamId: 'frm',
                value: true,
            });
        });

        it('EventStream with capacity=0 shows engine-default hint', () => {
            const streams = [makeStream({ id: 'evt', kind: 'EventStream', capacity: 0, maxReaders: 0 })];
            setupMocks(makeManifest(streams));
            render(<StreamsTab />);
            fireEvent.click(screen.getByTestId('stream-row'));

            const capHint = screen.getByTestId('stream-capacity-hint');
            const readersHint = screen.getByTestId('stream-max-readers-hint');
            expect(capHint.textContent).toContain('engine default');
            expect(capHint.textContent).toContain('256');
            expect(readersHint.textContent).toContain('engine default');
            expect(readersHint.textContent).toContain('8');
        });

        it('EventStream with non-zero capacity shows no default hint', () => {
            const streams = [makeStream({ id: 'evt', kind: 'EventStream', capacity: 64, maxReaders: 4 })];
            setupMocks(makeManifest(streams));
            render(<StreamsTab />);
            fireEvent.click(screen.getByTestId('stream-row'));

            expect(screen.getByTestId('stream-capacity-hint').textContent).toBe('');
            expect(screen.getByTestId('stream-max-readers-hint').textContent).toBe('');
        });

        it('FrameStream shows fixed-buffer hint regardless of capacity', () => {
            const streams = [makeStream({ id: 'frm', kind: 'FrameStream', capacity: 0, maxReaders: 0 })];
            setupMocks(makeManifest(streams));
            render(<StreamsTab />);
            fireEvent.click(screen.getByTestId('stream-row'));

            const capHint = screen.getByTestId('stream-capacity-hint');
            const readersHint = screen.getByTestId('stream-max-readers-hint');
            expect(capHint.textContent).toContain('fixed 2-slot');
            expect(readersHint.textContent).toContain('unbounded');
        });

        it('kind select change calls bridgeRequest with SetStreamKind', async () => {
            const { bridgeRequest } = await import('./bridge');
            (bridgeRequest as ReturnType<typeof vi.fn>).mockClear();

            const streams = [makeStream({ id: 'frm', kind: 'FrameStream' })];
            setupMocks(makeManifest(streams));
            render(<StreamsTab />);
            fireEvent.click(screen.getByTestId('stream-row'));

            const sel = screen.getByTestId('stream-kind-select') as HTMLSelectElement;
            fireEvent.change(sel, { target: { value: 'EventStream' } });

            expect(bridgeRequest).toHaveBeenCalledWith('manifest.applyCommand', {
                commandType: 'SetStreamKind',
                streamId: 'frm',
                value: 'EventStream',
            });
        });
    });
});
