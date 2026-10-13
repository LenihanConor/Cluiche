// Ported from the throwaway headless-Chrome harness run_palette_test.py (46 checks).
// NOTE: runs in jsdom, not Ultralight's JavaScriptCore: proves page logic only.
import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import { loadPanel, type Panel } from "./loadPanel";

const SAMPLE = {
  total: 3,
  counts: { all: 3, commands: 2, domains: 1 },
  items: [
    { type: "command", name: "debug.layer.enable", description: "Enable <b>layer</b>", category: "debug", example: "debug.layer.enable X", domainId: "", action: "fill", actionText: "debug.layer.enable Entities", hl: "110000000000000000" },
    { type: "domain", name: "Physics2D", description: "Rigid bodies", category: "Physics", example: "", domainId: "Physics2D", action: "reveal", actionText: "Physics2D", hl: "010000000" },
    { type: "command", name: "<b>x</b>", description: "d", category: "c", example: "", domainId: "", action: "fill", actionText: "x", hl: "0000000" },
  ],
};

let p: Panel;
let input: HTMLInputElement;
let pin: HTMLInputElement;
let grp: Element;
let card: Element;

const shown = () => p.$("pal-overlay").classList.contains("show");
const rows = () => p.doc.querySelectorAll("#pal-list .pal-row");
const ctrlP = (k = "p") => p.key(p.doc.body, k, { ctrlKey: true });
const open = () => {
  ctrlP();
  p.win.paletteResults(JSON.stringify(SAMPLE));
};

beforeEach(() => {
  vi.useFakeTimers();
  p = loadPanel();
  input = p.$("cmd-input") as HTMLInputElement;
  pin = p.$("pal-input") as HTMLInputElement;
  p.win.updateDomainState({
    Physics2D: { _displayName: "Physics2D", _group: "Physics", _description: "rb", drawers: [{ name: "A", enabled: true }], stats: {}, controls: [] },
    Coord2D: { _displayName: "Coord2D", _group: "Spatial", _description: "sp", drawers: [], stats: {}, controls: [] },
  });
  grp = p.doc.querySelector('.group[data-group-key="Physics"]')!;
  card = p.doc.querySelector('.domain[data-domain="Physics2D"]')!;
  grp.classList.remove("open");
  grp.classList.add("closed");
  p.win.ui.groupOpen["Physics"] = false;
});
afterEach(() => {
  p.close();
  vi.useRealTimers();
});

describe("palette: setup and open/close", () => {
  it("setup: physics card exists and closed", () => {
    expect(card).not.toBeNull();
    expect(card.classList.contains("closed")).toBe(true);
    expect(grp.classList.contains("closed")).toBe(true);
  });
  it("overlay hidden initially", () => expect(shown()).toBe(false));
  it("Ctrl+P opens overlay", () => {
    input.focus();
    ctrlP();
    expect(shown()).toBe(true);
  });
  it("Ctrl+P default prevented", () => expect(ctrlP().defaultPrevented).toBe(true));
  it("search box focused", () => {
    input.focus();
    ctrlP();
    expect(p.doc.activeElement).toBe(pin);
  });
  it("onPaletteOpen called once", () => {
    ctrlP();
    expect(p.count("open")).toBe(1);
  });
  it("Esc closes", () => {
    ctrlP();
    p.key(p.doc.body, "Escape");
    expect(shown()).toBe(false);
  });
  it("Esc returns focus to console input", () => {
    input.focus();
    ctrlP();
    p.key(p.doc.body, "Escape");
    expect(p.doc.activeElement).toBe(input);
  });
  it("Ctrl+P (capital) reopens", () => {
    ctrlP();
    p.key(p.doc.body, "Escape");
    ctrlP("P");
    expect(shown()).toBe(true);
    expect(p.count("open")).toBe(2);
  });
  it("Up prevented only when open", () => {
    ctrlP();
    expect(p.key(p.doc.body, "ArrowUp").defaultPrevented).toBe(true);
  });
});

describe("palette: results rendering", () => {
  beforeEach(open);
  it("3 rows rendered", () => expect(rows().length).toBe(3));
  it("3 tabs, no Panels", () => {
    expect(p.doc.querySelectorAll("#pal-tabs .pal-tab").length).toBe(3);
    expect(p.$("pal-tabs").textContent).not.toContain("Panels");
  });
  it("tab counts", () => {
    const tabs = p.doc.querySelectorAll("#pal-tabs .pal-tab");
    expect(tabs[0].textContent).toBe("All3");
    expect(tabs[1].textContent).toBe("Commands2");
    expect(tabs[2].textContent).toBe("Domains1");
  });
  it("highlight marks from hl", () => {
    expect(rows()[0].querySelectorAll("mark").length).toBe(2);
    expect(rows()[0].querySelector("mark")!.textContent).toBe("d");
  });
  it("description with <b> escaped", () => {
    expect(rows()[0].querySelector(".pal-ds b")).toBeNull();
    expect(rows()[0].querySelector(".pal-ds")!.textContent).toBe("Enable <b>layer</b>");
  });
  it("name with <b> escaped", () => {
    expect(rows()[2].querySelector(".pal-nm b")).toBeNull();
    expect(rows()[2].querySelector(".pal-nm")!.textContent).toBe("<b>x</b>");
  });
  it("first row selected", () => expect(rows()[0].classList.contains("sel")).toBe(true));
  it("type badges", () => {
    expect(rows()[0].querySelector(".pal-type")!.className.indexOf("command")).toBeGreaterThan(0);
    expect(rows()[1].querySelector(".pal-type")!.className.indexOf("domain")).toBeGreaterThan(0);
  });
  it("object (not string) accepted", () => {
    p.win.paletteResults(SAMPLE);
    expect(rows().length).toBe(3);
  });
});

describe("palette: selection and activation", () => {
  beforeEach(open);
  it("Down selects row 1", () => {
    p.key(p.doc.body, "ArrowDown");
    expect(rows()[1].classList.contains("sel")).toBe(true);
  });
  it("Down wraps to row 0", () => {
    for (let i = 0; i < 3; ++i) p.key(p.doc.body, "ArrowDown");
    expect(rows()[0].classList.contains("sel")).toBe(true);
  });
  it("Up wraps to last row", () => {
    p.key(p.doc.body, "ArrowUp");
    expect(rows()[2].classList.contains("sel")).toBe(true);
  });
  it("hover selects", () => {
    p.mousemove(rows()[1]);
    expect(rows()[1].classList.contains("sel")).toBe(true);
  });
  describe("Enter fill", () => {
    let lines0: number;
    beforeEach(() => {
      p.mousemove(rows()[0]);
      lines0 = p.count("line");
      p.key(p.doc.body, "Enter");
    });
    it("overlay closed", () => expect(shown()).toBe(false));
    it("cmd-input has actionText", () => expect(input.value).toBe("debug.layer.enable Entities"));
    it("input focused", () => expect(p.doc.activeElement).toBe(input));
    it("caret at end", () => {
      expect(input.selectionStart).toBe(input.value.length);
      expect(input.selectionEnd).toBe(input.value.length);
    });
    it("not run", () => expect(p.count("line")).toBe(lines0));
    it("counter updated", () => expect(p.$("cmd-counter").textContent).toBe(input.value.length + "/240"));
  });
  describe("Enter reveal", () => {
    beforeEach(() => {
      input.value = "";
      p.key(p.doc.body, "ArrowDown");
      p.key(p.doc.body, "Enter");
    });
    it("overlay closed", () => expect(shown()).toBe(false));
    it("group opened", () => {
      expect(grp.classList.contains("open")).toBe(true);
      expect(grp.classList.contains("closed")).toBe(false);
      expect(p.win.ui.groupOpen["Physics"]).toBe(true);
    });
    it("card opened + flash", () => {
      expect(card.classList.contains("open")).toBe(true);
      expect(card.classList.contains("flash")).toBe(true);
    });
    it("cmd-input untouched", () => expect(input.value).toBe(""));
  });
  it("click activates (fill)", () => {
    p.click(rows()[0]);
    expect(shown()).toBe(false);
    expect(input.value).toBe("debug.layer.enable Entities");
  });
});

describe("palette: empty results", () => {
  beforeEach(() => {
    ctrlP();
    p.win.paletteResults(JSON.stringify({ total: 0, counts: { all: 0, commands: 0, domains: 0 }, items: [] }));
  });
  it("empty shows No results found", () => expect(p.$("pal-list").textContent).toContain("No results found"));
  it("Enter on empty does nothing", () => {
    p.key(p.doc.body, "Enter");
    expect(shown()).toBe(true);
  });
});

describe("palette: typing and tabs", () => {
  let s0: number;
  beforeEach(() => {
    ctrlP();
    s0 = p.count("search");
    p.typeInto(pin, "lay");
  });
  it("search is debounced (not immediate)", () => expect(p.count("search")).toBe(s0));
  it("search after debounce with mask 3", () => {
    vi.advanceTimersByTime(200);
    const l = p.last("search")!;
    expect(p.count("search")).toBe(s0 + 1);
    expect(l[1]).toBe("lay");
    expect(l[2]).toBe("3");
  });
  it("mask is a string", () => {
    vi.advanceTimersByTime(200);
    expect(typeof p.last("search")![2]).toBe("string");
  });
  it("Tab -> Commands, mask 1, same query", () => {
    vi.advanceTimersByTime(200);
    p.key(p.doc.body, "Tab");
    const l = p.last("search")!;
    expect(l[1]).toBe("lay");
    expect(l[2]).toBe("1");
    expect(p.$("pal-tabs").querySelector(".pal-tab.active")!.textContent!.indexOf("Commands")).toBe(0);
  });
  it("Tab -> Domains mask 2, Tab -> All mask 3, Shift+Tab -> Domains", () => {
    vi.advanceTimersByTime(200);
    p.key(p.doc.body, "Tab");
    p.key(p.doc.body, "Tab");
    expect(p.last("search")![2]).toBe("2");
    p.key(p.doc.body, "Tab");
    expect(p.last("search")![2]).toBe("3");
    p.key(p.doc.body, "Tab", { shiftKey: true });
    expect(p.last("search")![2]).toBe("2");
  });
  it("tab click -> mask 1", () => {
    vi.advanceTimersByTime(200);
    p.click(p.doc.querySelectorAll("#pal-tabs .pal-tab")[1]);
    expect(p.last("search")![2]).toBe("1");
  });
  it(">60 char query truncated to 60", () => {
    vi.advanceTimersByTime(200);
    p.typeInto(pin, "q".repeat(99));
    vi.advanceTimersByTime(150);
    expect(p.last("search")![1].length).toBe(60);
  });
  it("console Enter still sends line when palette closed", () => {
    vi.advanceTimersByTime(200);
    p.key(p.doc.body, "Escape");
    const n = p.count("line");
    input.value = "abc";
    p.key(input, "Enter");
    expect(p.count("line")).toBe(n + 1);
  });
});
