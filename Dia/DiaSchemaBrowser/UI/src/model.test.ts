import { describe, it, expect } from 'vitest';
import {
    buildModel, fieldMatchScore, computeDupePairs, computeOrphans,
    visibleMessages, computeDiff, DUPE_THRESHOLD,
    type ScanResult, type ScanDocument, type Message,
} from './model';

// ── Fixtures modelled on the five real *.diagamemessages files in the repo ─────

const economy: ScanDocument = {
    file: 'Dia/DiaEconomy/Messages/economy_messages.diagamemessages',
    doc: {
        namespace: 'Dia::Economy::Messages',
        messages: [
            { id: 'PoolChangedEvent', router: 'broadcast', pass: 'primary', producers: ['EconomyBusAdapter'], consumers: [],
              fields: [ { name: 'instanceName', type: 'Dia::Core::StringCRC' }, { name: 'resourceName', type: 'Dia::Core::StringCRC' }, { name: 'newValue', type: 'float' }, { name: 'delta', type: 'float' } ] },
            { id: 'PoolReachedMaximumEvent', router: 'broadcast', pass: 'primary', producers: ['EconomyBusAdapter'], consumers: [],
              fields: [ { name: 'instanceName', type: 'Dia::Core::StringCRC' }, { name: 'resourceName', type: 'Dia::Core::StringCRC' } ] },
            { id: 'PoolReachedMinimumEvent', router: 'broadcast', pass: 'primary', producers: ['EconomyBusAdapter'], consumers: [],
              fields: [ { name: 'instanceName', type: 'Dia::Core::StringCRC' }, { name: 'resourceName', type: 'Dia::Core::StringCRC' } ] },
        ],
    },
};

const assetRuntime: ScanDocument = {
    file: 'Dia/DiaAssetRuntime/Messages/assetruntime_messages.diagamemessages',
    doc: {
        namespace: 'Dia::AssetRuntime',
        messages: [
            { id: 'AssetReadyEvent', router: 'broadcast', pass: 'primary', producers: ['AssetRuntimeBusAdapter'], consumers: [],
              fields: [ { name: 'assetId', type: 'Dia::Core::StringCRC' }, { name: 'resolvedPath', type: 'Dia::Core::Containers::String512' } ] },
            { id: 'AssetUnloadingEvent', router: 'broadcast', pass: 'primary', producers: ['AssetRuntimeBusAdapter'], consumers: [],
              fields: [ { name: 'assetId', type: 'Dia::Core::StringCRC' } ] },
            { id: 'AssetLoadFailedEvent', router: 'broadcast', pass: 'primary', producers: ['AssetRuntimeBusAdapter'], consumers: [],
              fields: [ { name: 'assetId', type: 'Dia::Core::StringCRC' } ] },
        ],
    },
};

const callout: ScanDocument = {
    file: 'Dia/DiaAICallout/Messages/callout_messages.diagamemessages',
    doc: {
        namespace: 'Dia::AICallout',
        messages: [
            { id: 'CalloutClaimedEvent', router: 'broadcast', pass: 'primary', producers: ['CalloutBusAdapter'], consumers: [],
              fields: [ { name: 'handle', type: 'CalloutHandle' }, { name: 'claimerEntityId', type: 'Dia::Core::StringCRC' } ] },
            { id: 'CalloutReleasedEvent', router: 'broadcast', pass: 'primary', producers: ['CalloutBusAdapter'], consumers: [],
              fields: [ { name: 'handle', type: 'CalloutHandle' }, { name: 'claimerEntityId', type: 'Dia::Core::StringCRC' } ] },
        ],
    },
};

const messageBus: ScanDocument = {
    file: 'Cluiche/CluicheTest/Stages/MessageBusTestStage/messagebus_test_messages.diagamemessages',
    doc: {
        namespace: 'CluicheTest::Messages',
        messages: [
            { id: 'NetworkPulseEvent', router: 'broadcast', pass: 'primary', capacity: 128, overflow: 'drop_oldest',
              producers: ['EmitterSystem'], consumers: ['ReceiverSystem'],
              fields: [ { name: 'sequenceId', type: 'uint32_t' }, { name: 'emitterId', type: 'EntityId' } ] },
            { id: 'DirectPingEvent', router: 'entity', pass: 'primary', producers: ['EmitterSystem'], consumers: ['ReceiverSystem'],
              fields: [ { name: 'senderId', type: 'EntityId' }, { name: 'pingId', type: 'uint32_t' } ] },
            { id: 'PongEvent', router: 'broadcast', pass: 'reaction', producers: ['ReceiverSystem'], consumers: ['EmitterSystem'],
              fields: [ { name: 'responderId', type: 'EntityId' } ] },
            { id: 'BurstEvent', router: 'broadcast', pass: 'primary', capacity: 32, producers: ['EmitterSystem'], consumers: [],
              fields: [ { name: 'count', type: 'uint32_t' } ] },
        ],
    },
};

function makeScan(docs: ScanDocument[]): ScanResult {
    const messageCount = docs.reduce((n, d) => n + (d.doc.messages?.length ?? 0), 0);
    return { documents: docs, fileCount: docs.length, messageCount };
}

const ALL = makeScan([economy, assetRuntime, callout, messageBus]);

describe('buildModel', () => {
    it('resolves every message as a union-graph node keyed by id', () => {
        const m = buildModel(ALL);
        expect(m.fileCount).toBe(4);
        expect(m.messages.length).toBe(12);
        expect(m.byId['NetworkPulseEvent']).toBeDefined();
    });

    it('applies capacity/overflow defaults', () => {
        const m = buildModel(ALL);
        expect(m.byId['PongEvent'].capacity).toBe(64); // default
        expect(m.byId['PongEvent'].overflow).toBe('assert'); // default
        expect(m.byId['NetworkPulseEvent'].capacity).toBe(128);
        expect(m.byId['NetworkPulseEvent'].overflow).toBe('drop_oldest');
    });

    it('normalises router/pass and preserves producers/consumers', () => {
        const m = buildModel(ALL);
        const net = m.byId['NetworkPulseEvent'];
        expect(net.router).toBe('broadcast');
        expect(net.pass).toBe('primary');
        expect(net.producers).toEqual(['EmitterSystem']);
        expect(net.consumers).toEqual(['ReceiverSystem']);
        expect(m.byId['DirectPingEvent'].router).toBe('entity');
        expect(m.byId['PongEvent'].pass).toBe('reaction');
    });

    it('merges producers/consumers/fields for a message id repeated across files', () => {
        const dupA: ScanDocument = {
            file: 'a.diagamemessages',
            doc: { namespace: 'A', messages: [ { id: 'Shared', router: 'broadcast', pass: 'primary', producers: ['P1'], consumers: ['C1'], fields: [{ name: 'x', type: 'int' }] } ] },
        };
        const dupB: ScanDocument = {
            file: 'b.diagamemessages',
            doc: { namespace: 'B', messages: [ { id: 'Shared', router: 'broadcast', pass: 'primary', producers: ['P2'], consumers: ['C1', 'C2'], fields: [] } ] },
        };
        const m = buildModel(makeScan([dupA, dupB]));
        expect(m.messages.length).toBe(1);
        const s = m.byId['Shared'];
        expect(s.producers.sort()).toEqual(['P1', 'P2']);
        expect(s.consumers.sort()).toEqual(['C1', 'C2']);
        expect(s.files.length).toBe(2);
    });

    it('tolerates empty / missing scan input', () => {
        expect(buildModel(null).messages).toEqual([]);
        expect(buildModel(undefined).messages).toEqual([]);
        expect(buildModel({ documents: [], fileCount: 0, messageCount: 0 }).messages).toEqual([]);
    });
});

describe('fieldMatchScore', () => {
    it('returns 1 for identical payloads', () => {
        const f = [{ name: 'a', type: 'int', notes: '' }, { name: 'b', type: 'float', notes: '' }];
        expect(fieldMatchScore(f, f)).toBe(1);
    });
    it('returns 0 for two empty payloads (avoids flagging field-less messages)', () => {
        expect(fieldMatchScore([], [])).toBe(0);
    });
    it('computes the union-based fraction with name+type matching', () => {
        const a = [{ name: 'x', type: 'int', notes: '' }, { name: 'y', type: 'int', notes: '' }];
        const b = [{ name: 'x', type: 'int', notes: '' }, { name: 'z', type: 'int', notes: '' }];
        // union {x,y,z}=3, matched {x}=1 -> 1/3
        expect(fieldMatchScore(a, b)).toBeCloseTo(1 / 3, 5);
    });
    it('does not count a same-named field with a different type', () => {
        const a = [{ name: 'x', type: 'int', notes: '' }];
        const b = [{ name: 'x', type: 'float', notes: '' }];
        expect(fieldMatchScore(a, b)).toBe(0);
    });
});

describe('computed duplicate detection', () => {
    it('flags the three real 100%-identical structural pairs', () => {
        const m = buildModel(ALL);
        const key = (p: { a: string; b: string }) => [p.a, p.b].sort().join('|');
        const keys = m.dupePairs.map(key);
        expect(keys).toContain('PoolReachedMaximumEvent|PoolReachedMinimumEvent');
        expect(keys).toContain('AssetLoadFailedEvent|AssetUnloadingEvent');
        expect(keys).toContain('CalloutClaimedEvent|CalloutReleasedEvent');
        m.dupePairs
            .filter((p) => key(p) !== 'PoolChangedEvent|PoolReachedMaximumEvent')
            .forEach((p) => expect(p.score).toBeGreaterThanOrEqual(DUPE_THRESHOLD));
    });

    it('populates each message dupes list bidirectionally', () => {
        const m = buildModel(ALL);
        expect(m.byId['AssetUnloadingEvent'].dupes).toContain('AssetLoadFailedEvent');
        expect(m.byId['AssetLoadFailedEvent'].dupes).toContain('AssetUnloadingEvent');
    });

    it('does not flag structurally different messages', () => {
        const m = buildModel(ALL);
        expect(m.byId['NetworkPulseEvent'].dupes).toHaveLength(0);
        expect(m.byId['BurstEvent'].dupes).toHaveLength(0);
    });

    it('marks same-subscribers pairs', () => {
        const pairs = computeDupePairs(buildModel(ALL).messages);
        const asset = pairs.find((p) => [p.a, p.b].sort().join('|') === 'AssetLoadFailedEvent|AssetUnloadingEvent');
        expect(asset?.sameSubscribers).toBe(true); // both have empty consumers
    });
});

describe('bidirectional orphan analysis', () => {
    it('lists produced-never-consumed messages', () => {
        const o = computeOrphans(buildModel(ALL).messages);
        expect(o.producedNeverConsumed).toContain('BurstEvent');
        expect(o.producedNeverConsumed).toContain('AssetReadyEvent');
        expect(o.producedNeverConsumed).not.toContain('NetworkPulseEvent'); // has a consumer
    });

    it('lists consumed-never-produced messages', () => {
        const synth: Message[] = [
            { id: 'GhostEvent', router: 'broadcast', pass: 'primary', capacity: 64, overflow: 'assert',
              producers: [], consumers: ['SomeSystem'], payload: [], files: [], namespaces: [], dupes: [] },
        ];
        const o = computeOrphans(synth);
        expect(o.consumedNeverProduced).toEqual(['GhostEvent']);
    });

    it('lists isolated (no producer, no consumer) messages', () => {
        const synth: Message[] = [
            { id: 'LonelyEvent', router: 'broadcast', pass: 'primary', capacity: 64, overflow: 'assert',
              producers: [], consumers: [], payload: [], files: [], namespaces: [], dupes: [] },
        ];
        const o = computeOrphans(synth);
        expect(o.isolated).toEqual(['LonelyEvent']);
    });
});

describe('visibleMessages filtering', () => {
    it('filters by router / pass', () => {
        const msgs = buildModel(ALL).messages;
        expect(visibleMessages(msgs, 'entity', '').map((m) => m.id)).toEqual(['DirectPingEvent']);
        expect(visibleMessages(msgs, 'reaction', '').map((m) => m.id)).toEqual(['PongEvent']);
    });
    it('filters by dupes', () => {
        const msgs = buildModel(ALL).messages;
        const dupeIds = visibleMessages(msgs, 'dupes', '').map((m) => m.id).sort();
        expect(dupeIds).toEqual([
            'AssetLoadFailedEvent', 'AssetUnloadingEvent',
            'CalloutClaimedEvent', 'CalloutReleasedEvent',
            'PoolReachedMaximumEvent', 'PoolReachedMinimumEvent',
        ]);
    });
    it('filters by search substring (case-insensitive)', () => {
        const msgs = buildModel(ALL).messages;
        expect(visibleMessages(msgs, 'all', 'pool').map((m) => m.id).sort())
            .toEqual(['PoolChangedEvent', 'PoolReachedMaximumEvent', 'PoolReachedMinimumEvent']);
    });
});

describe('computeDiff', () => {
    it('classifies matched / left-only / right-only rows', () => {
        const m = buildModel(ALL);
        const diff = computeDiff(m.byId['PoolChangedEvent'], m.byId['PoolReachedMaximumEvent']);
        const byName = Object.fromEntries(diff.rows.map((r) => [r.name, r.kind]));
        expect(byName['instanceName']).toBe('match');
        expect(byName['resourceName']).toBe('match');
        expect(byName['newValue']).toBe('left');
        expect(byName['delta']).toBe('left');
    });
    it('classifies differing-type rows', () => {
        const a: Message = { id: 'A', router: 'broadcast', pass: 'primary', capacity: 64, overflow: 'assert',
            producers: [], consumers: [], payload: [{ name: 'x', type: 'int', notes: '' }], files: [], namespaces: [], dupes: [] };
        const b: Message = { ...a, id: 'B', payload: [{ name: 'x', type: 'float', notes: '' }] };
        const diff = computeDiff(a, b);
        expect(diff.rows[0].kind).toBe('differ');
    });
});
