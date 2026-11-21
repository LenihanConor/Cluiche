// CoW fork of celllayers.test.ts: same goldens/assertions against the CoW debug-panel.html.
// DiaCellLayers Registry-and-Viewer, panel card (AC 16, 17): the golden state of the REAL CellLayerViewerDomain
// (Cluiche/Tests/Contracts/debugpanel/domain_states_celllayers*.json) rendered by the real page.
// Layout reference: docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html.
// NOTE: runs in jsdom, not Ultralight's JavaScriptCore: proves page logic only.
import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadPanel as loadPanelFile, type Panel } from "./loadPanel";

const COW_PAGE = path.resolve(here0(), "../../../../../Assets/CoW/Global/Presentation/UI/debug-panel.html");
function here0(): string { return path.dirname(fileURLToPath(import.meta.url)); }
const loadPanel = (): Panel => loadPanelFile(process.env.COW_PANEL_HTML_OVERRIDE || COW_PAGE);

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

describe("selection card (Selection task 11: count, tool strip, group dropdown, clear, banner)", () => {
  const sel = () => viewer().querySelector("[data-role=selection]") as HTMLElement;
  const toolBtn = (t: string) => sel().querySelector(`[data-tool="${t}"]`) as HTMLElement;
  const groupSelect = () => sel().querySelector("[data-role=selection-group] select") as HTMLSelectElement;
  const banner = () => sel().querySelector("[data-role=selection-banner]") as HTMLElement | null;
  const withSelection = (patch: any) => {
    const s = goldenJson("domain_states_celllayers_selection.json");
    Object.assign(s[NAME].selection, patch);
    p.win.updateDomainState(JSON.stringify(s));
  };

  it("contract: the selection golden carries count, tool, group, groups and tooLarge", () => {
    const s = goldenJson("domain_states_celllayers_selection.json")[NAME].selection;
    expect(s.count).toBeGreaterThan(0);
    expect(s.tool).toBe("pan");
    expect(typeof s.group).toBe("string");
    expect(Array.isArray(s.groups)).toBe(true);
    expect(s.tooLarge).toBe(false);
  });

  it("shows the selection count", () => {
    push("domain_states_celllayers_selection.json");
    expect(txt(sel().querySelector("[data-role=selection-count]"))).toBe("4 cells selected");
    push("domain_states_celllayers.json");
    expect(txt(sel().querySelector("[data-role=selection-count]"))).toBe("0 cells selected");
  });

  it("the tool strip highlights the active tool and a click sends select_tool {tool}", () => {
    push("domain_states_celllayers_selection.json");
    expect(toolBtn("pan").classList.contains("active")).toBe(true);
    expect(toolBtn("select").classList.contains("active")).toBe(false);
    p.click(toolBtn("select"));
    expect(commands()).toEqual([["command", "celllayers", "select_tool", JSON.stringify({ tool: "select" })]]);
    withSelection({ tool: "select" });
    expect(toolBtn("select").classList.contains("active")).toBe(true);
    expect(toolBtn("pan").classList.contains("active")).toBe(false);
  });

  it("the group dropdown lists the groups with the selected one chosen; change sends select_group {layer}", () => {
    push("domain_states_celllayers_selection.json");
    const opts = Array.from(groupSelect().options).map((o) => [o.value, o.textContent]);
    expect(opts).toEqual([["biome", "Biome"]]);
    expect(groupSelect().value).toBe("biome");
    const s = goldenJson("domain_states_celllayers_category.json");
    p.win.updateDomainState(JSON.stringify(s));
    const sg = groupSelect();
    const ids = Array.from(sg.options).map((o) => o.value);
    expect(ids.length).toBeGreaterThan(0);
    sg.value = ids[ids.length - 1];
    sg.dispatchEvent(new p.win.Event("change", { bubbles: true }));
    expect(commands()).toEqual([["command", "celllayers", "select_group", JSON.stringify({ layer: ids[ids.length - 1] })]]);
  });

  it("the Clear button sends select_clear", () => {
    push("domain_states_celllayers_selection.json");
    p.click(sel().querySelector("[data-role=selection-clear]")!);
    expect(commands()).toEqual([["command", "celllayers", "select_clear", "{}"]]);
  });

  it("the too-large banner shows only when tooLarge and disappears again", () => {
    push("domain_states_celllayers_selection.json");
    expect(banner()).toBeNull();
    const section = viewer();
    withSelection({ count: 1120, tooLarge: true, tooLargeText: "Selection too large to outline (1120 cells)" });
    expect(viewer()).toBe(section);
    expect(txt(banner())).toBe("Selection too large to outline (1120 cells)");
    withSelection({ count: 0, tooLarge: false });
    expect(banner()).toBeNull();
  });

  it("the empty state still renders the selection card", () => {
    push("domain_states_celllayers_empty.json");
    expect(txt(sel().querySelector("[data-role=selection-count]"))).toBe("0 cells selected");
    expect(banner()).toBeNull();
  });
});


describe("selection inspector card (Selection Inspector task 9: accordions, stats rows, histogram, compare, cell details)", () => {
  const FILE = "domain_states_celllayers_inspector.json";
  const insp = () => viewer().querySelector("[data-role=inspector]") as HTMLElement;
  const row = (id: string) => insp().querySelector(`tr[data-layer="${id}"]`) as HTMLElement;
  const cells = (tr: Element) => Array.from(tr.querySelectorAll("td")).map((td) => td.textContent ?? "");
  const header = (g: string) => insp().querySelector(`.insp-group[data-group="${g}"] [data-role=inspector-group-header]`) as HTMLElement;
  const withInsp = (file: string, patch: (i: any) => void) => {
    const s = goldenJson(file);
    patch(s[NAME].inspector);
    p.win.updateDomainState(JSON.stringify(s));
  };

  it("contract: the inspector golden carries the card keys", () => {
    const i = goldenJson(FILE)[NAME].inspector;
    for (const k of ["hasSelection", "updating", "expandedGroups", "layers", "histogram", "pinned", "cell", "base"]) expect(k in i).toBe(true);
    expect(i.histogram.bins.length).toBe(16);
    expect(i.layers[0].a.count).toBe(4);
    expect(i.layers[0].delta.count).toBe(-3);
    expect(goldenJson("domain_states_celllayers.json")[NAME].inspector.hasSelection).toBe(false);
  });

  it("with nothing selected it says 'Select cells to inspect' and renders no stats", () => {
    push("domain_states_celllayers.json");
    expect(txt(insp().querySelector("[data-role=inspector-empty]"))).toBe("Select cells to inspect");
    expect(insp().querySelector("table")).toBeNull();
    expect(insp().querySelector("[data-role=inspector-pin-row]")!.querySelectorAll("button[disabled]").length).toBe(3);
  });

  it("groups are accordions: an expanded group shows its layer rows, a collapsed one only the header", () => {
    push(FILE);
    expect(txt(header("Ecology"))).toContain("Ecology");
    expect(header("Ecology").getAttribute("aria-expanded")).toBe("true");
    expect(header("Terrain").getAttribute("aria-expanded")).toBe("false");
    expect(insp().querySelector('.insp-group[data-group="Terrain"] table')).toBeNull();
    expect(row("temp")).not.toBeNull();
    expect(row("rain")).not.toBeNull();
    expect(row("biome")).toBeNull();
  });

  it("clicking a group header sends inspector_expand with the toggled state", () => {
    push(FILE);
    p.click(header("Terrain"));
    p.click(header("Ecology"));
    expect(commands()).toEqual([
      ["command", "celllayers", "inspector_expand", JSON.stringify({ group: "Terrain", expanded: true })],
      ["command", "celllayers", "inspector_expand", JSON.stringify({ group: "Ecology", expanded: false })],
    ]);
  });

  it("a layer row shows label (tooltip as title), count, min, mean, max and unit", () => {
    push(FILE);
    const tr = row("temp");
    expect(cells(tr).slice(0, 6)).toEqual(["Temperature", "1", "4", "4", "4", "C"]);
    expect(tr.querySelector("td")!.getAttribute("title")).toBe("Air temperature");
    expect(tr.querySelector(".range-bar")).toBeNull();   // pinned: the compare columns take the width
    expect(tr.querySelector("td:nth-child(2)")!.textContent).toBe("1");
  });

  it("compare columns: A values, delta (B minus A) with sign and colour, and a frozen badge", () => {
    push(FILE);
    expect(cells(row("temp")).slice(6)).toEqual(["4", "-2", "1", "5", "−3", "+6", "+3", "−1"]);
    const d = row("temp").querySelectorAll("td");
    expect(d[10].classList.contains("d-neg")).toBe(true);
    expect(d[11].classList.contains("d-pos")).toBe(true);
    expect(txt(insp().querySelector("[data-role=inspector-pinned]"))).toBe("A: 4 cellsfrozen");
    expect(insp().querySelector("[data-role=frozen-badge]")).not.toBeNull();
    withInsp(FILE, (i) => { i.pinned.frozen = false; });
    expect(insp().querySelector("[data-role=frozen-badge]")).toBeNull();
    expect(txt(insp().querySelector("[data-role=inspector-pinned]"))).toBe("A: 4 cells (live)");
  });

  it("a layer with no frozen stats shows 'not frozen'; without a pin there are no compare columns", () => {
    withInsp(FILE, (i) => { i.layers[1].a = { notFrozen: true }; delete i.layers[1].delta; });
    expect(txt(row("rain").querySelector("[data-role=not-frozen]"))).toBe("not frozen");
    withInsp(FILE, (i) => { delete i.pinned; i.layers.forEach((l: any) => { delete l.a; delete l.delta; }); });
    expect(cells(row("temp")).length).toBe(7);
    expect(row("temp").querySelector(".range-bar .mean")).not.toBeNull();   // unpinned: the range bar returns
    expect(insp().querySelector("[data-role=inspector-pinned]")).toBeNull();
    expect(insp().querySelector("[data-role=inspector-unpin]")!.hasAttribute("disabled")).toBe(true);
  });

  it("pin, freeze and unpin buttons send select_pin / select_pin {freeze} / select_unpin", () => {
    push(FILE);
    p.click(insp().querySelector("[data-role=inspector-pin]")!);
    p.click(insp().querySelector("[data-role=inspector-freeze]")!);
    p.click(insp().querySelector("[data-role=inspector-unpin]")!);
    expect(commands()).toEqual([
      ["command", "celllayers", "select_pin", "{}"],
      ["command", "celllayers", "select_pin", JSON.stringify({ freeze: true })],
      ["command", "celllayers", "select_unpin", "{}"],
    ]);
  });

  it("category layers show counts as chips, with an Other chip when non-zero", () => {
    withInsp(FILE, (i) => {
      i.expandedGroups.push("Terrain");
      i.layers.unshift({ id: "biome", schemaKey: "cells/biome", label: "Biome", unit: "", tooltip: "Dominant biome", kind: "category", group: "Terrain",
        hasData: true, count: 4, nanCount: 0, min: null, mean: null, max: null, other: 1,
        categories: [{ id: "a", label: "Grass", count: 3 }, { id: "b", label: "Water", count: 0 }],
        a: { notFrozen: true } });
    });
    const chips = Array.from(row("biome").querySelectorAll(".biome-chip")).map((c) => c.textContent);
    expect(chips).toEqual(["n: 4", "Grass: 3", "Other: 1"]);
  });

  it("the histogram has 16 bars scaled to the tallest bin and the out-of-range footer only when needed", () => {
    push(FILE);
    const bars = Array.from(insp().querySelectorAll("[data-role=histogram] .histogram-bar")) as HTMLElement[];
    expect(bars.length).toBe(16);
    expect(bars[11].style.height).toBe("100%");
    expect(bars[0].style.height).toBe("0%");
    expect(insp().querySelector("[data-role=histogram-oor]")).toBeNull();
    withInsp(FILE, (i) => { i.histogram.outOfRangeLow = 2; i.histogram.outOfRangeHigh = 1; });
    expect(txt(insp().querySelector("[data-role=histogram-oor]"))).toBe("out of range: 2 low, 1 high");
  });

  it("a category base shows its histogram as counts", () => {
    withInsp(FILE, (i) => { i.histogram = { categories: [{ id: "a", label: "Grass", count: 3 }], other: 0, nanCount: 0 }; });
    expect(insp().querySelector("[data-role=histogram] .histogram-bar")).toBeNull();
    expect(Array.from(insp().querySelectorAll("[data-role=histogram-categories] .biome-chip")).map((c) => c.textContent)).toEqual(["Grass: 3"]);
  });

  it("the 'updating' mark shows only while a pass is pending and flips without re-rendering", () => {
    push(FILE);
    const mark = () => insp().querySelector("[data-role=inspector-updating]") as HTMLElement;
    expect(mark().style.display).toBe("none");
    const first = mark();
    withInsp(FILE, (i) => { i.updating = true; });
    expect(mark()).toBe(first);
    expect(mark().style.display).toBe("");
  });

  it("exactly one cell adds the details section: exact values, enum labels, and provider rows", () => {
    push(FILE);
    const sec = () => insp().querySelector("[data-role=inspector-cell]") as HTMLElement;
    expect(txt(sec().querySelector(".insp-cell-title"))).toBe("Cell (2, 1)  #6");
    expect(cells(sec().querySelector('tr[data-layer="temp"]')!)).toEqual(["Temperature", "4", "C"]);
    withInsp(FILE, (i) => {
      i.cell.rows.push({ layer: "biome", label: "Biome", unit: "", kind: "category", hasData: true, isNaN: false, value: 1, otherValue: false, enumId: "b", enumLabel: "Water" });
      i.cell.rows[1].value = 0.30000001192092896;
      i.cell.details = [
        { kind: "section", label: "Ecology" },
        { kind: "value", label: "Producers", unit: "kg", tooltip: "Total producers", value: 0.25, resolved: true, schemaKey: "cells/producers" },
        { kind: "text", label: "Dominant cause", text: "Grazing" },
      ];
    });
    expect(cells(sec().querySelector('tr[data-layer="rain"]')!)).toEqual(["Rainfall", "0.3", "mm"]);
    expect(cells(sec().querySelector('tr[data-layer="biome"]')!)).toEqual(["Biome", "Water", ""]);
    const trs = Array.from(sec().querySelectorAll("tr")).map((tr) => cells(tr));
    expect(trs.slice(-3)).toEqual([["Ecology"], ["Producers", "0.25", "kg"], ["Dominant cause", "Grazing"]]);
    expect(sec().querySelectorAll("tr")[trs.length - 2].querySelector("td")!.getAttribute("title")).toBe("Total producers");
    withInsp(FILE, (i) => { delete i.cell; });
    expect(insp().querySelector("[data-role=inspector-cell]")).toBeNull();
  });

  it("the inspector host is kept across state pushes", () => {
    push(FILE);
    const host = insp();
    push(FILE);
    expect(insp()).toBe(host);
  });
});


describe("query box (Query task 7: debounced live check, error span + suggestion, Run, recents)", () => {
  const FILE = "domain_states_celllayers_query.json";
  const box = () => viewer().querySelector("[data-role=query]") as HTMLElement;
  const input = () => box().querySelector("[data-role=query-input]") as HTMLInputElement;
  const result = () => box().querySelector("[data-role=query-result]") as HTMLElement;
  const err = () => box().querySelector("[data-role=query-error]") as HTMLElement;
  const recents = () => Array.from(box().querySelectorAll("[data-role=query-recent] .q-recent-item")).map((e) => e.textContent);
  const type = (t: string) => p.typeInto(input(), t);
  const withQuery = (patch: (q: any) => void) => {
    const s = goldenJson(FILE);
    patch(s[NAME].query);
    p.win.updateDomainState(JSON.stringify(s));
  };

  it("contract: the query golden carries the card keys", () => {
    const q = goldenJson(FILE)[NAME].query;
    expect(q.available).toBe(true);
    expect(q.check.ok).toBe(false);
    for (const k of ["message", "column", "length", "suggestion"]) expect(k in q.check.error).toBe(true);
    expect(q.last.ok).toBe(true);
    expect(typeof q.last.count).toBe("number");
    expect(goldenJson("domain_states_celllayers.json")[NAME].query.available).toBe(true);
  });

  it("renders the box: input, Run button, empty result", () => {
    push("domain_states_celllayers.json");
    expect(box()).not.toBeNull();
    expect(input()).not.toBeNull();
    expect(box().querySelector("[data-role=query-run]")).not.toBeNull();
    expect(txt(result())).toBe("");
    expect(err()).toBeNull();
  });

  it("typing debounces the check: one query_check per 250 ms quiet period", () => {
    push("domain_states_celllayers.json");
    type("t");
    vi.advanceTimersByTime(100);
    type("te");
    vi.advanceTimersByTime(100);
    type("temp > 3");
    vi.advanceTimersByTime(249);
    expect(commands().length).toBe(0);
    vi.advanceTimersByTime(1);
    expect(commands()).toEqual([["command", "celllayers", "query_check", JSON.stringify({ e: "temp > 3" })]]);
  });

  it("shows '→ N cells match' when the pushed check matches the input", () => {
    push("domain_states_celllayers.json");
    type("temp > 3");
    vi.advanceTimersByTime(250);
    withQuery((q) => { q.check = { expr: "temp > 3", ok: true, count: 37 }; });
    expect(txt(result())).toBe("→ 37 cells match");
    expect(err()).toBeNull();
    expect(input().value).toBe("temp > 3");   // the input survives state pushes
  });

  it("singular: '→ 1 cell matches'", () => {
    push("domain_states_celllayers.json");
    type("temp > 3");
    withQuery((q) => { q.check = { expr: "temp > 3", ok: true, count: 1 }; });
    expect(txt(result())).toBe("→ 1 cell matches");
  });

  it("stale guard: a check result for an older expr is ignored", () => {
    push("domain_states_celllayers.json");
    type("temp > 3");
    withQuery((q) => { q.check = { expr: "temp > 3", ok: true, count: 37 }; });
    type("temp > 30");
    expect(txt(result())).toBe("");
    withQuery((q) => { q.check = { expr: "temp > 3", ok: true, count: 37 }; });
    expect(txt(result())).toBe("");
    expect(err()).toBeNull();
  });

  it("empty input clears the result and error and sends nothing", () => {
    push(FILE);
    type("tmp > 3");
    vi.advanceTimersByTime(250);
    commands().length = 0;
    p.calls.length = 0;
    push(FILE);
    expect(err()).not.toBeNull();
    type("   ");
    vi.advanceTimersByTime(500);
    expect(commands().length).toBe(0);
    expect(txt(result())).toBe("");
    expect(err()).toBeNull();
  });

  it("an error shows the message, the query with the offending span highlighted, and the suggestion", () => {
    push("domain_states_celllayers.json");
    type("tmp > 3");
    push(FILE);
    expect(txt(err().querySelector(".q-msg"))).toBe("unknown layer 'tmp'");
    const text = err().querySelector(".q-text") as HTMLElement;
    expect(txt(text)).toBe("tmp > 3");
    const mark = text.querySelector("mark.q-span") as HTMLElement;
    expect(txt(mark)).toBe("tmp");
    expect(txt(err().querySelector("[data-role=query-suggest]"))).toBe("did you mean 'temp'?");
    expect(txt(result())).toBe("");
  });

  it("an error without a suggestion shows no suggestion", () => {
    push("domain_states_celllayers.json");
    type("tmp > 3");
    withQuery((q) => { delete q.check.error.suggestion; });
    expect(err()).not.toBeNull();
    expect(err().querySelector("[data-role=query-suggest]")).toBeNull();
  });

  it("clicking the suggestion replaces the offending span in the input and re-checks", () => {
    push("domain_states_celllayers.json");
    type("tmp > 3");
    push(FILE);
    p.click(err().querySelector("[data-role=query-suggest]")!);
    expect(input().value).toBe("temp > 3");
    vi.advanceTimersByTime(250);
    expect(commands()).toEqual([["command", "celllayers", "query_check", JSON.stringify({ e: "temp > 3" })]]);
    expect(err()).toBeNull();   // the old error no longer matches the input
  });

  it("Run sends query_run and pushes the query to the recents list", () => {
    push("domain_states_celllayers.json");
    type("temp > 3");
    p.click(box().querySelector("[data-role=query-run]")!);
    expect(commands()).toEqual([["command", "celllayers", "query_run", JSON.stringify({ e: "temp > 3" })]]);
    expect(recents()).toEqual(["temp > 3"]);
  });

  it("Enter in the input runs the query too; Run on empty input does nothing", () => {
    push("domain_states_celllayers.json");
    p.click(box().querySelector("[data-role=query-run]")!);
    expect(commands().length).toBe(0);
    type("rain < 2");
    p.key(input(), "Enter");
    expect(commands()).toEqual([["command", "celllayers", "query_run", JSON.stringify({ e: "rain < 2" })]]);
    expect(recents()).toEqual(["rain < 2"]);
  });

  it("recents: newest first, deduped, capped at 10, click refills the input", () => {
    push("domain_states_celllayers.json");
    const run = (t: string) => { type(t); p.click(box().querySelector("[data-role=query-run]")!); };
    for (let i = 1; i <= 12; ++i) run("temp > " + i);
    expect(recents().length).toBe(10);
    expect(recents()[0]).toBe("temp > 12");
    expect(recents()[9]).toBe("temp > 3");
    run("temp > 5");
    expect(recents().length).toBe(10);
    expect(recents()[0]).toBe("temp > 5");
    expect(recents().filter((r) => r === "temp > 5").length).toBe(1);
    p.click(box().querySelectorAll("[data-role=query-recent] .q-recent-item")[2]);
    expect(input().value).toBe("temp > 11");
  });

  it("a 150-character query is sent as query_part calls then one final command, every argsJson <= 63 chars", () => {
    push("domain_states_celllayers.json");
    const long = "temp > 1 and temp > 2 and temp > 3 and temp > 4 and temp > 5 and temp > 6 and temp > 7 and temp > 8 and temp > 9 and temp > 10 and temp > 11 and ra";
    expect(long.length).toBeGreaterThan(140);
    type(long);
    vi.advanceTimersByTime(250);
    const sent = commands();
    expect(sent.length).toBeGreaterThan(2);
    for (const c of sent) expect(c[3].length).toBeLessThanOrEqual(63);
    const last = sent[sent.length - 1];
    expect(last[2]).toBe("query_check");
    const parts = sent.slice(0, -1);
    expect(parts.every((c) => c[2] === "query_part")).toBe(true);
    expect(JSON.parse(last[3])).toEqual({ n: parts.length });
    expect(parts.map((c) => JSON.parse(c[3]).i)).toEqual(parts.map((_, i) => i));
    expect(parts.map((c) => JSON.parse(c[3]).s).join("")).toBe(long);
    expect(err()).toBeNull();
    commands().length = 0;
    p.calls.length = 0;
    p.click(box().querySelector("[data-role=query-run]")!);
    const run = commands();
    expect(run[run.length - 1][2]).toBe("query_run");
    expect(run.slice(0, -1).every((c) => c[2] === "query_part")).toBe(true);
    expect(recents()).toEqual([long]);
  });

  it("splitting escapes correctly and never splits a surrogate pair", () => {
    push("domain_states_celllayers.json");
    const tricky = ("a\"\\" + "\u{1F600}").repeat(20);
    type(tricky);
    vi.advanceTimersByTime(250);
    const sent = commands();
    for (const c of sent) expect(c[3].length).toBeLessThanOrEqual(63);
    const parts = sent.slice(0, -1).map((c) => JSON.parse(c[3]).s as string);
    expect(parts.join("")).toBe(tricky);
    for (const s of parts) expect(/^[\uDC00-\uDFFF]|[\uD800-\uDBFF]$/.test(s)).toBe(false);
  });

  it("a query over 512 characters shows the local error and sends nothing", () => {
    push("domain_states_celllayers.json");
    type("t".repeat(513));
    vi.advanceTimersByTime(250);
    p.click(box().querySelector("[data-role=query-run]")!);
    expect(commands().length).toBe(0);
    expect(txt(err().querySelector(".q-msg"))).toBe("query too long (max 512 characters)");
  });

  it("a 512-character query is still sent", () => {
    push("domain_states_celllayers.json");
    type("t".repeat(512));
    vi.advanceTimersByTime(250);
    expect(commands().length).toBeGreaterThan(1);
    expect(err()).toBeNull();
  });

  it("the query box host is kept across state pushes", () => {
    push(FILE);
    const host = box();
    push(FILE);
    expect(box()).toBe(host);
  });
});
