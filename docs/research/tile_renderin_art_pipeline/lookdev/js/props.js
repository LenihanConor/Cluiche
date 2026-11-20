// Claude-authored SVG props, palette-masked: red = Primary, green = Secondary, blue = Accent, black = ink.
// The shader decodes the channels and recolours per biome / season / ecology, so one sprite serves many looks.
// Every prop's foot is at the bottom centre of its 100x100 viewBox.
"use strict";

const P = "#ff0000", S = "#00ff00", A = "#0000ff", K = "#000000";
const ink = (w) => `stroke="${K}" stroke-width="${w}" stroke-linejoin="round" stroke-linecap="round"`;

const PROP_SVG = {
  conifer: `
    <rect x="46" y="84" width="8" height="14" fill="${A}" ${ink(2.5)}/>
    <path d="M50 4 L66 34 L59 34 L72 56 L63 56 L80 84 L20 84 L37 56 L28 56 L41 34 L34 34 Z" fill="${P}"/>
    <path d="M50 4 L66 34 L59 34 L72 56 L63 56 L80 84 L52 84 Z" fill="${S}"/>
    <path d="M50 4 L66 34 L59 34 L72 56 L63 56 L80 84 L20 84 L37 56 L28 56 L41 34 L34 34 Z" fill="none" ${ink(3.2)}/>
    <path d="M38 70 l6 -3 M58 46 l5 3 M44 48 l4 -2" fill="none" ${ink(1.6)}/>`,

  broadleaf: `
    <path d="M47 62 L45 97 L55 97 L53 62 Z" fill="${A}" ${ink(2.5)}/>
    <defs><clipPath id="cb"><circle cx="50" cy="34" r="22"/><circle cx="32" cy="48" r="17"/><circle cx="68" cy="48" r="17"/><circle cx="50" cy="56" r="18"/></clipPath></defs>
    <g ${ink(6)} fill="${K}"><circle cx="50" cy="34" r="22"/><circle cx="32" cy="48" r="17"/><circle cx="68" cy="48" r="17"/><circle cx="50" cy="56" r="18"/></g>
    <g fill="${P}"><circle cx="50" cy="34" r="22"/><circle cx="32" cy="48" r="17"/><circle cx="68" cy="48" r="17"/><circle cx="50" cy="56" r="18"/></g>
    <g clip-path="url(#cb)" fill="${S}"><circle cx="66" cy="62" r="20"/><circle cx="74" cy="40" r="10"/></g>
    <path d="M36 44 q4 -3 8 0 M54 26 q4 -3 8 0 M56 50 q4 -3 8 0" fill="none" ${ink(1.8)}/>`,

  shrub: `
    <defs><clipPath id="cs"><circle cx="38" cy="76" r="15"/><circle cx="60" cy="74" r="17"/><circle cx="50" cy="66" r="14"/></clipPath></defs>
    <g ${ink(5)} fill="${K}"><circle cx="38" cy="76" r="15"/><circle cx="60" cy="74" r="17"/><circle cx="50" cy="66" r="14"/><rect x="24" y="76" width="52" height="20"/></g>
    <g fill="${P}"><circle cx="38" cy="76" r="15"/><circle cx="60" cy="74" r="17"/><circle cx="50" cy="66" r="14"/><rect x="24" y="76" width="52" height="19"/></g>
    <g clip-path="url(#cs)" fill="${S}"><circle cx="66" cy="88" r="16"/></g>
    <rect x="22" y="90" width="56" height="8" fill="${S}"/>
    <path d="M24 96 L76 96" ${ink(3)}/>
    <path d="M42 72 q3 -2 6 0 M58 70 q3 -2 6 0" fill="none" ${ink(1.5)}/>`,

  grass: `
    <path d="M20 97 L26 60 L31 97 Z M30 97 L40 50 L43 97 Z M40 97 L50 42 L55 97 Z M52 97 L63 52 L64 97 Z M62 97 L76 62 L72 97 Z" fill="${P}" ${ink(2.4)}/>
    <path d="M40 97 L50 42 L48 97 Z M62 97 L76 62 L69 97 Z" fill="${S}"/>`,

  reeds: `
    <path d="M30 97 L32 40 M42 97 L44 26 M54 97 L52 34 M66 97 L64 46 M74 97 L78 58" fill="none" stroke="${K}" stroke-width="5" stroke-linecap="round"/>
    <path d="M30 97 L32 40 M42 97 L44 26 M54 97 L52 34 M66 97 L64 46 M74 97 L78 58" fill="none" stroke="${P}" stroke-width="2.4" stroke-linecap="round"/>
    <rect x="40" y="22" width="8" height="18" rx="4" fill="${A}" ${ink(2.2)}/>
    <rect x="49" y="30" width="7" height="16" rx="3.5" fill="${A}" ${ink(2.2)}/>
    <path d="M18 96 L84 96" ${ink(3)}/>`,

  rock: `
    <path d="M18 96 L24 70 L40 56 L62 58 L78 72 L84 96 Z" fill="${P}"/>
    <path d="M62 58 L78 72 L84 96 L56 96 L52 74 Z" fill="${S}"/>
    <path d="M18 96 L24 70 L40 56 L62 58 L78 72 L84 96 Z" fill="none" ${ink(3.2)}/>
    <path d="M40 56 L44 72 L52 74 L62 58 M44 72 L36 86" fill="none" ${ink(1.8)}/>`,

  peak: `
    <path d="M58 30 L74 20 L98 96 L60 96 Z" fill="${S}" ${ink(2.6)}/>
    <path d="M2 96 L44 6 L82 96 Z" fill="${P}"/>
    <path d="M44 6 L82 96 L48 96 L52 60 L40 40 Z" fill="${S}"/>
    <path d="M44 6 L56 32 L50 28 L46 38 L40 30 L32 34 Z" fill="${A}"/>
    <path d="M74 20 L82 42 L76 38 L72 44 L68 36 Z" fill="${A}"/>
    <path d="M2 96 L44 6 L82 96" fill="none" ${ink(3.4)}/>
    <path d="M52 60 L40 40 M58 66 l8 -4 M62 76 l9 -4 M66 86 l9 -4 M56 48 l6 -3" fill="none" ${ink(1.7)}/>
    <path d="M2 96 L98 96" ${ink(3)}/>`,

  volcano: `
    <path d="M6 96 L38 24 L62 24 L94 96 Z" fill="${P}"/>
    <path d="M50 24 L62 24 L94 96 L56 96 Z" fill="${S}"/>
    <path d="M46 24 L42 46 L48 62 L44 80 L50 96 L56 96 L52 78 L56 60 L50 44 L54 24 Z" fill="${A}"/>
    <path d="M6 96 L38 24 L62 24 L94 96 Z" fill="none" ${ink(3.4)}/>
    <ellipse cx="50" cy="24" rx="12" ry="4" fill="${K}"/>
    <path d="M70 70 l8 -4 M76 84 l8 -4" fill="none" ${ink(1.7)}/>`,

  jungle: `
    <path d="M48 40 Q44 70 46 97 L54 97 Q52 70 54 40 Z" fill="${A}" ${ink(2.4)}/>
    <path d="M50 38 Q30 26 8 44 Q30 36 50 44 Z M50 38 Q70 26 92 44 Q70 36 50 44 Z M50 40 Q36 50 22 72 Q40 52 52 44 Z M50 40 Q64 50 78 72 Q60 52 48 44 Z M50 40 Q48 20 60 8 Q52 24 54 40 Z" fill="${P}" ${ink(2.6)}/>
    <path d="M50 38 Q70 26 92 44 Q70 36 50 44 Z M50 40 Q64 50 78 72 Q60 52 48 44 Z" fill="${S}" ${ink(2.6)}/>`,

  deadtree: `
    <path d="M47 97 L48 50 L36 30 M48 62 L62 38 L70 30 M62 38 L60 24 M48 50 L44 20 M40 36 L30 34" fill="none" stroke="${K}" stroke-width="7" stroke-linecap="round" stroke-linejoin="round"/>
    <path d="M47 97 L48 50 L36 30 M48 62 L62 38 L70 30 M62 38 L60 24 M48 50 L44 20 M40 36 L30 34" fill="none" stroke="${P}" stroke-width="3.4" stroke-linecap="round" stroke-linejoin="round"/>`,

  cloud: `
    <g ${ink(5)} fill="${K}"><circle cx="34" cy="62" r="18"/><circle cx="54" cy="50" r="22"/><circle cx="74" cy="62" r="16"/><rect x="22" y="62" width="62" height="20" rx="10"/></g>
    <g fill="${P}"><circle cx="34" cy="62" r="18"/><circle cx="54" cy="50" r="22"/><circle cx="74" cy="62" r="16"/><rect x="22" y="62" width="62" height="20" rx="10"/></g>
    <path d="M24 74 Q50 86 84 72 L84 80 Q50 90 24 82 Z" fill="${S}"/>
    <path d="M44 40 q6 -6 14 -2 M64 54 q5 -3 9 0" fill="none" ${ink(1.8)}/>`,

  beast: `
    <path d="M22 66 Q24 52 40 52 L66 52 Q78 50 82 40 L88 36 L92 42 L86 50 Q84 60 76 64 L76 84 L70 84 L68 68 L36 68 L34 84 L28 84 L28 70 Q22 70 22 66 Z" fill="${P}" ${ink(3)}/>
    <path d="M36 62 L66 62 L68 68 L36 68 Z" fill="${S}"/>
    <path d="M84 36 L80 28 M88 36 L90 27" fill="none" ${ink(2.4)}/>`,

  bird: `
    <path d="M20 58 Q34 44 50 56 Q66 44 80 58 Q66 50 50 62 Q34 50 20 58 Z" fill="${K}"/>`,

  hill: `
    <path d="M4 96 Q20 52 50 50 Q80 52 96 96 Z" fill="${P}"/>
    <path d="M50 50 Q80 52 96 96 L56 96 Q62 72 50 50 Z" fill="${S}"/>
    <path d="M4 96 Q20 52 50 50 Q80 52 96 96" fill="none" ${ink(3.2)}/>
    <path d="M64 66 l7 -3 M70 78 l8 -3 M74 88 l8 -3 M30 70 l2 -6 M38 64 l2 -6" fill="none" ${ink(1.6)}/>`,
};

// Fixed order: the shaders refer to types by index (5 rock, 6 peak, 7 volcano, 10 hill, 11 cloud, 12 beast, 13 bird).
const PROP_TYPES = ["conifer", "broadleaf", "shrub", "grass", "reeds", "rock", "peak", "volcano", "jungle", "deadtree", "hill", "cloud", "beast", "bird"];
const PROP_INDEX = Object.fromEntries(PROP_TYPES.map((k, i) => [k, i]));
const ATLAS_CELL = 128, ATLAS_COLS = 4;

function buildPropAtlas(done) {
  const rows = Math.ceil(PROP_TYPES.length / ATLAS_COLS);
  const canvas = document.createElement("canvas");
  canvas.width = ATLAS_CELL * ATLAS_COLS; canvas.height = ATLAS_CELL * rows;
  const ctx = canvas.getContext("2d");
  let pending = PROP_TYPES.length;
  PROP_TYPES.forEach((name, i) => {
    const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${ATLAS_CELL}" height="${ATLAS_CELL}" viewBox="0 0 100 100">${PROP_SVG[name]}</svg>`;
    const img = new Image();
    img.onload = () => {
      ctx.drawImage(img, (i % ATLAS_COLS) * ATLAS_CELL, Math.floor(i / ATLAS_COLS) * ATLAS_CELL, ATLAS_CELL, ATLAS_CELL);
      if (--pending === 0) done(canvas, rows);
    };
    img.onerror = () => { console.error("prop svg failed", name); if (--pending === 0) done(canvas, rows); };
    img.src = "data:image/svg+xml;charset=utf-8," + encodeURIComponent(svg);
  });
}

// Prop colours [Primary, Secondary, Accent]; key "prop:biome" overrides "prop".
const PROP_PAL = {
  "conifer": ["#3E6A3A", "#24432A", "#6A4E2A"],
  "conifer:6": ["#2E4A32", "#1E3424", "#5A4632"],
  "conifer:7": ["#3E5A48", "#283E32", "#5A4632"],
  "broadleaf": ["#5A8A3E", "#355A2A", "#6A4E2A"],
  "broadleaf:2": ["#7A9A48", "#4E7034", "#6A4E2A"],
  "shrub": ["#6A8A48", "#46623A", "#6A4E2A"],
  "shrub:5": ["#8C8048", "#6A6034", "#6A4E2A"],
  "shrub:7": ["#8A9A80", "#62746A", "#6A4E2A"],
  "grass": ["#D4B84A", "#8DB558", "#8DB558"],
  "reeds": ["#5A8C6E", "#3E6A50", "#8A6A3A"],
  "rock": ["#9A9488", "#6A665E", "#6A665E"],
  "rock:5": ["#C88A58", "#8C5A38", "#8C6840"],
  "rock:9": ["#4A4248", "#2E2830", "#7A2818"],
  "peak": ["#9AA8B4", "#6A7A8A", "#F2F4F6"],
  "volcano": ["#3A3238", "#221C22", "#C84020"],
  "jungle": ["#2E7A3E", "#1A4A28", "#6A4E2A"],
  "deadtree": ["#7A6A58", "#5A4E40", "#5A4E40"],
  "hill": ["#A8B05A", "#7A8A48", "#7A8A48"],
  "cloud": ["#F4F2EC", "#C8CCD0", "#C8CCD0"],
  "hill:3": ["#5A7A44", "#3E5A34", "#3E5A34"],
  "hill:5": ["#C89A5A", "#9A7040", "#9A7040"],
  "hill:6": ["#8A96A0", "#646E78", "#646E78"],
  "hill:7": ["#9AAAA8", "#748480", "#748480"],
  "hill:9": ["#4A4248", "#2E2830", "#2E2830"],
};

// Season callouts for props (CPU side; the canvas grade is applied on the GPU for everything).
function propColours(type, biome, season) {
  let pal = (PROP_PAL[type + ":" + biome] || PROP_PAL[type]).slice();
  if (type === "broadleaf" || type === "shrub") {
    if (season === "Bloom") pal = ["#6AA048", "#447A34", pal[2]];
    if (season === "Dry") pal = ["#9A9A44", "#6E7034", pal[2]];
    if (season === "Freeze") pal = ["#8A7A58", "#6A5A44", pal[2]];
  }
  if (type === "grass") {
    const g = { Bloom: "#A8C858", Hunt: "#D4B84A", Dry: "#B07830", Freeze: "#A89040" }[season];
    pal[0] = g;
  }
  if (type === "reeds" && season === "Dry") pal[0] = "#8A7850";
  if (type === "jungle" && season === "Dry") pal[1] = "#6A7A30";
  if (type === "volcano" && season === "Freeze") pal = ["#1A1418", "#120E12", "#8A1A08"];
  return pal.map(hexToRgb);
}

// Thriving-baseline prop counts per cell, by biome.
const BIOME_PROPS = {
  2: [["grass", 3.0, 0.42], ["shrub", 0.7, 0.42], ["broadleaf", 0.35, 0.62]],
  3: [["broadleaf", 4.2, 0.62], ["conifer", 1.6, 0.62], ["shrub", 0.8, 0.4]],
  4: [["reeds", 4.0, 0.42], ["shrub", 0.6, 0.38]],
  5: [["rock", 0.7, 0.42], ["shrub", 0.5, 0.32]],
  6: [["conifer", 2.2, 0.56], ["rock", 0.8, 0.4]],
  7: [["conifer", 1.3, 0.5], ["shrub", 0.9, 0.34], ["rock", 0.5, 0.36]],
  8: [["jungle", 4.2, 0.7], ["broadleaf", 1.5, 0.66]],
  9: [["rock", 1.0, 0.42], ["deadtree", 0.5, 0.46]],
};
const TREE_TYPES = new Set(["broadleaf", "conifer", "jungle", "shrub"]);

// Builds the instance list. healthFn(cell) is 0 (Collapsed) .. 1 (Thriving).
function placeProps(world, healthFn, season, opts) {
  const out = [];
  const L = world.L;
  const push = (type, x, y, scale, biome, health, seed) => {
    const pal = propColours(type, biome, season);
    out.push(x, y, world.elevAt(x, y), scale, PROP_INDEX[type], health, seed, 0, ...pal[0], ...pal[1], ...pal[2]);
  };
  for (let c = 0; c < world.n; ++c) {
    if (world.water[c] !== 0) continue;
    const b = L.biome[c], t = L.terrain[c], h = healthFn(c);
    const rnd = mulberry(c * 7919 + 13);
    const cx = world.cx[c], cy = world.cy[c];
    if (opts.peaks && (t === TERRAIN.Mountain || t === TERRAIN.Peak)) {
      // Fewer, bigger, overlapping peaks read as a range (reference: Strategic Map Overview), not a field of tents.
      if (t === TERRAIN.Peak || rnd() < 0.5)
        push("peak", cx + (rnd() - 0.5) * 0.6, cy + 0.8, (2.2 + L.elevation[c] * 3.2) * (0.85 + rnd() * 0.3), b, 1, rnd());
    } else if (opts.peaks && t === TERRAIN.Volcano) {
      push("volcano", cx, cy + 0.6, 2.4, b, 1, rnd());
    } else if (opts.peaks && t === TERRAIN.Hills && rnd() < 0.7) {
      push("hill", cx + (rnd() - 0.5) * 0.6, cy + 0.5 + (rnd() - 0.5) * 0.3, 1.25 + rnd() * 0.4, b, 1, rnd());
    }
    if (!opts.vegetation) continue;
    const rules = BIOME_PROPS[b] || [];
    // Ecology drives density: Collapsed ~5%, Stressed ~45%, Stable 100%, Thriving 130%.
    const dens = h < 0.33 ? 0.05 + h * 1.2 : h < 0.66 ? 0.45 + (h - 0.33) * 1.67 : 1.0 + (h - 0.66) * 0.9;
    const onSlope = t === TERRAIN.Mountain || t === TERRAIN.Peak ? 0.35 : 1;
    for (const [type, base, size] of rules) {
      const n = base * dens * onSlope * opts.density;
      let count = Math.floor(n) + (rnd() < n - Math.floor(n) ? 1 : 0);
      for (let k = 0; k < count; ++k) {
        let x, y, tries = 0;
        do { const a = rnd() * Math.PI * 2, r = Math.sqrt(rnd()) * 0.85; x = cx + Math.cos(a) * r; y = cy + Math.sin(a) * r; }
        while (world.cellAtWarped(x, y) !== c && ++tries < 6);
        if (tries >= 6) continue;
        let tt = type;
        if (TREE_TYPES.has(type) && h < 0.22 && rnd() < 0.6) tt = "deadtree";
        if (type === "broadleaf" && season === "Freeze" && b === BIOME.TemperateForest && rnd() < 0.7) tt = "deadtree";
        push(tt, x, y, size * (0.8 + rnd() * 0.4), b, h, rnd());
      }
    }
  }
  return new Float32Array(out);
}
const PROP_STRIDE = 17;
