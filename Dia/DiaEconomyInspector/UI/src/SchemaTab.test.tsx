import { render, screen } from '@testing-library/react';
import { act } from 'react';
import { SchemaTab } from './SchemaTab';
import { useEconomyStore } from './useEconomyStore';

beforeEach(() => {
    act(() => {
        useEconomyStore.getState().clearAll();
    });
});

const SAMPLE_SCHEMA = {
    resources: [
        { name: 'gold',  type: 'base'    as const, base_cap: 1000, income_rule: 'linear' },
        { name: 'mana',  type: 'derived' as const, base_cap: 200,  income_rule: 'regen'  },
        { name: 'stone', type: 'base'    as const, base_cap: 500,  income_rule: 'flat'   },
    ],
    cost_table: [
        { action: 'build_wall', gold: 100, stone: 50, mana: 0 },
        { action: 'cast_spell', gold: 0,   stone: 0,  mana: 30 },
    ],
};

function seedSchema() {
    act(() => {
        useEconomyStore.getState().setSchema(SAMPLE_SCHEMA);
    });
}

describe('SchemaTab', () => {
    it('renders resource rows', () => {
        seedSchema();
        render(<SchemaTab />);
        const rows = screen.getAllByTestId('schema-resource-row');
        expect(rows.length).toBe(3);
    });

    it('derived resource row has purple type indicator', () => {
        seedSchema();
        render(<SchemaTab />);
        const rows = screen.getAllByTestId('schema-resource-row');
        const manaRow = rows.find((r) => r.textContent?.includes('mana'));
        expect(manaRow).toBeDefined();
        // The type badge inside should have purple background
        const badge = manaRow!.querySelector('span');
        expect(badge).not.toBeNull();
        // purple #9b59b6 → rgb(155, 89, 182)
        expect((badge as HTMLElement).style.background).toMatch(/rgb\(155,\s*89,\s*182\)/);
    });

    it('base resource row does not have purple type indicator', () => {
        seedSchema();
        render(<SchemaTab />);
        const rows = screen.getAllByTestId('schema-resource-row');
        const goldRow = rows.find((r) => r.textContent?.includes('gold'));
        expect(goldRow).toBeDefined();
        const badge = goldRow!.querySelector('span');
        expect(badge).not.toBeNull();
        expect((badge as HTMLElement).style.background).not.toContain('9b59b6');
    });

    it('cost table rows render correctly', () => {
        seedSchema();
        render(<SchemaTab />);
        const costRows = screen.getAllByTestId('schema-cost-row');
        expect(costRows.length).toBe(2);
    });

    it('cost table row shows action name', () => {
        seedSchema();
        render(<SchemaTab />);
        expect(screen.getByText('build_wall')).toBeInTheDocument();
        expect(screen.getByText('cast_spell')).toBeInTheDocument();
    });

    it('shows no schema message when schema is null', () => {
        render(<SchemaTab />);
        expect(screen.getByText('No schema data received yet.')).toBeInTheDocument();
    });

    it('search bar filters resource rows', async () => {
        seedSchema();
        render(<SchemaTab />);
        const input = screen.getByPlaceholderText('Search resources...');
        // Simulate typing in the search box
        act(() => {
            input.dispatchEvent(new Event('input', { bubbles: true }));
            Object.defineProperty(input, 'value', { value: 'gold', writable: true });
        });

        // Use userEvent or fireEvent for change
        const { fireEvent } = await import('@testing-library/react');
        fireEvent.change(input, { target: { value: 'gold' } });

        const rows = screen.getAllByTestId('schema-resource-row');
        expect(rows.length).toBe(1);
        expect(rows[0].textContent).toContain('gold');
    });
});
