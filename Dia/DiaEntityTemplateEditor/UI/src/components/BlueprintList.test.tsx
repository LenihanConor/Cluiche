import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { BlueprintList } from './BlueprintList';
import type { BlueprintGroup } from '../types';

vi.mock('@dia/editor-ui', () => ({
    theme: { bg: '#1e1e1e', bgPanel: '#252526', border: '#3c3c3c', text: '#d4d4d4', textMuted: '#888', accent: '#0e639c' },
    inputStyle: (o?: object) => ({ ...o }),
    EmptyState: ({ message }: { message: string }) => <div data-testid="empty-state">{message}</div>,
}));

const sampleGroups: BlueprintGroup[] = [
    {
        label: 'Characters',
        items: [
            { id: 'hero', label: 'Hero', path: 'Assets/hero.diaentitytemplate' },
            { id: 'villain', label: 'Villain', path: 'Assets/villain.diaentitytemplate' },
        ],
    },
    {
        label: 'Props',
        items: [
            { id: 'barrel', label: 'Barrel', path: 'Assets/barrel.diaentitytemplate' },
        ],
    },
];

function makeProps(overrides?: Partial<Parameters<typeof BlueprintList>[0]>) {
    return {
        groups: sampleGroups,
        selectedId: null,
        onSelect: vi.fn(),
        onRefresh: vi.fn(),
        ...overrides,
    };
}

describe('BlueprintList', () => {
    it('renders group header labels', () => {
        render(<BlueprintList {...makeProps()} />);
        expect(screen.getByText('Characters')).toBeInTheDocument();
        expect(screen.getByText('Props')).toBeInTheDocument();
    });

    it('renders blueprint items with labels', () => {
        render(<BlueprintList {...makeProps()} />);
        expect(screen.getByText('Hero')).toBeInTheDocument();
        expect(screen.getByText('Villain')).toBeInTheDocument();
        expect(screen.getByText('Barrel')).toBeInTheDocument();
    });

    it('clicking an item calls onSelect with correct id and path', async () => {
        const onSelect = vi.fn();
        render(<BlueprintList {...makeProps({ onSelect })} />);

        const heroItem = screen.getByText('Hero');
        await userEvent.click(heroItem);

        expect(onSelect).toHaveBeenCalledWith('hero', 'Assets/hero.diaentitytemplate');
    });

    it('selected item has accent background color', () => {
        render(<BlueprintList {...makeProps({ selectedId: 'villain' })} />);
        const selectedEl = screen.getAllByTestId('blueprint-item').find(
            el => el.getAttribute('data-item-id') === 'villain'
        );
        expect(selectedEl).toBeDefined();
        expect(selectedEl?.style.background).toBe('rgb(14, 99, 156)');
    });

    it('search filter — typing "hero" only shows matching item, hides non-matching', async () => {
        render(<BlueprintList {...makeProps()} />);

        const input = screen.getByRole('textbox', { name: /search/i });
        await userEvent.type(input, 'hero');

        const items = screen.getAllByTestId('blueprint-item');
        expect(items).toHaveLength(1);
        expect(items[0].getAttribute('data-item-id')).toBe('hero');
        expect(screen.queryByText('Villain')).not.toBeInTheDocument();
        expect(screen.queryByText('Barrel')).not.toBeInTheDocument();
    });

    it('search filter — empty query shows all items', async () => {
        render(<BlueprintList {...makeProps()} />);

        const input = screen.getByRole('textbox', { name: /search/i });
        await userEvent.type(input, 'hero');
        await userEvent.clear(input);

        const items = screen.getAllByTestId('blueprint-item');
        expect(items).toHaveLength(3);
    });

    it('empty groups list renders EmptyState with default message', () => {
        render(<BlueprintList {...makeProps({ groups: [] })} />);
        expect(screen.getByTestId('empty-state')).toBeInTheDocument();
        expect(screen.getByTestId('empty-state')).toHaveTextContent('No blueprints registered.');
    });

    it('no matches after filtering renders EmptyState with query message', async () => {
        render(<BlueprintList {...makeProps()} />);

        const input = screen.getByRole('textbox', { name: /search/i });
        await userEvent.type(input, 'zzznomatch');

        expect(screen.getByTestId('empty-state')).toBeInTheDocument();
        expect(screen.getByTestId('empty-state')).toHaveTextContent('No matches for "zzznomatch"');
    });

    it('onRefresh called when refresh button clicked', async () => {
        const onRefresh = vi.fn();
        render(<BlueprintList {...makeProps({ onRefresh })} />);

        const btn = screen.getByRole('button', { name: /refresh/i });
        await userEvent.click(btn);

        expect(onRefresh).toHaveBeenCalledTimes(1);
    });
});
