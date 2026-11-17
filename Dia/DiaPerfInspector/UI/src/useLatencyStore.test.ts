import { useLatencyStore } from './useLatencyStore';

describe('useLatencyStore', () => {
    beforeEach(() => useLatencyStore.getState().setActions(null));
    it('starts null', () => expect(useLatencyStore.getState().actions).toBeNull());
    it('holds and clears actions', () => {
        useLatencyStore.getState().setActions([{ action: 'a', targetMs: 100, p50Ms: 1, p95Ms: 2, samples: 1 }]);
        expect(useLatencyStore.getState().actions).toHaveLength(1);
        useLatencyStore.getState().setActions(null);
        expect(useLatencyStore.getState().actions).toBeNull();
    });
});
