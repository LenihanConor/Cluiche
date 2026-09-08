import { render, screen } from '@testing-library/react';
import { act } from 'react';
import { ModifiersTab } from './ModifiersTab';
import { useEconomyStore } from './useEconomyStore';
import type { ModifierInstance } from './useEconomyStore';

function seedModifiers(instances: ModifierInstance[]) {
    act(() => {
        useEconomyStore.getState().setModifiers(instances);
    });
}

beforeEach(() => {
    act(() => {
        useEconomyStore.getState().clearAll();
    });
});

const BASE_MODIFIER_INSTANCE: ModifierInstance = {
    id: 'player1',
    resources: [
        {
            name: 'gold',
            modifiers: [
                { type: 'multiply', value: 1.5, source: 'perk_A', condition: 'hp > 50', active: true },
                { type: 'add',      value: -10,  source: 'debuff_B', condition: null,    active: false },
            ],
        },
    ],
};

describe('ModifiersTab', () => {
    it('inactive modifier row has opacity 0.45', () => {
        seedModifiers([BASE_MODIFIER_INSTANCE]);
        render(<ModifiersTab />);
        const rows = screen.getAllByTestId('modifier-row');
        const inactiveRow = rows.find((r) => r.getAttribute('data-active') === 'false');
        expect(inactiveRow).toBeDefined();
        expect((inactiveRow as HTMLElement).style.opacity).toBe('0.45');
    });

    it('active modifier row has full opacity', () => {
        seedModifiers([BASE_MODIFIER_INSTANCE]);
        render(<ModifiersTab />);
        const rows = screen.getAllByTestId('modifier-row');
        const activeRow = rows.find((r) => r.getAttribute('data-active') === 'true');
        expect(activeRow).toBeDefined();
        const opacity = parseFloat((activeRow as HTMLElement).style.opacity || '1');
        expect(opacity).toBeGreaterThanOrEqual(0.9);
    });

    it('condition shown when present', () => {
        seedModifiers([BASE_MODIFIER_INSTANCE]);
        render(<ModifiersTab />);
        expect(screen.getByText('hp > 50')).toBeInTheDocument();
    });

    it('shows em dash when condition is null', () => {
        seedModifiers([BASE_MODIFIER_INSTANCE]);
        render(<ModifiersTab />);
        // The null condition should render as '—'
        expect(screen.getByText('—')).toBeInTheDocument();
    });

    it('data-active attribute reflects modifier active state', () => {
        seedModifiers([BASE_MODIFIER_INSTANCE]);
        render(<ModifiersTab />);
        const rows = screen.getAllByTestId('modifier-row');
        const activeRows   = rows.filter((r) => r.getAttribute('data-active') === 'true');
        const inactiveRows = rows.filter((r) => r.getAttribute('data-active') === 'false');
        expect(activeRows.length).toBe(1);
        expect(inactiveRows.length).toBe(1);
    });

    it('renders no modifier data message when empty', () => {
        render(<ModifiersTab />);
        expect(screen.getByText('No modifier data')).toBeInTheDocument();
    });
});
