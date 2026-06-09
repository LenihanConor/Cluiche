// useBridge.test.ts
// Tests for useBridgeSubscribe and useBridgeRequest hooks.
// Covers both main-frame (window.CluicheEditor) and iframe (postMessage) contexts.

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useBridgeSubscribe, useBridgeRequest } from './useBridge';
import type { BridgeSubscription } from './useBridge';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function makeMainFrameBridge() {
    const listeners: Record<string, Array<(data: unknown) => void>> = {};
    const unsubscribeMocks: ReturnType<typeof vi.fn>[] = [];

    const bridge = {
        subscribe: vi.fn((topic: string, cb: (data: unknown) => void) => {
            if (!listeners[topic]) listeners[topic] = [];
            listeners[topic].push(cb);
            const unsub = vi.fn(() => {
                listeners[topic] = listeners[topic].filter((l) => l !== cb);
            });
            unsubscribeMocks.push(unsub);
            return unsub;
        }),
        request: vi.fn((_topic: string, _payload?: unknown) => Promise.resolve({ ok: true })),
        emit: (topic: string, data: unknown) => {
            (listeners[topic] ?? []).forEach((cb) => cb(data));
        },
    };
    return { bridge, unsubscribeMocks };
}

// ---------------------------------------------------------------------------
// useBridgeSubscribe — main-frame path
// ---------------------------------------------------------------------------

describe('useBridgeSubscribe — main frame (window.CluicheEditor)', () => {
    let bridge: ReturnType<typeof makeMainFrameBridge>['bridge'];
    let unsubscribeMocks: ReturnType<typeof makeMainFrameBridge>['unsubscribeMocks'];

    beforeEach(() => {
        const result = makeMainFrameBridge();
        bridge = result.bridge;
        unsubscribeMocks = result.unsubscribeMocks;
        (window as any).CluicheEditor = bridge;
    });

    afterEach(() => {
        delete (window as any).CluicheEditor;
        vi.clearAllMocks();
    });

    it('calls subscribe on CluicheEditor for each topic on mount', () => {
        const subs: BridgeSubscription[] = [
            { topic: 'manifest.state', handler: vi.fn() },
            { topic: 'validation.result', handler: vi.fn() },
        ];
        renderHook(() => useBridgeSubscribe(subs));
        expect(bridge.subscribe).toHaveBeenCalledTimes(2);
        expect(bridge.subscribe).toHaveBeenCalledWith('manifest.state', expect.any(Function));
        expect(bridge.subscribe).toHaveBeenCalledWith('validation.result', expect.any(Function));
    });

    it('calls unsubscribe for all topics on unmount', () => {
        const subs: BridgeSubscription[] = [
            { topic: 'topic.a', handler: vi.fn() },
            { topic: 'topic.b', handler: vi.fn() },
        ];
        const { unmount } = renderHook(() => useBridgeSubscribe(subs));
        unmount();
        expect(unsubscribeMocks).toHaveLength(2);
        unsubscribeMocks.forEach((unsub) => expect(unsub).toHaveBeenCalledOnce());
    });

    it('calls handler when the subscribed topic fires', () => {
        const handler = vi.fn();
        const subs: BridgeSubscription[] = [{ topic: 'live.state', handler }];
        renderHook(() => useBridgeSubscribe(subs));
        act(() => bridge.emit('live.state', { stage: 'Boot' }));
        expect(handler).toHaveBeenCalledWith({ stage: 'Boot' });
    });

    it('calls correct handler for each topic', () => {
        const handlerA = vi.fn();
        const handlerB = vi.fn();
        const subs: BridgeSubscription[] = [
            { topic: 'topic.a', handler: handlerA },
            { topic: 'topic.b', handler: handlerB },
        ];
        renderHook(() => useBridgeSubscribe(subs));
        act(() => bridge.emit('topic.b', 'data-b'));
        expect(handlerA).not.toHaveBeenCalled();
        expect(handlerB).toHaveBeenCalledWith('data-b');
    });

    it('is safe to call with an empty subscriptions array', () => {
        expect(() => renderHook(() => useBridgeSubscribe([]))).not.toThrow();
        expect(bridge.subscribe).not.toHaveBeenCalled();
    });

    it('does not re-subscribe when only handler reference changes (same topics)', () => {
        let callCount = 0;
        const { rerender } = renderHook(() => {
            const handler = vi.fn(); // new fn reference each render
            useBridgeSubscribe([{ topic: 'stable.topic', handler }]);
            callCount++;
        });
        rerender();
        rerender();
        // subscribe should only have been called once (initial mount)
        expect(bridge.subscribe).toHaveBeenCalledTimes(1);
    });
});

// ---------------------------------------------------------------------------
// useBridgeSubscribe — iframe path (no window.CluicheEditor)
// ---------------------------------------------------------------------------

describe('useBridgeSubscribe — iframe (postMessage)', () => {
    beforeEach(() => {
        delete (window as any).CluicheEditor;
    });

    it('registers a message listener on mount', () => {
        const addSpy = vi.spyOn(window, 'addEventListener');
        const subs: BridgeSubscription[] = [{ topic: 'live.state', handler: vi.fn() }];
        renderHook(() => useBridgeSubscribe(subs));
        expect(addSpy).toHaveBeenCalledWith('message', expect.any(Function));
        addSpy.mockRestore();
    });

    it('removes message listener on unmount', () => {
        const removeSpy = vi.spyOn(window, 'removeEventListener');
        const subs: BridgeSubscription[] = [{ topic: 'live.state', handler: vi.fn() }];
        const { unmount } = renderHook(() => useBridgeSubscribe(subs));
        unmount();
        expect(removeSpy).toHaveBeenCalledWith('message', expect.any(Function));
        removeSpy.mockRestore();
    });

    it('calls handler when a matching __dia message arrives', () => {
        const handler = vi.fn();
        const subs: BridgeSubscription[] = [{ topic: 'live.modules', handler }];
        renderHook(() => useBridgeSubscribe(subs));

        act(() => {
            window.dispatchEvent(
                new MessageEvent('message', {
                    data: { __dia: true, topic: 'live.modules', data: [{ moduleId: 'Renderer' }] },
                })
            );
        });

        expect(handler).toHaveBeenCalledWith([{ moduleId: 'Renderer' }]);
    });

    it('does not call handler for a different topic', () => {
        const handler = vi.fn();
        const subs: BridgeSubscription[] = [{ topic: 'live.modules', handler }];
        renderHook(() => useBridgeSubscribe(subs));

        act(() => {
            window.dispatchEvent(
                new MessageEvent('message', {
                    data: { __dia: true, topic: 'live.streams', data: [] },
                })
            );
        });

        expect(handler).not.toHaveBeenCalled();
    });

    it('ignores messages without __dia flag', () => {
        const handler = vi.fn();
        const subs: BridgeSubscription[] = [{ topic: 'live.modules', handler }];
        renderHook(() => useBridgeSubscribe(subs));

        act(() => {
            window.dispatchEvent(
                new MessageEvent('message', {
                    data: { topic: 'live.modules', data: 'some data' },
                })
            );
        });

        expect(handler).not.toHaveBeenCalled();
    });

    it('is safe to call with empty array in iframe context', () => {
        expect(() => renderHook(() => useBridgeSubscribe([]))).not.toThrow();
    });
});

// ---------------------------------------------------------------------------
// useBridgeRequest — main-frame path
// ---------------------------------------------------------------------------

describe('useBridgeRequest — main frame (window.CluicheEditor)', () => {
    let requestMock: ReturnType<typeof vi.fn>;

    beforeEach(() => {
        requestMock = vi.fn(() => Promise.resolve({ ok: true, payload: 'response' }));
        (window as any).CluicheEditor = { request: requestMock };
    });

    afterEach(() => {
        delete (window as any).CluicheEditor;
        vi.clearAllMocks();
    });

    it('returns a stable function reference across renders', () => {
        const { result, rerender } = renderHook(() => useBridgeRequest());
        const first = result.current;
        rerender();
        expect(result.current).toBe(first);
    });

    it('calls CluicheEditor.request with the correct topic', async () => {
        const { result } = renderHook(() => useBridgeRequest());
        await act(async () => {
            await result.current('manifest.get');
        });
        expect(requestMock).toHaveBeenCalledWith('manifest.get', undefined);
    });

    it('calls CluicheEditor.request with topic and payload', async () => {
        const { result } = renderHook(() => useBridgeRequest());
        await act(async () => {
            await result.current('manifest.save', { path: '/foo.diaapp' });
        });
        expect(requestMock).toHaveBeenCalledWith('manifest.save', { path: '/foo.diaapp' });
    });

    it('resolves with the value returned by the bridge', async () => {
        requestMock.mockResolvedValueOnce({ data: 42 });
        const { result } = renderHook(() => useBridgeRequest<{ data: number }>());
        let response: { data: number } | undefined;
        await act(async () => {
            response = await result.current('some.topic');
        });
        expect(response).toEqual({ data: 42 });
    });
});

// ---------------------------------------------------------------------------
// useBridgeRequest — iframe path
// ---------------------------------------------------------------------------

describe('useBridgeRequest — iframe (postMessage)', () => {
    beforeEach(() => {
        delete (window as any).CluicheEditor;
        vi.useFakeTimers();
    });

    afterEach(() => {
        vi.useRealTimers();
        vi.clearAllMocks();
    });

    it('posts a message to window.parent with the correct shape', async () => {
        const postMessageSpy = vi.spyOn(window.parent, 'postMessage');
        const { result } = renderHook(() => useBridgeRequest());

        let promise!: Promise<unknown>;
        act(() => {
            promise = result.current('manifest.get', { filter: 'all' });
        });

        // Resolve it to avoid hanging — send back a fake response
        const callArgs = postMessageSpy.mock.calls[0];
        const sentPayload = (callArgs[0] as any).payload;
        act(() => {
            window.dispatchEvent(
                new MessageEvent('message', {
                    data: {
                        __diaResponse: true,
                        reqId: sentPayload.reqId,
                        result: { stages: [] },
                    },
                })
            );
        });

        const res = await promise;
        expect(res).toEqual({ stages: [] });
        expect(postMessageSpy).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: expect.objectContaining({
                    type: 'manifest.get',
                    data: { filter: 'all' },
                }),
            }),
            '*'
        );
        postMessageSpy.mockRestore();
    });

    it('rejects with a timeout error when no response arrives within 5000ms', async () => {
        const { result } = renderHook(() => useBridgeRequest());

        let promise!: Promise<unknown>;
        act(() => {
            promise = result.current('slow.request');
        });

        act(() => {
            vi.advanceTimersByTime(5001);
        });

        await expect(promise).rejects.toThrow("useBridgeRequest: 'slow.request' timed out after 5000ms");
    });

    it('resolves with the legacy DiaEditor_onResponse envelope', async () => {
        const postMessageSpy = vi.spyOn(window.parent, 'postMessage');
        const { result } = renderHook(() => useBridgeRequest());

        let promise!: Promise<unknown>;
        act(() => {
            promise = result.current('history.get');
        });

        const callArgs = postMessageSpy.mock.calls[0];
        const sentPayload = (callArgs[0] as any).payload;

        act(() => {
            window.dispatchEvent(
                new MessageEvent('message', {
                    data: {
                        payload: {
                            type: 'DiaEditor_onResponse',
                            reqId: sentPayload.reqId,
                            result: { canUndo: true },
                        },
                    },
                })
            );
        });

        const res = await promise;
        expect(res).toEqual({ canUndo: true });
        postMessageSpy.mockRestore();
    });
});
