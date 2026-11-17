// Uses the SYNTHETIC chain fixture (not recorded from a live game); see model/chain.test.ts.
import { render, screen } from '@testing-library/react';
import { ChainPanel } from './ChainPanel';
import { useChainStore } from '../useChainStore';
import fixture from '../model/__fixtures__/chain.fixture.json';
import type { ChainData } from '../types';

describe('ChainPanel', () => {
    beforeEach(() => useChainStore.setState({ chain: null, reason: null }));

    it('says the chain is unavailable when no data has arrived', () => {
        render(<ChainPanel budgetMs={33.3} />);
        expect(screen.getByTestId('chain-panel')).toHaveTextContent('chain unavailable: no chain data from the game yet');
    });

    it('renders stages, the stream edge, total and share of budget', () => {
        useChainStore.setState({ chain: fixture as ChainData });
        render(<ChainPanel budgetMs={33.3} />);
        const names = screen.getAllByTestId('chain-stage').map((e) => e.textContent ?? '');
        expect(names).toHaveLength(4);
        expect(names[0]).toContain('IDebugDomain.GetJSONState');
        expect(names[0]).toContain('4.2');
        expect(names[2]).toContain('Main');
        const edge = screen.getByTestId('chain-edge');
        expect(edge).toHaveTextContent('stream DomainStateSnapshot');
        expect(edge).toHaveTextContent('6.5 KB');
        expect(edge).toHaveTextContent('30/s');
        expect(edge).toHaveTextContent('write 0.2 ms');
        expect(edge).toHaveTextContent('data age 12 ms');
        expect(screen.getByTestId('chain-total')).toHaveTextContent('11.5 ms');
        expect(screen.getByTestId('chain-total')).toHaveTextContent('35% of one budget');
    });

    it('shows no share text when the budget is unknown', () => {
        useChainStore.setState({ chain: fixture as ChainData });
        render(<ChainPanel budgetMs={0} />);
        expect(screen.getByTestId('chain-total')).toHaveTextContent('11.5 ms');
        expect(screen.getByTestId('chain-total')).toHaveTextContent('budget unknown');
    });

    it('says so for a chain with no stages', () => {
        useChainStore.setState({ chain: { name: 'x', stages: [], edges: [] } });
        render(<ChainPanel budgetMs={33.3} />);
        expect(screen.getByTestId('chain-panel')).toHaveTextContent('chain has no stages');
    });

    it('shows the games reason when it gave one', () => {
        useChainStore.setState({ chain: null, reason: 'no spans recorded (is tracing on?)' });
        render(<ChainPanel budgetMs={33.3} />);
        expect(screen.getByTestId('chain-panel')).toHaveTextContent('chain unavailable: no spans recorded (is tracing on?)');
    });

    it('renders an edge with only the statistics the game could read', () => {
        useChainStore.setState({
            chain: { ...(fixture as ChainData), edges: [{ stream: 'DomainStateSnapshot', fromId: '1002', toId: '2001', dataAgeMs: 12 }] },
        });
        render(<ChainPanel budgetMs={33.3} />);
        const edge = screen.getByTestId('chain-edge');
        expect(edge).toHaveTextContent('data age 12 ms');
        expect(edge).not.toHaveTextContent('KB');
        expect(edge).not.toHaveTextContent('/s');
        expect(edge).not.toHaveTextContent('write');
    });
});
