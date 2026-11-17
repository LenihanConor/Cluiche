// Shared tolerant reading of relayed game command replies ({success, result}). No React or DOM imports.

export interface ReplyBody {
    success: boolean;
    /** The command's own payload (unwrapped from `data` when the registry wrapped it). */
    body: Record<string, unknown>;
}

export function readReply(raw: unknown): ReplyBody | null {
    if (!raw || typeof raw !== 'object') return null;
    const r = raw as { success?: unknown; result?: unknown };
    const result = r.result && typeof r.result === 'object' ? (r.result as Record<string, unknown>) : null;
    if (!result) return null;
    const data = result.data;
    const body = data && typeof data === 'object' && result.v === undefined && result.error === undefined
        ? (data as Record<string, unknown>)
        : result;
    return { success: r.success === true, body };
}
