import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useLiveConnection } from './useLiveConnection';

// Helper to dispatch a fake __dia postMessage into the window.
function postDiaMessage(topic: string, data: unknown) {
    window.dispatchEvent(
        new MessageEvent('message', {
            data: { __dia: true, topic, data },
        }),
    );
}

let mockBridgeRequest: ReturnType<typeof vi.fn>;

beforeEach(() => {
    mockBridgeRequest = vi.fn();
});

describe('useLiveConnection', () => {
    it('starts as disconnected before the bridge request resolves', () => {
        // Never-resolving promise so we can observe the initial state.
        mockBridgeRequest.mockReturnValue(new Promise(() => {}));
        const { result } = renderHook(() =>
            useLiveConnection('my_plugin', mockBridgeRequest),
        );
        expect(result.current).toBe('disconnected');
    });

    it('resolves to connected when bridgeRequestFn returns { connected: true }', async () => {
        mockBridgeRequest.mockResolvedValue({ connected: true });
        const { result } = renderHook(() =>
            useLiveConnection('my_plugin', mockBridgeRequest),
        );
        // Wait for the promise microtask to flush.
        await act(async () => {});
        expect(result.current).toBe('connected');
    });

    it('stays disconnected when bridgeRequestFn returns { connected: false }', async () => {
        mockBridgeRequest.mockResolvedValue({ connected: false });
        const { result } = renderHook(() =>
            useLiveConnection('my_plugin', mockBridgeRequest),
        );
        await act(async () => {});
        expect(result.current).toBe('disconnected');
    });

    it('stays disconnected when bridgeRequestFn returns null', async () => {
        mockBridgeRequest.mockResolvedValue(null);
        const { result } = renderHook(() =>
            useLiveConnection('my_plugin', mockBridgeRequest),
        );
        await act(async () => {});
        expect(result.current).toBe('disconnected');
    });

    it('updates to connected on a postMessage push with connected: true', async () => {
        mockBridgeRequest.mockReturnValue(new Promise(() => {}));
        const { result } = renderHook(() =>
            useLiveConnection('my_plugin', mockBridgeRequest),
        );
        act(() => {
            postDiaMessage('my_plugin.connection_state', { connected: true });
        });
        expect(result.current).toBe('connected');
    });

    it('updates to disconnected on a postMessage push with connected: false', async () => {
        mockBridgeRequest.mockResolvedValue({ connected: true });
        const { result } = renderHook(() =>
            useLiveConnection('my_plugin', mockBridgeRequest),
        );
        await act(async () => {});
        expect(result.current).toBe('connected');

        act(() => {
            postDiaMessage('my_plugin.connection_state', { connected: false });
        });
        expect(result.current).toBe('disconnected');
    });

    it('ignores postMessages with a mismatched topic prefix', () => {
        mockBridgeRequest.mockReturnValue(new Promise(() => {}));
        const { result } = renderHook(() =>
            useLiveConnection('my_plugin', mockBridgeRequest),
        );
        act(() => {
            postDiaMessage('other_plugin.connection_state', { connected: true });
        });
        expect(result.current).toBe('disconnected');
    });

    it('ignores postMessages where __dia is not true', () => {
        mockBridgeRequest.mockReturnValue(new Promise(() => {}));
        const { result } = renderHook(() =>
            useLiveConnection('my_plugin', mockBridgeRequest),
        );
        act(() => {
            window.dispatchEvent(
                new MessageEvent('message', {
                    data: { __dia: false, topic: 'my_plugin.connection_state', data: { connected: true } },
                }),
            );
        });
        expect(result.current).toBe('disconnected');
    });

    it('removes the message listener on unmount', async () => {
        mockBridgeRequest.mockReturnValue(new Promise(() => {}));
        const { result, unmount } = renderHook(() =>
            useLiveConnection('my_plugin', mockBridgeRequest),
        );
        unmount();
        act(() => {
            postDiaMessage('my_plugin.connection_state', { connected: true });
        });
        // After unmount the hook's state is no longer observable, but the main
        // assertion is that no error is thrown and no state update warning fires.
        expect(result.current).toBe('disconnected');
    });

    it('fires bridgeRequestFn with the correct topic', async () => {
        mockBridgeRequest.mockResolvedValue({});
        renderHook(() => useLiveConnection('app_flow_inspector', mockBridgeRequest));
        await act(async () => {});
        expect(mockBridgeRequest).toHaveBeenCalledWith(
            'app_flow_inspector.get_connection_state',
            {},
        );
    });
});
