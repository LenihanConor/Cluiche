// Tolerant reading of the relayed perf.chain reply. Never throws: anything unexpected becomes an unavailable result.
import { readReply } from './reply';
import type { ChainData, ChainEdgeData, ChainStageData } from '../types';

export interface ChainReplyResult {
    /** null when the game reports no chain (or the reply is unusable). */
    chain: ChainData | null;
    /** Why there is no chain (the game's reason when it gave one); null when a chain is present. */
    reason: string | null;
}

const isStr = (v: unknown): v is string => typeof v === 'string';
const isNum = (v: unknown): v is number => typeof v === 'number' && Number.isFinite(v);

function parseStage(raw: unknown): ChainStageData | null {
    if (!raw || typeof raw !== 'object') return null;
    const s = raw as Record<string, unknown>;
    if (!isStr(s.id) || !isStr(s.unit) || !isStr(s.name) || !isNum(s.startMs) || !isNum(s.endMs)) return null;
    return { id: s.id, unit: s.unit, name: s.name, startMs: s.startMs, endMs: s.endMs };
}

function parseEdge(raw: unknown): ChainEdgeData | null {
    if (!raw || typeof raw !== 'object') return null;
    const e = raw as Record<string, unknown>;
    if (!isStr(e.stream) || !isStr(e.fromId) || !isStr(e.toId)) return null;
    const edge: ChainEdgeData = { stream: e.stream, fromId: e.fromId, toId: e.toId };
    if (isNum(e.bytes)) edge.bytes = e.bytes;
    if (isNum(e.ratePerSec)) edge.ratePerSec = e.ratePerSec;
    if (isNum(e.dataAgeMs)) edge.dataAgeMs = e.dataAgeMs;
    if (isNum(e.writeCostMs)) edge.writeCostMs = e.writeCostMs;
    return edge;
}

export function parseChainReply(raw: unknown): ChainReplyResult {
    const reply = readReply(raw);
    if (!reply) return { chain: null, reason: 'no reply from the game' };
    const b = reply.body;
    if (!reply.success) {
        return { chain: null, reason: isStr(b.error) && b.error ? b.error : 'perf.chain query failed' };
    }
    if (b.available !== true) {
        return { chain: null, reason: isStr(b.reason) && b.reason ? b.reason : 'the game reports no chain' };
    }
    if (!Array.isArray(b.stages)) return { chain: null, reason: 'malformed chain reply' };
    const stages = b.stages.map(parseStage).filter((s): s is ChainStageData => s !== null);
    const edges = (Array.isArray(b.edges) ? b.edges : []).map(parseEdge).filter((e): e is ChainEdgeData => e !== null);
    return { chain: { name: isStr(b.name) ? b.name : 'chain', stages, edges }, reason: null };
}
