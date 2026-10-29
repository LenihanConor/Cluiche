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
    expect(s._id).toBe("celllayers");
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

describe("command target is the registry id, not the display name", () => {
  it("the celllayers golden carries _id and no per-card domainId", () => {
    const s = goldenJson("domain_states_celllayers.json")[NAME];
    expect(s._id).toBe("celllayers");
    expect(s.domainId).toBeUndefined();
  });

  it("a domain whose id differs from its display name: drawer click targets the id", () => {
    p.win.updateDomainState(golden("domain_states.json"));
    const bus = p.doc.querySelector('.domain[data-domain="Message Bus"]') as HTMLElement;
    p.click(bus.querySelector('[data-drawer="Ledger"]')!);
    expect(commands()[0]).toEqual(["command", "MessageBus", "toggle", JSON.stringify({ drawer: "Ledger" })]);
  });

  it("the card on/off switch (All On/Off) targets the id", () => {
    p.win.updateDomainState(golden("domain_states.json"));
    const flow = p.doc.querySelector('.domain[data-domain="Flow Field"]') as HTMLElement;
    p.click(flow.querySelector(".tog")!);   // currently on -> turn all off: Vectors and Costs
    expect(commands().map((c) => c[1])).toEqual(["NavFlowField", "NavFlowField"]);
    expect(commands().map((c) => c[2])).toEqual(["toggle", "toggle"]);
  });

  it("a slider change targets the id", () => {
    p.win.updateDomainState(golden("domain_states_schema.json"));
    const eco = p.doc.querySelector('.domain[data-domain="Ecology"]') as HTMLElement;
    const range = eco.querySelector("input[type=range]") as HTMLInputElement;
    range.value = "0.5";
    range.dispatchEvent(new p.win.Event("change", { bubbles: true }));
    expect(commands()[0][1]).toBe("CowEcology");
    expect(commands()[0][2]).toBe("setScale");
  });

  it("falls back to the display name when the state has no _id", () => {
    const s = goldenJson("domain_states.json");
    delete s["Message Bus"]._id;
    p.win.updateDomainState(JSON.stringify(s));
    const bus = p.doc.querySelector('.domain[data-domain="Message Bus"]') as HTMLElement;
    p.click(bus.querySelector('[data-drawer="Ledger"]')!);
    expect(commands()[0][1]).toBe("Message Bus");
  });
});

describe("layer description (i) icon", () => {
  const icon = (id: string) => item(id).querySelector(".info-icon") as HTMLElement | null;
  const tips = () => Array.from(p.doc.querySelectorAll("body > [data-vt-tooltip]")) as HTMLElement[];
  beforeEach(() => push("domain_states_celllayers.json"));

  it("is present on layers with a description and absent otherwise", () => {
    expect(icon("temp")).not.toBeNull();
    expect(icon("biome")).not.toBeNull();
    const s = goldenJson("domain_states_celllayers.json");
    delete s[NAME].layers.find((l: any) => l.id === "rain").description;
    s[NAME].layers.find((l: any) => l.id === "temp").description = "";
    p.win.updateDomainState(JSON.stringify(s));
    expect(icon("rain")).toBeNull();
    expect(icon("temp")).toBeNull();
    expect(icon("biome")).not.toBeNull();
  });

  it("is keyboard reachable and labelled", () => {
    expect(icon("temp")!.getAttribute("tabindex")).toBe("0");
    expect(icon("temp")!.getAttribute("aria-label")).toBe("About Temperature");
  });

  it("hover shows the description in the shared tooltip; leaving hides it", () => {
    expect(tips().length).toBe(0);
    icon("temp")!.dispatchEvent(new p.win.MouseEvent("mouseenter", { bubbles: false }));
    expect(tips().length).toBe(1);
    expect(txt(tips()[0].querySelector("[data-vt-section=title]"))).toBe("Temperature");
    expect(txt(tips()[0].querySelector("[data-vt-section=description]"))).toBe("Air temperature");
    icon("temp")!.dispatchEvent(new p.win.MouseEvent("mouseleave", { bubbles: false }));
    vi.advanceTimersByTime(400);
    expect(tips().length).toBe(0);
  });

  it("focus shows the description", () => {
    icon("rain")!.focus();
    expect(tips().length).toBe(1);
    expect(txt(tips()[0].querySelector("[data-vt-section=description]"))).toBe("Rainfall");
  });

  it("clicking the icon does not change the base", () => {
    p.click(icon("rain")!);
    expect(commands().length).toBe(0);
    p.click(item("rain"));
    expect(commands().length).toBe(1);
  });
});

describe("noData message", () => {
  const nodata = () => viewer().querySelector("[data-role=nodata]") as HTMLElement | null;

  it("shows 'No data in this view' in place of legend and hover, picker stays usable", () => {
    push("domain_states_celllayers_nodata.json");
    expect(txt(nodata())).toBe("No data in this view");
    expect(viewer().querySelector(".legend-strip")).toBeNull();
    expect(viewer().querySelector("[data-role=hover]")!.children.length).toBe(0);
    expect(viewer().querySelectorAll(".layer-item[data-layer]").length).toBe(2);
    p.click(item("rain"));
    expect(commands()[0]).toEqual(["command", "celllayers", "base", JSON.stringify({ id: "rain" })]);
  });

  it("is absent when noData is not set", () => {
    push("domain_states_celllayers.json");
    expect(nodata()).toBeNull();
    expect(viewer().querySelector(".legend-strip")).not.toBeNull();
  });

  it("appears and disappears across frames without rebuilding the picker", () => {
    push("domain_states_celllayers.json");
    const section = viewer();
    const s = goldenJson("domain_states_celllayers.json");
    s[NAME].noData = true;
    s[NAME].legend = { available: false, category: false };
    s[NAME].hover = { valid: false };
    p.win.updateDomainState(JSON.stringify(s));
    expect(viewer()).toBe(section);
    expect(nodata()).not.toBeNull();
    expect(viewer().querySelector(".legend-strip")).toBeNull();
    push("domain_states_celllayers.json");
    expect(nodata()).toBeNull();
    expect(viewer().querySelector(".legend-strip")).not.toBeNull();
    expect(txt(viewer().querySelector("[data-role=hover] .cl-hover-value"))).toBe("4 C");
  });

  it("noData wins over a stale legend/hover in the same state", () => {
    const s = goldenJson("domain_states_celllayers.json");
    s[NAME].noData = true;
    p.win.updateDomainState(JSON.stringify(s));
    expect(nodata()).not.toBeNull();
    expect(viewer().querySelector(".legend-strip")).toBeNull();
    expect(viewer().querySelector("[data-role=hover]")!.children.length).toBe(0);
  });
});
