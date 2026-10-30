import { describe, it, expect } from 'vitest';
import hashJson from '../../DiaCellLayers/Testing/Conformance/selection_hash.json';
import valuesJson from '../../DiaCellLayers/Testing/Conformance/layer_values.json';
import {
  selectionHash, decodeLayerValues, concatChunks,
  isCellsViewState, isCellsLayerValues, isCellsSelectionPage, isCellsLayerList, isCellsError,
} from './cells';

describe('selectionHash', () => {
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
    expect(isCellsViewState({ ...example, history: { mode: 'delta', a: 1, b: 2, scale: 'abs' } })).toBe(true);
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
