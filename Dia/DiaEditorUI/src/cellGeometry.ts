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
