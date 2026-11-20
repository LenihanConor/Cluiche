// Turns S-1 per-cell layers into continuous field textures (the "a tile is a sample of the cell fields" idea).
// Hex convention matches Dia HexTopology: flat-top, odd-q (odd columns shifted down), y down, cell = row*w + col,
// directions NE, N, NW, SW, S, SE.
"use strict";

const SQ3 = Math.sqrt(3);
const HEX_OFFSETS = [
  [[+1, -1], [0, -1], [-1, -1], [-1, 0], [0, +1], [+1, 0]],   // even column
  [[+1, 0], [0, -1], [-1, 0], [-1, +1], [0, +1], [+1, +1]],   // odd column
];

// Enum values from CoW TerrainEnums.h.
const BIOME = { Ocean: 0, Lake: 1, Grassland: 2, TemperateForest: 3, Wetland: 4, Desert: 5, Alpine: 6, Tundra: 7, Rainforest: 8, Volcanic: 9 };
const BIOME_NAMES = ["Ocean", "Lake", "Grassland", "Temperate Forest", "Wetland", "Desert", "Alpine", "Tundra", "Rainforest", "Volcanic"];
const TERRAIN = { DeepWater: 0, ShallowWater: 1, Lake: 2, River: 3, Lowland: 4, Hills: 5, Highland: 6, Mountain: 7, Peak: 8, Cliff: 9, Pass: 10, Volcano: 11 };

// Extra relief by terrain type (S-1 TerrainType): Hills, Highland, Mountain, Peak, Cliff, Volcano.
const MOUNTAIN_WEIGHT = { 5: 0.22, 6: 0.18, 7: 1.0, 8: 1.45, 9: 0.15, 11: 1.3 };
function ridged(x, y) {
  let s = 0, a = 0.55, f = 1;
  for (let o = 0; o < 4; ++o) { s += a * (1 - Math.abs(2 * vnoise(x * f + o * 13.1, y * f + o * 7.7) - 1)); a *= 0.5; f *= 2.1; }
  return Math.min(1, s / 0.95);
}

class World {
  constructor(src, texelsPerUnit) {
    this.name = src.name;
    this.w = src.width; this.h = src.height;
    this.L = src.layers;
    this.n = this.w * this.h;
    this.T = texelsPerUnit;
    this.extentX = (this.w - 1) * 1.5 + 2;
    this.extentY = this.h * SQ3 + SQ3 / 2;
    this.cx = new Float32Array(this.n); this.cy = new Float32Array(this.n);
    for (let c = 0; c < this.n; ++c) {
      const col = c % this.w, row = Math.floor(c / this.w);
      this.cx[c] = col * 1.5 + 1;
      this.cy[c] = row * SQ3 + (col & 1) * SQ3 / 2 + SQ3 / 2;
    }
    this.nb = new Int32Array(this.n * 6);
    for (let c = 0; c < this.n; ++c) {
      const col = c % this.w, row = Math.floor(c / this.w);
      for (let d = 0; d < 6; ++d) {
        const o = HEX_OFFSETS[col & 1][d], x = col + o[0], y = row + o[1];
        this.nb[c * 6 + d] = (x < 0 || y < 0 || x >= this.w || y >= this.h) ? -1 : y * this.w + x;
      }
    }
    // Cell + ring 1 + ring 2 (deduplicated, -1 padded).
    this.ring = new Int32Array(this.n * 19).fill(-1);
    for (let c = 0; c < this.n; ++c) {
      const set = new Set([c]);
      for (let d = 0; d < 6; ++d) { const m = this.nb[c * 6 + d]; if (m < 0) continue; set.add(m); for (let e = 0; e < 6; ++e) { const q = this.nb[m * 6 + e]; if (q >= 0) set.add(q); } }
      let a = 0; for (const m of set) this.ring[c * 19 + a++] = m;
    }
    // Water class per cell: 0 land, 1 ocean, 2 lake.
    this.water = new Uint8Array(this.n);
    for (let c = 0; c < this.n; ++c) {
      const b = this.L.biome[c];
      this.water[c] = b === BIOME.Ocean ? 1 : (b === BIOME.Lake || this.L.lake[c] > 0) ? 2 : 0;
    }
    this.buildFields();
    this.buildRivers();
  }

  pointToCell(x, y) {
    x -= 1; y -= SQ3 / 2;
    const q = (2 / 3) * x, r = (-1 / 3) * x + (SQ3 / 3) * y, s = -q - r;
    let rq = Math.round(q), rr = Math.round(r), rs = Math.round(s);
    const dq = Math.abs(rq - q), dr = Math.abs(rr - r), ds = Math.abs(rs - s);
    if (dq > dr && dq > ds) rq = -rr - rs; else if (dr > ds) rr = -rq - rs;
    const col = rq, row = rr + (rq - (rq & 1)) / 2;
    if (col < 0 || row < 0 || col >= this.w || row >= this.h) return -1;
    return row * this.w + col;
  }

  // Noise-warped lookup: biome borders and coasts stop following the hex outline.
  warp(x, y, amp) {
    const f = 0.9, g = 2.9;
    return [x + amp * ((fbm2(x * f, y * f) - 0.5) * 2 + 0.35 * (vnoise(x * g + 5.1, y * g) - 0.5) * 2),
            y + amp * ((fbm2(x * f + 31.7, y * f + 11.3) - 0.5) * 2 + 0.35 * (vnoise(x * g, y * g + 8.3) - 0.5) * 2)];
  }

  buildFields() {
    const T = this.T, TW = Math.ceil(this.extentX * T), TH = Math.ceil(this.extentY * T);
    this.TW = TW; this.TH = TH;
    this.cellTex = new Uint8Array(TW * TH * 4);     // col, row, water class, terrain
    this.field = new Float32Array(TW * TH * 4);     // elevation, landness, lakeness, temperature
    this.edge = new Uint8Array(TW * TH * 4);        // biome margin (0 on a border), pad
    this.elevCPU = new Float32Array(TW * TH);
    const L = this.L, sig2 = 0.7;
    const ids = new Int32Array(19), ws = new Float32Array(19), bw = new Float32Array(16);
    for (let j = 0; j < TH; ++j) {
      for (let i = 0; i < TW; ++i) {
        const x = (i + 0.5) / T, y = (j + 0.5) / T;
        const [wx, wy] = this.warp(x, y, 0.5);
        const c = this.pointToCell(wx, wy);
        const k = (j * TW + i);
        if (c < 0) {
          this.cellTex.set([255, 255, 1, 0], k * 4);
          this.field.set([-0.3, 0, 0, 0.5], k * 4);
          continue;
        }
        // Smooth fields: Gaussian over the cell and its six neighbours, sampled at the warped point.
        // Two rings (19 cells) so the neighbourhood never changes abruptly at a hex edge: the weighted fields
        // and the biome decision stay continuous, which is what removes the hex staircase.
        let n = 0;
        for (let a = 0; a < 19; ++a) { const m = this.ring[c * 19 + a]; if (m >= 0) ids[n++] = m; }
        let wsum = 0, e = 0, land = 0, lake = 0, temp = 0, mnt = 0;
        for (let a = 0; a < n; ++a) {
          const m = ids[a], dx = this.cx[m] - wx, dy = this.cy[m] - wy;
          const wgt = Math.exp(-(dx * dx + dy * dy) / sig2);
          ws[a] = wgt; wsum += wgt;
          const wc = this.water[m];
          const el = wc === 1 ? -0.05 - 0.25 * L.oceanDepth[m] : L.elevation[m];   // lake cells carry their surface height
          e += wgt * el;
          land += wgt * (wc === 0 ? 1 : 0);
          lake += wgt * (wc === 2 ? 1 : 0);
          temp += wgt * L.temperature[m];
          mnt += wgt * (MOUNTAIN_WEIGHT[L.terrain[m]] || 0);
        }
        e /= wsum; land /= wsum; lake /= wsum; temp /= wsum; mnt /= wsum;
        // Mountains live in the terrain, not in sprites: extra relief on Mountain/Peak/Volcano cells, shaped by
        // ridged noise so ranges read as crags and spurs.
        if (mnt > 0.01) e += mnt * (0.35 + 0.65 * ridged(wx * 0.9, wy * 0.9)) * 0.36;
        // Class and biome by summed weight (a blurred majority), not nearest cell: this rounds off the hex
        // staircase so coasts and biome borders read as drawn curves.
        const cls = land >= 0.5 ? 0 : (1 - land - lake) >= lake ? 1 : 2;
        let best = -1, bestW = -1;
        bw.fill(0);
        for (let a = 0; a < n; ++a) if (this.water[ids[a]] === cls) bw[L.biome[ids[a]]] += ws[a];
        let bestB = -1, bestBW = -1, secondBW = 0;
        for (let b = 0; b < bw.length; ++b) if (bw[b] > bestBW) { secondBW = bestBW; bestBW = bw[b]; bestB = b; } else if (bw[b] > secondBW) secondBW = bw[b];
        if (secondBW < 0) secondBW = 0;
        this.edge[k * 4] = Math.round(Math.min(1, (bestBW - secondBW) / (bestBW + secondBW + 1e-6) * 2.5) * 255);
        for (let a = 0; a < n; ++a) { const m = ids[a]; if (this.water[m] === cls && L.biome[m] === bestB && ws[a] > bestW) { bestW = ws[a]; best = m; } }
        if (best < 0) best = c;
        this.cellTex[k * 4] = best % this.w;
        this.cellTex[k * 4 + 1] = Math.floor(best / this.w);
        this.cellTex[k * 4 + 2] = cls;
        this.cellTex[k * 4 + 3] = L.terrain[best];
        this.field[k * 4] = e; this.field[k * 4 + 1] = land; this.field[k * 4 + 2] = lake; this.field[k * 4 + 3] = temp;
        this.elevCPU[k] = e;
      }
    }
  }

  elevAt(x, y) {
    const i = Math.min(this.TW - 1, Math.max(0, Math.floor(x * this.T)));
    const j = Math.min(this.TH - 1, Math.max(0, Math.floor(y * this.T)));
    return this.elevCPU[j * this.TW + i];
  }
  // Cell that owns the drawn ground at (x, y), or -1 over water (matches what the shader draws).
  cellAtWarped(x, y) {
    const i = Math.min(this.TW - 1, Math.max(0, Math.floor(x * this.T))), j = Math.min(this.TH - 1, Math.max(0, Math.floor(y * this.T)));
    const k = (j * this.TW + i) * 4;
    if (this.cellTex[k + 2] !== 0 || this.cellTex[k] === 255) return -1;
    return this.cellTex[k + 1] * this.w + this.cellTex[k];
  }

  // Rivers: river cells flow centre -> downstream centre. Each segment gets a hand-drawn wiggle, then a distance
  // field is rasterised so the shader can draw water, ink banks and flow from one texture.
  buildRivers() {
    const L = this.L, segs = [];
    for (let c = 0; c < this.n; ++c) {
      if (!(L.river[c] > 0)) continue;
      const d = L.flowDir[c];
      if (d < 0 || d > 5) continue;
      const m = this.nb[c * 6 + d];
      if (m < 0) continue;
      const hw = Math.min(0.24, 0.05 + 0.03 * Math.log2(1 + L.flowAcc[c]));
      const ax = this.cx[c], ay = this.cy[c], bx = this.cx[m], by = this.cy[m];
      const px = -(by - ay), py = bx - ax, pl = Math.hypot(px, py);
      const STEPS = 8; let prev = [ax, ay];
      for (let s = 1; s <= STEPS; ++s) {
        const t = s / STEPS;
        const wig = s === STEPS ? 0 : Math.sin(t * Math.PI) * (vnoise(ax * 3.1 + t * 2.3, ay * 3.1) - 0.5) * 0.7;
        const p = [ax + (bx - ax) * t + px / pl * wig, ay + (by - ay) * t + py / pl * wig];
        segs.push([prev[0], prev[1], p[0], p[1], hw, (bx - ax), (by - ay)]);
        prev = p;
      }
    }
    this.riverSegs = segs.length;
    const T = this.T, TW = this.TW, TH = this.TH, MAXD = 0.6;
    const dist = new Float32Array(TW * TH).fill(MAXD), hwT = new Float32Array(TW * TH), dir = new Float32Array(TW * TH * 2);
    for (const [ax, ay, bx, by, hw, fx, fy] of segs) {
      const fl = Math.hypot(fx, fy);
      const i0 = Math.max(0, Math.floor((Math.min(ax, bx) - MAXD) * T)), i1 = Math.min(TW - 1, Math.ceil((Math.max(ax, bx) + MAXD) * T));
      const j0 = Math.max(0, Math.floor((Math.min(ay, by) - MAXD) * T)), j1 = Math.min(TH - 1, Math.ceil((Math.max(ay, by) + MAXD) * T));
      const vx = bx - ax, vy = by - ay, vl2 = vx * vx + vy * vy || 1;
      for (let j = j0; j <= j1; ++j)
        for (let i = i0; i <= i1; ++i) {
          const x = (i + 0.5) / T, y = (j + 0.5) / T;
          const t = Math.max(0, Math.min(1, ((x - ax) * vx + (y - ay) * vy) / vl2));
          const dx = x - (ax + vx * t), dy = y - (ay + vy * t);
          const dd = Math.hypot(dx, dy) - hw;   // signed: negative inside the channel
          const k = j * TW + i;
          if (dd < dist[k]) { dist[k] = dd; hwT[k] = hw; dir[k * 2] = fx / fl; dir[k * 2 + 1] = fy / fl; }
        }
    }
    this.river = new Uint8Array(TW * TH * 4);
    for (let k = 0; k < TW * TH; ++k) {
      this.river[k * 4] = Math.round(Math.max(0, Math.min(1, (dist[k] + 0.3) / (MAXD + 0.3))) * 255);
      this.river[k * 4 + 1] = Math.round(hwT[k] / 0.3 * 255);
      this.river[k * 4 + 2] = Math.round((dir[k * 2] * 0.5 + 0.5) * 255);
      this.river[k * 4 + 3] = Math.round((dir[k * 2 + 1] * 0.5 + 0.5) * 255);
    }
  }

  // Per-cell data texture (DiaCellLayers-style): biome, terrain, depth zone, ecology health.
  cellData(healthFn) {
    const d = new Uint8Array(this.n * 4);
    for (let c = 0; c < this.n; ++c) {
      d[c * 4] = this.L.biome[c];
      d[c * 4 + 1] = this.L.terrain[c];
      d[c * 4 + 2] = this.L.depthZone[c];
      d[c * 4 + 3] = Math.round(Math.max(0, Math.min(1, healthFn(c))) * 255);
    }
    return d;
  }
}
