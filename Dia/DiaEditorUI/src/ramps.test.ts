import { describe, it, expect } from 'vitest';
import samplesJson from '../../DiaValueSchema/Testing/Conformance/ramps/samples.json';
import legendJson from '../../DiaValueSchema/Testing/Conformance/ramps/legend-ticks.json';
import palettesJson from '../../DiaValueSchema/Testing/Conformance/ramps/palettes.json';
import rampsFile from '../../DiaValueSchema/Data/ramps.json';
import {
  RampResolver,
  parseHexColour,
  rgba8ToHex,
  sampleRamp,
  buildLegend,
  rampT,
  type RampJson,
} from './ramps';
import type { ValueInfoLike } from './valueInfo';

const ramps = rampsFile.ramps as unknown as RampJson[];
const fixtureRamps = samplesJson.ramps as unknown as RampJson[];

const floatEntry = (extra: Partial<ValueInfoLike> = {}): ValueInfoLike => ({
  key: 'a.b', label: 'B', description: 'd', type: 'float', min: 0, max: 1, ...extra,
});
const enumEntry = (n: number, colours: Array<string | undefined> = []): ValueInfoLike => ({
  key: 'e.k', label: 'K', description: 'd', type: 'enum',
  values: Array.from({ length: n }, (_, i) => ({
    id: `v${i}`, label: `Value ${i}`, description: '', colour: colours[i],
  })),
});

function newResolver() {
  const warns: Array<[string, string]> = [];
  const r = new RampResolver(ramps, (k, m) => warns.push([k, m]));
  return { r, warns };
}

describe('ramps conformance', () => {
  it('fixture ramps match Data/ramps.json', () => {
    expect(fixtureRamps).toEqual(ramps);
  });

  it('parseHexColour', () => {
    expect(parseHexColour('#102030')).toBe(0x102030ff);
    expect(parseHexColour('#ABCDEF')).toBe(0xabcdefff);
    expect(parseHexColour('102030')).toBeNull();
    expect(parseHexColour('#12345')).toBeNull();
    expect(parseHexColour('#12345g')).toBeNull();
    expect(rgba8ToHex(0x102030ff)).toBe('#102030');
  });

  for (const s of samplesJson.samples) {
    it(`sample ${s.ramp} @ ${s.t}`, () => {
      const ramp = fixtureRamps.find(r => r.id === s.ramp)!;
      expect(rgba8ToHex(sampleRamp(ramp, s.t))).toBe(s.expectedHex);
      expect(sampleRamp(ramp, s.t) & 0xff).toBe(255);
    });
  }

  legendJson.cases.forEach((c, i) => {
    it(`legend case ${i}: ${c.min}..${c.max}`, () => {
      const { r } = newResolver();
      const unit = (c as { unit?: string }).unit;
      const entry = floatEntry({ min: c.min, max: c.max, ...(unit !== undefined ? { unit } : {}) });
      const lg = buildLegend(r, entry);
      expect(lg.unit).toBe(c.expectedUnit);
      expect(lg.ticks.length).toBe(c.expectedTicks.length);
      c.expectedTicks.forEach((t, j) => expect(lg.ticks[j]).toBeCloseTo(t, 12));
      expect(lg.isCategory).toBe(false);
    });
  });

  for (const c of palettesJson.cases) {
    it(`palette ${c.name}`, () => {
      const { r } = newResolver();
      const entry: ValueInfoLike = {
        key: 'e.k', label: 'K', description: 'd', type: 'enum',
        values: c.values.map(v => ({ id: v.id, label: v.label, description: '', colour: (v as { colour?: string }).colour })),
      };
      const pal = r.resolvePalette(entry);
      expect(pal.entries.map(e => rgba8ToHex(e.colour))).toEqual(c.expectedColours);
      expect(pal.wrapped).toBe(c.expectedWrapped);
      expect(pal.entries.map(e => e.id)).toEqual(c.values.map(v => v.id));
    });
  }
});

describe('ramps edge cases', () => {
  const grey = ramps.find(r => r.id === 'sequential.grey')!;

  it('clamps t outside [0,1] and NaN', () => {
    expect(sampleRamp(grey, -3)).toBe(sampleRamp(grey, 0));
    expect(sampleRamp(grey, 9)).toBe(sampleRamp(grey, 1));
    expect(sampleRamp(grey, NaN)).toBe(sampleRamp(grey, 0));
  });

  it('absent ramp -> grey, no warning, no fallback flag', () => {
    const { r, warns } = newResolver();
    const out = r.resolveRamp(floatEntry());
    expect(out.ramp.id).toBe('sequential.grey');
    expect(out.fallback).toBe(false);
    expect(out.min).toBe(0);
    expect(out.max).toBe(1);
    expect(warns.length).toBe(0);
  });

  it('missing ui object and non-string ramp', () => {
    const { r, warns } = newResolver();
    expect(r.resolveRamp(floatEntry({ ui: undefined })).ramp.id).toBe('sequential.grey');
    expect(r.resolveRamp(floatEntry({ ui: { ramp: 5 as unknown as string } })).ramp.id).toBe('sequential.grey');
    expect(warns.length).toBe(0);
  });

  it('known ramp is used', () => {
    const { r } = newResolver();
    const out = r.resolveRamp(floatEntry({ ui: { ramp: 'sequential.heat' } }));
    expect(out.ramp.id).toBe('sequential.heat');
    expect(out.fallback).toBe(false);
  });

  it('unknown ramp falls back with exactly one warning per key', () => {
    const { r, warns } = newResolver();
    const e = floatEntry({ ui: { ramp: 'nope' } });
    for (let i = 0; i < 4; i++) {
      const out = r.resolveRamp(e);
      expect(out.ramp.id).toBe('sequential.grey');
      expect(out.fallback).toBe(true);
    }
    expect(warns.length).toBe(1);
    expect(warns[0][0]).toContain('a.b');
    r.resolveRamp(floatEntry({ key: 'other.k', ui: { ramp: 'nope' } }));
    expect(warns.length).toBe(2);
  });

  it('categorical id on a float entry is a fallback', () => {
    const { r, warns } = newResolver();
    const out = r.resolveRamp(floatEntry({ ui: { ramp: 'categorical.12' } }));
    expect(out.ramp.id).toBe('sequential.grey');
    expect(out.fallback).toBe(true);
    expect(warns.length).toBe(1);
  });

  it('no ramps loaded -> built-in black to white', () => {
    const r = new RampResolver([]);
    const out = r.resolveRamp(floatEntry());
    expect(out.fallback).toBe(true);
    expect(rgba8ToHex(sampleRamp(out.ramp, 0))).toBe('#000000');
    expect(rgba8ToHex(sampleRamp(out.ramp, 1))).toBe('#ffffff');
  });

  it('range only when both bounds present', () => {
    const { r } = newResolver();
    const out = r.resolveRamp(floatEntry({ min: 2, max: undefined }));
    expect(out.hasRange).toBe(false);
    expect(r.resolveRamp(floatEntry({ min: 2, max: 8 })).hasRange).toBe(true);
  });

  it('rampT maps and clamps, never NaN', () => {
    const { r } = newResolver();
    const out = r.resolveRamp(floatEntry({ min: 10, max: 20 }));
    expect(rampT(out, 15)).toBe(0.5);
    expect(rampT(out, -5)).toBe(0);
    expect(rampT(out, 99)).toBe(1);
    expect(rampT(out, NaN)).toBe(0);
    expect(rampT(out, Infinity)).toBe(1);
    const flat = r.resolveRamp(floatEntry({ min: 3, max: 3 }));
    expect(rampT(flat, 3)).toBe(0);
  });

  it('delta ramp is symmetric and defaults to diverging.blue_red', () => {
    const { r } = newResolver();
    const out = r.resolveDeltaRamp(floatEntry(), 4);
    expect(out.ramp.id).toBe('diverging.blue_red');
    expect(out.min).toBe(-4);
    expect(out.max).toBe(4);
    expect(rampT(out, 0)).toBe(0.5);
    expect(r.resolveDeltaRamp(floatEntry(), 0).max).toBe(1);
    expect(r.resolveDeltaRamp(floatEntry(), NaN).max).toBe(1);
    expect(r.resolveDeltaRamp(floatEntry({ ui: { delta_ramp: 'sequential.heat' } }), 2).ramp.id).toBe('sequential.heat');
  });

  it('unknown delta ramp warns once', () => {
    const { r, warns } = newResolver();
    const e = floatEntry({ ui: { delta_ramp: 'zzz' } });
    r.resolveDeltaRamp(e, 1);
    r.resolveDeltaRamp(e, 2);
    expect(warns.length).toBe(1);
    expect(r.resolveDeltaRamp(e, 1).ramp.id).toBe('diverging.blue_red');
    expect(r.resolveDeltaRamp(e, 1).fallback).toBe(true);
  });

  it('more than 12 colourless enum values warns once per entry', () => {
    const { r, warns } = newResolver();
    const e = enumEntry(14);
    r.resolvePalette(e);
    r.resolvePalette(e);
    expect(warns.length).toBe(1);
    r.resolvePalette(enumEntry(12));
    expect(warns.length).toBe(1);
  });

  it('non-enum palette is empty; fallback is neutral grey', () => {
    const { r } = newResolver();
    const p = r.resolvePalette(floatEntry());
    expect(p.entries.length).toBe(0);
    expect(rgba8ToHex(p.fallback)).toBe('#808080');
  });

  it('legend: single seen range, swapped seen, non-finite seen', () => {
    const { r } = newResolver();
    const e = floatEntry({ unit: 'm' });
    expect(buildLegend(r, e).seen).toBeUndefined();
    expect(buildLegend(r, e, { min: 0.2, max: 0.4 }).seen).toEqual({ min: 0.2, max: 0.4 });
    expect(buildLegend(r, e, { min: 0.4, max: 0.2 }).seen).toEqual({ min: 0.2, max: 0.4 });
    expect(buildLegend(r, e, { min: NaN, max: 1 }).seen).toBeUndefined();
    expect(buildLegend(r, e).unit).toBe('m');
  });

  it('legend: enum swatches in schema order, label falls back to id', () => {
    const { r } = newResolver();
    const e = enumEntry(3, ['#112233']);
    e.values![1].label = '';
    const lg = buildLegend(r, e);
    expect(lg.isCategory).toBe(true);
    expect(lg.ticks).toEqual([]);
    expect(lg.swatches.map(s => s.label)).toEqual(['Value 0', 'v1', 'Value 2']);
    expect(rgba8ToHex(lg.swatches[0].colour)).toBe('#112233');
  });

  it('legend: other types are empty', () => {
    const { r } = newResolver();
    const lg = buildLegend(r, floatEntry({ type: 'bool' }));
    expect(lg.ticks).toEqual([]);
    expect(lg.swatches).toEqual([]);
    expect(lg.isCategory).toBe(false);
  });
});
