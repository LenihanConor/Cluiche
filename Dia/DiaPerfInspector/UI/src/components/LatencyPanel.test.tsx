// Uses the SYNTHETIC latency fixture (not recorded from a game).
import { render, screen } from '@testing-library/react';
import { LatencyPanel } from './LatencyPanel';
import { SelfCostPanel } from './SelfCostPanel';
import fixture from '../model/__fixtures__/latency.fixture.json';
import type { LatencyAction } from '../types';

describe('LatencyPanel', () => {
    it('shows the honest message with null data and no reason', () => {
        render(<LatencyPanel actions={null} reason={null} />);
        expect(screen.getByTestId('latency-panel')).toHaveTextContent('latency unavailable: no latency data from the game yet');
    });
    it('shows the specific reason when the game could not answer', () => {
        render(<LatencyPanel actions={null} reason="latency probe needs a Debug build" />);
        expect(screen.getByTestId('latency-panel')).toHaveTextContent('latency unavailable: latency probe needs a Debug build');
    });
    it('shows the no-samples hint for an empty action list', () => {
        render(<LatencyPanel actions={[]} reason={null} />);
        expect(screen.getByTestId('latency-panel')).toHaveTextContent('no latency samples yet: drag a slider or run a panel command in the game');
        expect(screen.getByTestId('latency-panel')).not.toHaveTextContent('unavailable');
    });
    it('renders fixture rows with status', () => {
        render(<LatencyPanel actions={fixture as LatencyAction[]} reason={null} />);
        const rows = screen.getAllByTestId('latency-row');
        expect(rows).toHaveLength(4);
        expect(rows[0]).toHaveAttribute('data-status', 'over');
        expect(rows[0]).toHaveTextContent('TerrainDomain.loadPreset');
        expect(rows[3]).toHaveAttribute('data-status', 'no-samples');
    });
});

describe('SelfCostPanel', () => {
    it('shows selfMs, status, the shared-gauge note and unavailable rows', () => {
        render(<SelfCostPanel selfMs={0.2} unavailable={['latency', 'suppressed_logs']} />);
        const p = screen.getByTestId('selfcost-panel');
        expect(p).toHaveTextContent('0.200 ms');
        expect(screen.getByTestId('selfcost-status')).toHaveAttribute('data-status', 'over');
        expect(p).toHaveTextContent('last writer');
        expect(screen.getByTestId('selfcost-suppressed')).toHaveTextContent('unavailable');
    });
    it('shows unavailable with no push yet', () => {
        render(<SelfCostPanel selfMs={undefined} unavailable={undefined} />);
        expect(screen.getByTestId('selfcost-status')).toHaveAttribute('data-status', 'unavailable');
    });
});
