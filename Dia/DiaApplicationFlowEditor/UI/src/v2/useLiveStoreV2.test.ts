import { describe, it, expect, beforeEach } from 'vitest';

import { useLiveStoreV2 } from './useLiveStoreV2';

beforeEach(() => {
    useLiveStoreV2.setState({
        connectionState: 'disconnected',
        activeStage: null,
        modules: [],
        streams: [],
    });
});

describe('useLiveStoreV2', () => {
    it('initial state: disconnected, null activeStage, empty modules/streams', () => {
        const s = useLiveStoreV2.getState();
        expect(s.connectionState).toBe('disconnected');
        expect(s.activeStage).toBeNull();
        expect(s.modules).toEqual([]);
        expect(s.streams).toEqual([]);
    });

    it('setConnectionState updates connectionState', () => {
        useLiveStoreV2.getState().setConnectionState('connected');
        expect(useLiveStoreV2.getState().connectionState).toBe('connected');
    });

    it('updateModuleStates replaces modules array', () => {
        const newModules = [
            { moduleId: 'mod1', puId: 'pu1', isActive: true, activeStage: 'Boot' },
        ];
        useLiveStoreV2.getState().updateModuleStates(newModules);
        expect(useLiveStoreV2.getState().modules).toEqual(newModules);
    });

    it('clearLiveState resets to disconnected, null, empty', () => {
        useLiveStoreV2.setState({
            connectionState: 'connected',
            activeStage: 'Boot',
            modules: [{ moduleId: 'm', puId: 'p', isActive: true, activeStage: 'Boot' }],
            streams: [{ streamId: 's', msgPerSec: 10 }],
        });
        useLiveStoreV2.getState().clearLiveState();
        const s = useLiveStoreV2.getState();
        expect(s.connectionState).toBe('disconnected');
        expect(s.activeStage).toBeNull();
        expect(s.modules).toEqual([]);
        expect(s.streams).toEqual([]);
    });

    it('store has no connect/disconnect actions', () => {
        const s = useLiveStoreV2.getState() as Record<string, unknown>;
        expect(s['connect']).toBeUndefined();
        expect(s['disconnect']).toBeUndefined();
    });
});
