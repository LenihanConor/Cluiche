// DiaCellLayers Registry-and-Viewer, panel card (AC 16, 17): the golden state of the REAL CellLayerViewerDomain
// (Cluiche/Tests/Contracts/debugpanel/domain_states_celllayers*.json) rendered by the real page.
// Layout reference: docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html.
// NOTE: runs in jsdom, not Ultralight's JavaScriptCore: proves page logic only.
import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadPanel, type Panel } from "./loadPanel";

const here = path.dirname(fileURLToPath(import.meta.url));
const GOLDEN_DIR = path.resolve(here, "../../../../../Tests/Contracts/debugpanel");
const golden = (name: string): string => fs.readFileSync(path.join(GOLDEN_DIR, name), "utf-8");
const goldenJson = (name: string): any => JSON.parse(golden(name));

let p: Panel;
beforeEach(() => {
  vi.useFakeTimers();
  p = loadPanel();
});
afterEach(() => {
  p.close();
  vi.useRealTimers();
});

const NAME = "Cell Layers";
const push = (file: string) => p.win.updateDomainState(golden(file));
const card = () => p.doc.querySelector(`.domain[data-domain="${NAME}"]`) as HTMLElement;
const viewer = () => card().querySelector("[data-role=cell-layer-viewer]") as HTMLElement;
const item = (id: string) => viewer().querySelector(`.layer-item[data-layer="${id}"]`) as HTMLElement;
const overlay = (id: string) => viewer().querySelector(`.layer-item[data-overlay="${id}"]`) as HTMLElement;
const txt = (el: Element | null) => el?.textContent ?? "";
const commands = () => p.calls.filter((c) => c[0] === "command");

describe("contract: domain_states_celllayers.json shape", () => {
  it("has the keys the card reads", () => {
    const s = goldenJson("domain_states_celllayers.json")[NAME];
    expect(s._displayName).toBe(NAME);
    expect(s.domainId).toBe("celllayers");
    for (const k of ["layers", "groups", "overlays", "grids"]) expect(Array.isArray(s[k])).toBe(true);
    expect(typeof s.empty).toBe("boolean");
    expect(typeof s.activeBase).toBe("string");
    expect(s.legend.available).toBe(true);
    expect(s.hover.valid).toBe(true);
  });
});

describe("picker (AC 16)", () => {
  beforeEach(() => push("domain_states_celllayers.json"));

  it("renders one card with a viewer section and no generic drawers row", () => {
    expect(p.doc.querySelectorAll(".domain").length).toBe(1);
    expect(viewer()).not.toBeNull();
    expect(card().querySelector(".drawers .dr")).toBeNull();
  });

  it("groups layers under headers in registry order, one radio item per layer", () => {
    const groups = Array.from(viewer().querySelectorAll(".layer-group[data-group]")) as HTMLElement[];
    expect(groups.map((g) => g.getAttribute("data-group"))).toEqual(["Terrain", "Ecology", "Overlays"]);
    expect(txt(groups[0].querySelector(".layer-group-header"))).toBe("Terrain");
    const ids = (g: HTMLElement) => Array.from(g.querySelectorAll(".layer-item[data-layer]")).map((i) => i.getAttribute("data-layer"));
    expect(ids(groups[0])).toEqual(["biome"]);
    expect(ids(groups[1])).toEqual(["temp", "rain"]);
    expect(item("temp").querySelector(".layer-radio")).not.toBeNull();
  });

  it("shows label, group number and hotkey", () => {
    expect(txt(item("temp").querySelector(".layer-label"))).toBe("Temperature");
    expect(txt(item("temp").querySelector(".layer-num"))).toBe("1");
    expect(txt(item("rain").querySelector(".layer-num"))).toBe("2");
    expect(txt(item("temp").querySelector(".layer-hotkey"))).toBe("t");
    expect(item("rain").querySelector(".layer-hotkey")).toBeNull();
  });

  it("marks only the selected base as the active radio", () => {
    expect(item("temp").querySelector(".layer-radio")!.classList.contains("active")).toBe(true);
    expect(item("rain").querySelector(".layer-radio")!.classList.contains("active")).toBe(false);
    expect(item("biome").querySelector(".layer-radio")!.classList.contains("active")).toBe(false);
  });

  it("clicking a layer sends base {id} to the domain id", () => {
    p.click(item("rain"));
    expect(commands().length).toBe(1);
    expect(commands()[0]).toEqual(["command", "celllayers", "base", JSON.stringify({ id: "rain" })]);
  });

  it("renders overlays as checkboxes reflecting enabled, and a click sends toggle {drawer}", () => {
    expect(overlay("wind").querySelector(".layer-checkbox")!.classList.contains("checked")).toBe(true);
    expect(overlay("edges").querySelector(".layer-checkbox")!.classList.contains("checked")).toBe(false);
    expect(txt(overlay("wind").querySelector(".layer-label"))).toBe("Wind arrows");
    p.click(overlay("edges"));
    expect(commands()[0]).toEqual(["command", "celllayers", "toggle", JSON.stringify({ drawer: "edges" })]);
  });

  it("a later frame with another base updates the radio without rebuilding the section", () => {
    const section = viewer();
    const s = goldenJson("domain_states_celllayers.json");
    s[NAME].activeBase = "rain";
    s[NAME].layers.forEach((l: any) => (l.selected = l.id === "rain"));
    p.win.updateDomainState(JSON.stringify(s));
    expect(viewer()).toBe(section);
    expect(item("rain").querySelector(".layer-radio")!.classList.contains("active")).toBe(true);
    expect(item("temp").querySelector(".layer-radio")!.classList.contains("active")).toBe(false);
  });

  it("the search box filters the layer list by label", () => {
    const input = viewer().querySelector(".layer-search input") as HTMLInputElement;
    expect(input).not.toBeNull();
    p.typeInto(input, "rain");
    expect(item("rain").style.display).not.toBe("none");
    expect(item("temp").style.display).toBe("none");
  });

  it("there is no grid selector with a single grid", () => {
    expect(viewer().querySelector("[data-role=grid-selector]")).toBeNull();
  });
});

describe("picker: multi-grid and empty (AC 16)", () => {
  it("shows the grid selector only when gridSelector is present; click sends grid {index}", () => {
    push("domain_states_celllayers_category.json");
    const sel = viewer().querySelector("[data-role=grid-selector]") as HTMLElement;
    expect(sel).not.toBeNull();
    const btns = Array.from(sel.querySelectorAll("[data-grid]")) as HTMLElement[];
    expect(btns.map((b) => b.getAttribute("data-grid"))).toEqual(["0", "1"]);
    expect(btns[0].classList.contains("active")).toBe(true);
    expect(btns[1].classList.contains("active")).toBe(false);
    expect(txt(btns[1])).toContain("5");
    p.click(btns[1]);
    expect(commands()[0]).toEqual(["command", "celllayers", "grid", JSON.stringify({ index: 1 })]);
  });

  it("the empty state shows a message and no picker, legend or hover", () => {
    push("domain_states_celllayers_empty.json");
    expect(viewer().querySelector(".cl-empty")).not.toBeNull();
    expect(viewer().querySelectorAll(".layer-item").length).toBe(0);
    expect(viewer().querySelector(".legend-strip")).toBeNull();
    expect(viewer().querySelector("[data-role=hover]")!.children.length).toBe(0);
  });

  it("leaving the empty state rebuilds the picker", () => {
    push("domain_states_celllayers_empty.json");
    push("domain_states_celllayers.json");
    expect(viewer().querySelector(".cl-empty")).toBeNull();
    expect(viewer().querySelectorAll(".layer-item[data-layer]").length).toBe(3);
  });
});

describe("legend (AC 17)", () => {
  it("float base: ramp gradient, ticks, unit and observed range", () => {
    push("domain_states_celllayers.json");
    const lg = viewer().querySelector(".legend-strip") as HTMLElement;
    expect(lg).not.toBeNull();
    expect(txt(viewer().querySelector("[data-role=legend-title]"))).toBe("LEGEND: TEMPERATURE");
    const ramp = lg.querySelector(".legend-ramp") as HTMLElement;
    expect(ramp.style.background).toContain("linear-gradient");
    expect(ramp.style.background).toContain("rgb(0, 0, 0)");
    expect(ramp.style.background).toContain("rgb(255, 255, 255)");
    const labels = Array.from(lg.querySelectorAll(".legend-labels span")).map((s) => txt(s));
    expect(labels).toEqual(["-10", "-5", "0", "5", "10"]);
    expect(txt(lg.querySelector(".legend-unit"))).toBe("C");
    expect(txt(lg.querySelector(".legend-range"))).toBe("seen in view: -2 – 9");
  });

  it("category base: one swatch per category with its colour, no ramp", () => {
    push("domain_states_celllayers_category.json");
    const lg = viewer().querySelector(".legend-strip") as HTMLElement;
    expect(lg.querySelector(".legend-ramp")).toBeNull();
    const sw = Array.from(lg.querySelectorAll(".legend-swatch")) as HTMLElement[];
    expect(sw.map((s) => txt(s.querySelector(".legend-swatch-label")))).toEqual(["v0", "v1", "v2"]);
    expect((sw[0].querySelector(".legend-swatch-box") as HTMLElement).style.background).toBe("rgb(78, 121, 167)");
  });

  it("follows the active base on later frames", () => {
    push("domain_states_celllayers.json");
    push("domain_states_celllayers_category.json");
    expect(viewer().querySelector(".legend-swatch")).not.toBeNull();
    expect(txt(viewer().querySelector("[data-role=legend-title]"))).toBe("LEGEND: BIOME");
  });
});

describe("hover (AC 17)", () => {
  it("renders the value readout and the shared value tooltip for the hovered layer", () => {
    push("domain_states_celllayers.json");
    const h = viewer().querySelector("[data-role=hover]") as HTMLElement;
    expect(txt(h.querySelector(".cl-hover-cell"))).toContain("2, 1");
    expect(txt(h.querySelector(".cl-hover-value"))).toBe("4 C");
    const tip = h.querySelector("[data-vt-tooltip]") as HTMLElement;
    expect(tip).not.toBeNull();
    expect(txt(tip.querySelector("[data-vt-section=title]"))).toBe("Temperature");
    expect(txt(tip.querySelector("[data-vt-section=description]"))).toBe("Air temperature");
    const unit = tip.querySelector('[data-vt-section=meta] [data-vt-kind=unit] [data-vt-value]');
    expect(txt(unit)).toBe("C");
  });

  it("a category hover shows the enum label text", () => {
    push("domain_states_celllayers_category.json");
    const h = viewer().querySelector("[data-role=hover]") as HTMLElement;
    expect(txt(h.querySelector(".cl-hover-value"))).toBe("v2");
    expect(txt(h.querySelector("[data-vt-section=title]"))).toBe("Biome");
  });

  it("clears when the cursor leaves the grid", () => {
    push("domain_states_celllayers.json");
    const s = goldenJson("domain_states_celllayers.json");
    s[NAME].hover = { valid: false };
    p.win.updateDomainState(JSON.stringify(s));
    expect(viewer().querySelector("[data-role=hover]")!.children.length).toBe(0);
  });
});
