// S-1 World Generation card, "the rest" (worldgen-panel-rest.plan.md task 5): WORLD FILE, VALIDATION, progress,
// last-outcome card, locked-import confirm dialog, drawer labels, evidence-hidden note, cell.absorbed.
// NOTE: runs in jsdom, not Ultralight's JavaScriptCore: proves page logic only (no pixels).
import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadPanel as loadPanelFile, type Panel } from "./loadPanel";

const here = path.dirname(fileURLToPath(import.meta.url));
const COW_PAGE = path.resolve(here, "../../../../../Assets/CoW/Global/Presentation/UI/debug-panel.html");
const NAME = "World Generation";
const ID = "WorldGen";

function slider(key: string, stage: number, stageName: string, min: number, max: number, step: number, value: number, def: number) {
  return { key, label: key, type: "slider", min, max, step, value, default: def, stage, stageName };
}
function fixture(over: Record<string, any> = {}): any {
  const base: any = {
    _displayName: NAME, _id: ID, _group: "CoreDebug", drawers: [],
    stats: { status: "Complete", revision: 14, seed: 12345678, size: "Small", width: 66, height: 42, locked: false, lastMs: 38, preset: "continental", presetModified: false },
    controls: [slider("continents", 1, "Landmass", 1, 3, 1, 2, 1), slider("rivers", 2, "Rivers", 0, 1, 0.01, 0.5, 0.5)],
    presets: [{ id: "continental", displayName: "Continental" }, { id: "archipelago", displayName: "Archipelago" }],
    sizes: [{ name: "Small", width: 66, height: 42, selected: true }, { name: "Standard", width: 80, height: 52, selected: false }],
    timings: [],
    view: {
      colourMode: "terrain", modes: ["terrain"],
      drawers: [
        { name: "Tiles", enabled: true, available: true },
        { name: "regions.edges", label: "Region edges", enabled: false, available: true },
        { name: "sites", label: "Sites", enabled: false, available: true },
        { name: "sites.hearth", label: "Sites Hearth", enabled: true, available: true },
        { name: "dynasty.starts", label: "Dynasty starts", enabled: false, available: false },
        { name: "guarantee.evidence", label: "Guarantee evidence", enabled: true, available: true },
      ],
    },
    cell: null, lastSave: null,
    validation: null,
    file: { worlds: [], selected: "", defaultName: "world_12345678", lastExport: null, lastImport: null, locked: false, imported: false },
    progress: { state: "Complete", attempt: 0, maxAttempts: 0, elapsedMs: 0, timeoutMs: 0, lastOutcome: null },
  };
  return { ...base, ...over };
}
function fileState(over: Record<string, any> = {}): any {
  return { ...fixture().file, ...over };
}
function validationFail(): any {
  return {
    outcome: "Complete", seed: 12345678, worldRevision: 14, fresh: true, passed: false, source: "validate",
    guarantees: [
      { id: 1, name: "WorldLayoutGuarantee", ok: false, counts: { proofPaths: 0, violatingCells: 2, violatingPairs: 3 },
        diagnostics: ["RegionCountRange: mainland has 9 regions, expected 10-16.", "NodeSpacing: pair near (18, 14) and (19, 15)."] },
      { id: 2, name: "HearthNexus", ok: true, counts: { proofPaths: 4, violatingCells: 0, violatingPairs: 0 }, diagnostics: [] },
    ],
    attemptCount: 14,
    attempts: [
      { index: 1, subSeed: 2654435761, failedGuarantees: ["WorldLayoutGuarantee"], ms: 41 },
      { index: 2, subSeed: 17, failedGuarantees: [], ms: 12 },
    ],
  };
}

let p: Panel;
beforeEach(() => { vi.useFakeTimers(); p = loadPanelFile(process.env.COW_PANEL_HTML_OVERRIDE || COW_PAGE); });
afterEach(() => { p.close(); vi.useRealTimers(); });

const push = (st: any = fixture()) => p.win.updateDomainState({ [NAME]: st });
const card = () => p.doc.querySelector(`.domain[data-domain="${NAME}"]`) as HTMLElement;
const q = <T extends Element = HTMLElement>(sel: string) => card().querySelector(sel) as T;
const qa = (sel: string) => Array.from(card().querySelectorAll(sel)) as HTMLElement[];
const role = <T extends Element = HTMLElement>(r: string) => q<T>(`[data-role=${r}]`);
const commands = () => p.calls.filter((c) => c[0] === "command").map((c) => ({ id: c[1], cmd: c[2], args: JSON.parse(c[3]) }));
const sec = (k: string) => q(`.wg-sec[data-sec="${k}"]`);
const fire = (el: Element, type: string) => el.dispatchEvent(new p.win.Event(type, { bubbles: true }));
const click = (el: Element) => p.click(el);
const range = (k: string) => q(`.wg-row[data-control="${k}"] input[type=range]`) as HTMLInputElement;
const drag = (k: string, v: number) => { const r = range(k); r.value = String(v); fire(r, "input"); };

describe("layout", () => {
  it("sections follow the mockup order and WORLD FILE / VALIDATION start open", () => {
    push();
    expect(qa(".wg-sec").map((s) => s.getAttribute("data-sec"))).toEqual(["world", "file", "validation", "shape", "timings", "view", "cell"]);
    expect(sec("file").classList.contains("closed")).toBe(false);
    expect(sec("validation").classList.contains("closed")).toBe(false);
  });
});

describe("WORLD FILE", () => {
  it("lists merged worlds with an out/deployed badge, selected one highlighted", () => {
    push(fixture({ file: fileState({ worlds: [{ name: "a_world", source: "out" }, { name: "my_world", source: "deployed" }], selected: "my_world" }) }));
    const rows = qa(".wg-wl");
    expect(rows.map((r) => r.getAttribute("data-world"))).toEqual(["a_world", "my_world"]);
    expect(rows[0].textContent).toContain("out");
    expect(rows[1].textContent).toContain("deployed");
    expect(rows.map((r) => r.classList.contains("sel"))).toEqual([false, true]);
  });

  it("clicking a world sends selectWorld", () => {
    push(fixture({ file: fileState({ worlds: [{ name: "a_world", source: "out" }] }) }));
    click(q(".wg-wl[data-world=a_world]"));
    expect(commands().pop()).toEqual({ id: ID, cmd: "selectWorld", args: { name: "a_world" } });
  });

  it("Import selected sends import {name}; disabled with no selection", () => {
    push(fixture({ file: fileState({ worlds: [{ name: "a_world", source: "out" }] }) }));
    expect(role<HTMLButtonElement>("wg-import").disabled).toBe(true);
    push(fixture({ file: fileState({ worlds: [{ name: "a_world", source: "out" }], selected: "a_world" }) }));
    expect(role<HTMLButtonElement>("wg-import").disabled).toBe(false);
    click(role("wg-import"));
    expect(commands().pop()).toEqual({ id: ID, cmd: "import", args: { name: "a_world" } });
  });

  it("an over-long world name is not silently dropped: message, nothing sent", () => {
    const long = "w".repeat(60);
    push(fixture({ file: fileState({ worlds: [{ name: long, source: "out" }], selected: long }) }));
    click(role("wg-import"));
    expect(commands().length).toBe(0);
    expect(role("wg-filenote").textContent).toMatch(/too long/i);
    click(q(".wg-wl"));
    expect(commands().length).toBe(0);
  });

  it("export name defaults to file.defaultName and Export sends it", () => {
    push();
    const inp = role<HTMLInputElement>("wg-exp-name");
    expect(inp.value).toBe("world_12345678");
    click(role("wg-export"));
    expect(commands().pop()).toEqual({ id: ID, cmd: "export", args: { name: "world_12345678" } });
  });

  it("typed export name and overwrite flag are sent; blank sends no name", () => {
    push();
    p.typeInto(role<HTMLInputElement>("wg-exp-name"), "my-world_2");
    const ow = role<HTMLInputElement>("wg-exp-overwrite");
    ow.checked = true;
    click(role("wg-export"));
    expect(commands().pop()).toEqual({ id: ID, cmd: "export", args: { name: "my-world_2", overwrite: true } });
    p.typeInto(role<HTMLInputElement>("wg-exp-name"), "");
    ow.checked = false;
    click(role("wg-export"));
    expect(commands().pop()).toEqual({ id: ID, cmd: "export", args: {} });
  });

  it("export name with bad characters or too long is rejected client-side with a message", () => {
    push();
    const n0 = commands().length;
    p.typeInto(role<HTMLInputElement>("wg-exp-name"), "bad name!");
    click(role("wg-export"));
    expect(role("wg-filenote").textContent).toMatch(/A-Z/);
    p.typeInto(role<HTMLInputElement>("wg-exp-name"), "x".repeat(60));
    click(role("wg-export"));
    expect(role("wg-filenote").textContent).toMatch(/too long/i);
    expect(commands().length).toBe(n0);
  });

  it("a state push does not overwrite an edited export name", () => {
    push();
    p.typeInto(role<HTMLInputElement>("wg-exp-name"), "mine");
    push(fixture({ file: fileState({ defaultName: "world_999" }) }));
    expect(role<HTMLInputElement>("wg-exp-name").value).toBe("mine");
  });

  it("shows last export success and failure lines", () => {
    push(fixture({ file: fileState({ lastExport: { success: true, name: "w1", path: "out/CoW/worlds/w1.wld.json", bytes: 2048, revision: 14, warningCount: 0, warnings: [] } }) }));
    expect(role("wg-lastexport").textContent).toContain("out/CoW/worlds/w1.wld.json");
    push(fixture({ file: fileState({ lastExport: { success: false, name: "w1", error: "exists", status: "Exists" } }) }));
    expect(role("wg-lastexport").textContent).toContain("exists");
    expect(role("wg-lastexport").className).toContain("wg-err");
  });

  it("import error shows line and column; success shows name and warnings", () => {
    push(fixture({ file: fileState({ lastImport: { success: false, name: "bad", error: "unexpected token", status: "ParseError", line: 12, column: 7 } }) }));
    const t = role("wg-lastimport").textContent!;
    expect(t).toContain("unexpected token");
    expect(t).toContain("line 12");
    expect(t).toContain("column 7");
    push(fixture({ file: fileState({ lastImport: { success: true, name: "good", path: "p", bytes: 10, revision: 15, warningCount: 1, warnings: ["no provenance"] } }) }));
    expect(role("wg-lastimport").textContent).toContain("good");
    expect(role("wg-lastimport").textContent).toContain("no provenance");
  });

  it("shows locked/imported badge and origin summary", () => {
    push(fixture({ file: fileState({ locked: true, imported: true, origin: { seed: 77, generatorVersion: 1, rulesVersion: 4, preset: "continental" } }) }));
    expect(role("wg-lockbadge").textContent).toMatch(/locked/i);
    const o = role("wg-origin").textContent!;
    expect(o).toContain("77");
    expect(o).toContain("continental");
    expect(o).toContain("4");
    push();
    expect(role("wg-lockbadge").textContent).toBe("");
  });

  it("origin seed above 2^53 is shown as given", () => {
    push(fixture({ file: fileState({ imported: true, origin: { seed: 18446744073709552000, generatorVersion: 1, rulesVersion: 4, preset: "" } }) }));
    expect(role("wg-origin").textContent).toContain(String(18446744073709552000));
  });
});

describe("VALIDATION", () => {
  it("Validate button sends validate", () => {
    push();
    click(role("wg-validate"));
    expect(commands().pop()).toEqual({ id: ID, cmd: "validate", args: {} });
  });

  it("no validation shows a placeholder", () => {
    push();
    expect(sec("validation").textContent).toMatch(/not validated|No validation/i);
  });

  it("summary line shows outcome, seed, passed and fresh", () => {
    push(fixture({ validation: validationFail() }));
    const s = role("wg-valsum").textContent!;
    expect(s).toContain("Complete");
    expect(s).toContain("12345678");
    expect(s).toMatch(/failed/i);
    expect(s).toMatch(/fresh/i);
    const v = validationFail(); v.passed = true; v.fresh = false;
    push(fixture({ validation: v }));
    expect(role("wg-valsum").textContent).toMatch(/passed/i);
    expect(role("wg-valsum").textContent).toMatch(/stale/i);
  });

  it("one row per guarantee with name, ok badge and counts; failing open by default", () => {
    push(fixture({ validation: validationFail() }));
    const rows = qa(".wg-g");
    expect(rows.map((r) => r.getAttribute("data-gid"))).toEqual(["1", "2"]);
    expect(rows[0].querySelector(".wg-g-head")!.textContent).toContain("WorldLayoutGuarantee");
    expect(rows[0].querySelector(".badge")!.textContent).toMatch(/violated/i);
    expect(rows[1].querySelector(".badge")!.textContent).toMatch(/satisfied/i);
    expect(rows[0].textContent).toContain("RegionCountRange");
    expect(rows[1].textContent).toContain("4");
    expect(rows.map((r) => r.classList.contains("open"))).toEqual([true, false]);
  });

  it("clicking a head expands and sends expandGuarantee {id}; collapsing sends 0", () => {
    push(fixture({ validation: validationFail() }));
    click(q(".wg-g[data-gid='2'] .wg-g-head"));
    expect(q(".wg-g[data-gid='2']").classList.contains("open")).toBe(true);
    expect(commands().pop()).toEqual({ id: ID, cmd: "expandGuarantee", args: { id: 2 } });
    click(q(".wg-g[data-gid='2'] .wg-g-head"));
    expect(q(".wg-g[data-gid='2']").classList.contains("open")).toBe(false);
    expect(commands().pop()).toEqual({ id: ID, cmd: "expandGuarantee", args: { id: 0 } });
  });

  it("open state survives a state push", () => {
    push(fixture({ validation: validationFail() }));
    click(q(".wg-g[data-gid='2'] .wg-g-head"));
    push(fixture({ validation: validationFail() }));
    expect(q(".wg-g[data-gid='2']").classList.contains("open")).toBe(true);
  });

  it("violation cells in diagnostics are clickable and send selectCell", () => {
    push(fixture({ validation: validationFail() }));
    const chips = qa(".wg-chip");
    expect(chips.map((c) => c.textContent)).toEqual(["(18, 14)", "(19, 15)"]);
    click(chips[1]);
    expect(commands().pop()).toEqual({ id: ID, cmd: "selectCell", args: { x: 19, y: 15 } });
  });

  it("attempt rows show index, sub-seed, failed guarantees and ms", () => {
    push(fixture({ validation: validationFail() }));
    const a = qa(".wg-att");
    expect(a.length).toBe(2);
    expect(a[0].textContent).toContain("#1");
    expect(a[0].textContent).toContain("2654435761");
    expect(a[0].textContent).toContain("WorldLayoutGuarantee");
    expect(a[0].textContent).toContain("41");
    expect(a[1].textContent).toContain("ok");
    expect(sec("validation").textContent).toMatch(/12 more/);
  });

  it("validateError is shown", () => {
    push(fixture({ validateError: "no world generated yet" }));
    expect(role("wg-valerr").textContent).toContain("no world generated yet");
    push();
    expect(role("wg-valerr").textContent).toBe("");
  });

  it("Validate is disabled while generating", () => {
    push(fixture({ progress: { state: "Generating", attempt: 1, maxAttempts: 6, elapsedMs: 100, timeoutMs: 5000, lastOutcome: null } }));
    expect(role<HTMLButtonElement>("wg-validate").disabled).toBe(true);
  });
});

describe("progress and last outcome", () => {
  const gen = (o: any = {}) => fixture({ progress: { state: "Generating", attempt: 3, maxAttempts: 6, elapsedMs: 1800, timeoutMs: 5000, lastOutcome: null, ...o } });

  it("pill shows state", () => {
    push();
    expect(role("wg-pill").textContent).toBe("Complete");
    push(gen());
    expect(role("wg-pill").textContent).toBe("Generating");
  });

  it("generating shows attempt N of M, bar width and elapsed", () => {
    push(gen());
    expect(role("wg-attempt").textContent).toContain("3");
    expect(role("wg-attempt").textContent).toContain("6");
    expect(role("wg-pbar").querySelector("i")!.getAttribute("style")).toContain("36%");
    expect(role("wg-elapsed").textContent).toContain("1.8");
    expect(role("wg-elapsed").textContent).toContain("5.0");
  });

  it("Cancel enabled only while generating and sends cancel", () => {
    push();
    expect(role<HTMLButtonElement>("wg-cancel").disabled).toBe(true);
    push(gen());
    expect(role<HTMLButtonElement>("wg-cancel").disabled).toBe(false);
    click(role("wg-cancel"));
    expect(commands().pop()).toEqual({ id: ID, cmd: "cancel", args: {} });
  });

  it("failed generation: red card with 'previous world kept'", () => {
    push(fixture({ progress: { state: "Complete", attempt: 0, maxAttempts: 0, elapsedMs: 0, timeoutMs: 0,
      lastOutcome: { kind: "generation", outcome: "Unsatisfiable", message: "6 attempts failed", attempts: 6, maxAttempts: 6, revision: 14, previousWorldKept: true } } }));
    const c = role("wg-outcome");
    expect(c.classList.contains("bad")).toBe(true);
    expect(c.textContent).toContain("Unsatisfiable");
    expect(c.textContent).toContain("previous world kept");
  });

  it("successful generation card is not red and has no 'previous world kept'", () => {
    push(fixture({ progress: { state: "Complete", attempt: 0, maxAttempts: 0, elapsedMs: 0, timeoutMs: 0,
      lastOutcome: { kind: "generation", outcome: "Complete", message: "", attempts: 2, maxAttempts: 6, revision: 15, previousWorldKept: false } } }));
    const c = role("wg-outcome");
    expect(c.classList.contains("bad")).toBe(false);
    expect(c.textContent).toContain("Complete");
    expect(c.textContent).not.toContain("previous world kept");
  });

  it("import card names the import and never shows a generation outcome", () => {
    push(fixture({ progress: { state: "Complete", attempt: 0, maxAttempts: 0, elapsedMs: 0, timeoutMs: 0,
      lastOutcome: { kind: "import", outcome: "Imported", name: "my_world", revision: 15, seed: 77, previousWorldKept: false } } }));
    const c = role("wg-outcome");
    expect(c.textContent).toContain("Imported my_world");
    expect(c.textContent).not.toMatch(/Unsatisfiable|TIMEOUT|CANCEL|FAILURE|attempts/);
    expect(c.classList.contains("bad")).toBe(false);
  });

  it("no outcome hides the card", () => {
    push();
    expect(role("wg-outcome").style.display).toBe("none");
  });
});

describe("locked-import confirm dialog", () => {
  const locked = (o: Record<string, any> = {}) => fixture({ file: fileState({ locked: true, imported: true, origin: { seed: 1, generatorVersion: 1, rulesVersion: 4, preset: "" } }), ...o });
  const dlg = () => role("wg-dialog");
  const shown = () => dlg().classList.contains("show");

  it("is hidden when not locked and slider change goes straight to the domain", () => {
    push();
    drag("rivers", 0.7);
    expect(shown()).toBe(false);
    expect(commands().pop()).toEqual({ id: ID, cmd: "setScale", args: { key: "rivers", value: 0.7 } });
  });

  it("locked: slider change opens dialog, restores the value and sends nothing", () => {
    push(locked());
    drag("rivers", 0.7);
    expect(shown()).toBe(true);
    expect(range("rivers").value).toBe("0.5");
    expect(commands().length).toBe(0);
    vi.advanceTimersByTime(500);
    expect(commands().length).toBe(0);
  });

  it("Keep closes the dialog, keeps the old value, sends nothing", () => {
    push(locked());
    drag("rivers", 0.7);
    click(role("wg-keep"));
    expect(shown()).toBe(false);
    expect(range("rivers").value).toBe("0.5");
    expect(commands().length).toBe(0);
  });

  it("Discard sends discard {} then the refused slider change with its normal args", () => {
    push(locked());
    drag("rivers", 0.7);
    click(role("wg-discard"));
    expect(shown()).toBe(false);
    expect(commands()).toEqual([
      { id: ID, cmd: "discard", args: {} },
      { id: ID, cmd: "setScale", args: { key: "rivers", value: 0.7 } },
    ]);
  });

  it("seed, size, preset and regenerate changes are gated and discard carries them", () => {
    push(locked());
    const seed = role<HTMLInputElement>("wg-seed");
    seed.value = "42";
    fire(seed, "change");
    expect(shown()).toBe(true);
    click(role("wg-discard"));
    expect(commands().slice(-2)).toEqual([{ id: ID, cmd: "discard", args: {} }, { id: ID, cmd: "setSeed", args: { seed: 42 } }]);

    click(q("[data-role=wg-size] button[data-size=Standard]"));
    click(role("wg-discard"));
    expect(commands().slice(-2)).toEqual([{ id: ID, cmd: "discard", args: {} }, { id: ID, cmd: "setSize", args: { size: "Standard" } }]);

    const ps = role<HTMLSelectElement>("wg-preset");
    ps.value = "archipelago";
    fire(ps, "change");
    click(role("wg-preset-load"));
    click(role("wg-discard"));
    expect(commands().slice(-2)).toEqual([{ id: ID, cmd: "discard", args: {} }, { id: ID, cmd: "presetLoad", args: { id: "archipelago" } }]);

    click(role("wg-regen"));
    click(role("wg-discard"));
    expect(commands().slice(-2).map((c) => c.cmd)).toEqual(["discard", "regenerate"]);
  });

  it("Keep restores a changed seed input", () => {
    push(locked());
    const seed = role<HTMLInputElement>("wg-seed");
    seed.value = "42";
    fire(seed, "change");
    click(role("wg-keep"));
    expect(seed.value).toBe("12345678");
    expect(commands().length).toBe(0);
  });

  it("a long-keyed change still discards: discard {} then the short setScale, no too-long state", () => {
    push(locked({ controls: [slider("mountainousness", 1, "Landmass", 0, 1, 0.01, 0.5, 0.5)] }));
    drag("mountainousness", 0.73);
    click(role("wg-discard"));
    expect(commands()).toEqual([
      { id: ID, cmd: "discard", args: {} },
      { id: ID, cmd: "setScale", args: { key: "mountainousness", value: 0.73 } },
    ]);
    expect(shown()).toBe(false);
    expect(q("[data-role=wg-discard-bare]")).toBeNull();
    expect(q("[data-role=wg-dlgmsg]")).toBeNull();
  });

  it("dialog survives a state push while open", () => {
    push(locked());
    drag("rivers", 0.7);
    push(locked());
    expect(shown()).toBe(true);
  });
});

describe("drawers and evidence note", () => {
  it("renders label from view.drawers; unavailable disabled; per-kind sites toggle present", () => {
    push();
    const labs = qa("[data-role=wg-drawers] label");
    expect(labs.map((l) => l.textContent)).toEqual(["Tiles", "Region edges", "Sites", "Sites Hearth", "Dynasty starts", "Guarantee evidence"]);
    const boxes = qa("[data-role=wg-drawers] input") as HTMLInputElement[];
    expect(boxes.map((b) => b.disabled)).toEqual([true, false, false, false, true, false]);
    expect(boxes.map((b) => b.checked)).toEqual([true, false, false, true, false, true]);
    click(boxes[3]);
    expect(commands().pop()).toEqual({ id: ID, cmd: "toggle", args: { drawer: "sites.hearth" } });
  });

  it("evidence-hidden note shows view.evidenceHidden and hides without it", () => {
    const st = fixture(); st.view.evidenceHidden = "Evidence hidden: last generation failed (see Validation)";
    push(st);
    expect(role("wg-evhidden").textContent).toContain("last generation failed");
    expect(role("wg-evhidden").style.display).not.toBe("none");
    push();
    expect(role("wg-evhidden").style.display).toBe("none");
  });
});

describe("cell inspector", () => {
  it("shows the absorbed flag", () => {
    push(fixture({ cell: { success: true, x: 3, y: 4, index: 9, absorbed: true, layers: {}, neighbours: [], selected: { x: 3, y: 4 } } }));
    expect(role("wg-cell").textContent).toMatch(/absorbed/);
    expect(role("wg-cell").textContent).toMatch(/true/);
  });
});
