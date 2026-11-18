// jobs.fixture.json is SYNTHETIC; its step summaries exist only in the fixture (a live game sends steps: [] when no deferred step ran in the window).
import fixture from '../../model/__fixtures__/jobs.fixture.json';
import { render, screen, within, act, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { vi } from 'vitest';
import type { PerfWindowReply } from '../../model/laneModel';
import type { PerfTimingPush } from '../../types';

const queryJobs = vi.fn();
vi.mock('../../bridge', async () => {
    const real = await vi.importActual<typeof import('../../bridge')>('../../bridge');
    return { ...real, queryJobs: (...a: unknown[]) => queryJobs(...a) };
});

import { WorkersView } from './WorkersView';
import { useJobsStore } from '../../useJobsStore';
import { useWindowStore } from '../../useWindowStore';

const windowReply = (jobs: unknown) => ({ v: 1, seconds: 60, units: [], markers: [], trips: [], truncated: false, jobs }) as unknown as PerfWindowReply;
const timing = (jobs: unknown, unavailable: string[]) => ({ scale: 0.01, windowSec: 10, units: [], jobs, unavailable }) as unknown as PerfTimingPush;

function load(window: unknown = fixture.window, t1: unknown = fixture.timing.jobs, unavailable: string[] = fixture.timing.unavailable) {
    act(() => {
        useJobsStore.getState().applyWindowReply(windowReply(window));
        useJobsStore.getState().applyTiming(timing(t1, unavailable));
    });
}

beforeEach(() => {
    queryJobs.mockReset();
    queryJobs.mockResolvedValue({ success: true, result: fixture.raw });
    useJobsStore.getState().reset();
    useWindowStore.getState().reset();
});

describe('WorkersView with the synthetic fixture', () => {
    it('renders heat strip rows, cap row, step rows, late list, owner share and pool health (AC10)', () => {
        load();
        render(<WorkersView />);
        const heat = screen.getAllByTestId('heat-row');
        expect(heat.map((h) => h.getAttribute('data-unit'))).toEqual(['Sim', 'Main']);
        expect(within(heat[0]).getByTestId('heat-canvas')).toBeInTheDocument();
        expect(within(heat[1]).getByTestId('heat-off')).toHaveTextContent('per-job recording off: aggregates only');
        expect(screen.getByTestId('cap-row')).toBeInTheDocument();
        expect(screen.getAllByTestId('step-row').map((r) => r.getAttribute('data-channel'))).toEqual(['weather', 'ecology']);
        expect(screen.getAllByTestId('late-item')).toHaveLength(1);
        expect(screen.getByTestId('late-item')).toHaveTextContent('ecology key 4 to 6');
        expect(screen.queryByTestId('steps-empty')).toBeNull();
    });

    it('owner share for the window sums to 100 percent', () => {
        load();
        render(<WorkersView />);
        const pcts = screen.getAllByTestId('window-share-pct').map((e) => parseInt(e.textContent ?? '0', 10));
        expect(pcts.length).toBeGreaterThan(1);
        expect(pcts.reduce((a, b) => a + b, 0)).toBe(100);
    });

    it('pool health shows real fields and marks the unavailable ones as unavailable, not 0', () => {
        load();
        render(<WorkersView />);
        expect(screen.getByTestId('pool-time-at-cap')).toHaveTextContent('unavailable');
        expect(screen.getByTestId('pool-peak-depth')).toHaveTextContent('unavailable');
        expect(screen.getByTestId('pool-inline')).toHaveTextContent('3');
        expect(screen.getByTestId('pool-drops')).toHaveTextContent('1');
        const owners = screen.getAllByTestId('pool-owner').map((e) => e.textContent);
        expect(owners[0]).toContain('ecology');
        expect(screen.getAllByTestId('pool-owner-p95')[0]).toHaveTextContent('1.50 s');
    });

    it('cap row renders empty with the unavailable message while cap_use is unavailable', () => {
        load();
        render(<WorkersView />);
        expect(screen.getByTestId('cap-row')).toHaveTextContent('cap use unavailable');
    });

    it('cap row shows the value once the game samples it', () => {
        const t1 = { units: [{ ...fixture.timing.jobs.units[0], pool: { ...fixture.timing.jobs.units[0].pool, capUse: 2 } }] };
        load(fixture.window, t1, []);
        render(<WorkersView />);
        expect(screen.getByTestId('cap-row')).toHaveTextContent('in flight: 2');
    });

    it('selecting a late result opens step detail with cap wait and worker wait matching the summary (AC11)', async () => {
        load();
        render(<WorkersView />);
        await userEvent.click(screen.getByTestId('late-item'));
        await waitFor(() => expect(screen.getByTestId('step-detail')).toBeInTheDocument());
        const eco = fixture.window.steps[1];
        expect(screen.getByTestId('step-wait-cap')).toHaveTextContent('6.24 s');
        expect(screen.getByTestId('step-wait-worker')).toHaveTextContent('8.0 ms');
        expect(eco.capWaitMs).toBe(6240);
        expect(screen.getByTestId('step-status')).toHaveTextContent('late 85 ms');
        expect(screen.getByTestId('step-critical-path')).toHaveTextContent('88 ms');
        expect(screen.getByTestId('step-parallelism')).toHaveTextContent('x');
        await waitFor(() => expect(screen.getAllByTestId('step-range-worker')).toHaveLength(2));
        expect(screen.getAllByTestId('step-share-row').length).toBeGreaterThan(0);
        expect(queryJobs).toHaveBeenCalledTimes(1);
    });

    it('step share for the selected step sums to 100 percent', async () => {
        load();
        render(<WorkersView />);
        await userEvent.click(screen.getAllByTestId('step-item')[1]);
        await waitFor(() => expect(screen.getAllByTestId('step-share-row').length).toBeGreaterThan(0));
        const sum = screen.getAllByTestId('step-share-row').map((r) => parseInt(r.lastElementChild?.textContent ?? '0', 10)).reduce((a, b) => a + b, 0);
        expect(sum).toBe(100);
    });

    it('step detail says critical path is unavailable without capture/apply cost', async () => {
        const steps = fixture.window.steps.map(({ captureMs: _c, applyMs: _a, ...rest }) => rest);
        load({ ...fixture.window, steps });
        render(<WorkersView />);
        await userEvent.click(screen.getAllByTestId('step-item')[0]);
        expect(screen.getByTestId('step-critical-path')).toHaveTextContent('unavailable');
        expect(screen.getByTestId('step-sections')).toHaveTextContent('unavailable');
    });

    it('zooming to 2 s loads worker rows and shows the truncated notice when the game truncated', async () => {
        queryJobs.mockResolvedValue({ success: true, result: { ...fixture.raw, truncated: true } });
        load();
        render(<WorkersView />);
        expect(screen.getByTestId('slices-hint')).toBeInTheDocument();
        expect(queryJobs).not.toHaveBeenCalled();
        await userEvent.click(screen.getByRole('button', { name: '2 s' }));
        await waitFor(() => expect(screen.getAllByTestId('worker-row').length).toBe(7));
        expect(screen.getByTestId('workers-truncated')).toHaveTextContent('truncated');
    });

    it('does not request raw records while only viewing the aggregate window', () => {
        load();
        render(<WorkersView />);
        expect(queryJobs).not.toHaveBeenCalled();
    });

    it('reports an error reply from perf.jobs', async () => {
        queryJobs.mockResolvedValue({ success: true, result: { error: 'unknown unit' } });
        load();
        render(<WorkersView />);
        await userEvent.click(screen.getAllByTestId('step-item')[0]);
        await waitFor(() => expect(screen.getByTestId('slices-error')).toHaveTextContent('unknown unit'));
    });

    it('shows per-job recording off when the raw reply says so', async () => {
        queryJobs.mockResolvedValue({ success: true, result: { v: 1, unit: 'Sim', perJobRecording: false, truncated: false, count: 0 } });
        load();
        render(<WorkersView />);
        await userEvent.click(screen.getAllByTestId('step-item')[0]);
        await waitFor(() => expect(screen.getByTestId('slices-off')).toHaveTextContent('per-job recording off: aggregates only'));
    });
});

describe('WorkersView honest states', () => {
    it('with the live shape (steps: []) says there are no deferred steps in this window', () => {
        load({ ...fixture.window, steps: [] });
        render(<WorkersView />);
        expect(screen.getByTestId('steps-empty')).toHaveTextContent('No deferred steps in this window');
        expect(screen.queryAllByTestId('step-row')).toHaveLength(0);
        expect(screen.getByTestId('late-empty')).toBeInTheDocument();
        // aggregates still draw
        expect(screen.getAllByTestId('heat-row').length).toBe(2);
    });

    it('with no data at all renders every section empty without throwing', () => {
        render(<WorkersView />);
        expect(screen.getByTestId('heat-empty')).toHaveTextContent('no jobs data from the game yet');
        expect(screen.getByTestId('steps-empty')).toBeInTheDocument();
        expect(screen.getByTestId('share-empty')).toBeInTheDocument();
        expect(screen.getByTestId('pool-time-at-cap')).toHaveTextContent('unavailable');
        expect(screen.getByTestId('pool-inline')).toHaveTextContent('unavailable');
        expect(screen.getByTestId('cap-row')).toHaveTextContent('unavailable');
        expect(screen.getByTestId('pool-owners-empty')).toBeInTheDocument();
    });

    it('perJobRecording false on every unit shows the aggregates-only message', () => {
        load({ ...fixture.window, perJobRecording: false, units: fixture.window.units.map((u) => ({ ...u, perJobRecording: false })), steps: [] });
        render(<WorkersView />);
        expect(screen.getAllByTestId('heat-off')).toHaveLength(2);
        expect(screen.getByTestId('share-empty')).toBeInTheDocument();
    });
});
