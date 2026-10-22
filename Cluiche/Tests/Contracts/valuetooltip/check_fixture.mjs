// Self-check for the value-tooltip golden fixture. Plain node, no deps.
// Usage: node Cluiche/Tests/Contracts/valuetooltip/check_fixture.mjs
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const dir = dirname(fileURLToPath(import.meta.url));
const load = (f) => JSON.parse(readFileSync(join(dir, f), "utf8"));
const entries = load("entries.json");
const expected = load("expected.json");

const ORDER = ["title", "description", "enum_values", "hint", "meta", "affects", "related", "source", "pack_badge"];
const errors = [];
const err = (id, msg) => errors.push(id + ": " + msg);
const has = (o, k) => o[k] !== undefined && o[k] !== null;
const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);

// Reference implementation of the README conventions (used as an oracle only).
function derive(c) {
  const e = c.entry, o = c.options, s = [];
  s.push({ section: "title", text: e.label });
  s.push({ section: "description", text: e.description });
  if (e.values && e.values.length) s.push({ section: "enum_values", values: e.values });
  if (o.inspector && e.hint) s.push({ section: "hint", text: e.hint });
  const items = [];
  if (e.unit) items.push({ kind: "unit", text: e.unit });
  if (has(e, "min") && has(e, "max")) items.push({ kind: "range", text: String(e.min) + " – " + String(e.max) });
  else if (has(e, "min")) items.push({ kind: "range", text: "≥ " + String(e.min) });
  else if (has(e, "max")) items.push({ kind: "range", text: "≤ " + String(e.max) });
  if (has(e, "default")) {
    const d = e.default, dec = e.ui && e.ui.decimals;
    items.push({ kind: "default", text: typeof d === "number" && dec !== undefined ? d.toFixed(dec) : String(d) });
  }
  if (items.length) s.push({ section: "meta", items: items });
  if (e.affects && e.affects.length) s.push({ section: "affects", chips: e.affects });
  if (e.related && e.related.length)
    s.push({ section: "related", links: e.related.map((k) => ({ key: k, link: o.resolvable.includes(k) })) });
  if (e.source) s.push({ section: "source", source: e.source, defined_in: c.for + " · " + e.key });
  if (o.packId) s.push({ section: "pack_badge", text: "overridden by pack " + o.packId });
  return s;
}

const ids = entries.entries.map((c) => c.id);
if (new Set(ids).size !== ids.length) err("entries", "duplicate case ids");
const expIds = Object.keys(expected.cases);
for (const id of ids) if (!expected.cases[id]) err(id, "no expected entry");
for (const id of expIds) if (!ids.includes(id)) err(id, "expected entry has no case in entries.json");

for (const c of entries.entries) {
  const exp = expected.cases[c.id];
  if (!exp) continue;
  const names = exp.sections.map((s) => s.section);
  let last = -1;
  for (const n of names) {
    const i = ORDER.indexOf(n);
    if (i < 0) err(c.id, "unknown section '" + n + "'");
    else if (i <= last) err(c.id, "section '" + n + "' out of order or duplicated");
    else last = i;
  }
  const want = derive(c);
  const wantNames = want.map((s) => s.section);
  if (!eq(wantNames, names)) err(c.id, "sections " + JSON.stringify(names) + " != derived " + JSON.stringify(wantNames));
  else for (let i = 0; i < want.length; i++)
    if (!eq(want[i], exp.sections[i]))
      err(c.id, "section '" + names[i] + "' content mismatch\n  expected.json: " + JSON.stringify(exp.sections[i]) + "\n  derived:       " + JSON.stringify(want[i]));
}

if (errors.length) {
  console.log("FAIL (" + errors.length + ")");
  for (const e of errors) console.log("  " + e);
  process.exit(1);
}
console.log("OK: " + ids.length + " cases, entries.json and expected.json consistent");
