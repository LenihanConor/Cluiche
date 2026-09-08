import { render, screen } from '@testing-library/react';
import { UsagePanel } from './UsagePanel';
import type { UsageEntry } from '../types';

vi.mock('@dia/editor-ui', () => ({
    theme: { bgPanel: '#252526', border: '#3c3c3c', text: '#d4d4d4', textMuted: '#888' },
    EmptyState: ({ message }: { message: string }) => <div data-testid="empty-state">{message}</div>,
}));

const sampleUsage: UsageEntry[] = [
    { sceneId: 'level-01', instanceCount: 3 },
    { sceneId: 'level-02', instanceCount: 1 },
];

describe('UsagePanel', () => {
    it('shows "No blueprint selected" EmptyState when selectedId is null', () => {
        render(<UsagePanel usage={[]} selectedId={null} />);
        expect(screen.getByTestId('empty-state')).toBeInTheDocument();
        expect(screen.getByTestId('empty-state')).toHaveTextContent('No blueprint selected');
    });

    it('shows "No scene references" EmptyState when selected but usage is empty', () => {
        render(<UsagePanel usage={[]} selectedId="hero" />);
        expect(screen.getByTestId('empty-state')).toBeInTheDocument();
        expect(screen.getByTestId('empty-state')).toHaveTextContent('No scene references found');
    });

    it('renders usage items with sceneId', () => {
        render(<UsagePanel usage={sampleUsage} selectedId="hero" />);
        expect(screen.getByTestId('usage-item-0')).toHaveTextContent('level-01');
        expect(screen.getByTestId('usage-item-1')).toHaveTextContent('level-02');
    });

    it('renders instance count for each item', () => {
        render(<UsagePanel usage={sampleUsage} selectedId="hero" />);
        expect(screen.getByTestId('usage-item-0')).toHaveTextContent('3 instance(s)');
        expect(screen.getByTestId('usage-item-1')).toHaveTextContent('1 instance(s)');
    });

    it('renders multiple items', () => {
        const multiUsage: UsageEntry[] = [
            { sceneId: 'scene-a', instanceCount: 2 },
            { sceneId: 'scene-b', instanceCount: 5 },
            { sceneId: 'scene-c', instanceCount: 1 },
        ];
        render(<UsagePanel usage={multiUsage} selectedId="enemy" />);
        expect(screen.getByTestId('usage-item-0')).toBeInTheDocument();
        expect(screen.getByTestId('usage-item-1')).toBeInTheDocument();
        expect(screen.getByTestId('usage-item-2')).toBeInTheDocument();
        expect(screen.queryByTestId('usage-item-3')).not.toBeInTheDocument();
    });
});
