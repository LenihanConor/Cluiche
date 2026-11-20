// A4 stand-in: localised weather. Storm fronts drift with the wind and produce per-cell cloud, precipitation,
// snow fraction (from each cell's own temperature) and gust. This is what A4's per-cell CA will publish; the
// renderer only reads the per-cell texture, so swapping in real A4 data changes nothing downstream.
// GDD: TileRenderingPipeline 7.2 (particle emitter per cell, intensity and drift per cell), A4 smoothed state.
"use strict";

class WeatherSim {
  constructor(world) {
    this.world = world;
    this.storms = [];
    this.cell = new Float32Array(world.n * 4);   // cloud, precip, snowFrac, gust
    this.seed = 1;
  }
  syncStorms(count) {
    const W = this.world, rnd = mulberry(this.seed * 977 + count);
    while (this.storms.length < count)
      this.storms.push({ x: rnd() * W.extentX, y: rnd() * W.extentY, r: 5 + rnd() * 7, i: 0.6 + rnd() * 0.4, ph: rnd() * 6.28 });
    this.storms.length = count;
  }
  update(dt, s, seasonIdx, time) {
    const W = this.world;
    this.syncStorms(s.storms);
    const wd = s.windDir * Math.PI / 180, sp = (0.4 + s.wind * 3.2) * (s.stormsMove ? 1 : 0);
    for (const st of this.storms) {
      st.x += Math.cos(wd) * sp * dt; st.y += Math.sin(wd) * sp * dt;
      // Wrap around the map so the sky never empties.
      if (st.x > W.extentX + st.r) st.x = -st.r; if (st.x < -st.r) st.x = W.extentX + st.r;
      if (st.y > W.extentY + st.r) st.y = -st.r; if (st.y < -st.r) st.y = W.extentY + st.r;
    }
    const seasonT = [0.03, 0.06, 0.1, -0.17][seasonIdx];
    const L = W.L;
    for (let c = 0; c < W.n; ++c) {
      let g = 0;
      for (const st of this.storms) {
        const dx = W.cx[c] - st.x, dy = W.cy[c] - st.y;
        g += st.i * (0.85 + 0.15 * Math.sin(time * 0.7 + st.ph)) * Math.exp(-(dx * dx + dy * dy) / (st.r * st.r));
      }
      const orographic = W.water[c] === 0 ? L.elevation[c] * 0.35 : 0;   // mountains wring out more rain
      const cloud = Math.min(1, s.cloud * 0.45 + g * 1.05 + orographic * g);
      let precip = Math.max(0, Math.min(1, (g + orographic * g - 0.3) / 0.6)) * s.precipI;
      const t = L.temperature[c] + seasonT;
      let snow = Math.max(0, Math.min(1, (0.45 - t) / 0.12));
      if (s.precip === "rain") snow = 0; else if (s.precip === "snow") snow = 1; else if (s.precip === "none") precip = 0;
      this.cell.set([cloud, precip, snow, Math.min(1, g)], c * 4);
    }
  }
  at(x, y) { // nearest cell value for CPU particle spawning
    const c = this.world.pointToCell(x, y);
    return c < 0 ? null : c;
  }
}

// World-space precipitation particles: rain as short streaks, snow as flakes, only where the cell has precip.
class Precip {
  constructor(gl, max) {
    this.gl = gl; this.max = max;
    this.p = new Float32Array(max * 6);     // x, y, height above ground, ground elev, kind (0 rain 1 snow, -1 dead), seed
    for (let i = 0; i < max; ++i) this.p[i * 6 + 4] = -1;
    this.verts = new Float32Array(max * 2 * 5);
    this.buf = gl.createBuffer();
    this.prog = glCompile(gl, PRECIP_VS, PRECIP_FS, "precip");
    this.vao = gl.createVertexArray();
    gl.bindVertexArray(this.vao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.buf);
    gl.bufferData(gl.ARRAY_BUFFER, this.verts.byteLength, gl.DYNAMIC_DRAW);
    const a = gl.getAttribLocation(this.prog.prog, "aP"), b = gl.getAttribLocation(this.prog.prog, "aK");
    gl.enableVertexAttribArray(a); gl.vertexAttribPointer(a, 4, gl.FLOAT, false, 20, 0);
    gl.enableVertexAttribArray(b); gl.vertexAttribPointer(b, 1, gl.FLOAT, false, 20, 16);
    gl.bindVertexArray(null);
    this.rnd = mulberry(4242);
    this.nRain = 0; this.nSnow = 0;
  }
  update(dt, world, weather, s) {
    const P = this.p, rnd = this.rnd, wd = s.windDir * Math.PI / 180;
    const wx = Math.cos(wd), wy = Math.sin(wd);
    let spawnBudget = 1500;
    for (let i = 0; i < this.max; ++i) {
      const o = i * 6;
      if (P[o + 4] < 0) {
        if (spawnBudget-- <= 0 || weather.storms.length === 0) continue;
        // Sample around a storm (where precipitation can be), then accept by the cell's own precip.
        const st = weather.storms[Math.floor(rnd() * weather.storms.length)];
        const ang = rnd() * 6.2832, rad = Math.sqrt(-Math.log(1 - rnd() * 0.98)) * st.r * 0.7;
        const x = st.x + Math.cos(ang) * rad, y = st.y + Math.sin(ang) * rad;
        const c = weather.at(x, y);
        if (c === null) continue;
        const pr = weather.cell[c * 4 + 1];
        if (rnd() > pr) continue;
        P[o] = x; P[o + 1] = y; P[o + 2] = 2 + rnd() * 4; P[o + 3] = Math.max(0, world.elevAt(x, y));
        P[o + 4] = rnd() < weather.cell[c * 4 + 2] ? 1 : 0; P[o + 5] = rnd();
        continue;
      }
      if (P[o + 4] > 1.5) {   // smoke: rises and drifts downwind, then fades
        P[o] += wx * (0.6 + s.wind * 2) * dt; P[o + 1] += wy * (0.6 + s.wind * 2) * dt; P[o + 2] += 1.4 * dt;
        if (P[o + 2] > 7) P[o + 4] = -1;
        continue;
      }
      const snow = P[o + 4] > 0.5;
      const gust = 0.5 + s.wind * 1.5;
      const fall = snow ? 1.1 : 9, drift = snow ? 0.9 * gust : 1.6 * gust;
      P[o] += (wx * drift + (snow ? Math.sin(P[o + 5] * 40 + P[o + 2] * 2) * 0.4 : 0)) * dt;
      P[o + 1] += wy * drift * dt;
      P[o + 2] -= fall * dt;
      if (P[o + 2] <= 0) P[o + 4] = -1;
    }
  }
  // Smoke from burning cells (wildfire): spawned at the flame front, ink-wash grey.
  spawnSmoke(world, fire) {
    if (!fire || fire.active === 0) return;
    const P = this.p, rnd = this.rnd;
    let budget = 60;
    for (let i = 0; i < this.max && budget > 0; ++i) {
      const o = i * 6;
      if (P[o + 4] >= 0) continue;
      const c = Math.floor(rnd() * world.n);
      if (fire.burn[c] < 0.3) continue;
      P[o] = world.cx[c] + (rnd() - 0.5) * 1.2; P[o + 1] = world.cy[c] + (rnd() - 0.5) * 1.2;
      P[o + 2] = 0.3; P[o + 3] = Math.max(0, world.elevAt(P[o], P[o + 1])); P[o + 4] = 2; P[o + 5] = rnd();
      budget--;
    }
  }
  draw(cam, mode, s, world) {
    const gl = this.gl, P = this.p, V = this.verts;
    let nr = 0;
    const wd = s.windDir * Math.PI / 180, gust = 0.5 + s.wind * 1.5;
    const tx = Math.cos(wd) * 1.6 * gust * 0.05, ty = Math.sin(wd) * 1.6 * gust * 0.05;
    // Rain: line segments (head + tail).
    for (let i = 0; i < this.max; ++i) {
      const o = i * 6;
      if (P[o + 4] !== 0) continue;
      const base = nr * 10;
      V.set([P[o], P[o + 1], P[o + 2], P[o + 3], 0, P[o] - tx, P[o + 1] - ty, P[o + 2] + 0.45, P[o + 3], 0], base);
      ++nr;
    }
    let ns = 0;
    const snowOff = nr * 10;
    for (let i = 0; i < this.max; ++i) {
      const o = i * 6;
      if (P[o + 4] < 0.5) continue;
      V.set([P[o], P[o + 1], P[o + 2], P[o + 3], P[o + 4] > 1.5 ? 2 + Math.min(1, P[o + 2] / 7) : 1], snowOff + ns * 5);
      ++ns;
    }
    this.nRain = nr; this.nSnow = ns;
    if (nr + ns === 0) return;
    gl.useProgram(this.prog.prog);
    gl.uniformMatrix4fv(this.prog.u.uViewProj, false, cam.vp);
    gl.uniform1f(this.prog.u.uMode, mode === "2d" ? 0 : 1);
    gl.uniform1f(this.prog.u.uFake, s.fake); gl.uniform1f(this.prog.u.uHeight, s.height);
    gl.uniform1f(this.prog.u.uPointPx, Math.max(2, Math.min(6, 90 / s.cam.zoom)));
    gl.uniform2f(this.prog.u.uExtent, world.extentX, world.extentY);
    gl.bindVertexArray(this.vao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.buf);
    gl.bufferSubData(gl.ARRAY_BUFFER, 0, V, 0, snowOff + ns * 5);
    gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
    gl.depthMask(false);
    if (nr) gl.drawArrays(gl.LINES, 0, nr * 2);
    if (ns) gl.drawArrays(gl.POINTS, nr * 2, ns);
    gl.depthMask(true);
    gl.disable(gl.BLEND);
    gl.bindVertexArray(null);
  }
}

const PRECIP_VS = `#version 300 es
precision highp float;
in vec4 aP;   // x, y, height above ground, ground elevation
in float aK;  // 0 rain, 1 snow
uniform mat4 uViewProj;
uniform float uMode, uFake, uHeight, uPointPx;
uniform vec2 uExtent;
out float vK;
void main() {
  vec3 pos = uMode < .5 ? vec3(aP.x, aP.y - aP.w * uFake - aP.z * .35, 0.) : vec3(aP.x, aP.y, -(aP.w * uHeight + aP.z));
  gl_Position = uViewProj * vec4(pos, 1.);
  if (uMode < .5) gl_Position.z = -.95;
  gl_PointSize = aK > 1.5 ? uPointPx * (3. + (aK - 2.) * 6.) : uPointPx;
  vK = aK;
}`;
const PRECIP_FS = `#version 300 es
precision highp float;
in float vK;
out vec4 frag;
void main() {
  if (vK > 1.5) {
    float r = length(gl_PointCoord - .5) * 2.;
    if (r > 1.) discard;
    float age = vK - 2.;
    frag = vec4(mix(vec3(.30, .28, .28), vec3(.62, .60, .58), age), (1. - r) * .35 * (1. - age));
  } else if (vK > .5) {
    vec2 d = gl_PointCoord - .5;
    float r = length(d);
    if (r > .5) discard;
    vec3 c = r > .36 ? vec3(.165, .141, .125) : vec3(.97, .98, 1.);
    frag = vec4(c, .95);
  } else {
    frag = vec4(.22, .26, .32, .55);
  }
}`;
