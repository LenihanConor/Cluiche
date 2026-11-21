import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { buildLanes } from '../model/laneModel';
import type { PerfWindowReply } from '../model/laneModel';
import { encodeU16 } from '../model/__fixtures__/makeFixture';

const actions = { setWindowSeconds: vi.fn(), togglePause: vi.fn(), refresh: vi.fn() };
let state: Record<string, unknown>;

vi.mock('../useWindowStore', async () => {
    const real = await vi.importActual<typeof import('../useWindowStore')>('../useWindowStore');
    return {
        ...real,
        useWindowStore: (sel: (s: unknown) => unknown) => sel(state),
    };
});

import { LanesView } from './LanesView';

function windowOf(units: string[]): PerfWindowReply {
    return {
        v: 1, seconds: 60, markers: [], trips: [], truncated: false,
        units: units.map((u, i) => ({
            unit: u, unitCrc: i + 1, hz: 30, firstEntry: 300, count: 3,
            modules: [{ crc: 5 + i, name: `Mod${u}` }],
            unitMs: encodeU16([500, 600, 700]), moduleMs: encodeU16([200, 200, 200]),
        })),
    };
}

function setState(units: string[], pausedAt: number | null = null) {
    const reply = windowOf(units);
    state = {
        reply, lanes: buildLanes(reply), tripRanges: [], loading: false, error: null,
        view: { windowSeconds: 10, pausedAt }, ...actions,
    };
}

beforeEach(() => { Object.values(actions).forEach((a) => a.mockReset()); });

describe('LanesView', () => {
    it('renders one lane row per unit with a canvas and the unit title', () => {
        setState(['Main', 'Sim', 'Render']);
        render(<LanesView />);
        const rows = screen.getAllByTestId('lane-row');
        expect(rows).toHaveLength(3);
        expect(rows[0]).toHaveTextContent('Main');
        expect(rows[0]).toHaveTextContent('30 Hz');
        expect(rows[0]).toHaveTextContent('33.3 ms');
        expect(rows[1].querySelector('canvas')).not.toBeNull();
    });

    it('shows a legend entry per distinct module', () => {
        setState(['Main', 'Sim']);
        render(<LanesView />);
        const legend = screen.getByTestId('module-legend');
        expect(legend).toHaveTextContent('ModMain');
        expect(legend).toHaveTextContent('ModSim');
    });

    it('window buttons call setWindowSeconds and reflect the current choice', async () => {
        setState(['Main']);
        render(<LanesView />);
        expect(screen.getByRole('button', { name: '10 s' })).toHaveAttribute('aria-pressed', 'true');
        await userEvent.click(screen.getByRole('button', { name: '30 s' }));
        expect(actions.setWindowSeconds).toHaveBeenCalledWith(30);
        await userEvent.click(screen.getByRole('button', { name: '60 s' }));
        expect(actions.setWindowSeconds).toHaveBeenCalledWith(60);
    });

    it('Pause button calls togglePause and the label flips to Resume when paused', async () => {
        setState(['Main']);
        const { unmount } = render(<LanesView />);
        await userEvent.click(screen.getByRole('button', { name: 'Pause' }));
        expect(actions.togglePause).toHaveBeenCalledTimes(1);
        unmount();
        setState(['Main'], 31);
        render(<LanesView />);
        expect(screen.getByRole('button', { name: 'Resume' })).toBeInTheDocument();
    });

    it('renders no lane rows when there is no data', () => {
        state = { reply: null, lanes: [], tripRanges: [], loading: false, error: null, view: { windowSeconds: 10, pausedAt: null }, ...actions };
        render(<LanesView />);
        expect(screen.queryAllByTestId('lane-row')).toHaveLength(0);
        expect(screen.getByRole('button', { name: 'Pause' })).toBeInTheDocument();
    });

    it('shows the error text when the store has an error', () => {
        setState(['Main']);
        state.error = 'query timeout';
        render(<LanesView />);
        expect(screen.getByTestId('lanes-error')).toHaveTextContent('query timeout');
    });
});
