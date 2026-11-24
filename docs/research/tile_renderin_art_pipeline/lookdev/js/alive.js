// "More alive" + "better looking" layer for the look-dev page:
//   YearClock  - continuous day of year; season weights blend smoothly instead of snapping.
//   FireSim    - per-cell wildfire CA (fuel = biome x ecology, wind-biased spread, rain douses, scars recover).
//   Fauna      - herds walking the grasslands and bird flocks circling, as instanced ink sprites.
//   PostFX     - full-screen pass: tilt-shift, aerial haze + painted sky, inked depth silhouettes.
// None of these use art beyond tiny SVG sprites; all are driven by cell data or stand-ins for A1/A4.
"use strict";

const DAYS_PER_YEAR = 364;          // 4 seasons x 91 days
const SEASON_DAYS = DAYS_PER_YEAR / 4;

const YearClock = {
  // Weights for Bloom, Hunt, Dry, Freeze: each season holds, then cross-fades over the last 30% of it.
  weights(day) {
    const s = ((day % DAYS_PER_YEAR) + DAYS_PER_YEAR) % DAYS_PER_YEAR / SEASON_DAYS;
    const i = Math.floor(s) % 4, f = s - Math.floor(s);
    const t = f < 0.7 ? 0 : (f - 0.7) / 0.3, k = t * t * (3 - 2 * t);
    const w = [0, 0, 0, 0];
    w[i] += 1 - k; w[(i + 1) % 4] += k;
    return w;
  },
  dominant(w) { let b = 0; for (let i = 1; i < 4; ++i) if (w[i] > w[b]) b = i; return b; },
  // Seasonal drivers for the "living year": snow, lake level, river state, fire risk.
  drivers(w) {
    return {
      snow: w[3] * 0.85 + w[0] * 0.25,
      lake: 0.5 + w[0] * 0.25 - w[2] * 0.3 - w[3] * 0.05,
      flow: w[3] > 0.6 ? 4 : w[0] > 0.6 ? 3 : w[2] > 0.6 ? 1 : 2,
      fireRisk: w[2],
    };
  },
};

function blendPalettes(w) {
  const out = new Float32Array(90);
  SEASONS.forEach((s, i) => { if (w[i] > 0) { const p = seasonalPalette(s); for (let k = 0; k < 90; ++k) out[k] += p[k] * w[i]; } });
  return out;
}

// ---- Wildfire ----------------------------------------------------------------------------------------
const FUEL = { 2: 0.9, 3: 1.0, 4: 0.25, 5: 0.15, 6: 0.35, 7: 0.4, 8: 0.45, 9: 0.1 };

class FireSim {
  constructor(world) {
    this.world = world;
    this.burn = new Float32Array(world.n);   // 0..1 flame intensity
    this.scar = new Float32Array(world.n);   // 1 = freshly burned, recovers toward 0
    this.age = new Float32Array(world.n);
    this.tex = new Float32Array(world.n * 4);
    this.acc = 0; this.rnd = mulberry(99);
    this.active = 0;
  }
  ignite(c) { if (c >= 0 && this.world.water[c] === 0) { this.burn[c] = Math.max(this.burn[c], 0.4); this.age[c] = 0; } }
  igniteNear(x, y) {
    const W = this.world;
    let best = -1, bd = 1e9;
    for (let c = 0; c < W.n; ++c) {
      if (W.water[c] !== 0 || (FUEL[W.L.biome[c]] || 0) < 0.6) continue;
      const d = (W.cx[c] - x) ** 2 + (W.cy[c] - y) ** 2;
      if (d < bd) { bd = d; best = c; }
    }
    this.ignite(best);
  }
  // recoverYears: how many game-years a scar takes to heal at the current clock speed.
  update(dt, healthOf, weather, windDir, wind, daysPerSec, fireRisk, autoIgnite) {
    const W = this.world, rnd = this.rnd;
    this.acc += dt;
    const step = 0.12;
    const wx = Math.cos(windDir * Math.PI / 180), wy = Math.sin(windDir * Math.PI / 180);
    while (this.acc >= step) {
      this.acc -= step;
      if (autoIgnite && rnd() < fireRisk * fireRisk * 0.05) {
        const c = Math.floor(rnd() * W.n);
        if (W.water[c] === 0 && (FUEL[W.L.biome[c]] || 0) > 0.6 && healthOf(c) > 0.4) this.ignite(c);
      }
      let active = 0;
      const next = this.burn.slice();
      for (let c = 0; c < W.n; ++c) {
        const b = this.burn[c];
        if (b <= 0) continue;
        active++;
        this.age[c] += step;
        const wet = weather ? weather.cell[c * 4 + 1] : 0;
        // Flames build, peak, then burn out; rain shortens them.
        next[c] = this.age[c] < 1.2 ? Math.min(1, b + 0.35) : Math.max(0, b - (0.22 + wet * 0.8));
        if (next[c] <= 0) { this.scar[c] = 1; continue; }
        if (b < 0.5) continue;
        for (let d = 0; d < 6; ++d) {
          const m = W.nb[c * 6 + d];
          if (m < 0 || W.water[m] !== 0 || this.burn[m] > 0 || this.scar[m] > 0.4) continue;
          const fuel = (FUEL[W.L.biome[m]] || 0) * (0.3 + 0.7 * healthOf(m));
          const dx = W.cx[m] - W.cx[c], dy = W.cy[m] - W.cy[c], dl = Math.hypot(dx, dy);
          const downwind = 1 + (dx * wx + dy * wy) / dl * (0.5 + wind * 1.5);
          const wetM = weather ? weather.cell[m * 4 + 1] : 0;
          const p = 0.16 * fuel * Math.max(0, downwind) * (1 - wetM) * (0.6 + fireRisk);
          if (rnd() < p) { next[m] = 0.3; this.age[m] = 0; }
        }
      }
      this.burn = next;
      this.active = active;
    }
    // Scars recover over ~3 game-years: visible as a fade back through the ecology bands.
    const heal = dt * daysPerSec / (DAYS_PER_YEAR * 3);
    for (let c = 0; c < W.n; ++c) {
      if (this.scar[c] > 0 && this.burn[c] <= 0) this.scar[c] = Math.max(0, this.scar[c] - heal);
      this.tex[c * 4] = this.burn[c]; this.tex[c * 4 + 1] = this.scar[c];
    }
  }
}

// ---- Fauna -------------------------------------------------------------------------------------------
class Fauna {
  constructor(world) {
    this.world = world;
    const rnd = mulberry(7);
    this.herds = [];
    const grazing = [];
    for (let c = 0; c < world.n; ++c) if (world.water[c] === 0 && [2, 7, 4].includes(world.L.biome[c])) grazing.push(c);
    for (let k = 0; k < 7 && grazing.length; ++k) {
      const c = grazing[Math.floor(rnd() * grazing.length)];
      const members = [];
      const n = 10 + Math.floor(rnd() * 14);
      for (let i = 0; i < n; ++i) members.push({ ox: (rnd() - 0.5) * 2.2, oy: (rnd() - 0.5) * 1.4, ph: rnd() * 6.28 });
      this.herds.push({ x: world.cx[c], y: world.cy[c], heading: rnd() * 6.28, members, seed: rnd() * 100 });
    }
    this.flocks = [];
    for (let k = 0; k < 5; ++k) {
      const birds = [];
      for (let i = 0; i < 7; ++i) birds.push({ o: i - 3, ph: rnd() * 6.28 });
      this.flocks.push({ cx: rnd() * world.extentX, cy: rnd() * world.extentY, r: 6 + rnd() * 10, a: rnd() * 6.28, sp: (0.08 + rnd() * 0.08) * (rnd() < 0.5 ? -1 : 1), birds });
    }
  }
  landAt(x, y) { const c = this.world.cellAtWarped(x, y); return c >= 0; }
  update(dt, time, fire) {
    const W = this.world;
    for (const h of this.herds) {
      // Wander along a slow noise field; steer away from water and fire.
      const n = (vnoise(h.x * 0.08 + h.seed, h.y * 0.08 + time * 0.03) - 0.5) * 2.4;
      h.heading += n * dt * 0.8;
      let nx = h.x + Math.cos(h.heading) * 0.7 * dt, ny = h.y + Math.sin(h.heading) * 0.7 * dt;
      const ahead = W.cellAtWarped(h.x + Math.cos(h.heading) * 1.5, h.y + Math.sin(h.heading) * 1.5);
      const burning = ahead >= 0 && fire && (fire.burn[ahead] > 0 || fire.scar[ahead] > 0.6);
      if (ahead < 0 || burning) { h.heading += Math.PI * 0.6 * dt * 6; nx = h.x; ny = h.y; }
      h.x = nx; h.y = ny;
    }
    for (const f of this.flocks) f.a += f.sp * dt * 6;
  }
  instances(time) {
    const out = [], W = this.world;
    const body = hexToRgb("#6A4E36"), belly = hexToRgb("#C8B08A");
    for (const h of this.herds) {
      const ch = Math.cos(h.heading), sh = Math.sin(h.heading);
      for (const m of h.members) {
        const x = h.x + m.ox * ch - m.oy * sh + Math.sin(time * 0.6 + m.ph) * 0.08;
        const y = h.y + m.ox * sh + m.oy * ch + Math.cos(time * 0.5 + m.ph) * 0.06;
        if (W.cellAtWarped(x, y) < 0) continue;
        out.push(x, y, W.elevAt(x, y), 0.32, 12, 1, m.ph, ch < 0 ? 1 : 0, ...body, ...belly, ...body);
      }
    }
    const ink = hexToRgb("#2A2420");
    for (const f of this.flocks) {
      const fx = f.cx + Math.cos(f.a) * f.r, fy = f.cy + Math.sin(f.a) * f.r * 0.6;
      const dir = f.a + Math.PI / 2 * Math.sign(f.sp), dx = Math.cos(dir), dy = Math.sin(dir);
      for (const b of f.birds) {
        const back = Math.abs(b.o) * 0.35, side = b.o * 0.35;
        const x = fx - dx * back - dy * side, y = fy - dy * back + dx * side;
        out.push(x, y, 0, 0.42, 13, 1, b.ph, 0, ...ink, ...ink, ...ink);
      }
    }
    return new Float32Array(out);
  }
}

// ---- Post pass ---------------------------------------------------------------------------------------
class PostFX {
  constructor(gl) {
    this.gl = gl;
    this.prog = glCompile(gl, POST_VS, POST_FS, "post");
    this.w = 0; this.h = 0;
    this.vao = gl.createVertexArray();
  }
  ensure(w, h) {
    const gl = this.gl;
    if (w === this.w && h === this.h) return;
    this.w = w; this.h = h;
    if (this.fbo) { gl.deleteFramebuffer(this.fbo); gl.deleteTexture(this.color); gl.deleteTexture(this.depth); }
    this.color = glTexture(gl, { w, h, internal: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE, data: null, filter: gl.LINEAR });
    this.depth = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this.depth);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.DEPTH_COMPONENT24, w, h, 0, gl.DEPTH_COMPONENT, gl.UNSIGNED_INT, null);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    this.fbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.color, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, this.depth, 0);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }
  // view: {x, w, H, near, far, persp, camDist}
  draw(view, s, time) {
    const gl = this.gl, u = this.prog.u;
    gl.viewport(view.x, 0, view.w, view.H);
    gl.disable(gl.DEPTH_TEST);
    gl.useProgram(this.prog.prog);
    gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, this.color); gl.uniform1i(u.tColor, 0);
    gl.activeTexture(gl.TEXTURE1); gl.bindTexture(gl.TEXTURE_2D, this.depth); gl.uniform1i(u.tDepth, 1);
    gl.uniform2f(u.uFull, this.w, this.h);
    gl.uniform4f(u.uRect, view.x, 0, view.w, view.H);
    gl.uniform2f(u.uNearFar, view.near, view.far);
    gl.uniform1f(u.uPersp, view.persp ? 1 : 0);
    gl.uniform1f(u.uCamDist, view.camDist);
    gl.uniform1f(u.uTilt, s.tiltShift ? s.tiltK : 0);
    gl.uniform1f(u.uFocus, s.focusY);
    gl.uniform1f(u.uHaze, view.persp && s.haze ? s.hazeK : 0);
    gl.uniform1f(u.uSky, view.persp && s.sky ? 1 : 0);
    gl.uniform1f(u.uEdges, view.persp && s.silhouettes ? 1 : 0);
    gl.uniform1f(u.uBoil, s.boil ? Math.floor(time * 12) : 0);
    gl.uniform1f(u.uTime, time);
    gl.uniform1i(u.uSeason, YearClock.dominant(s.seasonW || [0, 1, 0, 0]));
    gl.bindVertexArray(this.vao);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
    gl.bindVertexArray(null);
    gl.enable(gl.DEPTH_TEST);
  }
}

const POST_VS = `#version 300 es
precision highp float;
out vec2 vUV;
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vUV = p;
  gl_Position = vec4(p * 2. - 1., 0., 1.);
}`;

const POST_FS = `#version 300 es
precision highp float;
in vec2 vUV;
out vec4 frag;
uniform sampler2D tColor, tDepth;
uniform vec2 uFull, uNearFar;
uniform vec4 uRect;
uniform float uPersp, uCamDist, uTilt, uFocus, uHaze, uSky, uEdges, uBoil, uTime;
uniform int uSeason;
const vec3 INK = vec3(.165, .141, .125);
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float vn(vec2 p) { vec2 i = floor(p), f = fract(p); vec2 u = f * f * (3. - 2. * f);
  return mix(mix(h21(i), h21(i + vec2(1, 0)), u.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), u.x), u.y); }
float fbm(vec2 p) { return .5 * vn(p) + .3 * vn(p * 2.07 + 17.) + .2 * vn(p * 4.13 + 5.); }
float linDepth(float d) {
  float n = uNearFar.x, f = uNearFar.y;
  if (uPersp > .5) { float z = d * 2. - 1.; return 2. * n * f / (f + n - z * (f - n)); }
  return n + d * (f - n);
}
vec2 fullUV(vec2 px) { return px / uFull; }
void main() {
  vec2 px = uRect.xy + vUV * uRect.zw;          // pixel in the full framebuffer
  vec2 uv = fullUV(px);
  float d = texture(tDepth, uv).r;
  bool sky = d >= .99999;
  float z = linDepth(d);
  float vy = vUV.y;                              // 0 bottom .. 1 top of this view

  // Tilt-shift: sharp focus band, blur grows above and below (miniature / diorama look).
  float blur = uTilt * smoothstep(.12, .55, abs(vy - uFocus)) * 7.;
  vec3 col = vec3(0.);
  if (blur > .3) {
    float wsum = 0.;
    for (int i = 0; i < 16; ++i) {
      float a = float(i) * 2.39996, r = sqrt((float(i) + .5) / 16.) * blur;
      vec2 o = vec2(cos(a), sin(a)) * r;
      col += texture(tColor, fullUV(px + o)).rgb; wsum += 1.;
    }
    col /= wsum;
  } else col = texture(tColor, uv).rgb;

  // Painted sky over the horizon (3D only): paper-toned gradient with soft inked cloud banks.
  if (sky && uSky > .5) {
    vec3 top = uSeason == 3 ? vec3(.74, .80, .86) : vec3(.66, .79, .86);
    vec3 hor = uSeason == 2 ? vec3(.95, .88, .74) : vec3(.94, .91, .84);
    col = mix(hor, top, smoothstep(.0, .5, vy - .45));
    float c = fbm(vec2(vUV.x * 4. + uTime * .01, vy * 9.));
    float band = smoothstep(.55, .62, c) * smoothstep(1., .5, vy);
    col = mix(col, vec3(.98, .97, .94), band * .8);
    float edge = smoothstep(.54, .56, c) - smoothstep(.56, .58, c);
    col = mix(col, INK, edge * .35 * smoothstep(1., .5, vy));
  }

  // Aerial perspective: distant land fades into a paper-blue haze, the way a painted map recedes.
  if (!sky && uHaze > 0.) {
    float t = 1. - exp(-max(0., z - uCamDist * .75) / (uCamDist * 1.1));
    vec3 haze = uSeason == 3 ? vec3(.80, .85, .88) : vec3(.84, .86, .82);
    col = mix(col, haze, clamp(t * uHaze, 0., .85));
  }

  // Inked silhouettes from depth. 1/z is linear across any flat surface in screen space, so its second
  // difference is zero on water and plains and only spikes at real occlusion edges (ridges, hill fronts).
  // The sign test keeps the ink on the near side of the edge.
  if (uEdges > .5 && !sky) {
    vec2 j = (vec2(vn(px * .35 + uBoil * 3.1), vn(px * .35 + 7. + uBoil * 2.3)) - .5) * .8;
    float w0 = 1. / z, e = 0.;
    for (int a = 0; a < 2; ++a) {
      vec2 o = (a == 0 ? vec2(1.5, 0.) : vec2(0., 1.5)) + j;
      float d1 = texture(tDepth, fullUV(px + o)).r, d2 = texture(tDepth, fullUV(px - o)).r;
      float w1 = d1 >= .99999 ? 0. : 1. / linDepth(d1);
      float w2 = d2 >= .99999 ? 0. : 1. / linDepth(d2);
      float lap = (2. * w0 - w1 - w2) / w0;      // > 0: this pixel is in front of its neighbours
      e = max(e, smoothstep(.025, .06, lap));
    }
    float wgt = 1. - min(blur / 7., .7);
    col = mix(col, INK, e * .85 * wgt);
  }
  // Soft vignette like a printed plate.
  vec2 q = vUV - .5;
  col *= 1. - dot(q, q) * .35;
  frag = vec4(col, 1.);
}`;
