// Generator for jobs.fixture.json. SYNTHETIC data that follows the jobs wire formats the game emits
// (C++ PackPerfWindowJobs / PackPerfJobsRecords / PerfTimingPublisher BuildJobs): base64 little-endian,
// frameAgg owner-major (owner * bucketCount + bucket), count saturating at 65535, busyMs / worstWaitMs in 0.01 ms.
//
// The `steps` array is FIXTURE ONLY: the live game sends `steps: []` today (DiaDeferredStep is not built).
// Regenerate: WRITE_JOBS_FIXTURE=1 npx vite-node src/model/__fixtures__/makeJobsFixture.ts

import { writeFileSync } from 'node:fs';

function enc(bytes: Buffer): string { return bytes.toString('base64'); }

export function encodeU16LE(values: ArrayLike<number>): string {
    const b = Buffer.alloc(values.length * 2);
    for (let i = 0; i < values.length; i++) b.writeUInt16LE(Math.min(65535, Math.max(0, Math.round(values[i]))), i * 2);
    return enc(b);
}
export function encodeU32LE(values: ArrayLike<number>): string {
    const b = Buffer.alloc(values.length * 4);
    for (let i = 0; i < values.length; i++) b.writeUInt32LE(values[i] >>> 0, i * 4);
    return enc(b);
}
export function encodeU64LE(values: ArrayLike<number>): string {
    const b = Buffer.alloc(values.length * 8);
    for (let i = 0; i < values.length; i++) b.writeBigUInt64LE(BigInt(values[i]), i * 8);
    return enc(b);
}

export const kOwners = [
    { crc: 0x5ea71001, name: 'weather' },
    { crc: 0x5ea71002, name: 'ecology' },
    { crc: 0x5ea71003, name: 'PathfindingModule' },
    { crc: 0, name: '(other)' },
];

const kBase = 5_000_000_000_000; // 5000 s on the steady clock, ns
const kKeyNs = 1_000_000_000;

export interface FixtureStep {
    unit: string; channel: string; kickKey: number; applyKey: number;
    kick: number; firstStart: number; ready: number; apply: number;
    stallMs: number; cpuMs: number; capWaitMs: number; workerWaitMs: number; ranges: number;
    captureMs: number; applyMs: number;
}

interface RawRec { startNs: number; durNs: number; owner: number; tag: number; waitUs: number; worker: number; range: number; rangeCount: number; flags: number }

/** Builds one step: all ranges submitted together after a 2 ms capture, run on `lanes` workers back to back. */
function makeStep(channel: string, ownerCrc: number, kickKey: number, k: number, ranges: number, lanes: number, durMs: number): { step: FixtureStep; recs: RawRec[] } {
    const kick = kBase + kickKey * kKeyNs + 2_000_000;
    const submit = kick + 2_000_000;
    const firstStart = submit + 1_000_000;
    const apply = kBase + (kickKey + k) * kKeyNs;
    const recs: RawRec[] = [];
    let capWaitUs = 0;
    let workerWaitUs = 0;
    let ready = 0;
    for (let i = 0; i < ranges; i++) {
        const slot = Math.floor(i / lanes);
        const start = firstStart + slot * durMs * 1e6;
        const dur = Math.round(durMs * 1e6 * (0.95 + (i % 3) * 0.05));
        const workerWaitNs = 1_000_000; // every range waits 1 ms for a free worker
        const capWaitNs = start - submit - workerWaitNs;
        capWaitUs += Math.round(capWaitNs / 1000);
        workerWaitUs += Math.round(workerWaitNs / 1000);
        recs.push({
            startNs: start, durNs: dur, owner: ownerCrc, tag: 1, waitUs: Math.round((start - submit) / 1000),
            worker: (i % lanes) + (channel === 'ecology' ? 4 : 0), range: i, rangeCount: ranges, flags: 0x01,
        });
        ready = Math.max(ready, start + dur);
    }
    const step: FixtureStep = {
        unit: 'Sim', channel, kickKey, applyKey: kickKey + k, kick, firstStart, ready, apply,
        stallMs: Math.max(0, (ready - apply) / 1e6),
        cpuMs: recs.reduce((a, r) => a + r.durNs, 0) / 1e6,
        capWaitMs: capWaitUs / 1000, workerWaitMs: workerWaitUs / 1000, ranges,
        captureMs: 1.9, applyMs: 1.4,
    };
    return { step, recs };
}

export function makeJobsFixture(): Record<string, unknown> {
    const weather = makeStep('weather', kOwners[0].crc, 3, 1, 4, 4, 52);   // ready well before apply
    const ecology = makeStep('ecology', kOwners[1].crc, 4, 2, 8, 2, 520);  // 2 workers (cap), late
    const weather2 = makeStep('weather', kOwners[0].crc, 5, 1, 4, 4, 52);
    const steps = [weather.step, ecology.step, weather2.step];
    // a pathfinding job inside the ecology run window
    const path: RawRec = { startNs: ecology.step.firstStart + 100_000_000, durNs: 4_000_000, owner: kOwners[2].crc, tag: 7, waitUs: 300, worker: 6, range: 0, rangeCount: 1, flags: 0 };
    const recs = [...weather.recs, ...ecology.recs, ...weather2.recs, path].sort((a, b) => a.startNs - b.startNs);

    // Sim unit: 30 Hz, 200 ms buckets = 6 frames, 10 s = 50 buckets.
    const hz = 30, fpb = 6, bucketCount = 50, firstFrame = 1500;
    const n = kOwners.length;
    const count = new Array(n * bucketCount).fill(0);
    const busy = new Array(n * bucketCount).fill(0);
    const wait = new Array(n * bucketCount).fill(0);
    for (let b = 0; b < bucketCount; b++) {
        const weatherOn = b % 5 < 2 ? 1 : 0;      // weather runs in bursts
        const ecoOn = b >= 20 && b < 40 ? 1 : 0;
        busy[0 * bucketCount + b] = weatherOn * 150 * 100;   // 150 ms per bucket, 0.01 ms units
        count[0 * bucketCount + b] = weatherOn * 4;
        busy[1 * bucketCount + b] = ecoOn * 380 * 100;
        count[1 * bucketCount + b] = ecoOn * 2;
        busy[2 * bucketCount + b] = (3 + (b % 4)) * 100;
        count[2 * bucketCount + b] = 2 + (b % 3);
        wait[2 * bucketCount + b] = (b % 7) * 25;
        busy[3 * bucketCount + b] = 25;
        count[3 * bucketCount + b] = 1;
    }
    // saturation sample: one busy cell at the u16 ceiling (655.35 ms)
    busy[2 * bucketCount + 49] = 65535;

    const window = {
        bucketMs: 200, perJobRecording: true,
        units: [
            {
                unit: 'Sim', unitCrc: 0x1a2b3c02, perJobRecording: true, hz, firstFrame, framesPerBucket: fpb, bucketCount,
                owners: kOwners,
                frameAgg: { count: encodeU16LE(count), busyMs: encodeU16LE(busy), worstWaitMs: encodeU16LE(wait) },
            },
            {
                unit: 'Main', unitCrc: 0x1a2b3c01, perJobRecording: false, hz: 60, firstFrame: 0, framesPerBucket: 12, bucketCount: 0,
                owners: [], frameAgg: { count: '', busyMs: '', worstWaitMs: '' },
            },
        ],
        steps,
    };

    const timing = {
        jobs: {
            units: [{
                unit: 'Sim',
                owners: [
                    { name: 'weather', crc: kOwners[0].crc, count: 40, busyMs: 600, waitP50Ms: 0.4, waitP95Ms: 1.5, worstWaitMs: 3 },
                    { name: 'ecology', crc: kOwners[1].crc, count: 16, busyMs: 3800, waitP50Ms: 400, waitP95Ms: 1500, worstWaitMs: 1560 },
                    { name: 'PathfindingModule', crc: kOwners[2].crc, count: 60, busyMs: 30, waitP50Ms: 0.1, waitP95Ms: 0.8, worstWaitMs: 2.5 },
                ],
                pool: { capUse: 0, timeAtCapMs: 0, peakQueueDepth: 0, inlineFallbacks: 3, inboxDrops: 1 },
            }],
        },
        unavailable: ['cap_use', 'peak_queue_depth', 'time_at_cap'],
    };

    const raw = {
        v: 1, unit: 'Sim', unitCrc: 0x1a2b3c02,
        fromNs: weather.step.kick, toNs: weather2.step.apply,
        perJobRecording: true, truncated: false, count: recs.length,
        startNs: encodeU64LE(recs.map((r) => r.startNs)),
        durNs: encodeU32LE(recs.map((r) => r.durNs)),
        owner: encodeU32LE(recs.map((r) => r.owner)),
        tag: encodeU32LE(recs.map((r) => r.tag)),
        waitUs: encodeU32LE(recs.map((r) => r.waitUs)),
        worker: encodeU16LE(recs.map((r) => r.worker)),
        range: encodeU16LE(recs.map((r) => r.range)),
        rangeCount: encodeU16LE(recs.map((r) => r.rangeCount)),
        flags: encodeU16LE(recs.map((r) => r.flags)),
    };

    return {
        _note: 'SYNTHETIC fixture generated by makeJobsFixture.ts. window.steps and the raw records are fixture-only; the live game sends steps: [] today (DiaDeferredStep not built).',
        window, timing, raw,
    };
}

if (process.env.WRITE_JOBS_FIXTURE === '1') {
    writeFileSync(new URL('./jobs.fixture.json', import.meta.url), JSON.stringify(makeJobsFixture()) + '\n');
}
