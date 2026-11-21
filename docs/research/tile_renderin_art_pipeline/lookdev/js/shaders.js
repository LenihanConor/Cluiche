// GLSL for the look-dev page. One style library (COMMON) is shared by the 2D and 3D modes so the bake-off compares
// geometry and camera only. Every visual channel is driven by cell data or a slider standing in for a future system.
"use strict";

const COMMON = `#version 300 es
precision highp float;
precision highp int;
const vec3 INK = vec3(0.165, 0.141, 0.125);      // #2A2420, GDD outline colour
const vec3 BARE = vec3(0.69, 0.60, 0.45);        // exposed ground (Collapsed ecology)
const vec3 SNOW = vec3(0.93, 0.95, 0.96);
uniform float uTime;
uniform int uSeason;            // dominant season: 0 Bloom, 1 Hunt, 2 Dry, 3 Freeze
uniform vec4 uSeasonW;          // smooth season weights from the year clock
uniform float uBoil, uBoilOn;   // line boil: ink redraws on twos (12 fps)
uniform sampler2D tEvents;      // per cell: burn, scar (wildfire)
uniform float uGradeK, uGrade, uGrain;
uniform float uCloud, uCloudOn, uWindSpeed;
uniform vec2 uWindDir;
uniform float uSnow;
uniform vec2 uExtent, uGrid;
uniform sampler2D tField;       // elevation, landness, lakeness, temperature

float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float vn(vec2 p) { vec2 i = floor(p), f = fract(p); vec2 u = f * f * (3. - 2. * f);
  return mix(mix(h21(i), h21(i + vec2(1, 0)), u.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), u.x), u.y); }
float fbm(vec2 p) { return .5 * vn(p) + .3 * vn(p * 2.07 + 17.) + .2 * vn(p * 4.13 + 5.); }
vec3 sat(vec3 c, float s) { float l = dot(c, vec3(.299, .587, .114)); return clamp(mix(vec3(l), c, s), 0., 1.); }
vec3 screenB(vec3 a, vec3 b) { return 1. - (1. - a) * (1. - b); }

// GDD ColourPaletteAndLighting Part 3 / Part 7: canvas-wide seasonal grade.
vec3 gradeOne(vec3 c, int s) {
  if (s == 0) { c = mix(c, screenB(c, vec3(.941, .847, .439)), .08 * uGradeK); c = sat(c, 1. + .10 * uGradeK); }
  else if (s == 2) { c = mix(c, screenB(c, vec3(.878, .659, .251)), .06 * uGradeK); c = sat(c, 1. - .08 * uGradeK); }
  else if (s == 3) { c = mix(c, c * vec3(.659, .769, .847), .12 * uGradeK * 2.); c = sat(c, 1. - .12 * uGradeK); }
  return c;
}
// Weighted by the year clock so seasons cross-fade instead of snapping.
vec3 seasonGrade(vec3 c) {
  if (uGrade < .5) return c;
  return gradeOne(c, 0) * uSeasonW.x + gradeOne(c, 1) * uSeasonW.y + gradeOne(c, 2) * uSeasonW.z + gradeOne(c, 3) * uSeasonW.w;
}
vec4 cellTexAt(sampler2D t, vec2 p) {
  return texture(t, vec2(((p.x - 1.) / 1.5 + .5) / uGrid.x, ((p.y - .866) / 1.732 + .25) / uGrid.y));
}
// GDD Layer 2 ecology expression: Thriving saturated, Stable baseline, Stressed -15..25%, Collapsed -40..60% + bare.
vec3 ecoGrade(vec3 c, float h, float n) {
  float s = h < .33 ? mix(.5, .8, h / .33) : h < .66 ? mix(.8, 1., (h - .33) / .33) : mix(1., 1.15, (h - .66) / .34);
  c = sat(c, s);
  float bare = smoothstep(.42, .05, h);
  return mix(c, BARE, bare * .6 * (.6 + .4 * n));
}
uniform sampler2D tWeather;     // per cell: cloud, precip, snow fraction, gust (A4 stand-in, linear-filtered)
vec4 weatherAt(vec2 p) {
  return texture(tWeather, vec2(((p.x - 1.) / 1.5 + .5) / uGrid.x, ((p.y - .866) / 1.732 + .25) / uGrid.y));
}
// Local cloud shadows: the per-cell cloud field, broken up by drifting noise so fronts have soft ragged edges.
float cloudShadow(vec2 p) {
  if (uCloudOn < .5) return 0.;
  float c = weatherAt(p).r;
  vec2 cp = p * .12 + uWindDir * uTime * .05 * (.3 + uWindSpeed);
  float d = fbm(cp) * .6 + fbm(cp * 2.7 + 4.) * .4;
  return smoothstep(.45, .8, c + (d - .5) * .55);
}
float snowAmount(float elev, float temp, float n) {
  float cold = (1. - temp) * .75 + max(elev, 0.) * .9;
  float line = mix(1.45, .15, uSnow);
  return smoothstep(line, line + .07, cold + (n - .5) * .14);
}
vec3 paper(vec3 c, vec2 frag) {
  if (uGrain < .5) return c;
  float g = vn(frag * .9) * .45 + vn(frag * .21 + 7.) * .35 + vn(frag * 2.7) * .2;
  c *= .93 + .1 * g;
  return mix(c, vec3(.95, .91, .83), .05);
}
`;

const GROUND_VS = `#version 300 es
precision highp float;
in vec2 aPos;
uniform mat4 uViewProj;
uniform sampler2D tField;
uniform vec2 uExtent;
uniform float uMode, uFake, uHeight;
out vec2 vP;
void main() {
  vP = aPos;
  float e = max(textureLod(tField, aPos / uExtent, 0.).r, 0.);
  vec3 pos = uMode < .5 ? vec3(aPos.x, aPos.y - e * uFake, 0.) : vec3(aPos, -e * uHeight);
  gl_Position = uViewProj * vec4(pos, 1.);
  if (uMode < .5) gl_Position.z = .95;
}`;

const GROUND_FS = COMMON + `
in vec2 vP;
out vec4 frag;
uniform sampler2D tCell;      // col, row, water class, terrain (nearest)
uniform sampler2D tCellData;  // biome, terrain, depth zone, health (per cell)
uniform sampler2D tRiver;     // signed distance, half width, flow dir
uniform sampler2D tEdge;      // biome margin, linear-filtered: 0 on a biome border
uniform vec2 uTexels;
uniform float uT;
uniform vec3 uPal[30];
uniform float uMode, uHeight, uFake;
uniform float uInk, uInkW, uHatch, uToon, uShadeK, uShadeStr, uZoneTint, uHexGrid, uMarks;
uniform int uDebug, uFlowState;
uniform float uLakeLevel, uIce, uFlood, uFormLines, uContours;

ivec2 texelOf(vec2 p) { return clamp(ivec2(p * uT), ivec2(0), ivec2(uTexels) - 1); }
vec4 cellAt(vec2 p) { return texelFetch(tCell, texelOf(p), 0) * 255.; }
vec4 dataOf(vec4 cell) { if (cell.x > 254.) return vec4(0.); return texelFetch(tCellData, ivec2(cell.xy + .5), 0); }
float lakeThr() { return mix(.92, .12, uLakeLevel); }
// Water from the linear-filtered fields (smooth at any zoom): ocean below land 0.5, lakes above the lake level.
int waterOf(vec4 f) {
  if (f.b > lakeThr()) return 2;
  if (f.g < .5) return (1. - f.g - f.b) >= f.b ? 1 : 2;
  return 0;
}
int biomeOf(vec4 d) { return int(d.r * 255. + .5); }

float ticks(vec2 p, float s, float len, float ang, float px, float keep) {
  if (s / px < 5.) return 0.;
  vec2 g = p / s, id = floor(g), f = fract(g) - .5;
  float r = h21(id); if (r > keep) return 0.;
  f -= (vec2(h21(id + 3.1), h21(id + 7.7)) - .5) * .5;
  float a = ang + (r - .3) * .7; vec2 d = vec2(sin(a), cos(a));
  float t = clamp(dot(f, d), -len, len);
  float dist = length(f - d * t) * s;
  return (1. - smoothstep(px * .45, px * 1.3, dist)) * smoothstep(5., 9., s / px);
}
float hatchLines(vec2 p, float s, float px, float dirSign) {
  float v = (p.x + dirSign * p.y) / s;
  float f = abs(fract(v) - .5);
  float w = .11;
  return 1. - smoothstep(w, w + px / s * 1.4, f);
}
float hexEdge(vec2 p) {
  p -= vec2(1., .8660254);
  float q = (2. / 3.) * p.x, r = (-1. / 3.) * p.x + (.57735) * p.y, s = -q - r;
  float rq = floor(q + .5), rr = floor(r + .5), rs = floor(s + .5);
  float dq = abs(rq - q), dr = abs(rr - r), ds = abs(rs - s);
  if (dq > dr && dq > ds) rq = -rr - rs; else if (dr > ds) rr = -rq - rs;
  vec2 c = vec2(1.5 * rq, 1.7320508 * (rr + rq * .5));
  vec2 d = abs(p - c);
  float e = max(d.x * .8660254 + d.y * .5, d.y);
  return .8660254 - e;
}

void main() {
  vec2 p = vP;
  float px = max(length(fwidth(p)), 1e-4);
  // Sub-texel jitter on the nearest-cell lookup turns texel stair-steps into a ragged, inked-looking edge.
  vec4 cell = cellAt(p + (vec2(vn(p * 37.), vn(p * 37. + 9.)) - .5) * 1.6 / uT);
  vec4 fld = texture(tField, p / uExtent);
  float elev = fld.r, land = fld.g, lakeness = fld.b, temp = fld.a;
  vec4 cd = dataOf(cell);
  int biome = biomeOf(cd);
  int terr = int(cell.w + .5);
  int zone = int(cd.b * 255. + .5);
  float health = cd.a;
  int water = waterOf(fld);
  float n1 = fbm(p * 1.6), n2 = fbm(p * 5.3 + 3.1);

  if (uDebug > 0) {
    vec3 dc = vec3(0.);
    if (uDebug == 1) dc = water == 1 ? vec3(.16, .47, .53) : uPal[biome * 3];
    else if (uDebug == 2) dc = vec3(clamp(elev * 1.5, 0., 1.));
    else if (uDebug == 3) dc = water != 0 ? vec3(.2) : mix(vec3(.8, .15, .1), vec3(.2, .75, .2), health);
    else if (uDebug == 4) dc = zone == 1 ? vec3(.95, .85, .5) : zone == 2 ? vec3(.6, .7, .45) : zone == 3 ? vec3(.35, .2, .3) : vec3(.2, .3, .5);
    else if (uDebug == 5) dc = mix(vec3(.2, .3, .9), vec3(.95, .3, .1), temp);
    else if (uDebug == 6) dc = vec3(texture(tEdge, p / uExtent).r);
    else if (uDebug == 7) { float mg = texture(tEdge, p / uExtent).r; dc = vec3(mg * 4., fwidth(mg) * 40., max(mg - .09, 0.) / max(fwidth(mg), 1e-5) / 4.); }
    if (uHexGrid > .5) dc = mix(dc, vec3(0.), 1. - smoothstep(0., px * 1.2, hexEdge(p)));
    frag = vec4(dc, 1.);
    return;
  }

  // Hillshade from the smoothed elevation field (light from the north-west, toon-banded).
  float e1 = 1. / uT;
  float gx = (texture(tField, (p + vec2(e1, 0)) / uExtent).r - texture(tField, (p - vec2(e1, 0)) / uExtent).r) / (2. * e1);
  float gy = (texture(tField, (p + vec2(0, e1)) / uExtent).r - texture(tField, (p - vec2(0, e1)) / uExtent).r) / (2. * e1);
  float k = uShadeK;
  vec3 nrm = normalize(vec3(-gx * k, -gy * k, 1.));
  vec3 Ld = normalize(vec3(-.55, -.7, .9));
  float shade = clamp(dot(nrm, Ld) / Ld.z, .35, 1.35);
  float toon = floor(shade * 3. + .5) / 3.;
  shade = mix(shade, toon, uToon);
  if (water != 0) shade = 1.;

  vec3 col;
  float inkA = 0.;
  if (water == 1) {
    float nearCoast = smoothstep(.0, .3, land);
    float deep = smoothstep(.45, .75, fbm(p * .05)) * .35 + .45;
    col = mix(vec3(.290, .722, .784), vec3(.165, .471, .533), clamp(deep - nearCoast * 1.4, 0., 1.));
    col = mix(col, vec3(.40, .78, .80), smoothstep(.18, .34, land) * .6);
    // Wave marks (Hilda-style little ink arcs), fading at strategic zoom.
    inkA = max(inkA, ticks(p + vec2(0., sin(p.x * 3.) * .05), .55, .22, 1.5708, px, .35) * .35 * (1. - nearCoast));
    float foam = smoothstep(.30, .36, land) * (1. - smoothstep(.40, .47, land));
    col = mix(col, vec3(.95, .96, .92), foam * .75);
  } else if (water == 2) {
    col = uSeason == 0 ? vec3(.22, .71, .78) : uSeason == 2 ? vec3(.42, .60, .58) : vec3(.29, .62, .66);
    col = mix(col, col * .85, smoothstep(.95, .99, lakeness));
    inkA = max(inkA, ticks(p, .4, .16, 1.5708, px, .3) * .3);
    float iceOn = max(uIce, uSeason == 3 && temp < .45 ? 1. : 0.);
    if (iceOn > .5) {
      col = vec3(.86, .92, .94);
      inkA = max(inkA, (1. - smoothstep(0., .025, abs(vn(p * 3.2) - .5))) * .55);
    }
  } else {
    if (biome == 1) {   // drained lake bed
      col = mix(vec3(.54, .47, .31), vec3(.62, .55, .38), n1);
    } else {
      vec3 Pc = uPal[biome * 3], Sc = uPal[biome * 3 + 1], Ac = uPal[biome * 3 + 2];
      float q = n1 * .88 + n2 * .12;
      col = mix(Pc, Sc, smoothstep(.52, .58, q));
      // Accent: small patches; Grassland accent is river blue, so only near rivers (GDD palette note).
      float qa = n2 * .7 + n1 * .3;
      float accentOK = biome == 2 ? 1. - smoothstep(.0, .6, texture(tRiver, p / uExtent).r * .9 - .3) : 1.;
      col = mix(col, Ac, smoothstep(.64, .68, qa) * accentOK);
      col = ecoGrade(col, health, n2);
      // Ground marks per biome (code-drawn texture, no AI art).
      if (uMarks > .5) {
        float m = 0.;
        if (biome == 2) m = ticks(p, .16, .2, 0., px, .5);
        else if (biome == 3 || biome == 8) m = ticks(p, .2, .12, .8, px, .3);
        else if (biome == 4) m = ticks(p, .22, .3, 1.5708, px, .55);
        else if (biome == 5) m = ticks(p, .12, .02, 0., px, .4);
        else if (biome == 6 || biome == 7) m = ticks(p, .26, .25, 1.2, px, .35);
        else if (biome == 9) m = (1. - smoothstep(0., .02, abs(vn(p * 4.) - .5))) * smoothstep(5., 9., .25 / px);
        inkA = max(inkA, m * .5);
        if (biome == 9) col = mix(col, vec3(.78, .25, .12), (1. - smoothstep(0., .012, abs(vn(p * 4.) - .5))) * .8);
      }
      // Collapsed ground cracks.
      float crack = (1. - smoothstep(0., .015, abs(vn(p * 5.5 + 2.) - .5))) * smoothstep(.3, .05, health);
      inkA = max(inkA, crack * .45 * smoothstep(4., 8., .2 / px));
    }
    // Sand / shore band and cliff faces at the coast.
    float shore = smoothstep(.62, .52, land) * step(.5, land);
    if (terr == 9) col = mix(col, mix(vec3(.55, .50, .45), vec3(.42, .38, .34), n2), shore);
    else col = mix(col, vec3(.86, .80, .58), shore * .85);
    float steep = uMode > .5 ? smoothstep(.9, 1.8, length(vec2(gx, gy)) * uHeight * .25) : 0.;
    col = mix(col, mix(vec3(.50, .48, .46), vec3(.38, .36, .35), n2), steep * .8);
    inkA = max(inkA, steep * ticks(p, .14, .3, 0., px, .6) * .5);
    col *= mix(1., shade, uShadeStr);
    // Wildfire: charcoal scar that heals back through the ecology bands; flames glow along the front.
    vec4 ev = cellTexAt(tEvents, p);
    float scar = clamp(ev.g, 0., 1.), burn = clamp(ev.r, 0., 1.);
    if (scar > .01) {
      vec3 ash = mix(vec3(.18, .16, .17), vec3(.48, .45, .42), smoothstep(.3, .7, n2));
      col = mix(col, ash, scar * .85);
      inkA = max(inkA, (1. - smoothstep(0., .02, abs(vn(p * 7.) - .5))) * scar * .4);
    }
    float sn = snowAmount(elev, temp, n2);
    col = mix(col, SNOW * mix(.82, 1.03, clamp(shade, 0., 1.)), sn);

    // Rivers from the distance field: water, flow streaks, ink banks; state from A5 (Dry/Low/Normal/Flood/Frozen).
    vec4 rv = texture(tRiver, p / uExtent);
    float hw = rv.g * .3;
    if (hw > .002) {
      float dist = rv.r * .9 - .3 + hw;
      float kw = uFlowState == 0 ? .35 : uFlowState == 1 ? .65 : uFlowState == 3 ? 1.7 : 1.;
      float w = hw * kw;
      vec2 dir = normalize(rv.ba * 2. - 1. + 1e-4);
      float inkw = max(px * 1.3, .022) * uInkW;
      if (uFlood > .5 || uFlowState == 3) {
        float fl = (1. - smoothstep(w * 2.5, w * 5.5, dist)) * smoothstep(.32, .08, elev);
        col = mix(col, vec3(.45, .66, .66), fl * .55);
      }
      if (dist < w) {
        vec3 wc = uFlowState == 0 ? vec3(.54, .47, .31) : vec3(.33, .66, .72);
        if (uFlowState == 4) wc = vec3(.84, .91, .94);
        float ph = dot(p, dir) * 16. - uTime * 2.6 + vn(p * 7.) * 3.;
        float streak = uFlowState >= 1 && uFlowState <= 3 ? smoothstep(.82, 1., sin(ph)) * smoothstep(w, w * .3, dist) : 0.;
        col = mix(wc, vec3(.9, .96, .97), streak * .45);
      } else if (dist < w + inkw && uInk > .5) {
        inkA = 1.;
      }
    }
  }

  // Ink outlines from data boundaries, drawn as constant-pixel-width lines on linear-filtered fields:
  // coast (landness = .5), lake shore (lakeness = level), biome borders (margin = 0, lighter).
  // Weight by depth zone per GDD ShapeLanguage Part 9 (1x / 1.2x / 1.5x).
  if (uInk > .5) {
    float zw = zone == 3 ? 1.5 : zone == 2 ? 1.2 : 1.;
    float wpx = 1.6 * zw * uInkW * (.75 + .5 * vn(p * 2.3 + uBoil * 1.7));
    // Line boil: the ink is re-sampled at a slightly jittered point that changes 12 times a second.
    vec2 pj = p + (vec2(vn(p * 6. + uBoil * .93), vn(p * 6. + 3. + uBoil * .71)) - .5) * .045 * uBoilOn;
    vec4 fj = texture(tField, pj / uExtent);
    float dl = abs(fj.g - .5) / max(fwidth(land), 1e-5);
    float dk = abs(fj.b - lakeThr()) / max(fwidth(lakeness), 1e-5);
    float coastI = 1. - smoothstep(wpx - .7, wpx + .7, min(dl, dk));
    float mg = texture(tEdge, pj / uExtent).r;
    // Value band, not pixel distance: the sampled margin bottoms out above 0, and a band also gives the line a
    // hand-drawn varying weight. The fwidth term keeps it at least ~2 px wide when zoomed out.
    float th = max(.34, fwidth(mg) * wpx * 1.4);
    float biomeI = water == 0 ? 1. - smoothstep(th, th + fwidth(mg) * 1.2, mg) : 0.;
    inkA = max(inkA, max(coastI, biomeI * .6));
  }

  // Form lines: ink where the light turns away (terminator), which reads as drawn ridges and spurs.
  if (uFormLines > .5 && water == 0) {
    float sh = dot(nrm, Ld) / Ld.z;
    float tl = abs(sh - .74) / max(fwidth(sh), 1e-4);
    inkA = max(inkA, (1. - smoothstep(.7, 1.7, tl)) * .6 * smoothstep(.1, .28, elev));
  }
  // Old-map contour lines every 0.06 of elevation.
  if (uContours > .5 && water == 0) {
    float cv = elev / .06;
    float cd = abs(fract(cv + .5) - .5) / max(fwidth(cv), 1e-4);
    inkA = max(inkA, (1. - smoothstep(.4, 1.1, cd)) * .32);
  }

  // Hatching by depth zone (GDD AestheticStyleCore Part 5): 10-20% / 30-50% / 60-80% ink coverage.
  if (uHatch > .5 && water == 0) {
    float cov = zone == 1 ? .15 : zone == 2 ? .40 : zone == 3 ? .70 : 0.;
    float m = (1.35 - clamp(shade, .35, 1.35)) * 1.6 + fbm(p * 1.1 + 9.) * .55 - .15;
    float region = smoothstep(1. - cov * 1.25, 1. - cov * 1.25 + .1, m);
    float s0 = .085;
    float lod = log2(max(px * 5. / s0, 1.));
    float l0 = floor(lod), fr = fract(lod);
    float s = s0 * exp2(l0);
    float hl = mix(hatchLines(p, s, px, 1.), hatchLines(p, s * 2., px, 1.), fr);
    float hc = zone == 3 ? mix(hatchLines(p, s, px, -1.), hatchLines(p, s * 2., px, -1.), fr) * smoothstep(.6, .9, m) : 0.;
    inkA = max(inkA, max(hl, hc) * region * .38 * mix(.45, 1., smoothstep(5., 12., s0 / px)));
  }

  if (water == 0) col *= 1. - weatherAt(p).g * (1. - weatherAt(p).b) * .14;   // wet ground under rain
  col = mix(col, INK, clamp(inkA, 0., 1.));
  if (water == 0) {
    vec4 ev2 = cellTexAt(tEvents, p);
    float b = clamp(ev2.r, 0., 1.);
    if (b > .01) {
      float fl = vn(p * 5. + vec2(0., -uTime * 3.)) * .6 + vn(p * 13. - uTime * 5.) * .4;
      vec3 ember = mix(vec3(.78, .25, .12), vec3(.98, .70, .25), smoothstep(.4, .9, fl));
      col = mix(col, ember, smoothstep(.05, .5, b) * (.55 + .45 * fl));
    }
  }
  col *= 1. - cloudShadow(p) * .26;
  col = seasonGrade(col);
  if (uZoneTint > .5 && water == 0) {
    if (zone == 2) col = sat(col, .95);
    if (zone == 3) col = sat(col, .9) * .92;
  }
  if (uHexGrid > .5) col = mix(col, INK, (1. - smoothstep(0., px * 1.1, hexEdge(p))) * .35);
  col = paper(col, gl_FragCoord.xy);
  frag = vec4(col, 1.);
}`;

const PROP_VS = `#version 300 es
precision highp float;
in vec2 aCorner;
in vec4 iA;       // x, y, elevation, scale
in vec4 iB;       // type, health, seed, pad
in vec3 iP; in vec3 iS; in vec3 iC;
uniform mat4 uViewProj;
uniform vec3 uRight, uUp;
uniform float uMode, uFake, uHeight, uTime, uWindSpeed, uPeak3D, uCloudH, uBoil, uBoilOn;
uniform vec2 uExtent, uAtlas, uGrid;
uniform sampler2D tWeather;
out vec2 vUV, vFoot;
out vec3 vP, vS, vC;
out float vHealth, vLocalY, vType;
void main() {
  float e = max(iA.z, 0.);
  int t0 = int(iB.x + .5);
  float lift = t0 == 11 ? uCloudH : t0 == 13 ? uCloudH * .6 : 0.;     // storm clouds and birds float
  vec3 foot = uMode < .5 ? vec3(iA.x, iA.y - e * uFake - lift * .45, 0.) : vec3(iA.x, iA.y, -e * uHeight - lift);
  float gust = texture(tWeather, vec2(((iA.x - 1.) / 1.5 + .5) / uGrid.x, ((iA.y - .866) / 1.732 + .25) / uGrid.y)).a;
  int t = int(iB.x + .5);
  float s = iA.w * ((uMode > .5 && (t == 6 || t == 7)) ? uPeak3D : 1.);
  float rigid = (t == 5 || t == 6 || t == 7 || t >= 10) ? 0. : 1.;   // rock, peak, volcano, hill, cloud, beast, bird
  float sway = aCorner.y * aCorner.y * sin(uTime * 1.6 + iB.z * 40.) * (uWindSpeed * .15 + gust * .5) * .06 * rigid;   // gentle sway, stronger under gusts (fixed frequency: no phase jumps)
  vec3 up = uMode < .5 ? uUp : normalize(mix(uUp, vec3(0., 0., -1.), .35));
  vec2 boil = vec2(0.);   // no boil on props: it read as vibration (user, 2026-11-20)
  vec3 pos = foot + uRight * ((aCorner.x + sway + boil.x) * s) + up * ((aCorner.y + boil.y) * s);
  gl_Position = uViewProj * vec4(pos, 1.);
  if (uMode < .5) gl_Position.z = (t0 == 11 || t0 == 13) ? -.92 : .9 - (iA.y / uExtent.y) * 1.8;
  vec2 cellUV = vec2(float(t % 4), float(t / 4));
  float fx = (t == 12 && iB.w > .5) ? -aCorner.x : aCorner.x;   // herds face their heading
  vUV = (cellUV + vec2(fx + .5, 1. - aCorner.y)) / uAtlas;
  vP = iP; vS = iS; vC = iC; vHealth = iB.y; vLocalY = aCorner.y; vType = float(t);
  vFoot = iA.xy;
}`;

const PROP_FS = COMMON + `
in vec2 vUV, vFoot;
in vec3 vP, vS, vC;
in float vHealth, vLocalY, vType;
out vec4 frag;
uniform sampler2D tAtlas;
uniform float uAlpha;
void main() {
  vec4 t = texture(tAtlas, vUV);
  if (t.a < .5) discard;
  vec3 w = t.rgb;
  float ink = clamp(1. - (w.r + w.g + w.b), 0., 1.);
  vec3 col = vP * w.r + vS * w.g + vC * w.b;
  int type = int(vType + .5);
  bool veg = !(type == 5 || type == 6 || type == 7 || type >= 10);
  if (veg) col = sat(col, vHealth < .33 ? .55 : vHealth < .66 ? .85 : 1.1);
  vec4 fld = texture(tField, vFoot / uExtent);
  float sn = snowAmount(fld.r, fld.a, .5);
  if (type != 6 && type < 11) col = mix(col, SNOW, sn * smoothstep(.35, .9, vLocalY) * .85 * (1. - ink));
  col = mix(col, INK, ink);
  if (type != 11 && type != 13) col *= 1. - cloudShadow(vFoot) * .26;
  col = seasonGrade(col);
  col = paper(col, gl_FragCoord.xy);
  frag = vec4(col, uAlpha);
}`;
