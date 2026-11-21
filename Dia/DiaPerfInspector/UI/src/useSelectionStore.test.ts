import { buildLanes } from './model/laneModel';
import { encodeU16 } from './model/__fixtures__/makeFixture';

vi.mock('./bridge', () => ({ queryFrame: vi.fn(), queryWindow: vi.fn() }));

import { queryFrame } from './bridge';
import { useSelectionStore } from './useSelectionStore';

const mockFrame = vi.mocked(queryFrame);

const okReply = (frame: number) => ({
    success: true,
    result: { v: 1, unit: 'Main', frame, unitMs: 5, modules: [{ crc: 1, name: 'A', ms: 2 }], sections: { available: false, reason: 'r' } },
});

function lanes(unit: string, firstEntry: number) {
    return buildLanes({
        v: 1, seconds: 60, markers: [], trips: [], truncated: false,
        units: [{
            unit, unitCrc: 1, hz: 10, firstEntry, count: 5, modules: [{ crc: 5, name: 'A' }],
            unitMs: encodeU16([500, 500, 500, 500, 500]), moduleMs: encodeU16([200, 200, 200, 200, 200]),
        }],
    });
}

beforeEach(() => {
    mockFrame.mockReset();
    mockFrame.mockResolvedValue(okReply(105) as never);
    useSelectionStore.getState().clear();
});

describe('useSelectionStore', () => {
    it('select queries the frame with the right unit and index and stores the detail', async () => {
        await useSelectionStore.getState().select('Main', 105);
        expect(mockFrame).toHaveBeenCalledWith('Main', 105);
        const s = useSelectionStore.getState();
        expect(s.selected).toEqual({ unit: 'Main', index: 105 });
        expect(s.detail.status).toBe('ok');
        expect(s.stale).toBeNull();
    });

    it('goes through a loading state', async () => {
        const p = useSelectionStore.getState().select('Main', 105);
        expect(useSelectionStore.getState().detail.status).toBe('loading');
        await p;
    });

    it('stores an error detail for an {error} reply without throwing', async () => {
        mockFrame.mockResolvedValue({ success: true, result: { error: 'frame outside retained range' } } as never);
        await useSelectionStore.getState().select('Main', 1);
        const d = useSelectionStore.getState().detail;
        expect(d.status).toBe('error');
        expect(d.status === 'error' && d.message).toBe('frame outside retained range');
    });

    it('survives a rejected query', async () => {
        mockFrame.mockRejectedValue(new Error('boom'));
        await useSelectionStore.getState().select('Main', 1);
        expect(useSelectionStore.getState().detail.status).toBe('error');
    });

    it('ignores a late reply for a superseded selection', async () => {
        let release: (v: unknown) => void = () => {};
        mockFrame.mockImplementationOnce(() => new Promise((r) => { release = r; }) as never);
        const first = useSelectionStore.getState().select('Main', 101);
        mockFrame.mockResolvedValueOnce(okReply(102) as never);
        await useSelectionStore.getState().select('Main', 102);
        release(okReply(101));
        await first;
        const s = useSelectionStore.getState();
        expect(s.selected?.index).toBe(102);
        expect(s.detail.status === 'ok' && s.detail.data.frame).toBe(102);
    });

    it('reconcile marks stale when the unit disappears and keeps the index', async () => {
        await useSelectionStore.getState().select('Main', 105);
        useSelectionStore.getState().reconcile(lanes('Sim', 100));
        const s = useSelectionStore.getState();
        expect(s.stale).toMatch(/Main/);
        expect(s.selected).toEqual({ unit: 'Main', index: 105 });
    });

    it('selection survives a refresh that still retains the entry', async () => {
        await useSelectionStore.getState().select('Main', 105);
        useSelectionStore.getState().reconcile(lanes('Main', 103));
        expect(useSelectionStore.getState().stale).toBeNull();
        expect(mockFrame).toHaveBeenCalledTimes(1);
    });

    it('reconcile marks stale when the entry is no longer retained, then recovers', async () => {
        await useSelectionStore.getState().select('Main', 105);
        useSelectionStore.getState().reconcile(lanes('Main', 200));
        expect(useSelectionStore.getState().stale).toMatch(/retained/);
        useSelectionStore.getState().reconcile(lanes('Main', 100));
        expect(useSelectionStore.getState().stale).toBeNull();
    });

    it('clear drops the selection', async () => {
        await useSelectionStore.getState().select('Main', 105);
        useSelectionStore.getState().clear();
        expect(useSelectionStore.getState().selected).toBeNull();
        expect(useSelectionStore.getState().detail.status).toBe('idle');
    });
});
