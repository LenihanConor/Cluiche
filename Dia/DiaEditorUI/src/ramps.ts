// Colour ramps, enum palettes and legends. TypeScript twin of Dia/DiaValueSchema (RampSample, RampResolver,
// RampLegend); both sides are tested against Dia/DiaValueSchema/Testing/Conformance/ramps/.
// Neutral colour data only: no graphics dependency. Colours are RGBA8 packed as 0xRRGGBBAA (unsigned number).

import type { ValueInfoLike } from './valueInfo';

export type Rgba8 = number;

export interface RampStop {
  t: number;
  colour: Rgba8;
}

/** A ramp as delivered by `valueschema.get` (`ramps` array) / `dia.ramps` files. */
export interface RampJson {
  id: string;
  kind: 'continuous' | 'categorical';
  stops?: Array<{ t: number; colour: string }>;
  colours?: string[];
}

export interface RampData {
  id: string;
  kind: 'continuous' | 'categorical';
  stops: RampStop[];
  colours: Rgba8[];
}

export interface ResolvedRamp {
  ramp: RampData;
  min: number;
  max: number;
  /** false when the entry lacks either bound (then 0..1 is used). */
  hasRange: boolean;
  /** The requested ramp was unknown/unusable. */
  fallback: boolean;
}

export interface PaletteEntry {
  id: string;
  colour: Rgba8;
}

export interface PaletteData {
  entries: PaletteEntry[];
  /** Neutral grey. */
  fallback: Rgba8;
  /** Colour-less values exceeded the categorical colour count. */
  wrapped: boolean;
}

export interface SwatchEntry {
  id: string;
  label: string;
  colour: Rgba8;
}

export interface LegendData {
  ramp?: ResolvedRamp;
  min: number;
  max: number;
  unit: string;
  ticks: number[];
  seen?: { min: number; max: number };
  swatches: SwatchEntry[];
  isCategory: boolean;
}

export const kGreyRampId = 'sequential.grey';
export const kDeltaDefaultRampId = 'diverging.blue_red';
export const kCategoricalRampId = 'categorical.12';
const kNeutralGrey: Rgba8 = 0x808080ff;
const kMaxLegendTicks = 5;
const kMaxInteriorTicks = 3;

/** Parses exactly "#rrggbb" (either case) to 0xRRGGBBFF; null otherwise. */
export function parseHexColour(text: unknown): Rgba8 | null {
  if (typeof text !== 'string' || !/^#[0-9a-fA-F]{6}$/.test(text)) return null;
  return ((parseInt(text.slice(1), 16) << 8) | 0xff) >>> 0;
}

/** 0xRRGGBBAA -> "#rrggbb" (alpha dropped). */
export function rgba8ToHex(c: Rgba8): string {
  return '#' + (c >>> 8).toString(16).padStart(6, '0');
}

function toRampData(j: RampJson): RampData {
  const stops: RampStop[] = [];
  for (const s of j.stops ?? []) {
    const c = parseHexColour(s.colour);
    if (c !== null) stops.push({ t: s.t, colour: c });
  }
  const colours: Rgba8[] = [];
  for (const h of j.colours ?? []) {
    const c = parseHexColour(h);
    if (c !== null) colours.push(c);
  }
  return { id: j.id, kind: j.kind, stops, colours };
}

const kBuiltInFallback: RampData = {
  id: 'builtin.fallback',
  kind: 'continuous',
  stops: [{ t: 0, colour: 0x000000ff }, { t: 1, colour: 0xffffffff }],
  colours: [],
};

/**
 * Samples a continuous ramp. t is clamped to [0,1] (NaN -> 0); per-channel linear interpolation in sRGB,
 * rounded half up; alpha is always 255. A categorical ramp returns its first colour; an empty ramp opaque black.
 */
export function sampleRamp(ramp: RampData | RampJson, t: number): Rgba8 {
  // RampData always carries both arrays; a RampJson carries only the one matching its kind.
  const r: RampData = Array.isArray(ramp.stops) && Array.isArray(ramp.colours) ? (ramp as RampData) : toRampData(ramp as RampJson);
  if (r.kind === 'categorical') return r.colours.length > 0 ? r.colours[0] : 0x000000ff;
  const stops = r.stops;
  if (stops.length === 0) return 0x000000ff;
  const tt = Number.isNaN(t) ? 0 : Math.min(1, Math.max(0, t));
  if (tt <= stops[0].t) return stops[0].colour;
  const last = stops[stops.length - 1];
  if (tt >= last.t) return last.colour;
  let i = 1;
  while (i < stops.length - 1 && tt > stops[i].t) i++;
  const a = stops[i - 1];
  const b = stops[i];
  const f = (tt - a.t) / (b.t - a.t);
  const ch = (shift: number) => {
    const x = (a.colour >>> shift) & 0xff;
    const y = (b.colour >>> shift) & 0xff;
    return Math.min(255, Math.max(0, Math.floor(x + (y - x) * f + 0.5)));
  };
  return (((ch(24) << 24) | (ch(16) << 16) | (ch(8) << 8) | 0xff) >>> 0);
}

/** Categorical colour by index, wrapping modulo the colour count. */
export function rampColourAt(ramp: RampData, index: number): Rgba8 {
  if (ramp.kind !== 'categorical' || ramp.colours.length === 0) return sampleRamp(ramp, 0);
  return ramp.colours[index % ramp.colours.length];
}

/** value in [min,max] -> t in [0,1], clamped. Never NaN: NaN value, non-finite bound or empty/inverted range give 0. */
export function rampT(ramp: ResolvedRamp, value: number): number {
  if (Number.isNaN(value) || !Number.isFinite(ramp.min) || !Number.isFinite(ramp.max)) return 0;
  const width = ramp.max - ramp.min;
  if (!(width > 0)) return 0;
  return Math.min(1, Math.max(0, (value - ramp.min) / width));
}

export type RampWarn = (key: string, message: string) => void;

/**
 * Resolves the ramp / palette an entry asks for. Mirrors C++ RampResolver: the once-per-key warning memory lives
 * in the instance; `onWarn` is called at most once per (kind, entry key). The resolver never logs itself.
 */
export class RampResolver {
  private readonly byId = new Map<string, RampData>();
  private readonly warned = new Set<string>();
  private fallbacks = 0;
  private unknownWarnings = 0;
  private wrapWarnings = 0;

  constructor(ramps: ReadonlyArray<RampJson>, private readonly onWarn?: RampWarn) {
    for (const j of ramps) this.byId.set(j.id, toRampData(j));
  }

  get fallbackCount(): number { return this.fallbacks; }
  get unknownRampWarningCount(): number { return this.unknownWarnings; }
  get wrapWarningCount(): number { return this.wrapWarnings; }

  resolveRamp(entry: ValueInfoLike): ResolvedRamp {
    const out = this.resolveNamed(entry, 'ramp', kGreyRampId);
    if (typeof entry.min === 'number' && typeof entry.max === 'number') {
      out.hasRange = true;
      out.min = entry.min;
      out.max = entry.max;
    }
    return out;
  }

  /** Symmetric range [-maxAbs, +maxAbs]; maxAbs <= 0, NaN or infinite uses 1. */
  resolveDeltaRamp(entry: ValueInfoLike, maxAbs: number): ResolvedRamp {
    const out = this.resolveNamed(entry, 'delta_ramp', kDeltaDefaultRampId);
    const m = Number.isFinite(maxAbs) && maxAbs > 0 ? maxAbs : 1;
    out.hasRange = true;
    out.min = -m;
    out.max = m;
    return out;
  }

  /** Enum colours: given colour, else categorical.12 by value order (wraps). Non-enum -> empty palette. */
  resolvePalette(entry: ValueInfoLike): PaletteData {
    const out: PaletteData = { entries: [], fallback: kNeutralGrey, wrapped: false };
    if (entry.type !== 'enum') return out;
    const cat = this.byId.get(kCategoricalRampId);
    const palette = cat && cat.kind === 'categorical' && cat.colours.length > 0 ? cat : null;
    let colourless = 0;
    (entry.values ?? []).forEach((v, i) => {
      let colour = parseHexColour(v.colour);
      if (colour === null) {
        colourless++;
        colour = palette ? palette.colours[i % palette.colours.length] : out.fallback;
      }
      out.entries.push({ id: v.id, colour });
    });
    if (palette && colourless > palette.colours.length) {
      out.wrapped = true;
      if (this.markWarned('palette', entry)) {
        this.wrapWarnings++;
        this.onWarn?.(entry.key, `${entry.key}: ${colourless} colour-less enum values exceed the ${palette.colours.length} categorical colours; colours repeat`);
      }
    }
    return out;
  }

  private markWarned(tag: string, entry: ValueInfoLike): boolean {
    const k = `${tag}\u0000${entry.key}`;
    if (this.warned.has(k)) return false;
    this.warned.add(k);
    return true;
  }

  private resolveNamed(entry: ValueInfoLike, uiKey: 'ramp' | 'delta_ramp', defaultId: string): ResolvedRamp {
    const ui = entry.ui && typeof entry.ui === 'object' ? entry.ui : undefined;
    const v = ui ? ui[uiKey] : undefined;
    const requested = typeof v === 'string';
    const wanted = requested ? (v as string) : defaultId;
    let found = this.byId.get(wanted);
    let unusable = !found || found.kind !== 'continuous';
    if (unusable) {
      if (requested && this.markWarned(uiKey, entry)) {
        this.unknownWarnings++;
        this.onWarn?.(entry.key, `${entry.key}: ui.${uiKey} '${wanted}' is not a known continuous ramp, using '${defaultId}'`);
      }
      found = this.byId.get(defaultId);
      if (found && found.kind !== 'continuous') found = undefined;
    }
    let ramp: RampData;
    if (found) {
      ramp = found;
    } else {
      ramp = kBuiltInFallback;
      unusable = true;
    }
    if (unusable) this.fallbacks++;
    return { ramp, min: 0, max: 1, hasRange: false, fallback: unusable };
  }
}

// Interior multiples of (m * 10^e) strictly between mn and mx, ascending; stops once more than `cap` are found.
function niceValue(k: number, m: number, e: number): number {
  const scaled = k * m;
  return e >= 0 ? scaled * Math.pow(10, e) : scaled / Math.pow(10, -e);
}

function collectInterior(mn: number, mx: number, m: number, e: number, cap: number): number[] {
  const step = niceValue(1, m, e);
  const eps = (mx - mn) * 1e-6;
  const kLo = Math.floor(mn / step) - 1;
  const kHi = Math.ceil(mx / step) + 1;
  const out: number[] = [];
  for (let k = kLo; k <= kHi; k++) {
    const v = niceValue(k, m, e);
    if (v > mn + eps && v < mx - eps) {
      if (out.length >= cap) return [...out, v];
      out.push(v);
    }
  }
  return out;
}

/**
 * Up to 5 'nice' ticks including both ends: the smallest step of the form {1,2,5} x 10^n leaving at most 3
 * multiples strictly inside (min,max). min >= max gives [min]; non-finite bounds give [].
 */
export function buildTicks(min: number, max: number): number[] {
  if (!Number.isFinite(min) || !Number.isFinite(max)) return [];
  if (!(max > min)) return [min];
  const e0 = Math.floor(Math.log10(max - min)) - 1;
  let interior: number[] = [];
  search:
  for (let e = e0; e <= e0 + 4; e++) {
    for (const m of [1, 2, 5]) {
      const found = collectInterior(min, max, m, e, kMaxInteriorTicks);
      if (found.length <= kMaxInteriorTicks) {
        interior = found;
        break search;
      }
    }
  }
  const ticks = [min];
  for (const v of interior) {
    if (v > ticks[ticks.length - 1] && v < max) ticks.push(v);
  }
  ticks.push(max);
  return ticks.slice(0, kMaxLegendTicks + 1);
}

/** Neutral legend data for one entry. Float/int: ramp + range + unit + ticks (+ seen). Enum: one swatch per value. */
export function buildLegend(
  resolver: RampResolver,
  entry: ValueInfoLike,
  seen?: { min: number; max: number },
): LegendData {
  const out: LegendData = { min: 0, max: 1, unit: '', ticks: [], swatches: [], isCategory: false };
  if (entry.type === 'float' || entry.type === 'int') {
    const ramp = resolver.resolveRamp(entry);
    out.ramp = ramp;
    out.min = ramp.min;
    out.max = ramp.max;
    out.unit = entry.unit ?? '';
    out.ticks = buildTicks(ramp.min, ramp.max);
    if (seen && Number.isFinite(seen.min) && Number.isFinite(seen.max)) {
      out.seen = { min: Math.min(seen.min, seen.max), max: Math.max(seen.min, seen.max) };
    }
  } else if (entry.type === 'enum') {
    out.isCategory = true;
    const pal = resolver.resolvePalette(entry);
    (entry.values ?? []).forEach((v, i) => {
      out.swatches.push({
        id: v.id,
        label: v.label !== '' ? v.label : v.id,
        colour: i < pal.entries.length ? pal.entries[i].colour : pal.fallback,
      });
    });
  }
  return out;
}
