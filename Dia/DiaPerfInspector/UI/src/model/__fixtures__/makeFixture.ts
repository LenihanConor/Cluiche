// Generator for perfWindow.fixture.json. SYNTHETIC data that follows the perf.window wire format
// (see C++ PerfWindowPacker): base64 of little-endian uint16 in 0.01 ms units, module-major,
// `flags` omitted when no flag is set. Regenerate: WRITE_PERF_FIXTURE=1 npx vite-node src/model/__fixtures__/makeFixture.ts

import { writeFileSync } from 'node:fs';

export interface FixtureModule { crc: number; name: string }

export interface FixtureUnitSpec {
    unit: string;
    unitCrc: number;
    hz: number;
    firstEntry: number;
    seconds: number;
    modules: FixtureModule[];
    /** Typical per-module cost in ms. */
    base: number[];
    /** entry offset -> tick multiplier (spike). */
    spikes?: Record<number, number>;
    /** entry offsets whose tick is clamped (65535, flag 0x01). */
    clamped?: number[];
    /** [startOffset, length] gap filler run (flag 0x02, values 0). */
    gap?: [number, number];
    /** entry offsets where module sum exceeds tick (nested/parallel scopes). */
    overAttributed?: number[];
}

export function encodeU16(values: ArrayLike<number>): string {
    const bytes = Buffer.alloc(values.length * 2);
    for (let i = 0; i < values.length; i++) bytes.writeUInt16LE(values[i], i * 2);
    return bytes.toString('base64');
}

export function encodeU8(values: ArrayLike<number>): string {
    return Buffer.from(Uint8Array.from(Array.from(values))).toString('base64');
}

function lcg(seed: number): () => number {
    let s = seed >>> 0;
    return () => {
        s = (Math.imul(s, 1664525) + 1013904223) >>> 0;
        return s / 4294967296;
    };
}

const q = (ms: number): number => Math.min(65535, Math.max(0, Math.round(ms * 100)));

export function makeUnit(spec: FixtureUnitSpec): Record<string, unknown> {
    const n = Math.round(spec.seconds * spec.hz);
    const rnd = lcg(spec.unitCrc);
    const m = spec.modules.length;
    const unitMs = new Uint16Array(n);
    const moduleMs = new Uint16Array(n * m);
    const flags = new Uint8Array(n);
    let anyFlag = false;
    for (let i = 0; i < n; i++) {
        const mult = spec.spikes?.[i] ?? 1;
        let sum = 0;
        for (let k = 0; k < m; k++) {
            const v = spec.base[k] * (0.85 + rnd() * 0.3) * mult;
            moduleMs[k * n + i] = q(v);
            sum += moduleMs[k * n + i] / 100;
        }
        let tick = sum + 0.2 + rnd() * 0.6; // small unattributed remainder
        if (spec.overAttributed?.includes(i)) tick = sum * 0.9;
        unitMs[i] = q(tick);
    }
    for (const c of spec.clamped ?? []) {
        unitMs[c] = 65535;
        flags[c] |= 0x01;
        anyFlag = true;
    }
    if (spec.gap) {
        for (let i = spec.gap[0]; i < spec.gap[0] + spec.gap[1]; i++) {
            unitMs[i] = 0;
            for (let k = 0; k < m; k++) moduleMs[k * n + i] = 0;
            flags[i] |= 0x02;
            anyFlag = true;
        }
    }
    const out: Record<string, unknown> = {
        unit: spec.unit, unitCrc: spec.unitCrc, hz: spec.hz, firstEntry: spec.firstEntry, count: n,
        modules: spec.modules,
        unitMs: encodeU16(unitMs),
        moduleMs: encodeU16(moduleMs),
    };
    if (anyFlag) out.flags = encodeU8(flags);
    return out;
}

export function makeWindow(seconds = 12): Record<string, unknown> {
    const mod = (crc: number, name: string): FixtureModule => ({ crc, name });
    const units = [
        makeUnit({
            unit: 'Main', unitCrc: 0x1a2b3c01, hz: 30, firstEntry: 9000, seconds,
            modules: [mod(0x11110001, 'Window'), mod(0x11110002, 'Input'), mod(0x11110003, 'Console'), mod(0x11110004, 'Debug'), mod(0x11110005, 'Logging')],
            base: [1.2, 0.6, 2.0, 1.1, 0.8],
            spikes: { 150: 6 }, clamped: [200],
        }),
        makeUnit({
            unit: 'Sim', unitCrc: 0x1a2b3c02, hz: 30, firstEntry: 9000, seconds,
            modules: [mod(0x22220001, 'Physics'), mod(0x22220002, 'AI'), mod(0x22220003, 'World'), mod(0x22220004, 'Weather'), mod(0x22220005, 'Ecology')],
            base: [4.0, 5.0, 6.0, 3.0, 2.5],
            spikes: { 60: 2.2, 61: 2.4, 62: 2.3, 280: 3 }, gap: [100, 10],
        }),
        makeUnit({
            unit: 'Render', unitCrc: 0x1a2b3c03, hz: 60, firstEntry: 18000, seconds,
            modules: [mod(0x33330001, 'Scene'), mod(0x33330002, 'Sprites'), mod(0x33330003, 'Present')],
            base: [2.0, 3.0, 1.5],
            spikes: { 400: 4 }, overAttributed: [10, 11],
        }),
    ];
    return { v: 1, seconds, units, markers: [], trips: [], truncated: false };
}

const entry = process.env.WRITE_PERF_FIXTURE === '1';
if (entry) {
    const out = new URL('./perfWindow.fixture.json', import.meta.url);
    writeFileSync(out, JSON.stringify(makeWindow()) + '\n');
}
