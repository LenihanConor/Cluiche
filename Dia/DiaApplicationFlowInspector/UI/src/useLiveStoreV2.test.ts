import { describe, it, expect, vi, beforeEach } from 'vitest';

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn(),
    bridgeEvent: vi.fn(),
}));

import { useLiveStoreV2 } from './useLiveStoreV2';
import { bridgeRequest } from './bridge';

const mockBridgeRequest = bridgeRequest as ReturnType<typeof vi.fn>;

beforeEach(() => {
    useLiveStoreV2.setState({
        connectionState: 'disconnected',
        activeStage: null,
        modules: [],
        streams: [],
    });
    mockBridgeRequest.mockReset();
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

    it('connect calls bridgeRequest with live.connect and sets state to connecting', async () => {
        mockBridgeRequest.mockResolvedValue(undefined);
        await useLiveStoreV2.getState().connect('localhost', 7777);
        expect(mockBridgeRequest).toHaveBeenCalledWith('live.connect', { host: 'localhost', port: 7777 });
        expect(useLiveStoreV2.getState().connectionState).toBe('connecting');
    });

    it('disconnect calls bridgeRequest with live.disconnect and sets state to disconnected', async () => {
        useLiveStoreV2.setState({ connectionState: 'connected' });
        mockBridgeRequest.mockResolvedValue(undefined);
        await useLiveStoreV2.getState().disconnect();
        expect(mockBridgeRequest).toHaveBeenCalledWith('live.disconnect');
        expect(useLiveStoreV2.getState().connectionState).toBe('disconnected');
    });
});
