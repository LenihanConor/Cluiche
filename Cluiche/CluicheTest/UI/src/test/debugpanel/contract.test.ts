// Contract tests: the golden JSON files produced by the REAL C++ builders
// (GoogleTest PanelContractTest, Cluiche/Tests/Contracts/debugpanel/) are fed into the real page.
// If either side drifts (keys, ids vs display names, shapes) one of these fails.
// NOTE: runs in jsdom, not Ultralight's JavaScriptCore: proves page logic only.
import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadPanel, type Panel } from "./loadPanel";

const here = path.dirname(fileURLToPath(import.meta.url));
const GOLDEN_DIR = path.resolve(here, "../../../../../Tests/Contracts/debugpanel");

function golden(name: string): string {
  const file = path.join(GOLDEN_DIR, name);
  if (!fs.existsSync(file)) throw new Error(`golden file missing: ${file}`);
  return fs.readFileSync(file, "utf-8");
}
const goldenJson = (name: string): any => JSON.parse(golden(name));

let p: Panel;
let input: HTMLInputElement;

beforeEach(() => {
  vi.useFakeTimers();
  p = loadPanel();
  input = p.$("cmd-input") as HTMLInputElement;
});
afterEach(() => {
  p.close();
  vi.useRealTimers();
});

const cards = () => Array.from(p.doc.querySelectorAll(".domain")) as HTMLElement[];
const cardFor = (name: string) => cards().find((c) => c.getAttribute("data-domain") === name);
const pushDomains = () => p.win.updateDomainState(golden("domain_states.json"));

describe("contract: domain_states.json", () => {
  it("one card per domain, data-domain is the display name, in the right group", () => {
    pushDomains();
    const states = goldenJson("domain_states.json");
    const names = Object.keys(states);
    expect(names.length).toBeGreaterThanOrEqual(3);
    expect(cards().length).toBe(names.length);
    for (const key of names) {
      const st = states[key];
      expect(key).toBe(st._displayName);
      const card = cardFor(st._displayName);
      expect(card, `card for ${st._displayName}`).toBeDefined();
      const grp = card!.closest(".group") as HTMLElement;
      expect(grp.getAttribute("data-group-key")).toBe(st._group);
    }
  });
  it("includes a domain whose id differs from its display name", () => {
    pushDomains();
    expect(cardFor("Message Bus")).toBeDefined();
    expect(cardFor("MessageBus")).toBeUndefined();
  });
  it("accent, description and drawers are rendered", () => {
    pushDomains();
    const physics = cardFor("Physics2D")!;
    expect(physics.style.getPropertyValue("--accent")).toBe("#e07030");
    expect(physics.querySelector(".desc")!.textContent).toBe("Rigid bodies and contacts");
    expect(physics.querySelectorAll("[data-drawer]").length).toBe(2);
  });
  it("shape: every domain has the keys the page reads", () => {
    const states = goldenJson("domain_states.json");
    for (const st of Object.values<any>(states)) {
      for (const k of ["_displayName", "_group", "_description", "_accent"]) expect(typeof st[k]).toBe("string");
      expect(st._accent).toMatch(/^#[0-9a-f]{6}$/);
    }
  });
});

describe("contract: palette results", () => {
  const files = ["palette_results_all.json", "palette_results_commands.json", "palette_results_domains.json"];
  const rows = () => Array.from(p.doc.querySelectorAll("#pal-list .pal-row")) as HTMLElement[];
  const openWith = (file: string) => {
    p.key(p.doc.body, "p", { ctrlKey: true });
    p.win.paletteResults(golden(file));
  };

  it.each(files)("%s: structural invariants the page relies on", (file) => {
    const g = goldenJson(file);
    expect(typeof g.total).toBe("number");
    for (const k of ["all", "commands", "domains"]) expect(typeof g.counts[k]).toBe("number");
    expect(g.counts.all).toBe(g.counts.commands + g.counts.domains);
    expect(Array.isArray(g.items)).toBe(true);
    expect(g.items.length).toBeGreaterThan(0);
    for (const it of g.items) {
      for (const k of ["type", "name", "action", "actionText", "hl"]) expect(typeof it[k]).toBe("string");
      expect(["command", "domain"]).toContain(it.type);
      expect(["fill", "reveal"]).toContain(it.action);
      expect(it.hl.length).toBe(it.name.length);
    }
  });

  it("the golden set contains a domain whose id differs from its display name", () => {
    const g = goldenJson("palette_results_all.json");
    const bus = g.items.find((i: any) => i.name === "Message Bus");
    expect(bus).toBeDefined();
    expect(bus.domainId).toBe("MessageBus");
    expect(bus.action).toBe("reveal");
    expect(bus.actionText).toBe("Message Bus");
  });

  it.each(files)("%s: every reveal item opens the card whose data-domain equals actionText", (file) => {
    const g = goldenJson(file);
    const reveals = g.items.map((it: any, i: number) => ({ it, i })).filter((x: any) => x.it.action === "reveal");
    for (const { it, i } of reveals) {
      // fresh page per item so the reveal state is not inherited
      p.close();
      p = loadPanel();
      pushDomains();
      p.key(p.doc.body, "p", { ctrlKey: true });
      p.win.paletteResults(golden(file));
      p.mousemove(rows()[i]);
      p.key(p.doc.body, "Enter");
      const card = cardFor(it.actionText);
      expect(card, `card with data-domain="${it.actionText}" for palette item "${it.name}"`).toBeDefined();
      expect(card!.classList.contains("open"), `${it.actionText} revealed`).toBe(true);
      expect(card!.classList.contains("closed")).toBe(false);
      const grp = card!.closest(".group") as HTMLElement;
      expect(grp.classList.contains("open")).toBe(true);
    }
  });

  it("all-results golden reveals Message Bus explicitly", () => {
    pushDomains();
    openWith("palette_results_all.json");
    const g = goldenJson("palette_results_all.json");
    const i = g.items.findIndex((x: any) => x.name === "Message Bus");
    expect(i).toBeGreaterThanOrEqual(0);
    p.mousemove(rows()[i]);
    p.key(p.doc.body, "Enter");
    const card = cardFor("Message Bus")!;
    expect(card.classList.contains("open")).toBe(true);
    expect(card.classList.contains("flash")).toBe(true);
  });

  it.each(files)("%s: every fill item puts exactly actionText in cmd-input and does not run it", (file) => {
    const g = goldenJson(file);
    const fills = g.items.map((it: any, i: number) => ({ it, i })).filter((x: any) => x.it.action === "fill");
    for (const { it, i } of fills) {
      input.value = "";
      p.key(p.doc.body, "Escape");
      openWith(file);
      const before = p.count("line");
      p.mousemove(rows()[i]);
      p.key(p.doc.body, "Enter");
      expect(input.value).toBe(it.actionText);
      expect(p.count("line")).toBe(before);
    }
  });
});

describe("contract: console results", () => {
  const res = () => p.$("cmd-result");
  const send = (text: string) => {
    p.typeInto(input, text);
    p.key(input, "Enter");
  };

  it("ok / not found / parse error render with the right classes and text", () => {
    const ok = goldenJson("console_result_ok.json");
    const nf = goldenJson("console_result_notfound.json");
    const pe = goldenJson("console_result_parse_error.json");
    expect([ok.seq, nf.seq, pe.seq]).toEqual([1, 2, 3]);

    send(ok.text);
    p.win.consoleResult(golden("console_result_ok.json"));
    expect(res().querySelector(".code.ok")).not.toBeNull();
    expect(res().textContent).toContain("exit 0");

    send(nf.text);
    p.win.consoleResult(golden("console_result_notfound.json"));
    expect(res().querySelector(".code.err")).not.toBeNull();
    expect(res().textContent).toContain("not found (3)");

    send(pe.text);
    p.win.consoleResult(golden("console_result_parse_error.json"));
    expect(res().querySelector(".code.err")).not.toBeNull();
    expect(res().textContent).toContain(`parse error (${pe.parseError})`);
  });

  it.each(["console_result_ok.json", "console_result_notfound.json", "console_result_parse_error.json"])(
    "%s: shape",
    (file) => {
      const g = goldenJson(file);
      for (const k of ["seq", "exitCode", "parseError"]) expect(typeof g[k]).toBe("number");
      expect(typeof g.text).toBe("string");
    },
  );
});

describe("contract: console completions and set-input", () => {
  it("completions render one row per item (max 5) with the names", () => {
    const g = goldenJson("console_completions.json");
    expect(g.items.length).toBeGreaterThan(0);
    for (const it of g.items) {
      expect(typeof it.name).toBe("string");
      expect(typeof it.rank).toBe("string");
    }
    p.typeInto(input, "debug.");
    p.win.consoleCompletions(golden("console_completions.json"));
    const rows = Array.from(p.doc.querySelectorAll("#ac .ac-item"));
    expect(rows.length).toBe(Math.min(5, g.items.length));
    rows.forEach((r, i) => expect(r.querySelector(".nm")!.textContent).toBe(g.items[i].name));
  });

  it("set-input puts the text in the input; restore brings the draft back", () => {
    const set = goldenJson("console_set_input.json");
    const restore = goldenJson("console_set_input_restore.json");
    expect(set.restore).toBe(false);
    expect(restore.restore).toBe(true);
    p.typeInto(input, "my draft");
    p.key(input, "ArrowUp");
    p.win.consoleSetInput(golden("console_set_input.json"));
    expect(input.value).toBe(set.text);
    p.key(input, "ArrowDown");
    p.win.consoleSetInput(golden("console_set_input_restore.json"));
    expect(input.value).toBe("my draft");
  });
});

describe("contract: append_log_lines.json", () => {
  it("renders every line, escapes HTML, shows DEBUG as INFO", () => {
    const g = goldenJson("append_log_lines.json");
    expect(g.lines.length).toBe(4);
    expect(g.lines.map((l: any) => l.level)).toEqual(["DEBUG", "INFO", "WARN", "ERROR"]);
    for (const l of g.lines) {
      expect(typeof l.t).toBe("number");
      expect(typeof l.channel).toBe("string");
      expect(typeof l.msg).toBe("string");
    }
    p.win.appendLogLines(golden("append_log_lines.json"));
    const body = p.$("log-body");
    const lines = Array.from(body.querySelectorAll(".log-line"));
    expect(lines.length).toBe(g.lines.length);
    expect(body.querySelector("b")).toBeNull();
    expect(body.querySelector("img")).toBeNull();
    expect(body.textContent).toContain("<b>slow</b> step & more");
    expect(lines[0].classList.contains("INFO")).toBe(true);
    expect(lines[0].classList.contains("DEBUG")).toBe(false);
    expect(lines[2].classList.contains("WARN")).toBe(true);
    expect(lines[3].classList.contains("ERROR")).toBe(true);
    expect(body.textContent).toContain("00:00:04.125");
  });
});
