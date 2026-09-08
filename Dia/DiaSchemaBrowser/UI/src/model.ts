// model.ts — the SINGLE place where .diagamemessages documents are interpreted.
//
// The C++ side only discovers files and JSON-parses them (syntactic parse).
// Everything semantic — union-graph construction, structural-duplicate scoring,
// and bidirectional orphan analysis — lives here so the logic is not duplicated
// across C++ and TS. Pure functions, unit-tested in model.test.ts.

export type Router = 'broadcast' | 'entity';
export type Pass = 'primary' | 'reaction';

export interface Field {
    name: string;
    type: string;
    notes: string;
}

// Raw shapes as they arrive from the C++ `schema.scan` request.
export interface RawField {
    name?: string;
    type?: string;
    notes?: string;
}

export interface RawMessage {
    id?: string;
    router?: string;
    pass?: string;
    capacity?: number;
    overflow?: string;
    producers?: string[];
    consumers?: string[];
    fields?: RawField[];
}

export interface RawDoc {
    schema?: string;
    namespace?: string;
    includes?: string[];
    messages?: RawMessage[];
}

export interface ScanDocument {
    file: string;
    doc: RawDoc;
}

export interface ScanResult {
    documents: ScanDocument[];
    fileCount: number;
    messageCount: number;
}

// A fully-resolved message node in the union graph. Messages sharing an `id`
// across files are merged into a single node (id is the sole cross-file join key).
export interface Message {
    id: string;
    router: Router;
    pass: Pass;
    capacity: number;
    overflow: string;
    producers: string[];
    consumers: string[];
    payload: Field[];
    files: string[];
    namespaces: string[];
    dupes: string[]; // COMPUTED — ids of structural-duplicate candidates
}

export interface DupePair {
    a: string;
    b: string;
    score: number; // 0..1 field-match fraction
    sameSubscribers: boolean;
}

export interface OrphanAnalysis {
    // Produced but nobody consumes it (emitted into the void).
    producedNeverConsumed: string[];
    // Consumed but nobody produces it (a listener with no source).
    consumedNeverProduced: string[];
    // Declared with neither producers nor consumers.
    isolated: string[];
}

export interface SchemaModel {
    messages: Message[];
    byId: Record<string, Message>;
    dupePairs: DupePair[];
    orphans: OrphanAnalysis;
    fileCount: number;
    messageCount: number;
}

// A pair is flagged as a structural duplicate candidate when its payloads share
// at least this fraction of (name AND type)-matching fields.
export const DUPE_THRESHOLD = 0.6;

const DEFAULT_CAPACITY = 64;
const DEFAULT_OVERFLOW = 'assert';

function normRouter(v: string | undefined): Router {
    return v === 'entity' ? 'entity' : 'broadcast';
}

function normPass(v: string | undefined): Pass {
    return v === 'reaction' ? 'reaction' : 'primary';
}

function toField(f: RawField): Field {
    return {
        name: f.name ?? '',
        type: f.type ?? '',
        notes: f.notes ?? '',
    };
}

function uniqueMerge(target: string[], incoming: string[] | undefined): void {
    if (!incoming) return;
    for (const v of incoming) {
        if (v && !target.includes(v)) target.push(v);
    }
}

/**
 * Build the union graph model from a scan result. Messages are keyed by `id`;
 * repeated ids across files are merged (producers/consumers/fields unioned).
 */
export function buildModel(scan: ScanResult | null | undefined): SchemaModel {
    const byId: Record<string, Message> = {};

    const documents = scan?.documents ?? [];
    for (const entry of documents) {
        const file = entry.file ?? '';
        const ns = entry.doc?.namespace ?? '';
        const messages = entry.doc?.messages ?? [];

        for (const raw of messages) {
            const id = raw.id;
            if (!id) continue;

            let msg = byId[id];
            if (!msg) {
                msg = {
                    id,
                    router: normRouter(raw.router),
                    pass: normPass(raw.pass),
                    capacity: typeof raw.capacity === 'number' ? raw.capacity : DEFAULT_CAPACITY,
                    overflow: raw.overflow ?? DEFAULT_OVERFLOW,
                    producers: [],
                    consumers: [],
                    payload: (raw.fields ?? []).map(toField),
                    files: [],
                    namespaces: [],
                    dupes: [],
                };
                byId[id] = msg;
            } else if (msg.payload.length === 0 && raw.fields && raw.fields.length > 0) {
                // First definition may have omitted fields; adopt fields from a later one.
                msg.payload = raw.fields.map(toField);
            }

            uniqueMerge(msg.producers, raw.producers);
            uniqueMerge(msg.consumers, raw.consumers);
            if (file) uniqueMerge(msg.files, [file]);
            if (ns) uniqueMerge(msg.namespaces, [ns]);
        }
    }

    const messages = Object.values(byId).sort((a, b) => a.id.localeCompare(b.id));

    const dupePairs = computeDupePairs(messages);
    for (const pair of dupePairs) {
        byId[pair.a].dupes.push(pair.b);
        byId[pair.b].dupes.push(pair.a);
    }

    const orphans = computeOrphans(messages);

    return {
        messages,
        byId,
        dupePairs,
        orphans,
        fileCount: scan?.fileCount ?? documents.length,
        messageCount: scan?.messageCount ?? messages.length,
    };
}

/**
 * Fraction of the union of field names that match by BOTH name and type.
 * Returns 0 when both payloads are empty (avoids flagging every field-less message).
 */
export function fieldMatchScore(a: Field[], b: Field[]): number {
    const names = new Set<string>();
    a.forEach((f) => names.add(f.name));
    b.forEach((f) => names.add(f.name));
    if (names.size === 0) return 0;

    const aMap = new Map(a.map((f) => [f.name, f.type]));
    const bMap = new Map(b.map((f) => [f.name, f.type]));

    let matched = 0;
    names.forEach((n) => {
        if (aMap.has(n) && bMap.has(n) && aMap.get(n) === bMap.get(n)) matched++;
    });
    return matched / names.size;
}

function sameSubscribers(a: Message, b: Message): boolean {
    const sa = a.consumers.slice().sort().join(',');
    const sb = b.consumers.slice().sort().join(',');
    return sa === sb;
}

/** All distinct pairs whose structural similarity meets DUPE_THRESHOLD. */
export function computeDupePairs(messages: Message[]): DupePair[] {
    const pairs: DupePair[] = [];
    for (let i = 0; i < messages.length; i++) {
        for (let j = i + 1; j < messages.length; j++) {
            const a = messages[i];
            const b = messages[j];
            const score = fieldMatchScore(a.payload, b.payload);
            if (score >= DUPE_THRESHOLD) {
                pairs.push({
                    a: a.id,
                    b: b.id,
                    score,
                    sameSubscribers: sameSubscribers(a, b),
                });
            }
        }
    }
    return pairs;
}

/** Bidirectional orphan analysis at the message level. */
export function computeOrphans(messages: Message[]): OrphanAnalysis {
    const producedNeverConsumed: string[] = [];
    const consumedNeverProduced: string[] = [];
    const isolated: string[] = [];

    for (const m of messages) {
        const hasProducers = m.producers.length > 0;
        const hasConsumers = m.consumers.length > 0;
        if (hasProducers && !hasConsumers) producedNeverConsumed.push(m.id);
        else if (!hasProducers && hasConsumers) consumedNeverProduced.push(m.id);
        else if (!hasProducers && !hasConsumers) isolated.push(m.id);
    }

    return { producedNeverConsumed, consumedNeverProduced, isolated };
}

// ── Role classification (shared by all views) ────────────────────────────────

export function producerIsAdapter(name: string): boolean {
    return name.includes('Adapter');
}

export function consumerIsComponent(router: Router, name: string): boolean {
    return router === 'entity' || name.endsWith('Comp');
}

export type WebNodeKind = 'system' | 'component' | 'adapter' | 'message';

export function nodeKindForName(name: string): WebNodeKind {
    if (name.includes('Adapter')) return 'adapter';
    if (name.endsWith('Comp')) return 'component';
    return 'system';
}

// ── Filtering (left panel + Dupes filter) ─────────────────────────────────────

export type FilterKind = 'all' | 'broadcast' | 'entity' | 'primary' | 'reaction' | 'dupes';

export function visibleMessages(messages: Message[], filter: FilterKind, search: string): Message[] {
    const q = search.toLowerCase();
    return messages.filter((m) => {
        if (q && !m.id.toLowerCase().includes(q)) return false;
        if (filter === 'broadcast') return m.router === 'broadcast';
        if (filter === 'entity') return m.router === 'entity';
        if (filter === 'primary') return m.pass === 'primary';
        if (filter === 'reaction') return m.pass === 'reaction';
        if (filter === 'dupes') return m.dupes.length > 0;
        return true;
    });
}

// ── Diff computation for the Schema Analysis tab ─────────────────────────────

export type DiffKind = 'match' | 'differ' | 'left' | 'right';

export interface DiffRow {
    name: string;
    aType: string | null;
    bType: string | null;
    kind: DiffKind;
}

export interface DupeDiff {
    otherId: string;
    score: number;
    sameSubscribers: boolean;
    rows: DiffRow[];
}

export function computeDiff(a: Message, b: Message): DupeDiff {
    const order: string[] = [];
    const seen = new Set<string>();
    const push = (n: string) => {
        if (!seen.has(n)) {
            seen.add(n);
            order.push(n);
        }
    };
    a.payload.forEach((f) => push(f.name));
    b.payload.forEach((f) => push(f.name));

    const aMap = new Map(a.payload.map((f) => [f.name, f.type]));
    const bMap = new Map(b.payload.map((f) => [f.name, f.type]));

    const rows: DiffRow[] = order.map((name) => {
        const at = aMap.has(name) ? aMap.get(name)! : null;
        const bt = bMap.has(name) ? bMap.get(name)! : null;
        let kind: DiffKind;
        if (at !== null && bt !== null) kind = at === bt ? 'match' : 'differ';
        else if (at !== null) kind = 'left';
        else kind = 'right';
        return { name, aType: at, bType: bt, kind };
    });

    return {
        otherId: b.id,
        score: fieldMatchScore(a.payload, b.payload),
        sameSubscribers: sameSubscribers(a, b),
        rows,
    };
}
