import { render, screen } from '@testing-library/react';
import { act } from 'react';
import { EventsTab } from './EventsTab';
import { useEconomyStore } from './useEconomyStore';
import type { EconomyEvent } from './useEconomyStore';

function seedEvents(events: EconomyEvent[]) {
    act(() => {
        useEconomyStore.getState().appendEvents({ full_ring: true, events });
    });
}

beforeEach(() => {
    act(() => {
        useEconomyStore.getState().clearAll();
    });
});

describe('EventsTab', () => {
    it('renders Earn row with green badge', () => {
        seedEvents([{ frame: 1, type: 'Earn', instance: 'p1', resource: 'gold', amount: 10 }]);
        render(<EventsTab />);
        const badge = screen.getByTestId('event-type-badge');
        expect(badge).toHaveTextContent('Earn');
        const style = (badge as HTMLElement).style;
        // green — #89d185 → rgb(137, 209, 133)
        expect(style.color).toMatch(/rgb\(137,\s*209,\s*133\)/);
    });

    it('renders Spend row with blue badge', () => {
        seedEvents([{ frame: 2, type: 'Spend', instance: 'p1', resource: 'gold', amount: 5 }]);
        render(<EventsTab />);
        const badge = screen.getByTestId('event-type-badge');
        expect(badge).toHaveTextContent('Spend');
        // blue — #4da6ff → rgb(77, 166, 255)
        expect((badge as HTMLElement).style.color).toMatch(/rgb\(77,\s*166,\s*255\)/);
    });

    it('renders Clamped row with orange badge', () => {
        seedEvents([{ frame: 3, type: 'Clamped', instance: 'p1', resource: 'gold', attempted: 20, actual: 10 }]);
        render(<EventsTab />);
        const badge = screen.getByTestId('event-type-badge');
        expect(badge).toHaveTextContent('Clamped');
        // orange — #cca700 → rgb(204, 167, 0)
        expect((badge as HTMLElement).style.color).toMatch(/rgb\(204,\s*167,\s*0\)/);
    });

    it('renders Clamped row with orange-tinted background', () => {
        seedEvents([{ frame: 3, type: 'Clamped', instance: 'p1', resource: 'gold', attempted: 20, actual: 10 }]);
        render(<EventsTab />);
        const row = screen.getByTestId('event-row');
        // Background should not be transparent — should contain the orange tint
        expect((row as HTMLElement).style.background).not.toBe('transparent');
    });

    it('renders Transfer row with purple badge', () => {
        seedEvents([{ frame: 4, type: 'Transfer', instance: 'p1', resource: 'gold', amount: 5, destination: 'p2' }]);
        render(<EventsTab />);
        const badge = screen.getByTestId('event-type-badge');
        expect(badge).toHaveTextContent('Transfer');
        // purple — #c084fc → rgb(192, 132, 252)
        expect((badge as HTMLElement).style.color).toMatch(/rgb\(192,\s*132,\s*252\)/);
    });

    it('Transfer row shows destination in Detail column', () => {
        seedEvents([{ frame: 4, type: 'Transfer', instance: 'p1', resource: 'gold', amount: 5, destination: 'p2' }]);
        render(<EventsTab />);
        expect(screen.getByText('→ p2')).toBeInTheDocument();
    });

    it('Earn row Detail column is empty', () => {
        seedEvents([{ frame: 1, type: 'Earn', instance: 'p1', resource: 'gold', amount: 10 }]);
        render(<EventsTab />);
        const rows = screen.getAllByTestId('event-row');
        expect(rows.length).toBe(1);
        // The row should not contain any '→' text
        expect(rows[0].textContent).not.toContain('→');
    });

    it('renders multiple rows', () => {
        seedEvents([
            { frame: 1, type: 'Earn',  instance: 'p1', resource: 'gold', amount: 10 },
            { frame: 2, type: 'Spend', instance: 'p1', resource: 'gold', amount: 5 },
        ]);
        render(<EventsTab />);
        const rows = screen.getAllByTestId('event-row');
        expect(rows.length).toBe(2);
    });
});
