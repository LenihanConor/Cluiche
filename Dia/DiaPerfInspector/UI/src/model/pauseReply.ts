// Interprets replies to perf.pause / perf.resume / perf.resume_all. No React or DOM imports.

import { readReply } from './reply';

export type PauseReply =
    | { kind: 'ok'; paused: boolean | null }
    | { kind: 'unsupported'; reason: string }
    | { kind: 'error'; message: string };

export const kUnsupportedReason = 'Pausing needs a Debug build of the game: these commands do not exist in Release.';

const kUnknownRe = /unknown command|command not found/i;

export function interpretPauseReply(raw: unknown): PauseReply {
    const r = readReply(raw);
    if (!r) return { kind: 'error', message: 'unusable reply' };
    const b = r.body;
    const text = [b.message, b.error].filter((x): x is string => typeof x === 'string').join(' ');
    if (kUnknownRe.test(text)) return { kind: 'unsupported', reason: kUnsupportedReason };
    if (typeof b.error === 'string') return { kind: 'error', message: b.error };
    if (!r.success) return { kind: 'error', message: text || 'command failed' };
    return { kind: 'ok', paused: typeof b.paused === 'boolean' ? b.paused : null };
}
