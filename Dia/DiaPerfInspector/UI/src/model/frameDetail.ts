// Parses a relayed `perf.frame` reply. No React or DOM imports.

import type { FrameDetailData } from '../types';
import { readReply } from './reply';

export type FrameParse = { kind: 'ok'; data: FrameDetailData } | { kind: 'error'; message: string };

const err = (message: string): FrameParse => ({ kind: 'error', message });

export function parseFrameReply(raw: unknown): FrameParse {
    const r = readReply(raw);
    if (!r) return err('unusable perf.frame reply');
    const b = r.body;
    if (typeof b.error === 'string') return err(b.error);
    if (!r.success) return err('perf.frame failed');
    if (b.v !== 1 || !Array.isArray(b.modules)) return err('unusable perf.frame reply');

    const modules = (b.modules as unknown[]).map((m) => {
        const o = (m ?? {}) as { crc?: unknown; name?: unknown; ms?: unknown };
        return {
            crc: typeof o.crc === 'number' ? o.crc : 0,
            name: typeof o.name === 'string' ? o.name : '?',
            ms: typeof o.ms === 'number' ? o.ms : 0,
        };
    });

    const s = (b.sections ?? {}) as { available?: unknown; reason?: unknown; sections?: unknown };
    const available = s.available === true;
    const sections = available && Array.isArray(s.sections)
        ? (s.sections as { name?: unknown; ms?: unknown }[]).map((x) => ({
            name: typeof x.name === 'string' ? x.name : '?',
            ms: typeof x.ms === 'number' ? x.ms : 0,
        }))
        : [];

    return {
        kind: 'ok',
        data: {
            unit: typeof b.unit === 'string' ? b.unit : '',
            frame: typeof b.frame === 'number' ? b.frame : 0,
            unitMs: typeof b.unitMs === 'number' ? b.unitMs : 0,
            flags: typeof b.flags === 'number' ? b.flags : 0,
            modules,
            sectionsUnavailableReason: available ? null : typeof s.reason === 'string' ? s.reason : 'not reported by the game',
            sections,
        },
    };
}
