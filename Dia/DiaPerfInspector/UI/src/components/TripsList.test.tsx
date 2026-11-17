import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { TripsList } from './TripsList';
import { EventsRow } from './EventsRow';
import type { TripRecord } from '../types';

function rec(over: Partial<TripRecord> = {}): TripRecord {
    return {
        unit: 'Main', buildConfig: 'Debug', firstFrame: 100, lastFrame: 120, budgetMs: 33.3, peakMs: 55.5,
        suppressedSinceLastTrip: 0, topModules: [{ module: 'A', ms: 20 }],
        sections: [], worstFrame: { index: 110, unitMs: 55.5, modules: [] }, recentUnitMs: [], ...over,
    };
}

describe('TripsList', () => {
    it('shows the honest message when trips are listed as unavailable', () => {
        render(<TripsList records={[]} unavailable={['trips']} onSelect={() => {}} />);
        expect(screen.getByTestId('trips-list')).toHaveTextContent('no trip data from the game yet');
    });

    it('shows a plain empty message when trips are supported but none occurred', () => {
        render(<TripsList records={[]} unavailable={[]} onSelect={() => {}} />);
        expect(screen.getByTestId('trips-list')).toHaveTextContent(/no trips/i);
        expect(screen.getByTestId('trips-list')).not.toHaveTextContent('from the game yet');
    });

    it('renders rows and selects the first frame on click', async () => {
        const onSelect = vi.fn();
        render(<TripsList records={[rec(), rec({ unit: 'Sim', firstFrame: 7, lastFrame: 9 })]} unavailable={[]} onSelect={onSelect} />);
        const rows = screen.getAllByTestId('trip-row');
        expect(rows).toHaveLength(2);
        expect(rows[0]).toHaveTextContent('Main');
        expect(rows[0]).toHaveTextContent('100');
        expect(rows[0]).toHaveTextContent('120');
        expect(rows[0]).toHaveTextContent('55.5');
        expect(rows[0]).toHaveTextContent('33.3');
        await userEvent.click(rows[1]);
        expect(onSelect).toHaveBeenCalledWith('Sim', 7);
    });
});

describe('EventsRow', () => {
    it('is empty with the honest message when there are no markers', () => {
        render(<EventsRow markers={[]} unavailable={['markers']} />);
        expect(screen.getByTestId('events-row')).toHaveTextContent('no event data from the game yet');
        expect(screen.queryAllByTestId('event-marker')).toHaveLength(0);
    });

    it('renders typed markers when given', () => {
        render(<EventsRow markers={[{ unit: 'Main', frame: 5, label: 'stage entered' }]} unavailable={[]} />);
        expect(screen.getAllByTestId('event-marker')).toHaveLength(1);
        expect(screen.getByTestId('events-row')).toHaveTextContent('stage entered');
    });
});
