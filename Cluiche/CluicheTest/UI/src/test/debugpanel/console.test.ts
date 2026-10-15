// Ported from the throwaway headless-Chrome harness run_panel_test.py (34 checks).
// NOTE: runs in jsdom, not Ultralight's JavaScriptCore: proves page logic only.
import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import { loadPanel, type Panel } from "./loadPanel";

let p: Panel;
let input: HTMLInputElement;
let res: HTMLElement;
const long = Array.from({ length: 200 }, (_, i) => String.fromCharCode(97 + (i % 26))).join("");

beforeEach(() => {
  vi.useFakeTimers();
  p = loadPanel();
  input = p.$("cmd-input") as HTMLInputElement;
  res = p.$("cmd-result");
});
afterEach(() => {
  p.close();
  vi.useRealTimers();
});

describe("console: send", () => {
  it("handlers exported", () => {
    for (const f of ["consoleResult", "appendLogLines", "consoleCompletions", "consoleSetInput"])
      expect(typeof p.win[f]).toBe("function");
  });
  it("counter has no warn class at exactly 200", () => {
    p.typeInto(input, long);
    expect(p.$("cmd-counter").className).not.toContain("warn");
  });
  it("counter warn class over 200", () => {
    p.typeInto(input, long + "xxxxx");
    expect(p.$("cmd-counter").className).toContain("warn");
  });
  describe("after sending a 200 char line", () => {
    beforeEach(() => {
      p.typeInto(input, long);
      p.key(input, "Enter");
    });
    it("onConsoleLine called", () => expect(p.last("line")).not.toBeNull());
    it("4 parts, each <= 60 chars", () => {
      const parts = p.last("line")!.slice(2);
      expect(parts.length).toBe(4);
      expect(parts.every((x: string) => x.length <= 60)).toBe(true);
    });
    it("parts reassemble to the original line", () => {
      expect(p.last("line")!.slice(2).join("")).toBe(long);
    });
    it("seq is 1", () => expect(p.last("line")![1]).toBe(1));
    it("input cleared after Enter", () => expect(input.value).toBe(""));
    it("result line shows pending ellipsis", () => expect(res.textContent).toContain("…"));
  });
});

describe("console: results", () => {
  beforeEach(() => {
    p.typeInto(input, long);
    p.key(input, "Enter"); // seq 1
  });
  it("stale result (wrong seq) ignored", () => {
    p.win.consoleResult(JSON.stringify({ seq: 99, exitCode: 0, parseError: 0, text: "x" }));
    expect(res.textContent).not.toContain("exit 0");
  });
  it("exit 0 shown green", () => {
    p.win.consoleResult(JSON.stringify({ seq: 1, exitCode: 0, parseError: 0, text: "" }));
    expect(res.querySelector(".code.ok")).not.toBeNull();
    expect(res.textContent).toContain("exit 0");
  });
  it("exit 3 shown red as not found (3)", () => {
    p.typeInto(input, "nonexistent.cmd");
    p.key(input, "Enter"); // seq 2
    p.win.consoleResult(JSON.stringify({ seq: 2, exitCode: 3, parseError: 0, text: "" }));
    expect(res.querySelector(".code.err")).not.toBeNull();
    expect(res.textContent).toContain("not found (3)");
  });
  it("parse error shown red with text", () => {
    p.typeInto(input, "bad");
    p.key(input, "Enter"); // seq 2
    p.win.consoleResult({ seq: 2, exitCode: 2, parseError: 2, text: "bad argument" });
    expect(res.querySelector(".code.err")).not.toBeNull();
    expect(res.textContent).toContain("bad argument");
  });
  it("over-240 line shows line too long without calling C++", () => {
    const before = p.count("line");
    input.value = "z".repeat(245);
    p.key(input, "Enter");
    expect(res.textContent).toContain("line too long");
    expect(p.count("line")).toBe(before);
  });
  it("HTML in result text is escaped", () => {
    p.typeInto(input, "q");
    p.key(input, "Enter"); // seq 2
    p.win.consoleResult({ seq: 2, exitCode: 0, parseError: 0, text: "<b>x</b>" });
    expect(res.querySelector("b")).toBeNull();
  });
});

describe("console: history", () => {
  it("Up asks C++ for prev", () => {
    p.typeInto(input, "my draft");
    const h0 = p.count("hist");
    p.key(input, "ArrowUp");
    expect(p.count("hist")).toBe(h0 + 1);
    expect(p.last("hist")![1]).toBe("prev");
  });
  it("history text placed in input", () => {
    p.typeInto(input, "my draft");
    p.key(input, "ArrowUp");
    p.win.consoleSetInput(JSON.stringify({ text: "older command", restore: false }));
    expect(input.value).toBe("older command");
  });
  it("Down asks C++ for next", () => {
    p.typeInto(input, "my draft");
    p.key(input, "ArrowUp");
    p.win.consoleSetInput(JSON.stringify({ text: "older command", restore: false }));
    p.key(input, "ArrowDown");
    expect(p.last("hist")![1]).toBe("next");
  });
  it("draft restored after newest entry", () => {
    p.typeInto(input, "my draft");
    p.key(input, "ArrowUp");
    p.win.consoleSetInput(JSON.stringify({ text: "older command", restore: false }));
    p.key(input, "ArrowDown");
    p.win.consoleSetInput({ text: "", restore: true });
    expect(input.value).toBe("my draft");
  });
});

describe("console: autocomplete", () => {
  const rows = () => p.doc.querySelectorAll("#ac .ac-item");
  const offer = () => {
    p.typeInto(input, "debug.l");
    vi.advanceTimersByTime(250);
    const items = Array.from({ length: 7 }, (_, i) => ({ name: "debug.layer." + i, rank: "prefix" }));
    p.win.consoleCompletions(JSON.stringify({ items }));
  };
  it("autocomplete requested for first token after debounce", () => {
    p.typeInto(input, "debug.l");
    vi.advanceTimersByTime(250);
    expect(p.last("complete")![1]).toBe("debug.l");
  });
  it("popup shows at most 5 rows", () => {
    offer();
    expect(rows().length).toBe(5);
  });
  it("first row highlighted", () => {
    offer();
    expect(rows()[0].className).toContain("sel");
  });
  it("Down moves the highlight", () => {
    offer();
    p.key(input, "ArrowDown");
    expect(rows()[1].className).toContain("sel");
  });
  it("Tab accepts highlighted name plus space", () => {
    offer();
    p.key(input, "ArrowDown");
    p.key(input, "Tab");
    expect(input.value).toBe("debug.layer.1 ");
  });
  it("popup closed after accept", () => {
    offer();
    p.key(input, "ArrowDown");
    p.key(input, "Tab");
    expect(rows().length).toBe(0);
  });
  it("no completion requested once a space is typed", () => {
    offer();
    p.key(input, "ArrowDown");
    p.key(input, "Tab");
    const n = p.count("complete");
    p.typeInto(input, "a b");
    vi.advanceTimersByTime(200);
    expect(p.count("complete")).toBe(n);
    expect(p.last("complete")![1]).toBe("debug.l");
  });
});

describe("console: log view", () => {
  const lines = [
    { t: 1.0, level: "INFO", channel: "Debug", msg: "one <i>" },
    { t: 2.5, level: "WARN", channel: "API", msg: "two" },
    { t: 3.5, level: "ERROR", channel: "API", msg: "three" },
    { t: 4.5, level: "DEBUG", channel: "X", msg: "four" },
  ];
  let body: HTMLElement;
  const n = () => body.querySelectorAll(".log-line").length;
  beforeEach(() => {
    body = p.$("log-body");
    p.win.appendLogLines(JSON.stringify({ lines }));
  });
  it("4 log lines rendered (DEBUG shown as INFO)", () => expect(n()).toBe(4));
  it("log message HTML escaped", () => expect(body.querySelector("i")).toBeNull());
  it("timestamp formatted", () => expect(body.textContent).toContain("00:00:02.500"));
  it("WARN filter button exists", () =>
    expect(p.doc.querySelector('#log-filters [data-lvl="WARN"]')).not.toBeNull());
  it("WARN filter shows 1 line", () => {
    p.click(p.doc.querySelector('#log-filters [data-lvl="WARN"]')!);
    expect(n()).toBe(1);
  });
  it("count label shows shown/total", () => {
    p.click(p.doc.querySelector('#log-filters [data-lvl="WARN"]')!);
    expect(p.$("log-count").textContent).toBe("1 / 4");
  });
  it("log capped at 500 lines", () => {
    const many = Array.from({ length: 600 }, (_, i) => ({ t: i, level: "INFO", channel: "c", msg: "m" + i }));
    p.win.appendLogLines({ lines: many });
    expect(n()).toBe(500);
  });
  it("header click toggles the log view", () => {
    const view = p.$("log-view");
    const was = view.classList.contains("open");
    p.click(p.$("log-hdr"));
    expect(view.classList.contains("open")).toBe(!was);
  });
});
