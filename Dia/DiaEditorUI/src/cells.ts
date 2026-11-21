// Helpers for the DiaCellLayers editor mirror (cells.view.state topic and cells.* queries).
// Spec: docs/specs/applications/dia/systems/diacelllayers/editor-mirror.md

// ---- Selection hash (SplitMix64 XOR fingerprint) ----------------------------------------------

const kGolden = 0x9e3779b97f4a7c15n;
const kMulA = 0xbf58476d1ce4e5b9n;
const kMulB = 0x94d049bb133111ebn;

function mix(i: number): bigint {
  let z = BigInt.asUintN(64, BigInt(i) + kGolden);
  z = BigInt.asUintN(64, (z ^ (z >> 30n)) * kMulA);
  z = BigInt.asUintN(64, (z ^ (z >> 27n)) * kMulB);
  return BigInt.asUintN(64, z ^ (z >> 31n));
}

/** Order-independent 64-bit selection fingerprint as 16 lower-case hex chars. Matches the C++ SelectionFingerprint. */
export function selectionHash(indices: Iterable<number>): string {
  let h = 0n;
  for (const i of indices) h ^= mix(i);
  return BigInt.asUintN(64, h).toString(16).padStart(16, '0');
}

// ---- Layer value decoding ---------------------------------------------------------------------

const kBase64 = /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/;

/** Decodes base64 little-endian float32 data (host-endianness independent). Throws on malformed input or count mismatch. */
export function decodeLayerValues(data: string, count?: number): Float32Array {
  if (!kBase64.test(data)) throw new Error('decodeLayerValues: data is not valid base64');
  const bin = atob(data);
  if (bin.length % 4 !== 0) {
    throw new Error(`decodeLayerValues: byte length ${bin.length} is not a multiple of 4`);
  }
  const n = bin.length / 4;
  if (count !== undefined && n !== count) {
    throw new Error(`decodeLayerValues: decoded ${n} values but count is ${count}`);
  }
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; ++i) bytes[i] = bin.charCodeAt(i);
  const view = new DataView(bytes.buffer);
  const out = new Float32Array(n);
  for (let i = 0; i < n; ++i) out[i] = view.getFloat32(i * 4, true);
  return out;
}

/**
 * Per-call limits of the sim-hosted queries (replies must fit the 16 KB sim-hosted cap). A larger `count` is clamped by the
 * server, so page with these. Must match MirrorQueries::kMaxValuesChunk / kMaxSelectionChunk in DiaCellLayers.
 */
export const kCellsValuesChunk = 1000;
export const kCellsSelectionChunk = 1000;

/** Splits [0, total) into consecutive { offset, count } pages of at most `chunk` (default kCellsValuesChunk). */
export function chunkRanges(total: number, chunk: number = kCellsValuesChunk): { offset: number; count: number }[] {
  if (!Number.isInteger(total) || total < 0) throw new Error('chunkRanges: total must be a non-negative integer');
  if (!Number.isInteger(chunk) || chunk < 1) throw new Error('chunkRanges: chunk must be a positive integer');
  const out: { offset: number; count: number }[] = [];
  for (let offset = 0; offset < total; offset += chunk) out.push({ offset, count: Math.min(chunk, total - offset) });
  return out;
}

/** Concatenates decoded chunks (in order) into one array. */
export function concatChunks(chunks: readonly Float32Array[]): Float32Array {
  let total = 0;
  for (const c of chunks) total += c.length;
  const out = new Float32Array(total);
  let at = 0;
  for (const c of chunks) { out.set(c, at); at += c.length; }
  return out;
}

// ---- Types ------------------------------------------------------------------------------------

export interface CellsGridInfo { id: string; topology: string; width: number; height: number }
export interface CellsSelectionInfo { count: number; version: number; hash: string }
/** History block of cells.view.state (DiaCellHistory; spec Open Design Question 2). a / b are "now", "sample:<i>" or "snapshot:<name>". */
export type CellsHistoryState =
  | { mode: 'live' | 'sample'; sample: number | null; label: string | null }
  | { mode: 'delta'; a: string; b: string; scale: number | null };

/** Payload of the cells.view.state topic. grid is null with no active grid; base may be ''. */
export interface CellsViewState {
  viewVersion: number;
  layersVersion: number;
  grid: CellsGridInfo | null;
  base: string;
  overlays: string[];
  selection: CellsSelectionInfo;
  history: CellsHistoryState | null;
}

export interface CellsLayerEntry { id: string; schemaKey: string; group: string; kind: 'float' | 'category'; hotkey: string }
export interface CellsLayerGrid { id: string; index: number; width: number; height: number; layers: CellsLayerEntry[] }
export interface CellsOverlayEntry { id: string; label: string; group: string }
export interface CellsLayerList { layersVersion: number; grids: CellsLayerGrid[]; overlays: CellsOverlayEntry[] }

export interface CellsLayerValues {
  layer: string; version: number; gridId: string; width: number; height: number;
  offset: number; count: number; total: number; encoding: 'f32le-base64'; data: string;
}

export interface CellsSelectionPage { version: number; hash: string; total: number; offset: number; indices: number[] }

/** cells.select.stats reply: Feature 4 stats JSON (per-layer blocks) plus selection count, version and hash. */
export interface CellsSelectionStats { count: number; version: number; hash: string; [key: string]: unknown }

export interface CellsError { error: string }

// ---- Guards -----------------------------------------------------------------------------------

type Rec = Record<string, unknown>;
const isRec = (x: unknown): x is Rec => typeof x === 'object' && x !== null && !Array.isArray(x);
const isNum = (x: unknown): x is number => typeof x === 'number' && Number.isFinite(x);
const isStr = (x: unknown): x is string => typeof x === 'string';
function isArrayOf<T>(x: unknown, f: (v: unknown) => v is T): x is T[] {
  return Array.isArray(x) && x.every(f);
}

const isGrid = (x: unknown): x is CellsGridInfo =>
  isRec(x) && isStr(x.id) && isStr(x.topology) && isNum(x.width) && isNum(x.height);
const isSelection = (x: unknown): x is CellsSelectionInfo =>
  isRec(x) && isNum(x.count) && isNum(x.version) && isStr(x.hash);
const isHistory = (x: unknown): x is CellsHistoryState => {
  if (!isRec(x)) return false;
  if (x.mode === 'live' || x.mode === 'sample') return (x.sample === null || isNum(x.sample)) && (x.label === null || isStr(x.label));
  if (x.mode === 'delta') return isStr(x.a) && isStr(x.b) && (x.scale === null || isNum(x.scale));
  return false;
};

export function isCellsViewState(x: unknown): x is CellsViewState {
  return isRec(x)
    && isNum(x.viewVersion) && isNum(x.layersVersion)
    && (x.grid === null || isGrid(x.grid))
    && isStr(x.base)
    && isArrayOf(x.overlays, isStr)
    && isSelection(x.selection)
    && (x.history === null || isHistory(x.history));
}

const isLayerEntry = (x: unknown): x is CellsLayerEntry =>
  isRec(x) && isStr(x.id) && isStr(x.schemaKey) && isStr(x.group)
  && (x.kind === 'float' || x.kind === 'category') && isStr(x.hotkey);
const isLayerGrid = (x: unknown): x is CellsLayerGrid =>
  isRec(x) && isStr(x.id) && isNum(x.index) && isNum(x.width) && isNum(x.height) && isArrayOf(x.layers, isLayerEntry);
const isOverlayEntry = (x: unknown): x is CellsOverlayEntry =>
  isRec(x) && isStr(x.id) && isStr(x.label) && isStr(x.group);

export function isCellsLayerList(x: unknown): x is CellsLayerList {
  return isRec(x) && isNum(x.layersVersion) && isArrayOf(x.grids, isLayerGrid) && isArrayOf(x.overlays, isOverlayEntry);
}

export function isCellsLayerValues(x: unknown): x is CellsLayerValues {
  return isRec(x) && isStr(x.layer) && isNum(x.version) && isStr(x.gridId)
    && isNum(x.width) && isNum(x.height) && isNum(x.offset) && isNum(x.count) && isNum(x.total)
    && x.encoding === 'f32le-base64' && isStr(x.data);
}

export function isCellsSelectionPage(x: unknown): x is CellsSelectionPage {
  return isRec(x) && isNum(x.version) && isStr(x.hash) && isNum(x.total) && isNum(x.offset) && isArrayOf(x.indices, isNum);
}

export function isCellsSelectionStats(x: unknown): x is CellsSelectionStats {
  return isRec(x) && isNum(x.count) && isNum(x.version) && isStr(x.hash);
}

export function isCellsError(x: unknown): x is CellsError {
  return isRec(x) && isStr(x.error);
}
