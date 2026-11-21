import { useChainStore } from './useChainStore';
import fixture from './model/__fixtures__/chain.fixture.json';
import type { ChainData } from './types';

describe('useChainStore', () => {
    beforeEach(() => useChainStore.setState({ chain: null }));
    it('starts empty', () => {
        expect(useChainStore.getState().chain).toBeNull();
    });
    it('holds and clears chain data', () => {
        useChainStore.getState().setChain(fixture as ChainData);
        expect(useChainStore.getState().chain?.stages).toHaveLength(4);
        useChainStore.getState().setChain(null);
        expect(useChainStore.getState().chain).toBeNull();
    });
});
