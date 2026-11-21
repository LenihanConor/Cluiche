// Pure cell-grid geometry for CellGridCanvas. Flat-top odd-q hex (odd columns shifted down half a cell) and square.
// Index order is row-major (index = r * width + q), matching Dia::Pathfinding::HexTopology.

/** Hex distance between two cell indices, flat-top odd-q. Mirrors HexTopology::Distance. */
export function hexDistance(a: number, b: number, width: number): number {
  const aq = a % width;
  const bq = b % width;
  const ar = Math.floor(a / width) - (aq - (aq & 1)) / 2;
  const br = Math.floor(b / width) - (bq - (bq & 1)) / 2;
  const dq = aq - bq;
  const dr = ar - br;
  return (Math.abs(dq) + Math.abs(dr) + Math.abs(dq + dr)) / 2;
}

/** Cells within `radius` hex steps of `centre`, clipped to the grid, ascending index order. */
export function cellsWithin(centre: number, radius: number, width: number, height: number): number[] {
  const cq = centre % width;
  const cr = Math.floor(centre / width);
  const out: number[] = [];
  // A hex of radius n spans at most n columns either side and n+1 rows either side in offset coords.
  const q0 = Math.max(0, cq - radius);
  const q1 = Math.min(width - 1, cq + radius);
  const r0 = Math.max(0, cr - radius - 1);
  const r1 = Math.min(height - 1, cr + radius + 1);
  for (let r = r0; r <= r1; r++) {
    for (let q = q0; q <= q1; q++) {
      const i = r * width + q;
      if (hexDistance(centre, i, width) <= radius) out.push(i);
    }
  }
  return out;
}

/** Cells in the offset-coordinate rectangle spanned by two corner cells (any order), row-major. */
export function cellsInBox(a: number, b: number, width: number): number[] {
  const aq = a % width, bq = b % width;
  const ar = Math.floor(a / width), br = Math.floor(b / width);
  const out: number[] = [];
  for (let r = Math.min(ar, br); r <= Math.max(ar, br); r++) {
    for (let q = Math.min(aq, bq); q <= Math.max(aq, bq); q++) out.push(r * width + q);
  }
  return out;
}

export type Topology = 'hex-flat-odd-q' | 'square';
export interface ViewState { zoom: number; panX: number; panY: number }

export interface LayoutInput {
  topology: Topology;
  width: number;
  height: number;
  boxW: number;
  boxH: number;
  view?: ViewState;
}

/** Screen transform for a grid. cellSize is hex circumradius or square side, in pixels. */
export interface Layout {
  topology: Topology;
  width: number;
  height: number;
  cellSize: number;
  /** Screen position of world origin (centre of cell 0 for hex, top-left corner for square). */
  originX: number;
  originY: number;
}

const kSqrt3 = Math.sqrt(3);

// World space uses unit cells: hex circumradius 1 (centre of cell (0,0) at the origin), square side 1.
function worldBounds(topology: Topology, width: number, height: number) {
  if (topology === 'square') return { minX: 0, minY: 0, w: width, h: height };
  const rows = height + (width > 1 ? 0.5 : 0);
  return { minX: -1, minY: -kSqrt3 / 2, w: 1.5 * width + 0.5, h: kSqrt3 * rows };
}

export function computeLayout(input: LayoutInput): Layout {
  const { topology, width, height, boxW, boxH, view } = input;
  const b = worldBounds(topology, width, height);
  const s0 = Math.max(0, Math.min(boxW / b.w, boxH / b.h));
  const o0x = (boxW - b.w * s0) / 2 - b.minX * s0;
  const o0y = (boxH - b.h * s0) / 2 - b.minY * s0;
  if (!view) return { topology, width, height, cellSize: s0, originX: o0x, originY: o0y };
  const cx = boxW / 2, cy = boxH / 2;
  return {
    topology, width, height,
    cellSize: s0 * view.zoom,
    originX: cx - (cx - o0x) * view.zoom + view.panX,
    originY: cy - (cy - o0y) * view.zoom + view.panY,
  };
}

export interface CellRef { q: number; r: number; index: number }

/** Screen position of a cell's centre. */
export function cellCentre(layout: Layout, index: number): { x: number; y: number } {
  const q = index % layout.width;
  const r = Math.floor(index / layout.width);
  const s = layout.cellSize;
  if (layout.topology === 'square') return { x: layout.originX + (q + 0.5) * s, y: layout.originY + (r + 0.5) * s };
  return { x: layout.originX + 1.5 * s * q, y: layout.originY + kSqrt3 * s * (r + 0.5 * (q & 1)) };
}

/** Flat-top hex corner offsets from the centre, starting at angle 0, clockwise on screen (y down). */
export function hexCorners(radius: number): { x: number; y: number }[] {
  const out: { x: number; y: number }[] = [];
  for (let k = 0; k < 6; k++) {
    const a = (k * Math.PI) / 3;
    out.push({ x: radius * Math.cos(a), y: radius * Math.sin(a) });
  }
  return out;
}

/** Inverse of cellCentre: the cell under a screen point, or null outside the grid. */
export function cellFromPoint(layout: Layout, x: number, y: number): CellRef | null {
  const s = layout.cellSize;
  if (!(s > 0)) return null;
  const wx = (x - layout.originX) / s;
  const wy = (y - layout.originY) / s;
  let q: number;
  let r: number;
  if (layout.topology === 'square') {
    q = Math.floor(wx);
    r = Math.floor(wy);
  } else {
    // Axial cube rounding (same as HexTopology::CellFromPoint), then axial -> odd-q offset.
    const qf = (2 / 3) * wx;
    const rf = -wx / 3 + (kSqrt3 / 3) * wy;
    const sf = -qf - rf;
    let rq = Math.round(qf), rr = Math.round(rf);
    const rs = Math.round(sf);
    const dq = Math.abs(rq - qf), dr = Math.abs(rr - rf), ds = Math.abs(rs - sf);
    if (dq > dr && dq > ds) rq = -rr - rs;
    else if (dr > ds) rr = -rq - rs;
    q = rq;
    r = rr + (rq - (rq & 1)) / 2;
  }
  if (q < 0 || r < 0 || q >= layout.width || r >= layout.height) return null;
  return { q, r, index: r * layout.width + q };
}
