// Look-dev app: loads an S-1 world, builds field textures, renders 2D / 3D / split, drives every channel from UI.
"use strict";

// GDD ColourPaletteAndLighting Part 1 base palette [Primary, Secondary, Accent], indexed by S-1 Biome enum.
const BASE_PAL = [
  ["#2A7888", "#4AB8C8", "#4AB8C8"],   // 0 Ocean (water colours handled in shader)
  ["#4A9EA8", "#38B4C8", "#8A7850"],   // 1 Lake
  ["#D4B84A", "#8DB558", "#7AB8C8"],   // 2 Grassland / Steppe
  ["#4A7A3C", "#2E5228", "#8A6E3A"],   // 3 Temperate Forest
  ["#5A8C6E", "#4A9EA8", "#8A7850"],   // 4 Wetland / Delta
  ["#C87840", "#D4C89A", "#8C6840"],   // 5 Desert / Dry Basin
  ["#7A8C9E", "#E8EEF2", "#2E4A32"],   // 6 Alpine / Mountain
  ["#8A9EA8", "#B0C4A4", "#D4DCE0"],   // 7 Tundra / Boreal
  ["#2E6B3A", "#1A4A28", "#4A7A5A"],   // 8 Rainforest
  ["#2E2830", "#7A2818", "#8A8480"],   // 9 Volcanic / Ashland
];
const SEASONS = ["Bloom", "Hunt", "Dry", "Freeze"];

function seasonalPalette(season) {
  const pal = BASE_PAL.map((t) => t.slice());
  if (season === "Bloom") { pal[2][0] = "#A8C858"; pal[4][1] = "#38B4C8"; pal[7][0] = "#8AB890"; pal[9][2] = "#4A5A28"; }
  if (season === "Dry") { pal[2][0] = "#B07830"; pal[4][1] = "#8A7850"; pal[8][1] = "#6A7A30"; }
  if (season === "Freeze") { pal[2][0] = "#A89040"; pal[4][1] = "#6A8878"; pal[7][0] = "#E4EEF2"; pal[9] = ["#1A1418", "#8A1A08", "#8A8480"]; }
  const f = new Float32Array(90);
  pal.forEach((t, b) => t.forEach((h, k) => f.set(hexToRgb(h), (b * 3 + k) * 3)));
  return f;
}

const state = {
  world: "continental_seed1", view: "split",
  fake: 1.2, pitch: 55, yaw: 0, height: 6, peak3d: 0.6,
  season: "Hunt", animSeason: false,
  health: 0.66, variation: 0.7, density: 1,
  flow: 2, lake: 0.5, flood: false, ice: false,
  cloud: 0.25, clouds: true, cloudSprites: true, storms: 3, stormsMove: true, precip: "auto", precipI: 0.8, windDir: 35, wind: 0.5, snow: 0.15,
  ink: true, inkW: 1, hatch: true, grain: true, toon: 0.6, shade: 0.7, grade: true, gradeK: 1.5, zone: true,
  hex: false, marks: true, vegetation: true, peaks: false, debug: 0,
  // Time & life
  yearRun: true, yearSpeed: 12, day: 120, livingYear: true, timelapse: false, fauna: true, fireAuto: true,
  // Presentation
  post: true, tiltShift: true, tiltK: 1, focusY: 0.42, haze: true, hazeK: 1, sky: true, silhouettes: true,
  formLines: true, contours: false, boil: true,
  seasonW: [0, 1, 0, 0],
  cam: null,
};

// URL hash overrides (also used for headless screenshots): #view=3d&season=Freeze&zoom=12&tx=40&ty=30
const hashArgs = Object.fromEntries(location.hash.slice(1).split("&").filter(Boolean).map((kv) => kv.split("=").map(decodeURIComponent)));
for (const [k, v] of Object.entries(hashArgs)) {
  if (!(k in state)) continue;
  const cur = state[k];
  state[k] = typeof cur === "number" ? parseFloat(v) : typeof cur === "boolean" ? v === "1" || v === "true" : v;
}

const canvas = document.getElementById("gl");
const gl = canvas.getContext("webgl2", { antialias: true, preserveDrawingBuffer: true });
if (!gl) { document.body.innerHTML = "<h2>WebGL2 is not available in this browser.</h2>"; throw new Error("no webgl2"); }

let groundP, propP, world = null, tex = {}, groundVAO, groundCount = 0, propVAO, propBuf, propCount = 0, atlasRows = 3;
let worldLoading = false, skirtVAO = null;
let weather = null, precip = null, cloudBuf = null, cloudCount = 0, lastFrame = 0;
let fire = null, fauna = null, faunaBuf = null, faunaCount = 0, post = null, ecoOffset = 0, ecoTimer = 0, uiTimer = 0;

function initGL() {
  groundP = glCompile(gl, GROUND_VS, GROUND_FS, "ground");
  propP = glCompile(gl, PROP_VS, PROP_FS, "prop");
  buildPropAtlas((cv, rows) => {
    atlasRows = rows;
    tex.atlas = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex.atlas);
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, cv);
    gl.generateMipmap(gl.TEXTURE_2D);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    document.getElementById("atlasPreview").src = cv.toDataURL();
  });
  // Prop quad + instance buffer.
  propVAO = gl.createVertexArray();
  gl.bindVertexArray(propVAO);
  const quad = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, quad);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-.5, 0, .5, 0, .5, 1, -.5, 0, .5, 1, -.5, 1]), gl.STATIC_DRAW);
  const aC = gl.getAttribLocation(propP.prog, "aCorner");
  gl.enableVertexAttribArray(aC); gl.vertexAttribPointer(aC, 2, gl.FLOAT, false, 0, 0);
  propBuf = gl.createBuffer();
  cloudBuf = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, propBuf);
  setPropAttribs();
  gl.bindVertexArray(null);
}

// Instance attributes read from whichever buffer is bound (terrain props or storm clouds).
function setPropAttribs() {
  const attrs = [["iA", 4, 0], ["iB", 4, 4], ["iP", 3, 8], ["iS", 3, 11], ["iC", 3, 14]];
  for (const [name, size, off] of attrs) {
    const loc = gl.getAttribLocation(propP.prog, name);
    gl.enableVertexAttribArray(loc);
    gl.vertexAttribPointer(loc, size, gl.FLOAT, false, PROP_STRIDE * 4, off * 4);
    gl.vertexAttribDivisor(loc, 1);
  }
}
// A few cloud puffs per storm, sized by its intensity, drifting with it.
function buildFauna(time) {
  if (!faunaBuf) faunaBuf = gl.createBuffer();
  const f = fauna.instances(time);
  gl.bindBuffer(gl.ARRAY_BUFFER, faunaBuf);
  gl.bufferData(gl.ARRAY_BUFFER, f, gl.DYNAMIC_DRAW);
  faunaCount = f.length / PROP_STRIDE;
}
function buildClouds() {
  const out = [];
  const W = hexToRgb("#F4F2EC"), G = hexToRgb("#C8CCD0");
  weather.storms.forEach((st, k) => {
    const rnd = mulberry(k * 131 + 7);
    const puffs = 3 + Math.floor(st.i * 3);
    for (let j = 0; j < puffs; ++j) {
      const a = rnd() * 6.28, r = rnd() * st.r * 0.45;
      const x = st.x + Math.cos(a) * r, y = st.y + Math.sin(a) * r * 0.6;
      out.push(x, y, 0, st.r * (0.35 + rnd() * 0.25) * st.i, 11, 1, rnd(), 0, ...W, ...G, ...G);
    }
  });
  const f = new Float32Array(out);
  gl.bindBuffer(gl.ARRAY_BUFFER, cloudBuf);
  gl.bufferData(gl.ARRAY_BUFFER, f, gl.DYNAMIC_DRAW);
  cloudCount = f.length / PROP_STRIDE;
}

function loadWorld(name) {
  if (worldLoading) return;
  worldLoading = true;
  setStatus("Loading " + name + " ...");
  const go = () => {
    const t0 = performance.now();
    world = new World(window.WORLDS[name], 14);
    weather = new WeatherSim(world);
    fire = new FireSim(world);
    fauna = new Fauna(world);
    if (!precip) precip = new Precip(gl, 6000);
    buildWorldGPU();
    rebuildProps();
    if (!state.cam || state.cam.world !== name) {
      state.cam = { world: name, tx: hashArgs.tx ? +hashArgs.tx : world.extentX / 2, ty: hashArgs.ty ? +hashArgs.ty : world.extentY / 2,
        zoom: hashArgs.zoom ? +hashArgs.zoom : world.extentY * 0.55 };
    }
    worldLoading = false;
    if (hashArgs.ignite) for (let k = 0; k < +hashArgs.ignite; ++k) fire.igniteNear(state.cam.tx + (k - 1) * 4, state.cam.ty);
    setStatus(`${name}: ${world.w}x${world.h} cells, field ${world.TW}x${world.TH}, ${world.riverSegs} river segments, built in ${(performance.now() - t0).toFixed(0)} ms`);
  };
  if (window.WORLDS && window.WORLDS[name]) return go();
  const s = document.createElement("script");
  s.src = "worlds/" + name + ".js";
  s.onload = go;
  s.onerror = () => { worldLoading = false; setStatus("Failed to load worlds/" + name + ".js (run tools/convert_golden.py)"); };
  document.head.appendChild(s);
}

function buildWorldGPU() {
  for (const k of ["cell", "field", "river", "cellData", "edge", "weather", "events"]) if (tex[k]) gl.deleteTexture(tex[k]);
  tex.cell = glTexture(gl, { w: world.TW, h: world.TH, internal: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE, data: world.cellTex, filter: gl.NEAREST });
  tex.field = glTexture(gl, { w: world.TW, h: world.TH, internal: gl.RGBA16F, format: gl.RGBA, type: gl.FLOAT, data: world.field, filter: gl.LINEAR });
  tex.river = glTexture(gl, { w: world.TW, h: world.TH, internal: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE, data: world.river, filter: gl.LINEAR });
  tex.edge = glTexture(gl, { w: world.TW, h: world.TH, internal: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE, data: world.edge, filter: gl.LINEAR });
  tex.events = glTexture(gl, { w: world.w, h: world.h, internal: gl.RGBA16F, format: gl.RGBA, type: gl.FLOAT, data: fire.tex, filter: gl.LINEAR });
  tex.weather = glTexture(gl, { w: world.w, h: world.h, internal: gl.RGBA16F, format: gl.RGBA, type: gl.FLOAT, data: weather.cell, filter: gl.LINEAR });
  updateCellData();
  // Ground grid mesh, 4 vertices per world unit.
  const step = 0.25, nx = Math.ceil(world.extentX / step) + 1, ny = Math.ceil(world.extentY / step) + 1;
  const v = new Float32Array(nx * ny * 2);
  for (let j = 0; j < ny; ++j) for (let i = 0; i < nx; ++i) { v[(j * nx + i) * 2] = Math.min(i * step, world.extentX); v[(j * nx + i) * 2 + 1] = Math.min(j * step, world.extentY); }
  const idx = new Uint32Array((nx - 1) * (ny - 1) * 6);
  let o = 0;
  for (let j = 0; j < ny - 1; ++j) for (let i = 0; i < nx - 1; ++i) {
    const a = j * nx + i, b = a + 1, c = a + nx, d = c + 1;
    idx[o++] = a; idx[o++] = c; idx[o++] = b; idx[o++] = b; idx[o++] = c; idx[o++] = d;
  }
  if (groundVAO) gl.deleteVertexArray(groundVAO);
  groundVAO = gl.createVertexArray();
  gl.bindVertexArray(groundVAO);
  const vb = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, vb); gl.bufferData(gl.ARRAY_BUFFER, v, gl.STATIC_DRAW);
  const loc = gl.getAttribLocation(groundP.prog, "aPos");
  gl.enableVertexAttribArray(loc); gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
  const ib = gl.createBuffer(); gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ib); gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, idx, gl.STATIC_DRAW);
  gl.bindVertexArray(null);
  groundCount = idx.length;
  // Sea skirt to the horizon so tilted views never show sky below the map.
  const M = 400, X0 = -M, Y0 = -M, X1 = world.extentX + M, Y1 = world.extentY + M;
  if (skirtVAO) gl.deleteVertexArray(skirtVAO);
  skirtVAO = gl.createVertexArray();
  gl.bindVertexArray(skirtVAO);
  const sb = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, sb);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([X0, Y0, X1, Y0, X1, Y1, X0, Y0, X1, Y1, X0, Y1]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(loc); gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
  gl.bindVertexArray(null);
}

function healthOf(c) {
  const v = (fbm2(world.cx[c] * 0.11 + 5 + ecoOffset, world.cy[c] * 0.11 + 9 + ecoOffset * 0.6) - 0.5) * 2.2;
  const h = Math.max(0, Math.min(1, state.health + state.variation * v * 0.5));
  return fire ? h * (1 - 0.95 * fire.scar[c]) * (fire.burn[c] > 0.3 ? 0.2 : 1) : h;
}
function baseHealthOf(c) {
  const v = (fbm2(world.cx[c] * 0.11 + 5 + ecoOffset, world.cy[c] * 0.11 + 9 + ecoOffset * 0.6) - 0.5) * 2.2;
  return Math.max(0, Math.min(1, state.health + state.variation * v * 0.5));
}
function updateCellData() {
  if (!world) return;
  if (tex.cellData) gl.deleteTexture(tex.cellData);
  tex.cellData = glTexture(gl, { w: world.w, h: world.h, internal: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE, data: world.cellData(healthOf), filter: gl.NEAREST });
}
function rebuildProps() {
  if (!world) return;
  const inst = placeProps(world, healthOf, state.season, { vegetation: state.vegetation, peaks: state.peaks, density: state.density });
  gl.bindBuffer(gl.ARRAY_BUFFER, propBuf);
  gl.bufferData(gl.ARRAY_BUFFER, inst, gl.DYNAMIC_DRAW);
  propCount = inst.length / PROP_STRIDE;
  document.getElementById("propCount").textContent = propCount.toLocaleString() + " props";
}

// ---- Camera ----
const FOV = 35 * Math.PI / 180;
function camFor(mode, aspect) {
  const c = state.cam;
  if (mode === "2d") {
    const hh = c.zoom;
    const la = M4.lookAt([c.tx, c.ty, -50], [c.tx, c.ty, 0], [0, -1, 0]);
    return { vp: M4.mul(M4.ortho(-hh * aspect, hh * aspect, -hh, hh, 0.1, 200), la.view), right: la.right, up: la.up, near: 0.1, far: 200, persp: false, camDist: 50 };
  }
  const d = c.zoom / Math.tan(FOV / 2);
  const p = Math.max(0.02, state.pitch * Math.PI / 180), y = state.yaw * Math.PI / 180;
  const eye = [c.tx + d * Math.sin(p) * Math.sin(y), c.ty + d * Math.sin(p) * Math.cos(y), -d * Math.cos(p)];
  const la = M4.lookAt(eye, [c.tx, c.ty, 0], [0, 0, -1]);
  return { vp: M4.mul(M4.persp(FOV, aspect, d * 0.02, d * 20), la.view), right: la.right, up: la.up, near: d * 0.02, far: d * 20, persp: true, camDist: d };
}

// ---- Render ----
let t0 = performance.now(), lastSeasonSwap = 0;
function frame(now) {
  requestAnimationFrame(frame);
  const time = (now - t0) / 1000;
  const dt = Math.min(0.05, lastFrame ? (now - lastFrame) / 1000 : 0.016); lastFrame = now;
  // Year clock: continuous day; seasons cross-fade; the living year drives snow, lakes, rivers and fire risk.
  const speed = state.timelapse ? 90 : state.yearSpeed;
  if (state.yearRun) state.day = (state.day + dt * speed) % DAYS_PER_YEAR;
  state.seasonW = YearClock.weights(state.day);
  const dom = SEASONS[YearClock.dominant(state.seasonW)];
  if (dom !== state.season) { state.season = dom; rebuildProps(); }
  const drv = YearClock.drivers(state.seasonW);
  if (state.yearRun && state.livingYear) { state.snow = drv.snow; state.lake = drv.lake; state.flow = drv.flow; }
  if (state.timelapse) ecoOffset += dt * speed / DAYS_PER_YEAR * 0.35;
  if ((uiTimer += dt) > 0.25) { uiTimer = 0; syncUI(); }
  if (world && fire) {
    fire.update(dt, baseHealthOf, weather, state.windDir, state.wind, state.yearRun ? speed : 0, drv.fireRisk, state.fireAuto && state.yearRun);
    gl.bindTexture(gl.TEXTURE_2D, tex.events);
    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, world.w, world.h, gl.RGBA, gl.FLOAT, fire.tex);
    ecoTimer += dt;
    const scarring = fire.active > 0 || fire.scar.some((x) => x > 0.01);
    if (ecoTimer > 0.5 && (scarring || state.timelapse)) { ecoTimer = 0; updateCellData(); rebuildProps(); }
  }
  if (world && fauna && state.fauna) { fauna.update(dt, time, fire); buildFauna(time); } else faunaCount = 0;
  if (world && weather && tex.weather) {
    weather.update(dt, state, SEASONS.indexOf(state.season), time);
    gl.bindTexture(gl.TEXTURE_2D, tex.weather);
    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, world.w, world.h, gl.RGBA, gl.FLOAT, weather.cell);
    precip.update(dt, world, weather, state);
    precip.spawnSmoke(world, fire);
    buildClouds();
  }
  resize();
  gl.clearColor(0.93, 0.89, 0.81, 1);
  gl.disable(gl.SCISSOR_TEST);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  if (!world || !tex.atlas) return;
  const W = canvas.width, H = canvas.height;
  const views = state.view === "split" ? [["2d", 0, W / 2], ["3d", W / 2, W / 2]] : [[state.view, 0, W]];
  // With the post pass on, the scene renders into an offscreen colour+depth target first.
  if (state.post) {
    if (!post) post = new PostFX(gl);
    post.ensure(W, H);
    gl.bindFramebuffer(gl.FRAMEBUFFER, post.fbo);
    gl.disable(gl.SCISSOR_TEST);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  }
  const cams = [];
  for (const [mode, x, w] of views) {
    gl.viewport(x, 0, w, H);
    gl.enable(gl.SCISSOR_TEST); gl.scissor(x, 0, w, H);
    gl.clear(gl.DEPTH_BUFFER_BIT);
    cams.push([mode, x, w, drawView(mode, w / H, time)]);
  }
  gl.disable(gl.SCISSOR_TEST);
  if (state.post) {
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    for (const [mode, x, w, cam] of cams) post.draw({ x, w, H, near: cam.near, far: cam.far, persp: cam.persp, camDist: cam.camDist }, state, time);
  }
  if (precip) document.getElementById("propCount").textContent = `${propCount.toLocaleString()} props, ${precip.nRain} rain, ${precip.nSnow} snow`;
}

function commonUniforms(prog, mode, time) {
  const u = prog.u;
  gl.uniform1f(u.uTime, time);
  gl.uniform1i(u.uSeason, YearClock.dominant(state.seasonW));
  gl.uniform1f(u.uGradeK, state.gradeK); gl.uniform1f(u.uGrade, state.grade ? 1 : 0); gl.uniform1f(u.uGrain, state.grain ? 1 : 0);
  gl.uniform1f(u.uCloud, state.cloud); gl.uniform1f(u.uCloudOn, state.clouds ? 1 : 0); gl.uniform1f(u.uWindSpeed, state.wind);
  const wd = state.windDir * Math.PI / 180;
  gl.uniform2f(u.uWindDir, Math.cos(wd), Math.sin(wd));
  gl.uniform1f(u.uSnow, state.snow);
  gl.uniform2f(u.uExtent, world.extentX, world.extentY);
  gl.uniform1f(u.uMode, mode === "2d" ? 0 : 1);
  gl.uniform1f(u.uFake, state.fake); gl.uniform1f(u.uHeight, state.height);
  gl.uniform2f(u.uGrid, world.w, world.h);
  gl.uniform4fv(u.uSeasonW, state.seasonW);
  gl.uniform1f(u.uBoil, state.boil ? Math.floor(time * 12) : 0); gl.uniform1f(u.uBoilOn, state.boil ? 1 : 0);
}

function bindTex(prog, name, unit, t) { gl.activeTexture(gl.TEXTURE0 + unit); gl.bindTexture(gl.TEXTURE_2D, t); gl.uniform1i(prog.u[name], unit); }

function drawView(mode, aspect, time) {
  const cam = camFor(mode, aspect);
  gl.enable(gl.DEPTH_TEST); gl.depthFunc(gl.LEQUAL);
  // Ground
  gl.useProgram(groundP.prog);
  commonUniforms(groundP, mode, time);
  const u = groundP.u;
  gl.uniformMatrix4fv(u.uViewProj, false, cam.vp);
  gl.uniform2f(u.uTexels, world.TW, world.TH); gl.uniform1f(u.uT, world.T);
  gl.uniform3fv(u.uPal, blendPalettes(state.seasonW));
  gl.uniform1f(u.uInk, state.ink ? 1 : 0); gl.uniform1f(u.uInkW, state.inkW); gl.uniform1f(u.uHatch, state.hatch ? 1 : 0);
  gl.uniform1f(u.uToon, state.toon); gl.uniform1f(u.uShadeK, mode === "2d" ? 3.5 : Math.max(2.5, state.height * 0.6));
  gl.uniform1f(u.uShadeStr, state.shade); gl.uniform1f(u.uZoneTint, state.zone ? 1 : 0); gl.uniform1f(u.uHexGrid, state.hex ? 1 : 0);
  gl.uniform1f(u.uMarks, state.marks ? 1 : 0); gl.uniform1i(u.uDebug, state.debug); gl.uniform1i(u.uFlowState, state.flow);
  gl.uniform1f(u.uFormLines, mode === "3d" && state.formLines ? 1 : 0); gl.uniform1f(u.uContours, state.contours ? 1 : 0);
  gl.uniform1f(u.uLakeLevel, state.lake); gl.uniform1f(u.uIce, state.ice ? 1 : 0); gl.uniform1f(u.uFlood, state.flood ? 1 : 0);
  bindTex(groundP, "tField", 0, tex.field); bindTex(groundP, "tCell", 1, tex.cell); bindTex(groundP, "tCellData", 2, tex.cellData); bindTex(groundP, "tRiver", 3, tex.river); bindTex(groundP, "tEdge", 5, tex.edge); bindTex(groundP, "tWeather", 6, tex.weather); bindTex(groundP, "tEvents", 7, tex.events);
  if (mode === "3d") { gl.bindVertexArray(skirtVAO); gl.drawArrays(gl.TRIANGLES, 0, 6); }
  gl.bindVertexArray(groundVAO);
  gl.drawElements(gl.TRIANGLES, groundCount, gl.UNSIGNED_INT, 0);
  // Props
  if (state.debug === 0 && propCount > 0) {
    gl.useProgram(propP.prog);
    commonUniforms(propP, mode, time);
    gl.uniformMatrix4fv(propP.u.uViewProj, false, cam.vp);
    gl.uniform1f(propP.u.uPeak3D, state.peak3d); gl.uniform3fv(propP.u.uRight, cam.right); gl.uniform3fv(propP.u.uUp, cam.up);
    gl.uniform2f(propP.u.uAtlas, ATLAS_COLS, atlasRows);
    bindTex(propP, "tField", 0, tex.field); bindTex(propP, "tAtlas", 4, tex.atlas); bindTex(propP, "tWeather", 6, tex.weather);
    gl.uniform1f(propP.u.uCloudH, 7); gl.uniform1f(propP.u.uAlpha, 1);
    gl.bindVertexArray(propVAO);
    gl.bindBuffer(gl.ARRAY_BUFFER, propBuf); setPropAttribs();
    gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, propCount);
  }
  if (state.debug === 0 && faunaCount > 0) {
    gl.useProgram(propP.prog);
    gl.bindVertexArray(propVAO);
    gl.bindBuffer(gl.ARRAY_BUFFER, faunaBuf); setPropAttribs();
    gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, faunaCount);
  }
  if (state.debug === 0) precip.draw(cam, mode, state, world);
  // Storm clouds last, translucent, so the weather reads from above without hiding the land.
  if (state.debug === 0 && state.cloudSprites && cloudCount > 0) {
    gl.useProgram(propP.prog);
    commonUniforms(propP, mode, time);
    gl.uniformMatrix4fv(propP.u.uViewProj, false, cam.vp);
    gl.uniform3fv(propP.u.uRight, cam.right); gl.uniform3fv(propP.u.uUp, cam.up);
    gl.uniform2f(propP.u.uAtlas, ATLAS_COLS, atlasRows);
    gl.uniform1f(propP.u.uCloudH, 7); gl.uniform1f(propP.u.uAlpha, 0.82);
    bindTex(propP, "tField", 0, tex.field); bindTex(propP, "tAtlas", 4, tex.atlas); bindTex(propP, "tWeather", 6, tex.weather);
    gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA); gl.depthMask(false);
    gl.bindVertexArray(propVAO);
    gl.bindBuffer(gl.ARRAY_BUFFER, cloudBuf); setPropAttribs();
    gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, cloudCount);
    gl.depthMask(true); gl.disable(gl.BLEND);
  }
  gl.bindVertexArray(null);
  return cam;
}

function resize() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  const w = Math.floor(canvas.clientWidth * dpr), h = Math.floor(canvas.clientHeight * dpr);
  if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
}

// ---- Input ----
let drag = null;
canvas.addEventListener("contextmenu", (e) => e.preventDefault());
canvas.addEventListener("mousedown", (e) => { drag = { x: e.clientX, y: e.clientY, b: e.button, shift: e.shiftKey }; });
window.addEventListener("mouseup", () => { drag = null; });
window.addEventListener("mousemove", (e) => {
  if (!drag || !state.cam) return;
  const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
  drag.x = e.clientX; drag.y = e.clientY;
  if (drag.b === 2 || drag.shift) {
    state.yaw = (state.yaw - dx * 0.3) % 360;
    state.pitch = Math.max(0, Math.min(78, state.pitch + dy * 0.25));
    syncUI();
  } else {
    // Pan along the camera's ground-plane right/forward (yaw only applies to the 3D view).
    const scale = (2 * state.cam.zoom) / canvas.clientHeight;
    const y = state.view === "2d" ? 0 : state.yaw * Math.PI / 180;
    const right = [Math.cos(y), -Math.sin(y)], fwd = [-Math.sin(y), -Math.cos(y)];
    state.cam.tx += (-right[0] * dx + fwd[0] * dy) * scale;
    state.cam.ty += (-right[1] * dx + fwd[1] * dy) * scale;
  }
});
canvas.addEventListener("wheel", (e) => {
  if (!state.cam) return;
  e.preventDefault();
  state.cam.zoom = Math.max(1.5, Math.min(90, state.cam.zoom * Math.exp(e.deltaY * 0.0012)));
}, { passive: false });

function setStatus(s) { document.getElementById("status").textContent = s; }

function screenshot() {
  const out = document.createElement("canvas");
  out.width = canvas.width; out.height = canvas.height;
  const c = out.getContext("2d");
  c.drawImage(canvas, 0, 0);
  const a = document.createElement("a");
  a.download = `lookdev_${state.world}_${state.view}_${state.season}.png`;
  a.href = out.toDataURL("image/png");
  a.click();
}

// ---- UI (built from a spec so every control is one line) ----
const UI = [
  ["World & camera", [
    ["world", "select", "World", () => window.WORLD_NAMES || [state.world], "rebuildWorld"],
    ["view", "select", "View", ["split", "2d", "3d"]],
    ["fake", "range", "2D fake height", 0, 4, 0.05],
    ["pitch", "range", "3D tilt (deg)", 0, 78, 1],
    ["yaw", "range", "3D yaw (deg)", -180, 180, 1],
    ["height", "range", "3D height scale", 0, 20, 0.1],
    ["peak3d", "range", "3D peak sprite size", 0, 1.5, 0.05],
  ]],
  ["Season (A2 / GDD grade)", [
    ["season", "select", "Jump to season", SEASONS, "jumpSeason"],
    ["day", "range", "Day of year", 0, 363, 1],
    ["grade", "check", "Season colour grade"],
    ["gradeK", "range", "Grade strength (1 = GDD)", 0, 4, 0.05],
  ]],
  ["Ecology (A1 stand-in)", [
    ["health", "range", "Health: Collapsed - Thriving", 0, 1, 0.01, "rebuildEco"],
    ["variation", "range", "Regional variation", 0, 1, 0.01, "rebuildEco"],
    ["density", "range", "Prop density", 0, 2, 0.05, "rebuildProps"],
    ["vegetation", "check", "Vegetation props", null, null, null, "rebuildProps"],
    ["peaks", "check", "Peak / hill / volcano sprites (off: terrain carries them)", null, null, null, "rebuildProps"],
  ]],
  ["Water (A5 stand-in)", [
    ["flow", "select", "River flow state", [["Dry", 0], ["Low", 1], ["Normal", 2], ["Flood", 3], ["Frozen", 4]]],
    ["lake", "range", "Lake level", 0, 1, 0.01],
    ["flood", "check", "Floodplain flooding"],
    ["ice", "check", "Lake ice"],
  ]],
  ["Weather (A4 stand-in: local storm fronts)", [
    ["storms", "range", "Storm fronts", 0, 8, 1],
    ["stormsMove", "check", "Fronts drift with the wind"],
    ["precipI", "range", "Storm intensity", 0, 1, 0.01],
    ["precip", "select", "Precipitation type", [["Auto (by cell temperature)", "auto"], ["Rain only", "rain"], ["Snow only", "snow"], ["None", "none"]]],
    ["cloud", "range", "Background cloud", 0, 1, 0.01],
    ["clouds", "check", "Cloud shadows"],
    ["cloudSprites", "check", "Storm cloud sprites"],
    ["windDir", "range", "Wind direction", 0, 360, 1],
    ["wind", "range", "Wind strength", 0, 1, 0.01],
    ["snow", "range", "Snow pack (season)", 0, 1, 0.01],
  ]],
  ["Time & life", [
    ["yearRun", "check", "Year clock running"],
    ["yearSpeed", "range", "Days per second", 1, 60, 1],
    ["livingYear", "check", "Living year (season drives snow, lakes, rivers, fire risk)"],
    ["timelapse", "check", "Time-lapse: decades of ecology drift (fake history)"],
    ["fireAuto", "check", "Wildfires start in Dry season"],
    ["ignite", "button", "Ignite wildfire at view centre"],
    ["fauna", "check", "Herds and birds"],
  ]],
  ["Presentation", [
    ["post", "check", "Post pass (tilt-shift, haze, sky, silhouettes)"],
    ["tiltShift", "check", "Tilt-shift (miniature look)"],
    ["tiltK", "range", "Tilt-shift strength", 0, 2, 0.05],
    ["focusY", "range", "Focus band height", 0, 1, 0.01],
    ["haze", "check", "Aerial haze"],
    ["hazeK", "range", "Haze strength", 0, 2, 0.05],
    ["sky", "check", "Painted sky"],
    ["silhouettes", "check", "Inked silhouettes (depth)"],
    ["formLines", "check", "Form lines on slopes"],
    ["contours", "check", "Contour lines"],
    ["boil", "check", "Line boil (ink redraws on twos)"],
  ]],
  ["Style (shader-owned)", [
    ["ink", "check", "Ink outlines (coast, biome, river)"],
    ["inkW", "range", "Ink weight", 0.3, 3, 0.05],
    ["hatch", "check", "Depth-zone hatching"],
    ["marks", "check", "Ground marks per biome"],
    ["toon", "range", "Toon banding", 0, 1, 0.01],
    ["shade", "range", "Hillshade strength", 0, 1, 0.01],
    ["zone", "check", "Depth-zone ambient"],
    ["grain", "check", "Paper grain"],
    ["hex", "check", "Hex grid overlay"],
    ["debug", "select", "Data view", [["Off (art)", 0], ["Biome", 1], ["Elevation", 2], ["Ecology health", 3], ["Depth zone", 4], ["Temperature", 5], ["Biome margin", 6]]],
  ]],
];
const ACTIONS = {
  rebuildWorld: () => { state.cam = null; loadWorld(state.world); },
  rebuildProps: () => rebuildProps(),
  rebuildEco: () => { updateCellData(); rebuildProps(); },
  jumpSeason: () => { state.day = SEASONS.indexOf(state.season) * SEASON_DAYS + 20; },
  ignite: () => { if (fire && state.cam) fire.igniteNear(state.cam.tx, state.cam.ty); },
};
const controls = {};
function buildUI() {
  const root = document.getElementById("controls");
  for (const [title, items] of UI) {
    const sec = document.createElement("details"); sec.open = true;
    sec.innerHTML = `<summary>${title}</summary>`;
    for (const [key, kind, label, a, b, c, action] of items) {
      const row = document.createElement("label"); row.className = "row";
      let el;
      if (kind === "range") {
        el = document.createElement("input"); el.type = "range"; el.min = a; el.max = b; el.step = c;
        const val = document.createElement("span"); val.className = "val";
        el.addEventListener("input", () => { state[key] = parseFloat(el.value); val.textContent = el.value; if (action) ACTIONS[action](); });
        row.append(label, el, val); el._val = val;
      } else if (kind === "button") {
        el = document.createElement("button"); el.textContent = label;
        el.addEventListener("click", (e) => { e.preventDefault(); ACTIONS[key](); });
        row.className = "row check"; row.style.gridTemplateColumns = "1fr"; row.append(el);
      } else if (kind === "check") {
        el = document.createElement("input"); el.type = "checkbox";
        el.addEventListener("change", () => { state[key] = el.checked; if (action) ACTIONS[action](); });
        row.className = "row check"; row.append(el, label);
      } else {
        el = document.createElement("select");
        const opts = typeof a === "function" ? a() : a;
        for (const o of opts) { const [l, v] = Array.isArray(o) ? o : [o, o]; const op = document.createElement("option"); op.textContent = l; op.value = v; el.append(op); }
        const act = typeof b === "string" ? b : action;
        el.addEventListener("change", () => { const cur = state[key]; state[key] = typeof cur === "number" ? parseFloat(el.value) : el.value; if (act) ACTIONS[act](); });
        row.append(label, el);
      }
      controls[key] = el;
      sec.append(row);
    }
    root.append(sec);
  }
  syncUI();
}
function syncUI() {
  for (const [k, el] of Object.entries(controls)) {
    if (el.tagName === "BUTTON" || document.activeElement === el) continue;
    if (el.type === "checkbox") el.checked = !!state[k];
    else { el.value = state[k]; if (el._val) el._val.textContent = typeof state[k] === "number" ? +state[k].toFixed(2) : state[k]; }
  }
}

// Reference images from the GDD Visual Bible (absolute path; the page is opened from file://).
const REF_ROOT = "file:///C:/GitHub/DesignDocs/COW/Output%20Docs/GDD/03_VisualBible/Visual%20Artifacts/";
const REFS = ["Strategic Map Overview.png", "Biome Palette Reference Sheet.png", "Edge Biome Transitions.png", "Ecology State Comparison.png", "Four Seasons Wetland.png", "Full Map Freeze Season.png", "River Corridor Node.png"];
function buildRefs() {
  const bar = document.getElementById("refs");
  for (const r of REFS) {
    const img = document.createElement("img");
    img.src = REF_ROOT + encodeURIComponent(r); img.title = r;
    img.onclick = () => {
      const big = document.getElementById("refBig");
      if (big.dataset.src === img.src && big.style.display === "block") { big.style.display = "none"; return; }
      big.src = img.src; big.dataset.src = img.src; big.style.display = "block";
    };
    bar.append(img);
  }
  document.getElementById("refBig").onclick = (e) => { e.target.style.display = "none"; };
}

document.getElementById("shot").onclick = screenshot;
document.getElementById("atlasToggle").onclick = () => { const a = document.getElementById("atlasPreview"); a.style.display = a.style.display === "block" ? "none" : "block"; };
initGL();
buildUI();
buildRefs();
loadWorld(state.world);
requestAnimationFrame(frame);
window.lookdev = { state, screenshot, loadWorld, rebuildProps };
