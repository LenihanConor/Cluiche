// Generates Cluiche/out/preview/ramp-preview.html: a self-contained visual check of the DiaValueSchema ramp library
// (Feature 5, AC 14). Uses the real functions from src/ramps.ts (sampleRamp, buildLegend, RampResolver).
//
// Run from Dia/DiaEditorUI (node is not on PATH on this machine; prepend C:\Program Files\nodejs):
//   node node_modules/vite-node/vite-node.mjs tools/make-ramp-preview.ts

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { RampResolver, sampleRamp, buildLegend, rgba8ToHex, parseHexColour } from '../src/ramps';
import type { RampJson, Rgba8 } from '../src/ramps';
import type { ValueInfoLike } from '../src/valueInfo';

const here = dirname(fileURLToPath(import.meta.url));
const repo = resolve(here, '../../..');
const rampsFile = JSON.parse(readFileSync(resolve(repo, 'Dia/DiaValueSchema/Data/ramps.json'), 'utf8'));
const ramps: RampJson[] = rampsFile.ramps;
const kMinCategoricalDeltaE = 40;

// sRGB -> CIELAB D65 (port of Dia/DiaValueSchema/Testing/ColourMath.h) and CIE76.
type Lab = { L: number; a: number; b: number };
const lin = (c: number) => (c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4));
function toLab(c: Rgba8): Lab {
  const r = lin(((c >>> 24) & 255) / 255), g = lin(((c >>> 16) & 255) / 255), b = lin(((c >>> 8) & 255) / 255);
  const X = 0.4124564 * r + 0.3575761 * g + 0.1804375 * b;
  const Y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
  const Z = 0.0193339 * r + 0.1191920 * g + 0.9503041 * b;
  const f = (t: number) => (t > 216 / 24389 ? Math.cbrt(t) : ((24389 / 27) * t + 16) / 116);
  const fx = f(X / 0.95047), fy = f(Y / 1.0), fz = f(Z / 1.08883);
  return { L: 116 * fy - 16, a: 500 * (fx - fy), b: 200 * (fy - fz) };
}
const dE = (x: Lab, y: Lab) => Math.hypot(x.L - y.L, x.a - y.a, x.b - y.b);

const esc = (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
const fmt = (v: number) => (Math.abs(v) < 1e-9 ? '0' : String(+v.toFixed(4)));

const warnings: string[] = [];
const resolver = new RampResolver(ramps, (_k, m) => warnings.push(m));
const byId = (id: string) => ramps.find((r) => r.id === id)!;

function entry(over: Partial<ValueInfoLike>): ValueInfoLike {
  return { key: 'preview.x', label: 'x', description: '', ...over };
}

function strip(r: RampJson): string {
  let cells = '';
  for (let i = 0; i < 256; i++) {
    cells += `<i style="background:${rgba8ToHex(sampleRamp(r, i / 255))}"></i>`;
  }
  return `<div class="strip">${cells}</div>`;
}

function stopMarkers(r: RampJson): string {
  return (r.stops ?? []).map((s) =>
    `<span class="stop" style="left:${s.t * 100}%"><b></b><em>${fmt(s.t)}<br>${esc(s.colour)}</em></span>`).join('');
}

function legendTicks(min: number, max: number, ticks: number[], unit: string): string {
  const span = max - min || 1;
  const t = ticks.map((v) => {
    const p = ((v - min) / span) * 100;
    return `<span class="tick" style="left:${p}%"><b></b>${fmt(v)}</span>`;
  }).join('');
  return `<div class="ticks">${t}</div><div class="unit">unit: ${esc(unit || '(none)')} &nbsp; range ${fmt(min)} .. ${fmt(max)}</div>`;
}

function rampBlock(e: ValueInfoLike, title: string): string {
  const lg = buildLegend(resolver, e);
  const rj = byId(lg.ramp!.ramp.id);
  return `<div class="ramp"><h3>${esc(title)}</h3>${strip(rj)}
    <div class="stops">${stopMarkers(rj)}</div>
    ${legendTicks(lg.min, lg.max, lg.ticks, lg.unit)}</div>`;
}

const seqRanges: Record<string, [number, number, string]> = {
  'sequential.green': [0, 1, 'fraction'], 'sequential.blue': [0, 1, 'fraction'], 'sequential.grey': [0, 1, 'fraction'],
  'sequential.purple': [0, 1, 'fraction'], 'sequential.heat': [-10, 40, '\u00b0C'], 'sequential.terrain': [0, 4000, 'm'],
};

let body = '';
body += '<h2>a. Continuous ramps (256 samples via sampleRamp; legend via buildLegend)</h2>';
for (const r of ramps.filter((x) => x.kind === 'continuous' && x.id.startsWith('sequential.'))) {
  const [mn, mx, unit] = seqRanges[r.id] ?? [0, 1, ''];
  body += rampBlock(entry({ type: 'float', min: mn, max: mx, unit, ui: { ramp: r.id } }), r.id);
}

body += '<h2>b. Diverging ramp, symmetric delta range</h2>';
{
  const e = entry({ type: 'float', unit: '\u0394' });
  const res = resolver.resolveDeltaRamp(e, 5);
  const rj = byId(res.ramp.id);
  const lg = buildLegend(resolver, { ...e, min: res.min, max: res.max, ui: { ramp: res.ramp.id } });
  body += `<div class="ramp mid"><h3>${esc(res.ramp.id)} (resolveDeltaRamp, maxAbs 5)</h3>${strip(rj)}
    <div class="stops">${stopMarkers(rj)}</div>
    ${legendTicks(lg.min, lg.max, lg.ticks, lg.unit)}
    <div class="note">neutral midpoint (t=0.5) = ${rgba8ToHex(sampleRamp(rj, 0.5))}, marked by the white line</div></div>`;
}

function swatches(items: { label: string; colour: Rgba8 }[]): string {
  return '<div class="sw">' + items.map((s) =>
    `<div><span style="background:${rgba8ToHex(s.colour)}"></span><em>${esc(s.label)}<br>${rgba8ToHex(s.colour)}</em></div>`).join('') + '</div>';
}

body += '<h2>c. Categorical palette</h2>';
const cat = byId('categorical.12');
const catColours = (cat.colours ?? []).map((h) => parseHexColour(h)!) as Rgba8[];
body += `<div class="ramp"><h3>categorical.12 (raw)</h3>${swatches(catColours.map((c, i) => ({ label: String(i), colour: c })))}</div>`;
const val = (id: string, colour?: string) => ({ id, label: id, description: '', ...(colour ? { colour } : {}) });
{
  const e5 = entry({ key: 'preview.enum5', type: 'enum',
    values: [val('alpha', '#e41a1c'), val('beta'), val('gamma', '#377eb8'), val('delta'), val('epsilon', '#4daf4a')] });
  const lg = buildLegend(resolver, e5);
  body += `<div class="ramp"><h3>5-value enum via resolvePalette: 3 explicit colours (alpha, gamma, epsilon), 2 default by order (beta = idx 1, delta = idx 3)</h3>${swatches(lg.swatches)}</div>`;
  const ids = Array.from({ length: 14 }, (_, i) => 'v' + i);
  const e14 = entry({ key: 'preview.enum14', type: 'enum', values: ids.map((i) => val(i)) });
  const lg14 = buildLegend(resolver, e14);
  buildLegend(resolver, e14); // second call must not warn again
  const wrap = warnings.filter((w) => w.includes('enum14'));
  body += `<div class="ramp"><h3>14-value colour-less enum (wraps: v12 = v0, v13 = v1)</h3>${swatches(lg14.swatches)}
    <div class="warn">onWarn (fired ${wrap.length}x across two legend builds, once-only): ${esc(wrap.join(' | ') || '(none!)')}</div></div>`;
}

body += '<h2>d. Unknown ramp id falls back to sequential.grey</h2>';
{
  const before = warnings.length;
  const e = entry({ key: 'preview.unknown', type: 'float', min: 0, max: 1, unit: 'fraction', ui: { ramp: 'sequential.doesnotexist' } });
  const blk = rampBlock(e, "ui.ramp = 'sequential.doesnotexist'  ->  resolved: sequential.grey");
  body += blk + `<div class="warn">onWarn: ${esc(warnings.slice(before).join(' | ') || '(none!)')}</div>`;
}

body += '<h2>e. Numbers</h2>';
{
  let rows = '';
  for (const r of ramps.filter((x) => x.id.startsWith('sequential.'))) {
    const L = [0, 0.5, 1].map((t) => toLab(sampleRamp(r, t)).L);
    const mono3 = (L[0] > L[1] && L[1] > L[2]) || (L[0] < L[1] && L[1] < L[2]);
    const ls = Array.from({ length: 256 }, (_, i) => toLab(sampleRamp(r, i / 255)).L);
    const dir = Math.sign(ls[255] - ls[0]);
    const fullMono = ls.every((v, i) => i === 0 || (v - ls[i - 1]) * dir >= -1e-9);
    rows += `<tr><td>${r.id}</td><td>${L[0].toFixed(1)}</td><td>${L[1].toFixed(1)}</td><td>${L[2].toFixed(1)}</td><td>${mono3 ? 'yes' : 'NO'}</td><td>${fullMono ? 'yes' : 'NO'}</td></tr>`;
  }
  body += `<table><tr><th>ramp</th><th>L* t=0</th><th>L* t=0.5</th><th>L* t=1</th><th>3-pt monotonic</th><th>256-pt monotonic</th></tr>${rows}</table>`;
  const labs = catColours.map(toLab);
  let min = Infinity, pa = 0, pb = 0;
  for (let i = 0; i < labs.length; i++) for (let j = i + 1; j < labs.length; j++) {
    const d = dE(labs[i], labs[j]);
    if (d < min) { min = d; pa = i; pb = j; }
  }
  body += `<p class="note">categorical.12 minimum pairwise CIE76 deltaE = <b>${min.toFixed(1)}</b> (pair ${pa}, ${pb}); constant kMinCategoricalDeltaE = ${kMinCategoricalDeltaE}; plan measured 46.1; ${min >= kMinCategoricalDeltaE ? 'PASS' : 'FAIL'}.</p>`;
}

const css = `
body{background:#181818;color:#ddd;font:13px/1.4 "Segoe UI",system-ui,sans-serif;margin:24px;max-width:1100px}
h1{font-size:18px;font-weight:600} h2{font-size:15px;margin:32px 0 10px;border-bottom:1px solid #333;padding-bottom:4px;color:#9ab}
h3{font-size:12px;font-weight:600;margin:0 0 6px;color:#ccc;font-family:Consolas,monospace}
.ramp{background:#1f1f1f;border:1px solid #2c2c2c;border-radius:4px;padding:10px 14px 14px;margin-bottom:12px}
.strip{display:flex;height:34px;border:1px solid #444;position:relative}.strip i{flex:1}
.mid .strip::after{content:"";position:absolute;left:50%;top:-4px;bottom:-4px;width:2px;margin-left:-1px;background:#fff}
.stops{position:relative;height:34px;margin-top:2px}
.stop{position:absolute;transform:translateX(-50%);text-align:center}
.stop b{display:block;width:1px;height:5px;background:#777;margin:0 auto}
.stop em{font:9px/1.15 Consolas,monospace;color:#888;font-style:normal;white-space:nowrap}
.ticks{position:relative;height:22px;margin-top:2px;border-top:1px solid #555}
.tick{position:absolute;transform:translateX(-50%);font:11px Consolas,monospace;color:#ccc;text-align:center}
.tick b{display:block;width:1px;height:6px;background:#aaa;margin:0 auto 1px}
.unit{font-size:11px;color:#8a8;margin-top:2px}
.note{color:#999;font-size:11px;margin-top:6px}.warn{color:#e0b050;font:11px Consolas,monospace;margin-top:8px;word-break:break-word}
.sw{display:flex;flex-wrap:wrap;gap:8px}.sw div{width:80px;text-align:center}
.sw span{display:block;height:44px;border:1px solid #444;border-radius:3px}
.sw em{font:10px/1.2 Consolas,monospace;font-style:normal;color:#aaa}
table{border-collapse:collapse}td,th{border:1px solid #333;padding:3px 10px;text-align:right;font:12px Consolas,monospace}th{color:#9ab}td:first-child{text-align:left}
`;

const html = `<!doctype html><html><head><meta charset="utf-8"><title>Ramp preview</title><style>${css}</style></head><body><h1>DiaValueSchema ramp library preview</h1>
<div class="note">Generated by Dia/DiaEditorUI/tools/make-ramp-preview.ts from src/ramps.ts and Dia/DiaValueSchema/Data/ramps.json. CIE76 / D65.</div>${body}</body></html>`;

const outDir = resolve(repo, 'Cluiche/out/preview');
mkdirSync(outDir, { recursive: true });
const out = resolve(outDir, 'ramp-preview.html');
writeFileSync(out, html);
console.log('wrote ' + out);
