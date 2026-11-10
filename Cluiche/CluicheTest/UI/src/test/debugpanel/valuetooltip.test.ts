// DiaValueSchema Feature 3 (tooltips), panel side: renderValueTooltip against the shared golden
// fixture (Cluiche/Tests/Contracts/valuetooltip), schemaKey resolution, default tick, pack marker,
// fetch/cache/re-query, keyboard and Related links.
// NOTE: runs in jsdom, not Ultralight's JavaScriptCore: proves page logic only.
import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadPanel, type Panel } from "./loadPanel";

const here = path.dirname(fileURLToPath(import.meta.url));
const CONTRACTS = path.resolve(here, "../../../../../Tests/Contracts");
const readJson = (rel: string): any => JSON.parse(fs.readFileSync(path.join(CONTRACTS, rel), "utf-8"));
const schemaState = (): string => fs.readFileSync(path.join(CONTRACTS, "debugpanel/domain_states_schema.json"), "utf-8");

const fixtureEntries = readJson("valuetooltip/entries.json").entries as Array<{
  id: string;
  for: string;
  entry: any;
  options: { inspector: boolean; packId: string | null; resolvable: string[] };
}>;
const fixtureExpected = readJson("valuetooltip/expected.json").cases as Record<string, { sections: unknown[] }>;

const txt = (el: Element | null) => el?.textContent ?? "";

// DOM -> expected.json shape (same extraction as Dia/DiaEditorUI/src/ValueTooltip.test.tsx).
function extractSections(root: Element): any[] {
  const out: any[] = [];
  root.querySelectorAll(":scope > [data-vt-section]").forEach((el) => {
    const name = el.getAttribute("data-vt-section")!;
    switch (name) {
      case "enum_values":
        out.push({
          section: name,
          values: Array.from(el.querySelectorAll("[data-vt-enum-id]")).map((v) => ({
            id: v.getAttribute("data-vt-enum-id"),
            label: txt(v.querySelector("[data-vt-enum-label]")),
            description: txt(v.querySelector("[data-vt-enum-desc]")),
          })),
        });
        break;
      case "meta":
        out.push({
          section: name,
          items: Array.from(el.querySelectorAll("[data-vt-kind]")).map((i) => ({
            kind: i.getAttribute("data-vt-kind"),
            text: txt(i.querySelector("[data-vt-value]")),
          })),
        });
        break;
      case "affects":
        out.push({ section: name, chips: Array.from(el.querySelectorAll("[data-vt-chip]")).map((c) => c.getAttribute("data-vt-chip")) });
        break;
      case "related":
        out.push({
          section: name,
          links: Array.from(el.querySelectorAll("[data-vt-key]")).map((l) => ({
            key: l.getAttribute("data-vt-key"),
            link: l.getAttribute("data-vt-link") === "true",
          })),
        });
        break;
      case "source":
        out.push({ section: name, source: txt(el.querySelector("[data-vt-source]")), defined_in: txt(el.querySelector("[data-vt-defined-in]")) });
        break;
      default:
        out.push({ section: name, text: txt(el) });
    }
  });
  return out;
}

// Schema entries (query result shape: sidecar field names, `for` + `key`).
const FOR = "cow.ecology_params";
const grazing = {
  for: FOR, key: "grazing.rate", kind: "parameter", label: "Grazing rate",
  description: "How much vegetation herbivores eat each season.", hint: "Panel must not show this.",
  unit: "x per season", type: "float", min: 0, max: 2, default: 1.0,
  affects: ["producers_total"], related: ["growth.rate", "absent.key"], source: "A1 step 6",
  ui: { group: "Herbivory", step: 0.05, decimals: 2 },
};
const growth = {
  for: FOR, key: "growth.rate", kind: "parameter", label: "Growth (schema)",
  description: "How fast plants regrow.", type: "float",
};
const result = (version: number, ...entries: any[]) => JSON.stringify({ version, entries });

let p: Panel;
beforeEach(() => {
  vi.useFakeTimers();
  p = loadPanel();
});
afterEach(() => {
  p.close();
  vi.useRealTimers();
});

const rows = () => Array.from(p.doc.querySelectorAll(".ctrl")) as HTMLElement[];
const rowFor = (k: string) => rows().find((r) => r.getAttribute("data-control") === k)!;
const labelOf = (r: HTMLElement) => r.querySelector(".ctrl-label")!.textContent;
const rangeOf = (r: HTMLElement) => r.querySelector("input[type=range]") as HTMLInputElement;
const tooltip = () => p.doc.querySelector("[data-vt-tooltip]");
const iconOf = (r: HTMLElement) => r.querySelector(".info-icon") as HTMLElement | null;
const schemaGets = () => p.calls.filter((c) => c[0] === "schemaget");
const state = (controls: any[], extra: Record<string, any> = {}, name = "Ecology") =>
  JSON.stringify({ [name]: { _displayName: name, _group: "Navigation", _description: "", _accent: "#60b040", controls, ...extra } });
const slider = (key: string, over: Record<string, any> = {}) => ({ key, type: "slider", value: 1, schemaKey: `${FOR}/${key}`, ...over });

describe("renderValueTooltip: shared golden fixture", () => {
  it("fixture has cases", () => expect(fixtureEntries.length).toBeGreaterThan(5));
  for (const c of fixtureEntries) {
    it(`renders case ${c.id}`, () => {
      const el = p.win.renderValueTooltip(c.entry, {
        inspector: c.options.inspector,
        packId: c.options.packId,
        resolvable: c.options.resolvable,
        forId: c.for,
      });
      expect(el.hasAttribute("data-vt-tooltip")).toBe(true);
      expect(extractSections(el)).toEqual(fixtureExpected[c.id].sections);
    });
  }
});

describe("schemaKey controls: resolution rules", () => {
  it("schema controls ({name,type,schemaKey}) render as sliders; buttons do not", () => {
    p.win.updateDomainState(schemaState());
    expect(rows().length).toBe(2);
    expect(rows().map((r) => r.getAttribute("data-control"))).toEqual(["Grazing rate", "Growth rate"]);
  });

  it("schema fills label, min, max, step and decimals when the control omits them", () => {
    p.win.updateDomainState(state([slider("grazing.rate")]));
    p.win.valueSchemaResult(result(7, grazing));
    const r = rowFor("grazing.rate");
    expect(labelOf(r)).toBe("Grazing rate");
    expect(rangeOf(r).min).toBe("0");
    expect(rangeOf(r).max).toBe("2");
    expect(rangeOf(r).step).toBe("0.05");
    expect(r.querySelector(".cv")!.textContent).toBe("1.00");
  });

  it("any field present on the control wins over the schema", () => {
    p.win.updateDomainState(state([slider("grazing.rate", { label: "Mine", min: 0.5, max: 4, step: 0.5, decimals: 1 })]));
    p.win.valueSchemaResult(result(7, grazing));
    const r = rowFor("grazing.rate");
    expect(labelOf(r)).toBe("Mine");
    expect(rangeOf(r).min).toBe("0.5");
    expect(rangeOf(r).max).toBe("4");
    expect(rangeOf(r).step).toBe("0.5");
    expect(r.querySelector(".cv")!.textContent).toBe("1.0");
  });

  it("the value display keeps the schema decimals on later frames", () => {
    p.win.updateDomainState(state([slider("grazing.rate", { value: 1 })]));
    p.win.valueSchemaResult(result(7, grazing));
    p.win.updateDomainState(state([slider("grazing.rate", { value: 1.5 })]));
    expect(rowFor("grazing.rate").querySelector(".cv")!.textContent).toBe("1.50");
  });

  it("a legacy control (key/label/min/max/step, no schemaKey) is unchanged: no (i), no tick", () => {
    p.win.updateDomainState(state([{ key: "speed", label: "Speed", min: 0, max: 4, step: 1, value: 2 }]));
    const r = rowFor("speed");
    expect(labelOf(r)).toBe("Speed");
    expect(iconOf(r)).toBeNull();
    expect(r.querySelector(".dtick")).toBeNull();
    expect(schemaGets().length).toBe(0);
  });
});

describe("default tick", () => {
  const tickOf = (r: HTMLElement) => r.querySelector(".dtick") as HTMLElement | null;
  it("sits at the schema default as a fraction of the track", () => {
    p.win.updateDomainState(state([slider("grazing.rate")]));
    p.win.valueSchemaResult(result(7, grazing));
    expect(parseFloat(tickOf(rowFor("grazing.rate"))!.style.left)).toBeCloseTo(50, 5);
  });
  it("uses the control's min/max when they override the schema", () => {
    p.win.updateDomainState(state([slider("grazing.rate", { min: 0.5, max: 2 })]));
    p.win.valueSchemaResult(result(7, grazing));
    expect(parseFloat(tickOf(rowFor("grazing.rate"))!.style.left)).toBeCloseTo(33.3333, 3);
  });
  it("a default of 0 still shows a tick at 0%", () => {
    p.win.updateDomainState(state([slider("grazing.rate")]));
    p.win.valueSchemaResult(result(7, { ...grazing, default: 0 }));
    expect(parseFloat(tickOf(rowFor("grazing.rate"))!.style.left)).toBeCloseTo(0, 5);
  });
  it("is absent when the entry has no default", () => {
    p.win.updateDomainState(state([slider("growth.rate")]));
    p.win.valueSchemaResult(result(7, growth));
    expect(rowFor("growth.rate")).toBeDefined();
    expect(tickOf(rowFor("growth.rate"))).toBeNull();
  });
});

describe("pack marker", () => {
  it("shows 'pack: lush' on the row and the badge in the tooltip; no marker without packId", () => {
    p.win.updateDomainState(schemaState());
    p.win.valueSchemaResult(result(7, { ...grazing, key: "grazing.rate" }, { ...growth }));
    const withPack = rowFor("Grazing rate");
    const without = rowFor("Growth rate");
    expect(withPack.querySelector("[data-role=pack-marker]")!.textContent).toBe("pack: lush");
    expect(without.querySelector("[data-role=pack-marker]")).toBeNull();
    iconOf(withPack)!.focus();
    const badge = tooltip()!.querySelector("[data-vt-section=pack_badge]");
    expect(badge!.textContent).toBe("overridden by pack lush");
  });
});

describe("fetch, cache and re-query", () => {
  it("calls valueschema.get once per distinct prefix, not per control or frame", () => {
    p.win.updateDomainState(schemaState());
    expect(schemaGets()).toEqual([["schemaget", `${FOR}/`]]);
    p.win.updateDomainState(schemaState());
    p.win.updateDomainState(schemaState());
    expect(schemaGets().length).toBe(1);
  });

  it("two prefixes across domains give two calls", () => {
    p.win.updateDomainState(JSON.stringify({
      ...JSON.parse(state([slider("a")])),
      ...JSON.parse(state([{ key: "b", type: "slider", value: 0, schemaKey: "other.params/b" }], {}, "Other")),
    }));
    expect(schemaGets().map((c) => c[1]).sort()).toEqual([`${FOR}/`, "other.params/"]);
  });

  it("does not re-query while schemaVersion is unchanged, even after the result arrives", () => {
    p.win.updateDomainState(schemaState());
    p.win.valueSchemaResult(result(7, grazing, growth));
    p.win.updateDomainState(schemaState());
    expect(schemaGets().length).toBe(1);
  });

  it("re-queries when a frame reports a different schemaVersion", () => {
    p.win.updateDomainState(schemaState());
    p.win.valueSchemaResult(result(7, grazing));
    const v8 = JSON.parse(schemaState());
    v8.Ecology.schemaVersion = 8;
    p.win.updateDomainState(JSON.stringify(v8));
    expect(schemaGets().length).toBe(2);
    p.win.updateDomainState(JSON.stringify(v8));
    expect(schemaGets().length).toBe(2);
  });

  it("controls without schemaKey never query", () => {
    p.win.updateDomainState(state([{ key: "k", label: "K", value: 1 }]));
    expect(schemaGets().length).toBe(0);
  });
});

describe("unresolved schemaKey", () => {
  it("renders the raw label with no (i), no tooltip and does not throw", () => {
    expect(() => p.win.updateDomainState(state([slider("missing.key", { label: "Raw label" })]))).not.toThrow();
    expect(() => p.win.valueSchemaResult(result(7, grazing))).not.toThrow();
    const r = rowFor("missing.key");
    expect(labelOf(r)).toBe("Raw label");
    expect(iconOf(r)).toBeNull();
    expect(tooltip()).toBeNull();
  });
  it("before any result arrives the raw name is shown", () => {
    p.win.updateDomainState(schemaState());
    expect(labelOf(rowFor("Grazing rate"))).toBe("Grazing rate");
    expect(iconOf(rowFor("Grazing rate"))).toBeNull();
  });
  it("garbage results are ignored", () => {
    p.win.updateDomainState(schemaState());
    expect(() => p.win.valueSchemaResult("not json")).not.toThrow();
    expect(() => p.win.valueSchemaResult(JSON.stringify({ version: 1 }))).not.toThrow();
    expect(rows().length).toBe(2);
  });
});

describe("tooltip interaction", () => {
  beforeEach(() => {
    p.win.updateDomainState(state([slider("grazing.rate"), slider("growth.rate")]));
    p.win.valueSchemaResult(result(7, grazing, growth));
  });

  it("hover on the (i) shows the tooltip; leaving hides it", () => {
    const icon = iconOf(rowFor("grazing.rate"))!;
    expect(icon).not.toBeNull();
    expect(tooltip()).toBeNull();
    icon.dispatchEvent(new p.win.MouseEvent("mouseenter", { bubbles: false }));
    expect(tooltip()).not.toBeNull();
    icon.dispatchEvent(new p.win.MouseEvent("mouseleave", { bubbles: false }));
    vi.advanceTimersByTime(500);
    expect(tooltip()).toBeNull();
  });

  it("keyboard focus on the (i) shows the tooltip, sections in order, hint hidden on panel rows", () => {
    iconOf(rowFor("grazing.rate"))!.focus();
    const t = tooltip()!;
    expect(t).not.toBeNull();
    expect(extractSections(t).map((s) => s.section)).toEqual(["title", "description", "meta", "affects", "related", "source"]);
    expect(extractSections(t)[0].text).toBe("Grazing rate");
  });

  it("blur hides it", () => {
    const icon = iconOf(rowFor("grazing.rate"))!;
    icon.focus();
    icon.blur();
    vi.advanceTimersByTime(500);
    expect(tooltip()).toBeNull();
  });

  it("Esc closes it", () => {
    iconOf(rowFor("grazing.rate"))!.focus();
    expect(tooltip()).not.toBeNull();
    p.key(p.doc.body, "Escape");
    expect(tooltip()).toBeNull();
  });

  it("empty sections are omitted (bare entry shows title and description only)", () => {
    iconOf(rowFor("growth.rate"))!.focus();
    expect(extractSections(tooltip()!).map((s) => s.section)).toEqual(["title", "description"]);
  });

  it("only one tooltip is open at a time", () => {
    iconOf(rowFor("grazing.rate"))!.focus();
    iconOf(rowFor("growth.rate"))!.focus();
    expect(p.doc.querySelectorAll("[data-vt-tooltip]").length).toBe(1);
  });

  it("the tooltip stays inside the viewport width (max width capped)", () => {
    iconOf(rowFor("grazing.rate"))!.focus();
    const t = tooltip() as HTMLElement;
    expect(t.style.position).toBe("fixed");
    expect(parseInt(t.style.maxWidth, 10)).toBeLessThanOrEqual(320);
  });
});

describe("Related links", () => {
  beforeEach(() => {
    p.win.updateDomainState(state([slider("grazing.rate"), slider("growth.rate")]));
    p.win.valueSchemaResult(result(7, grazing, growth));
  });

  it("a Related key that is a control on the panel is a link; others are plain text", () => {
    iconOf(rowFor("grazing.rate"))!.focus();
    const sec = extractSections(tooltip()!).find((s) => s.section === "related");
    expect(sec.links).toEqual([
      { key: "growth.rate", link: true },
      { key: "absent.key", link: false },
    ]);
  });

  it("clicking the link focuses the matching control", () => {
    iconOf(rowFor("grazing.rate"))!.focus();
    const link = tooltip()!.querySelector('[data-vt-key="growth.rate"]') as HTMLElement;
    p.click(link);
    expect(p.doc.activeElement).toBe(rangeOf(rowFor("growth.rate")));
  });

  it("the plain-text entry is not clickable", () => {
    iconOf(rowFor("grazing.rate"))!.focus();
    const plain = tooltip()!.querySelector('[data-vt-key="absent.key"]') as HTMLElement;
    expect(plain.tagName).not.toBe("BUTTON");
    const before = p.doc.activeElement;
    p.click(plain);
    expect(p.doc.activeElement).toBe(before);
  });
});
