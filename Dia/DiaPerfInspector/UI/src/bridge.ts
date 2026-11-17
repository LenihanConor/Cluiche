// CEF bridge utilities for DiaPerfInspector.
// Requests go to C++ via window.parent.postMessage; pushes arrive as { __dia: true, topic, data }.

import type { PerfQueryReply } from './types';

const REQUEST_TIMEOUT_MS = 5000;

export const kTimingTopic = 'perf_inspector.timing';
export const kTripTopic = 'perf_inspector.trip';
export const kQueryResultTopic = 'perf_inspector.query_result';
export const kConnectionPrefix = 'perf_inspector';

export function bridgeRequest(type: string, data: object = {}): Promise<unknown> {
    return new Promise((resolve) => {
        const reqId = `req_${Date.now()}_${Math.random().toString(36).slice(2)}`;

        const handler = (e: MessageEvent) => {
            const p = e.data?.payload ?? e.data;
            const isShellReply = e.data?.__diaResponse === true && e.data?.reqId === reqId;
            const isLegacyReply = p?.type === 'DiaEditor_onResponse' && p?.reqId === reqId;
            if (isShellReply || isLegacyReply) {
                clearTimeout(timer);
                window.removeEventListener('message', handler);
                resolve(isShellReply ? e.data.result : p.result);
            }
        };

        const timer = setTimeout(() => {
            window.removeEventListener('message', handler);
            // eslint-disable-next-line no-console
            console.error(`[DiaPerfInspector] bridgeRequest('${type}') timed out after ${REQUEST_TIMEOUT_MS}ms`);
            resolve({ ok: false, error: `bridgeRequest timeout: '${type}'` });
        }, REQUEST_TIMEOUT_MS);

        window.addEventListener('message', handler);
        window.parent.postMessage({ __diaFromFrame: true, payload: { type, reqId, data } }, '*');
    });
}

/** Subscribes to a pushed topic. Returns an unsubscribe function. */
export function onTopic(topic: string, cb: (data: unknown) => void): () => void {
    const handler = (e: MessageEvent) => {
        const env = e.data;
        if (env && env.__dia === true && env.topic === topic) cb(env.data);
    };
    window.addEventListener('message', handler);
    return () => window.removeEventListener('message', handler);
}

interface QueryResultPush<T> {
    queryId: number;
    success: boolean;
    result: T;
}

/**
 * Relays a query through C++: the request returns a queryId, and the game reply arrives
 * later on the query_result topic tagged with that id.
 */
function relayQuery<T>(type: string, data: object): Promise<PerfQueryReply<T>> {
    return new Promise((resolve) => {
        let queryId: number | null = null;
        let early: QueryResultPush<T> | null = null;
        let off: () => void = () => {};
        let done = false;

        const finish = (reply: PerfQueryReply<T>) => {
            if (done) return;
            done = true;
            clearTimeout(timer);
            off();
            resolve(reply);
        };
        const fail = (error: string) => finish({ success: false, result: { error } as unknown as T });

        const timer = setTimeout(() => fail(`query timeout: ${type}`), REQUEST_TIMEOUT_MS);

        off = onTopic(kQueryResultTopic, (d) => {
            const n = d as QueryResultPush<T>;
            if (queryId === null) { early = n; return; }
            if (n.queryId === queryId) finish({ success: n.success, result: n.result });
        });

        bridgeRequest(type, data).then((res) => {
            const r = res as { ok?: boolean; queryId?: number; error?: string } | null;
            if (r?.ok !== true || typeof r.queryId !== 'number') {
                fail(r?.error ?? 'request failed');
                return;
            }
            queryId = r.queryId;
            const e = early as QueryResultPush<T> | null;
            if (e && e.queryId === queryId) finish({ success: e.success, result: e.result });
        });
    });
}

export function queryWindow<T = unknown>(seconds: number): Promise<PerfQueryReply<T>> {
    return relayQuery<T>('perf_inspector.query_window', { seconds });
}

export function queryFrame<T = unknown>(unit: string, frame: number): Promise<PerfQueryReply<T>> {
    return relayQuery<T>('perf_inspector.query_frame', { unit, frame });
}

/**
 * Raw per-job records for one unit over a steady-clock ns range (tier 3, perf.jobs). fromNs / toNs go over the wire as JSON
 * numbers (the game requires integers), so they are exact only below 2^53 ns (about 104 days of steady-clock uptime).
 */
export function queryJobs<T = unknown>(unit: string, fromNs: number, toNs: number): Promise<PerfQueryReply<T>> {
    return relayQuery<T>('perf_inspector.query_jobs', { unit, fromNs, toNs });
}

/** The most recent cross-thread trace chain (perf.chain). The game replies {available:false, reason} when tracing is off. */
export function queryChain<T = unknown>(): Promise<PerfQueryReply<T>> {
    return relayQuery<T>('perf_inspector.query_chain', {});
}

/**
 * Sends a whitelisted perf command (perf.pause, perf.resume, perf.resume_all) to the game through the
 * plugin relay. The plugin rejects any other command name.
 */
export function sendPerfCommand<T = unknown>(command: string, args: object = {}): Promise<PerfQueryReply<T>> {
    return relayQuery<T>('perf_inspector.command', { command, args });
}
