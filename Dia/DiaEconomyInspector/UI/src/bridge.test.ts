import { describe, it, expect, vi, afterEach } from 'vitest';
import { bridgeRequest } from './bridge';

describe('bridgeRequest', () => {
    afterEach(() => { vi.useRealTimers(); vi.restoreAllMocks(); });

    function captureReqId(): { get: () => string } {
        let id = '';
        vi.spyOn(window.parent, 'postMessage').mockImplementation(((msg: any) => {
            id = msg.payload.reqId;
        }) as any);
        return { get: () => id };
    }

    it('sends a non-empty reqId and resolves on the shell { __diaResponse } reply', async () => {
        const cap = captureReqId();
        const p = bridgeRequest('x.get', {});
        expect(cap.get()).toMatch(/^req_/);
        window.dispatchEvent(new MessageEvent('message', {
            data: { __diaResponse: true, reqId: cap.get(), result: { ok: true, v: 7 } },
        }));
        await expect(p).resolves.toEqual({ ok: true, v: 7 });
    });

    it('ignores a reply for a different reqId', async () => {
        vi.useFakeTimers();
        const cap = captureReqId();
        const p = bridgeRequest('x.get', {});
        window.dispatchEvent(new MessageEvent('message', {
            data: { __diaResponse: true, reqId: 'other', result: { v: 1 } },
        }));
        expect(cap.get()).not.toBe('other');
        vi.advanceTimersByTime(5001);
        await expect(p).resolves.toMatchObject({ ok: false });
    });

    it('still accepts the legacy DiaEditor_onResponse shape', async () => {
        const cap = captureReqId();
        const p = bridgeRequest('x.get', {});
        window.dispatchEvent(new MessageEvent('message', {
            data: { payload: { type: 'DiaEditor_onResponse', reqId: cap.get(), result: { v: 2 } } },
        }));
        await expect(p).resolves.toEqual({ v: 2 });
    });
});
