import { render, screen, fireEvent, act } from '@testing-library/react';
import { encodeU16 } from '../model/__fixtures__/makeFixture';

vi.mock('../bridge', () => ({ queryFrame: vi.fn(), queryWindow: vi.fn() }));

import { queryFrame } from '../bridge';
import { LanesView } from './LanesView';
import { useWindowStore } from '../useWindowStore';
import { useSelectionStore } from '../useSelectionStore';
import { buildLanes } from '../model/laneModel';

const mockFrame = vi.mocked(queryFrame);

beforeEach(() => {
    mockFrame.mockReset();
    mockFrame.mockResolvedValue({ success: true, result: { error: 'x' } } as never);
    useSelectionStore.getState().clear();
    const reply = {
        v: 1, seconds: 60, markers: [], trips: [], truncated: false,
        units: [{
            unit: 'Main', unitCrc: 1, hz: 10, firstEntry: 100, count: 100, modules: [{ crc: 5, name: 'A' }],
            unitMs: encodeU16(new Array(100).fill(500)), moduleMs: encodeU16(new Array(100).fill(200)),
        }],
    };
    useWindowStore.setState({ reply, lanes: buildLanes(reply), view: { windowSeconds: 10, pausedAt: null } });
});

describe('lane click selection', () => {
    it('clicking a lane queries the nearest frame (unit + absolute index)', async () => {
        render(<LanesView />);
        const canvas = screen.getByTestId('lane-canvas');
        canvas.getBoundingClientRect = () => ({ left: 0, width: 100, top: 0, height: 74, right: 100, bottom: 74, x: 0, y: 0, toJSON: () => ({}) });
        // entries 100..199 at hz 10 -> t 10.0..19.9, domain (9.9, 19.9]; x=50 -> t = 14.9 -> entry 149
        await act(async () => { fireEvent.click(canvas, { clientX: 50 }); });
        expect(mockFrame).toHaveBeenCalledWith('Main', 149);
        expect(useSelectionStore.getState().selected).toEqual({ unit: 'Main', index: 149 });
    });
});
