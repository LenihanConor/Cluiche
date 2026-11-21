import { describe, it, expect } from 'vitest';
import hashJson from '../../DiaCellLayers/Testing/Conformance/selection_hash.json';
import valuesJson from '../../DiaCellLayers/Testing/Conformance/layer_values.json';
import {
  selectionHash, decodeLayerValues, concatChunks, chunkRanges, kCellsValuesChunk, kCellsSelectionChunk,
  isCellsViewState, isCellsLayerValues, isCellsSelectionPage, isCellsLayerList, isCellsError, isCellsSelectionStats,
} from './cells';

describe('selectionHash', () => {
  it('fixture vectors have exact expected names', () => {
    const kExpectedNames = ['empty', 'zero', 'one', 'zero_one_two', 'last_cell', 'fifty_sorted', 'fifty_shuffled', 'full_grid'];
    const actualNames = hashJson.vectors.map((v) => v.name);
    expect(actualNames).toEqual(kExpectedNames);
    expect(new Set(actualNames).size).toBe(actualNames.length);
  });
  it('has fixture vectors including empty, [0], full grid and shuffled', () => {
    const names = hashJson.vectors.map((v) => v.name);
    for (const n of ['empty', 'zero', 'full_grid', 'fifty_shuffled']) expect(names).toContain(n);
  });
  for (const v of hashJson.vectors) {
    it(`matches vector ${v.name}`, () => {
      expect(selectionHash(v.indices)).toBe(v.hash);
    });
  }
  it('returns 16 zeros for empty', () => {
    expect(selectionHash([])).toBe('0000000000000000');
  });
  it('is order independent', () => {
    expect(selectionHash([5, 1, 9])).toBe(selectionHash([9, 5, 1]));
  });
  it('accepts any iterable', () => {
    expect(selectionHash(new Set([0]))).toBe('e220a8397b1dcdaf');
  });
});

describe('decodeLayerValues', () => {
  it('fixture cases have exact expected names', () => {
    const kExpectedNames = ['one_value_pad2', 'two_values_pad1', 'three_values_nopad', 'awkward_floats', 'awkward_slice', 'category_ids', 'empty_at_end'];
    const actualNames = valuesJson.cases.map((c) => c.name);
    expect(actualNames).toEqual(kExpectedNames);
    expect(new Set(actualNames).size).toBe(actualNames.length);
  });
  for (const c of valuesJson.cases) {
    it(`decodes case ${c.name}`, () => {
      const out = decodeLayerValues(c.data, c.count);
      expect(out).toBeInstanceOf(Float32Array);
      const expected = c.values.slice(c.offset, c.offset + c.count).map((x: number) => Math.fround(x));
      expect(Array.from(out)).toEqual(expected);
    });
  }
  it('decodes without a count', () => {
    expect(Array.from(decodeLayerValues('AACAPwAAAEA='))).toEqual([1, 2]);
  });
  it('throws when byte length is not a multiple of 4', () => {
    expect(() => decodeLayerValues('AAA=')).toThrow(/multiple of 4/);
  });
  it('throws on count mismatch', () => {
    expect(() => decodeLayerValues('AACAPwAAAEA=', 3)).toThrow(/count/);
  });
  it('throws on non-base64 input', () => {
    expect(() => decodeLayerValues('not*base64!')).toThrow(/base64/i);
  });
  it('concatChunks joins chunks in order', () => {
    const out = concatChunks([new Float32Array([1, 2]), new Float32Array([]), new Float32Array([3])]);
    expect(Array.from(out)).toEqual([1, 2, 3]);
  });
  it('chunk limits are 1,000 (sim-hosted 16 KB reply cap)', () => {
    expect(kCellsValuesChunk).toBe(1000);
    expect(kCellsSelectionChunk).toBe(1000);
  });
  it('chunkRanges pages a Large grid (6,656 cells) into 7 calls', () => {
    const pages = chunkRanges(6656);
    expect(pages.length).toBe(7);
    expect(pages[0]).toEqual({ offset: 0, count: 1000 });
    expect(pages[6]).toEqual({ offset: 6000, count: 656 });
    expect(pages.reduce((n, p) => n + p.count, 0)).toBe(6656);
  });
  it('chunkRanges edge cases', () => {
    expect(chunkRanges(0)).toEqual([]);
    expect(chunkRanges(1000)).toEqual([{ offset: 0, count: 1000 }]);
    expect(chunkRanges(1001).length).toBe(2);
    expect(chunkRanges(5, 2)).toEqual([{ offset: 0, count: 2 }, { offset: 2, count: 2 }, { offset: 4, count: 1 }]);
    expect(() => chunkRanges(-1)).toThrow();
    expect(() => chunkRanges(10, 0)).toThrow();
  });
});

const example = {
  viewVersion: 412,
  layersVersion: 7,
  grid: { id: 'world.hex', topology: 'hex.flat.oddq', width: 104, height: 64 },
  base: 'weather.precipitation',
  overlays: ['weather.wind', 'selection.outline'],
  selection: { count: 138, version: 29, hash: '9c3f0a41d27be615' },
  history: null,
};

describe('isCellsViewState', () => {
  it('accepts the spec example', () => expect(isCellsViewState(example)).toBe(true));
  it('accepts null grid and empty base', () => {
    expect(isCellsViewState({ ...example, grid: null, base: '' })).toBe(true);
  });
  it('accepts history variants', () => {
    expect(isCellsViewState({ ...example, history: { mode: 'live', sample: 3, label: 'x' } })).toBe(true);
    expect(isCellsViewState({ ...example, history: { mode: 'sample', sample: 3, label: 'x' } })).toBe(true);
    expect(isCellsViewState({ ...example, history: { mode: 'delta', a: 'now', b: 'sample:2', scale: null } })).toBe(true);
  });
  it('rejects bad history', () => {
    expect(isCellsViewState({ ...example, history: { mode: 'bogus' } })).toBe(false);
    expect(isCellsViewState({ ...example, history: { mode: 'delta', a: 1 } })).toBe(false);
  });
  it('rejects non-objects and missing fields', () => {
    expect(isCellsViewState(null)).toBe(false);
    expect(isCellsViewState('x')).toBe(false);
    for (const k of Object.keys(example)) {
      const copy: Record<string, unknown> = { ...example };
      delete copy[k];
      expect(isCellsViewState(copy)).toBe(false);
    }
  });
  it('rejects wrong-typed fields', () => {
    expect(isCellsViewState({ ...example, viewVersion: '1' })).toBe(false);
    expect(isCellsViewState({ ...example, overlays: [1] })).toBe(false);
    expect(isCellsViewState({ ...example, grid: { id: 1, topology: 't', width: 1, height: 1 } })).toBe(false);
    expect(isCellsViewState({ ...example, selection: { count: 1, version: 1, hash: 5 } })).toBe(false);
  });
});

describe('query guards', () => {
  const values = {
    layer: 'a', version: 1, gridId: 'g', width: 2, height: 2, offset: 0, count: 4, total: 4,
    encoding: 'f32le-base64', data: '',
  };
  it('isCellsLayerValues', () => {
    expect(isCellsLayerValues(values)).toBe(true);
    expect(isCellsLayerValues({ ...values, encoding: 'raw' })).toBe(false);
    expect(isCellsLayerValues({ ...values, total: undefined })).toBe(false);
  });
  it('isCellsSelectionPage', () => {
    const p = { version: 1, hash: '0000000000000000', total: 2, offset: 0, indices: [1, 2] };
    expect(isCellsSelectionPage(p)).toBe(true);
    expect(isCellsSelectionPage({ ...p, indices: ['a'] })).toBe(false);
  });
  it('isCellsLayerList', () => {
    const l = {
      layersVersion: 1,
      grids: [{ id: 'g', index: 0, width: 1, height: 1, layers: [{ id: 'a', schemaKey: 'k', group: 'g', kind: 'float', hotkey: '1' }] }],
      overlays: [{ id: 'o', label: 'O', group: 'g' }],
    };
    expect(isCellsLayerList(l)).toBe(true);
    expect(isCellsLayerList({ ...l, grids: 3 })).toBe(false);
  });
  it('isCellsError', () => {
    expect(isCellsError({ error: 'nope' })).toBe(true);
    expect(isCellsError({ error: 3 })).toBe(false);
    expect(isCellsError(null)).toBe(false);
  });
});

// ---- Task 10a exhaustive pass ----

describe('selectionHash edges', () => {
  it('matches independently computed values at bit-width boundaries', () => {
    expect(selectionHash([0xffffffff])).toBe('73b13ba2aff181c0');
    expect(selectionHash([0x80000000])).toBe('25493cc63225736c');
    expect(selectionHash([0xffff])).toBe('cf0f635e85b85d76');
    expect(selectionHash([0x10000])).toBe('09aab36cfda2d1b3');
    expect(selectionHash([0x100000000])).toBe('c42c5a1aa3820138');
    expect(selectionHash([1, 0xffffffff])).toBe('e2bb164e26f3dd01');
  });
  it('is always 16 lower-case hex digits, including leading zeros', () => {
    expect(selectionHash([0x10000])).toMatch(/^[0-9a-f]{16}$/);
    expect(selectionHash([0x10000]).startsWith('0')).toBe(true);
  });
  it('an index listed twice cancels (callers pass sets)', () => {
    expect(selectionHash([77, 77])).toBe('0000000000000000');
  });
  it('rejects non-integer indices instead of hashing garbage', () => {
    expect(() => selectionHash([1.5])).toThrow();
    expect(() => selectionHash([Number.NaN])).toThrow();
    expect(() => selectionHash([Number.POSITIVE_INFINITY])).toThrow();
  });
  it('hashes a generator and agrees with the array form', () => {
    function* gen() { yield 3; yield 1; yield 2; }
    expect(selectionHash(gen())).toBe(selectionHash([1, 2, 3]));
  });
});

describe('decodeLayerValues edges', () => {
  it('decodes the empty string to an empty array (offset == total reply)', () => {
    const out = decodeLayerValues('', 0);
    expect(out).toBeInstanceOf(Float32Array);
    expect(out.length).toBe(0);
    expect(decodeLayerValues('').length).toBe(0);
  });
  it('rejects whitespace, url-safe alphabet, missing or excess padding', () => {
    expect(() => decodeLayerValues('AACA PwAA')).toThrow(/base64/i);
    expect(() => decodeLayerValues('AACAPw\n==')).toThrow(/base64/i);
    expect(() => decodeLayerValues('AACA-w==')).toThrow(/base64/i);
    expect(() => decodeLayerValues('AACA_w==')).toThrow(/base64/i);
    expect(() => decodeLayerValues('AACAPwAAAEA')).toThrow(/base64/i);
    expect(() => decodeLayerValues('AACAPwAAAEA===')).toThrow(/base64/i);
    expect(() => decodeLayerValues('====')).toThrow(/base64/i);
    expect(() => decodeLayerValues('A')).toThrow(/base64/i);
  });
  it('rejects a payload of 1, 2 or 3 bytes (not a whole float)', () => {
    expect(() => decodeLayerValues('AA==')).toThrow(/multiple of 4/);
    expect(() => decodeLayerValues('AAA=')).toThrow(/multiple of 4/);
    expect(() => decodeLayerValues('AAAA')).toThrow(/multiple of 4/);
  });
  it('keeps NaN, infinities, negative zero and denormals exactly', () => {
    // bits: 0x7fc00000 NaN, 0x7f800000 +inf, 0xff800000 -inf, 0x80000000 -0, 0x00000001 denormal
    const bytes = new Uint8Array([
      0x00, 0x00, 0xc0, 0x7f, 0x00, 0x00, 0x80, 0x7f, 0x00, 0x00, 0x80, 0xff, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00,
    ]);
    let bin = '';
    for (const b of bytes) bin += String.fromCharCode(b);
    const out = decodeLayerValues(btoa(bin), 5);
    expect(Number.isNaN(out[0])).toBe(true);
    expect(out[1]).toBe(Infinity);
    expect(out[2]).toBe(-Infinity);
    expect(Object.is(out[3], -0)).toBe(true);
    expect(out[4]).toBeGreaterThan(0);
    expect(out[4]).toBeLessThan(1e-44);
  });
  it('round-trips a Large grid as 7 chunks', () => {
    const total = 6656;
    const all = new Float32Array(total);
    for (let i = 0; i < total; ++i) all[i] = i * 0.37 - 123;
    const chunks = chunkRanges(total).map(({ offset, count }) => {
      const view = new DataView(new ArrayBuffer(count * 4));
      for (let i = 0; i < count; ++i) view.setFloat32(i * 4, all[offset + i], true);
      let bin = '';
      for (let i = 0; i < count * 4; ++i) bin += String.fromCharCode(view.getUint8(i));
      return decodeLayerValues(btoa(bin), count);
    });
    expect(chunks.length).toBe(7);
    expect(chunks[0].length).toBe(1000);
    expect(Array.from(concatChunks(chunks))).toEqual(Array.from(all));
  });
  it('count 0 mismatches a non-empty payload', () => {
    expect(() => decodeLayerValues('AACAPw==', 0)).toThrow(/count/);
  });
});

describe('chunkRanges and concatChunks edges', () => {
  it('rejects non-integer arguments', () => {
    expect(() => chunkRanges(1.5)).toThrow();
    expect(() => chunkRanges(10, 2.5)).toThrow();
    expect(() => chunkRanges(Number.NaN)).toThrow();
    expect(() => chunkRanges(10, -1)).toThrow();
  });
  it('pages are contiguous, in order and never exceed the chunk', () => {
    for (const total of [1, 999, 1000, 1001, 1999, 2000, 2001, 6656]) {
      const pages = chunkRanges(total);
      let at = 0;
      for (const p of pages) {
        expect(p.offset).toBe(at);
        expect(p.count).toBeGreaterThan(0);
        expect(p.count).toBeLessThanOrEqual(1000);
        at += p.count;
      }
      expect(at).toBe(total);
    }
  });
  it('a chunk of 1 gives one page per cell', () => {
    expect(chunkRanges(3, 1)).toEqual([{ offset: 0, count: 1 }, { offset: 1, count: 1 }, { offset: 2, count: 1 }]);
  });
  it('concatChunks of nothing is empty and does not alias its inputs', () => {
    expect(concatChunks([]).length).toBe(0);
    const a = new Float32Array([1, 2]);
    const out = concatChunks([a]);
    out[0] = 9;
    expect(a[0]).toBe(1);
  });
});

describe('guard edges', () => {
  it('isCellsSelectionStats needs count, version and hash', () => {
    expect(isCellsSelectionStats({ count: 1, version: 2, hash: '0000000000000000', layers: [] })).toBe(true);
    expect(isCellsSelectionStats({ count: 1, version: 2 })).toBe(false);
    expect(isCellsSelectionStats({ error: 'x' })).toBe(false);
    expect(isCellsSelectionStats(null)).toBe(false);
  });
  it('rejects non-finite numbers and arrays standing in for objects', () => {
    expect(isCellsViewState({ ...example, viewVersion: Number.NaN })).toBe(false);
    expect(isCellsViewState({ ...example, layersVersion: Infinity })).toBe(false);
    expect(isCellsViewState([])).toBe(false);
    expect(isCellsViewState({ ...example, selection: [] })).toBe(false);
  });
  it('an error reply is not any success shape', () => {
    const err = { error: 'unknown layer' };
    expect(isCellsError(err)).toBe(true);
    expect(isCellsLayerValues(err)).toBe(false);
    expect(isCellsSelectionPage(err)).toBe(false);
    expect(isCellsLayerList(err)).toBe(false);
    expect(isCellsSelectionStats(err)).toBe(false);
    expect(isCellsViewState(err)).toBe(false);
  });
  it('accepts an empty hotkey (unbound layer) and rejects a bad kind or hotkey type', () => {
    const entry = { id: 'a', schemaKey: 'k', group: 'g', kind: 'float', hotkey: '' };
    const list = (e: unknown) => ({ layersVersion: 0, grids: [{ id: 'g', index: 0, width: 1, height: 1, layers: [e] }], overlays: [] });
    expect(isCellsLayerList(list(entry))).toBe(true);
    expect(isCellsLayerList(list({ ...entry, kind: 'int' }))).toBe(false);
    expect(isCellsLayerList(list({ ...entry, hotkey: 5 }))).toBe(false);
  });
  it('accepts the history shapes DiaCellHistory emits (spec Open Design Question 2)', () => {
    expect(isCellsViewState({ ...example, history: { mode: 'live', sample: null, label: null } })).toBe(true);
    expect(isCellsViewState({ ...example, history: { mode: 'sample', sample: 12, label: 'Spring' } })).toBe(true);
    expect(isCellsViewState({ ...example, history: { mode: 'sample', sample: 12, label: null } })).toBe(true);
    expect(isCellsViewState({ ...example, history: { mode: 'delta', a: 'now', b: 'sample:3', scale: null } })).toBe(true);
    expect(isCellsViewState({ ...example, history: { mode: 'delta', a: 'snapshot:before', b: 'sample:3', scale: 2.5 } })).toBe(true);
  });
  it('rejects malformed history of each mode', () => {
    expect(isCellsViewState({ ...example, history: { mode: 'sample', sample: 'x', label: null } })).toBe(false);
    expect(isCellsViewState({ ...example, history: { mode: 'live' } })).toBe(false);
    expect(isCellsViewState({ ...example, history: { mode: 'delta', a: 'now', scale: null } })).toBe(false);
    expect(isCellsViewState({ ...example, history: { mode: 'delta', a: 1, b: 2, scale: null } })).toBe(false);
    expect(isCellsViewState({ ...example, history: 'live' })).toBe(false);
  });
});
