// S-1 World Generation card (worldgen-panel.plan.md task 4) in the CoW debug-panel.html.
// Fixture matches the 'State JSON contract' of the plan / WorldGenDebugDomain::GetJSONState.
// NOTE: runs in jsdom, not Ultralight's JavaScriptCore: proves page logic only.
import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadPanel as loadPanelFile, type Panel } from "./loadPanel";

const here = path.dirname(fileURLToPath(import.meta.url));
const COW_PAGE = path.resolve(here, "../../../../../Assets/CoW/Global/Presentation/UI/debug-panel.html");
const GOLDEN_DIR = path.resolve(here, "../../../../../Tests/Contracts/debugpanel");
const loadPanel = (): Panel => loadPanelFile(process.env.COW_PANEL_HTML_OVERRIDE || COW_PAGE);

const NAME = "World Generation";
const ID = "WorldGen";

function slider(key: string, stage: number, stageName: string, min: number, max: number, step: number, value: number, def: number) {
  return { key, label: key, type: "slider", min, max, step, value, default: def, stage, stageName };
}
function fixture(over: Record<string, any> = {}): any {
  const base: any = {
    _displayName: NAME,
    _id: ID,
    _group: "CoreDebug",
    drawers: [],
    stats: {
      status: "Complete", revision: 14, seed: 12345678, size: "Small", width: 66, height: 42, locked: false,
      lastMs: 38, preset: "continental", presetModified: false, stagesRan: ["Landmass", "Mountains"],
    },
    controls: [
      slider("landFraction", 1, "Landmass", 0.2, 0.65, 0.01, 0.42, 0.42),
      slider("continents", 1, "Landmass", 1, 3, 1, 2, 1),
      slider("mountainousness", 2, "Mountains", 0, 1, 0.01, 0.55, 0.55),
      { key: "barrierForm", label: "Barrier form", type: "select", values: ["Ridge", "Sea", "Desert"], value: "Ridge", default: "Ridge", stage: 3, stageName: "Barrier" },
      slider("barrier", 3, "Barrier", 0, 1, 0.01, 0.6, 0.6),
    ],
    presets: [{ id: "continental", displayName: "Continental" }, { id: "archipelago", displayName: "Archipelago" }],
    sizes: [
      { name: "Small", width: 66, height: 42, selected: true },
      { name: "Standard", width: 80, height: 52, selected: false },
      { name: "Large", width: 104, height: 64, selected: false },
    ],
    timings: [
      { stage: 0, name: "SeedFields", ms: 3, ran: false },
      { stage: 1, name: "Landmass", ms: 9, ran: true },
      { stage: 2, name: "Mountains", ms: 6, ran: true },
    ],
    view: {
      colourMode: "terrain",
      modes: ["terrain", "elevation", "latitude"],
      drawers: [
        { name: "Tiles", enabled: true, available: true },
        { name: "Grid lines", enabled: false, available: true },
        { name: "Selection", enabled: true, available: true },
        { name: "Evidence", enabled: false, available: false },
      ],
    },
    cell: null,
    lastSave: null,
  };
  return { ...base, ...over };
}
function cellState(): any {
  return {
    success: true, x: 23, y: 17, index: 1121, elevation: 0.5, latitude: 0.4, waterSource: "none", terrain: "Plain",
    depthZone: "None", landmass: 1, flags: 0, downstream: null,
    layers: { landPotential: 0.55, coastDistance: 3 },
    neighbours: [{ dir: 0, x: 24, y: 17 }, "edge", { dir: 2, x: 22, y: 17 }, "edge", { dir: 4, x: 23, y: 18 }, "edge"],
    selected: { x: 23, y: 17 },
  };
}

let p: Panel;
beforeEach(() => {
  vi.useFakeTimers();
  p = loadPanel();
});
afterEach(() => {
  p.close();
  vi.useRealTimers();
});

const push = (st: any = fixture()) => p.win.updateDomainState({ [NAME]: st });
const card = () => p.doc.querySelector(`.domain[data-domain="${NAME}"]`) as HTMLElement;
const q = <T extends Element = HTMLElement>(sel: string) => card().querySelector(sel) as T;
const qa = (sel: string) => Array.from(card().querySelectorAll(sel)) as HTMLElement[];
const commands = () => p.calls.filter((c) => c[0] === "command").map((c) => ({ id: c[1], cmd: c[2], args: JSON.parse(c[3]) }));
const sec = (k: string) => q(`.wg-sec[data-sec="${k}"]`);
const row = (k: string) => q(`.wg-row[data-control="${k}"]`);
const range = (k: string) => row(k).querySelector("input[type=range]") as HTMLInputElement;
const fire = (el: Element, type: string) => el.dispatchEvent(new p.win.Event(type, { bubbles: true }));
const drag = (k: string, v: number) => { const r = range(k); r.value = String(v); fire(r, "input"); };
const click = (el: Element) => p.click(el);

describe("rendering", () => {
  beforeEach(() => push());

  it("renders one card with the six sections in mockup order", () => {
    expect(p.doc.querySelectorAll(".domain").length).toBe(1);
    const keys = qa(".wg-sec").map((s) => s.getAttribute("data-sec"));
    expect(keys).toEqual(["world", "shape", "timings", "view", "cell", "validation"]);
  });

  it("renders one slider per slider control, a select for barrierForm, grouped by stage", () => {
    expect(qa(".wg-row input[type=range]").length).toBe(4);
    for (const k of ["landFraction", "continents", "mountainousness", "barrier"]) expect(range(k)).not.toBeNull();
    expect(row("barrierForm").querySelector("select")).not.toBeNull();
    expect(row("barrierForm").querySelector("input[type=range]")).toBeNull();
    const subs = qa(".wg-sec[data-sec=shape] .wg-sub").map((s) => s.textContent);
    expect(subs.length).toBe(3);
    expect(subs[0]).toContain("LANDMASS");
    expect(subs[2]).toContain("BARRIER");
  });

  it("slider carries min/max/step/value and a default tick", () => {
    const r = range("continents");
    expect([r.min, r.max, r.step, r.value]).toEqual(["1", "3", "1", "2"]);
    expect(row("continents").querySelector(".dtick")).not.toBeNull();
    expect((row("continents").querySelector(".wg-v") as HTMLElement).textContent).toBe("2");
    expect((row("landFraction").querySelector(".wg-v") as HTMLElement).textContent).toBe("0.42");
  });

  it("marks a control modified only when value != default", () => {
    expect(row("continents").classList.contains("modified")).toBe(true);
    expect(row("landFraction").classList.contains("modified")).toBe(false);
    const st = fixture();
    st.controls[1].value = 1;
    push(st);
    expect(row("continents").classList.contains("modified")).toBe(false);
  });

  it("select lists values and shows current", () => {
    const s = row("barrierForm").querySelector("select") as HTMLSelectElement;
    expect(Array.from(s.options).map((o) => o.value)).toEqual(["Ridge", "Sea", "Desert"]);
    expect(s.value).toBe("Ridge");
  });

  it("world section: seed, size segments, presets, status", () => {
    expect((q("[data-role=wg-seed]") as HTMLInputElement).value).toBe("12345678");
    const segs = qa("[data-role=wg-size] button");
    expect(segs.map((b) => b.getAttribute("data-size"))).toEqual(["Small", "Standard", "Large"]);
    expect(segs.map((b) => b.classList.contains("on"))).toEqual([true, false, false]);
    expect(segs[0].textContent).toContain("66");
    const ps = q<HTMLSelectElement>("[data-role=wg-preset]");
    expect(ps.value).toBe("continental");
    expect(q("[data-role=wg-status]").textContent).toContain("Complete");
    expect(q("[data-role=wg-status]").textContent).toContain("14");
  });

  it("preset select shows (modified) when the preset was edited", () => {
    const st = fixture();
    st.stats.preset = ""; st.stats.presetModified = true;
    push(st);
    expect(q<HTMLSelectElement>("[data-role=wg-preset]").value).toBe("");
  });

  it("view section: colour modes and drawer checkboxes (unavailable disabled)", () => {
    const m = q<HTMLSelectElement>("[data-role=wg-mode]");
    expect(Array.from(m.options).map((o) => o.value)).toEqual(["terrain", "elevation", "latitude"]);
    const boxes = qa("[data-role=wg-drawers] input[type=checkbox]") as HTMLInputElement[];
    expect(boxes.map((b) => b.checked)).toEqual([true, false, true, false]);
    expect(boxes.map((b) => b.disabled)).toEqual([false, false, false, true]);
  });

  it("validation section is a disabled stub", () => {
    const s = sec("validation");
    expect(s.querySelectorAll("button:disabled").length).toBe(4);
    expect(s.textContent).toContain("tasks 13-16");
  });

  it("headline stat and enabled card", () => {
    expect(q("[data-role=headline]").textContent).toContain("Complete");
    expect(card().classList.contains("enabled")).toBe(true);
  });
});

describe("collapsible sections", () => {
  it("opens only WORLD and TERRAIN SHAPE by default", () => {
    push();
    const closed = qa(".wg-sec").filter((s) => s.classList.contains("closed")).map((s) => s.getAttribute("data-sec"));
    expect(closed).toEqual(["timings", "view", "cell", "validation"]);
  });

  it("collapse/expand survives a state push and a structural rebuild", () => {
    push();
    click(sec("world").querySelector(".wg-sh")!);
    click(sec("timings").querySelector(".wg-sh")!);
    expect(sec("world").classList.contains("closed")).toBe(true);
    expect(sec("timings").classList.contains("closed")).toBe(false);
    push(fixture());
    expect(sec("world").classList.contains("closed")).toBe(true);
    expect(sec("timings").classList.contains("closed")).toBe(false);
    const st = fixture();
    st.controls.push(slider("rivers", 6, "Rivers", 0, 1, 0.01, 0.5, 0.5));   // structure change => rebuild
    push(st);
    expect(range("rivers")).not.toBeNull();
    expect(sec("world").classList.contains("closed")).toBe(true);
    expect(sec("timings").classList.contains("closed")).toBe(false);
  });
});

describe("live slider drag (throttled setScale)", () => {
  beforeEach(() => push());

  it("sends the first input immediately, throttles the rest and delivers the trailing value", () => {
    drag("continents", 2.0);
    drag("continents", 3);
    drag("continents", 1);
    drag("continents", 2);
    const sent = () => commands().filter((c) => c.cmd === "setScale");
    expect(sent().length).toBe(1);
    expect(sent()[0]).toEqual({ id: ID, cmd: "setScale", args: { key: "continents", value: 2 } });
    vi.advanceTimersByTime(60);
    expect(sent().length).toBe(2);
    expect(sent()[1].args).toEqual({ key: "continents", value: 2 });
    vi.advanceTimersByTime(500);
    expect(sent().length).toBe(2);   // nothing pending, nothing re-sent
  });

  it("trailing send carries the LAST value, not the first throttled one", () => {
    drag("landFraction", 0.3);
    drag("landFraction", 0.4);
    drag("landFraction", 0.5);
    vi.advanceTimersByTime(60);
    const vals = commands().filter((c) => c.cmd === "setScale").map((c) => c.args.value);
    expect(vals).toEqual([0.3, 0.5]);
  });

  it("at most one send per ~50 ms window", () => {
    for (let i = 0; i < 20; ++i) { drag("landFraction", 0.2 + i * 0.01); vi.advanceTimersByTime(10); }
    vi.advanceTimersByTime(100);
    const n = commands().filter((c) => c.cmd === "setScale").length;
    expect(n).toBeGreaterThanOrEqual(3);
    expect(n).toBeLessThanOrEqual(6);
  });

  it("change delivers the final value and cancels the pending trailing send", () => {
    drag("continents", 3);
    drag("continents", 1);
    fire(range("continents"), "change");
    const sent = commands().filter((c) => c.cmd === "setScale").map((c) => c.args.value);
    expect(sent).toEqual([3, 1]);
    vi.advanceTimersByTime(200);
    expect(commands().filter((c) => c.cmd === "setScale").length).toBe(2);
  });

  it("updates the displayed value and modified highlight locally while dragging", () => {
    drag("landFraction", 0.5);
    expect((row("landFraction").querySelector(".wg-v") as HTMLElement).textContent).toBe("0.50");
    expect(row("landFraction").classList.contains("modified")).toBe(true);
  });

  it("barrierForm select sends setScale with the string value", () => {
    const s = row("barrierForm").querySelector("select") as HTMLSelectElement;
    s.value = "Sea";
    fire(s, "change");
    expect(commands().pop()).toEqual({ id: ID, cmd: "setScale", args: { key: "barrierForm", value: "Sea" } });
  });
});

describe("state pushes do not clobber user input", () => {
  beforeEach(() => push());

  it("an actively dragged slider keeps its value across a push", () => {
    drag("continents", 3);
    const st = fixture();
    st.controls[1].value = 1;
    push(st);
    expect(range("continents").value).toBe("3");
  });

  it("after release the slider is held briefly (stale push), then follows the state", () => {
    drag("continents", 3);
    fire(range("continents"), "change");
    const st = fixture();
    st.controls[1].value = 1;
    push(st);
    expect(range("continents").value).toBe("3");
    vi.advanceTimersByTime(600);
    push(st);
    expect(range("continents").value).toBe("1");
  });

  it("an idle slider follows the pushed value", () => {
    const st = fixture();
    st.controls[1].value = 3;
    push(st);
    expect(range("continents").value).toBe("3");
    expect((row("continents").querySelector(".wg-v") as HTMLElement).textContent).toBe("3");
  });

  it("a focused seed text input is not overwritten", () => {
    const seed = q<HTMLInputElement>("[data-role=wg-seed]");
    seed.focus();
    seed.value = "99";
    fire(seed, "input");
    const st = fixture();
    st.stats.seed = 555;
    push(st);
    expect(seed.value).toBe("99");
  });

  it("an unfocused, untouched seed input follows the state", () => {
    const st = fixture();
    st.stats.seed = 555;
    push(st);
    expect(q<HTMLInputElement>("[data-role=wg-seed]").value).toBe("555");
  });

  it("an open/focused select is not overwritten", () => {
    const s = row("barrierForm").querySelector("select") as HTMLSelectElement;
    s.focus();
    s.value = "Desert";
    const st = fixture();
    st.controls[3].value = "Sea";
    push(st);
    expect(s.value).toBe("Desert");
  });

  it("typed cell coordinates are kept until Select/Clear", () => {
    const x = q<HTMLInputElement>("[data-role=wg-cx]");
    x.value = "7";
    fire(x, "input");
    push(fixture({ cell: cellState() }));
    expect(x.value).toBe("7");
  });
});

describe("commands", () => {
  beforeEach(() => push());
  const last = () => commands().pop()!;

  it("Regenerate with an unchanged seed sends the typed seed", () => {
    click(q("[data-role=wg-regen]"));
    expect(last()).toEqual({ id: ID, cmd: "regenerate", args: { seed: 12345678 } });
  });

  it("Regenerate with a typed seed sends it as a number", () => {
    const seed = q<HTMLInputElement>("[data-role=wg-seed]");
    p.typeInto(seed, "42");
    click(q("[data-role=wg-regen]"));
    expect(last()).toEqual({ id: ID, cmd: "regenerate", args: { seed: 42 } });
  });

  it("Regenerate with an invalid seed sends no seed", () => {
    const seed = q<HTMLInputElement>("[data-role=wg-seed]");
    p.typeInto(seed, "abc");
    click(q("[data-role=wg-regen]"));
    expect(last()).toEqual({ id: ID, cmd: "regenerate", args: {} });
  });

  it("committing the seed field (change) sends setSeed; invalid does not", () => {
    const seed = q<HTMLInputElement>("[data-role=wg-seed]");
    seed.value = "777";
    fire(seed, "change");
    expect(last()).toEqual({ id: ID, cmd: "setSeed", args: { seed: 777 } });
    const n = commands().length;
    seed.value = "-3";
    fire(seed, "change");
    expect(commands().length).toBe(n);
  });

  it("Random fills a numeric seed and regenerates with it", () => {
    click(q("[data-role=wg-random]"));
    const c = last();
    expect(c.cmd).toBe("regenerate");
    expect(typeof c.args.seed).toBe("number");
    expect(q<HTMLInputElement>("[data-role=wg-seed]").value).toBe(String(c.args.seed));
  });

  it("size segment sends setSize", () => {
    click(qa("[data-role=wg-size] button")[2]);
    expect(last()).toEqual({ id: ID, cmd: "setSize", args: { size: "Large" } });
  });

  it("Load sends presetLoad with the selected id; (modified) sends nothing", () => {
    const ps = q<HTMLSelectElement>("[data-role=wg-preset]");
    ps.value = "archipelago";
    click(q("[data-role=wg-preset-load]"));
    expect(last()).toEqual({ id: ID, cmd: "presetLoad", args: { id: "archipelago" } });
    const st = fixture();
    st.stats.preset = ""; st.stats.presetModified = true;
    push(st);
    const n = commands().length;
    click(q("[data-role=wg-preset-load]"));
    expect(commands().length).toBe(n);
  });

  it("Save... reveals the save row; Save sends presetSave (overwrite optional)", () => {
    const saveRow = q("[data-role=wg-saverow]");
    expect(saveRow.style.display).toBe("none");
    click(q("[data-role=wg-preset-save]"));
    expect(saveRow.style.display).not.toBe("none");
    p.typeInto(q<HTMLInputElement>("[data-role=wg-save-id]"), "my_world");
    p.typeInto(q<HTMLInputElement>("[data-role=wg-save-name]"), "My World");
    click(q("[data-role=wg-save-go]"));
    expect(last()).toEqual({ id: ID, cmd: "presetSave", args: { id: "my_world", displayName: "My World" } });
    const ow = q<HTMLInputElement>("[data-role=wg-save-overwrite]");
    ow.checked = true;
    click(q("[data-role=wg-save-go]"));
    expect(last().args).toEqual({ id: "my_world", displayName: "My World", overwrite: true });
  });

  it("Save without an id sends nothing; an over-long payload is refused with a message", () => {
    click(q("[data-role=wg-preset-save]"));
    const n = commands().length;
    click(q("[data-role=wg-save-go]"));
    expect(commands().length).toBe(n);
    p.typeInto(q<HTMLInputElement>("[data-role=wg-save-id]"), "an_id");
    p.typeInto(q<HTMLInputElement>("[data-role=wg-save-name]"), "x".repeat(80));
    click(q("[data-role=wg-save-go]"));
    expect(commands().length).toBe(n);
    expect(q("[data-role=wg-savemsg]").textContent).toMatch(/too long/i);
  });

  it("lastSave result is displayed", () => {
    push(fixture({ lastSave: { success: true, writtenTo: "out/my_world.worldshape.json", isSource: false } }));
    expect(q("[data-role=wg-savemsg]").textContent).toContain("out/my_world.worldshape.json");
    push(fixture({ lastSave: { success: false, error: "exists" } }));
    expect(q("[data-role=wg-savemsg]").textContent).toContain("exists");
  });

  it("Select sends integer selectCell; Clear sends clearCell", () => {
    p.typeInto(q<HTMLInputElement>("[data-role=wg-cx]"), "23");
    p.typeInto(q<HTMLInputElement>("[data-role=wg-cy]"), "17");
    click(q("[data-role=wg-cell-go]"));
    expect(last()).toEqual({ id: ID, cmd: "selectCell", args: { x: 23, y: 17 } });
    click(q("[data-role=wg-cell-clear]"));
    expect(last()).toEqual({ id: ID, cmd: "clearCell", args: {} });
  });

  it("Select with non-integer coordinates sends nothing", () => {
    p.typeInto(q<HTMLInputElement>("[data-role=wg-cx]"), "1.5");
    p.typeInto(q<HTMLInputElement>("[data-role=wg-cy]"), "2");
    const n = commands().length;
    click(q("[data-role=wg-cell-go]"));
    expect(commands().length).toBe(n);
  });

  it("colour mode sends setColourMode", () => {
    const m = q<HTMLSelectElement>("[data-role=wg-mode]");
    m.value = "elevation";
    fire(m, "change");
    expect(last()).toEqual({ id: ID, cmd: "setColourMode", args: { mode: "elevation" } });
  });

  it("drawer checkbox sends toggle", () => {
    const box = qa("[data-role=wg-drawers] input[type=checkbox]")[1] as HTMLInputElement;
    box.checked = true;
    fire(box, "change");
    expect(last()).toEqual({ id: ID, cmd: "toggle", args: { drawer: "Grid lines" } });
  });
});

describe("stage timings", () => {
  beforeEach(() => push());

  it("one bar per stage; ran vs cached marked, with total of the ran stages", () => {
    const rows = qa("[data-role=wg-timings] .wg-trow");
    expect(rows.length).toBe(3);
    expect(rows.map((r) => r.classList.contains("skip"))).toEqual([true, false, false]);
    expect(rows[1].textContent).toContain("Landmass");
    expect(rows[1].textContent).toContain("9 ms");
    expect((rows[1].querySelector(".wg-bar i") as HTMLElement).style.width).toBe("100%");
    expect((rows[0].querySelector(".wg-bar i") as HTMLElement).style.width).toBe("33.3%");
    const head = sec("timings").querySelector(".wg-sr") as HTMLElement;
    expect(head.textContent).toContain("ran 2 of 3");
    expect(head.textContent).toContain("15 ms");
  });

  it("updates when a later push changes which stages ran", () => {
    const st = fixture();
    st.timings.forEach((t: any) => (t.ran = true));
    push(st);
    expect(qa("[data-role=wg-timings] .wg-trow.skip").length).toBe(0);
  });
});

describe("cell inspector", () => {
  it("shows a stub with no selection", () => {
    push();
    expect(q("[data-role=wg-cell]").textContent).toMatch(/no cell selected|Select/i);
    expect(sec("cell").querySelector(".wg-sr")!.textContent).toContain("no selection");
  });

  it("shows fields, stage values and neighbours generically", () => {
    push(fixture({ cell: cellState() }));
    const t = q("[data-role=wg-cell]").textContent!;
    for (const s of ["terrain", "Plain", "elevation", "0.5", "waterSource", "landPotential", "0.55", "coastDistance", "neighbours", "0:(24,17)", "1:edge"]) expect(t).toContain(s);
    expect(t).not.toContain("success");
    expect(sec("cell").querySelector(".wg-sr")!.textContent).toContain("(23, 17)");
    expect(sec("cell").querySelector(".wg-sr")!.textContent).toContain("1121");
  });

  it("shows cellError when present", () => {
    push(fixture({ cellError: "no world generated yet" }));
    expect(q("[data-role=wg-cell]").textContent).toContain("no world generated yet");
  });
});

describe("other domains are unchanged", () => {
  it("a generic domain still renders slider-only controls with commit-on-change", () => {
    p.win.updateDomainState({
      "Game Speed": {
        _displayName: "Game Speed", _id: "gamespeed", _group: "CoreDebug",
        drawers: [], stats: { tier: 2 },
        controls: [{ key: "scale", label: "Scale", min: 0, max: 4, value: 1 }, { key: "x", type: "select", values: ["a"], value: "a" }],
      },
    });
    const c = p.doc.querySelector('.domain[data-domain="Game Speed"]') as HTMLElement;
    expect(c.querySelector(".wg")).toBeNull();
    expect(c.querySelectorAll(".ctrl").length).toBe(1);
    const r = c.querySelector(".ctrl input[type=range]") as HTMLInputElement;
    r.value = "3";
    r.dispatchEvent(new p.win.Event("input", { bubbles: true }));
    vi.advanceTimersByTime(200);
    expect(commands().length).toBe(0);               // no live send on input
    r.dispatchEvent(new p.win.Event("change", { bubbles: true }));
    expect(commands()).toEqual([{ id: "gamespeed", cmd: "setScale", args: { key: "scale", value: 3 } }]);
  });

  it("a CellLayers golden still renders its viewer next to a WorldGen card", () => {
    const g = JSON.parse(fs.readFileSync(path.join(GOLDEN_DIR, "domain_states_celllayers.json"), "utf-8"));
    p.win.updateDomainState({ ...g, [NAME]: fixture() });
    expect(p.doc.querySelectorAll(".domain").length).toBe(2);
    expect(p.doc.querySelector("[data-role=cell-layer-viewer]")).not.toBeNull();
    expect(card().querySelector(".wg")).not.toBeNull();
  });
});
