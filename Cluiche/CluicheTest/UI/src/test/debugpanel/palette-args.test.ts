// Exhaustive page-side tests for the palette command-args lines (palArgsHtml), run against BOTH copies of
// debug-panel.html (CluicheTest and CoW). C++ side: TestPaletteArgsEdge.cpp / TestPanelContract.cpp.
// NOTE: runs in jsdom, not Ultralight's JavaScriptCore: proves page logic only.
import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadPanel, type Panel } from "./loadPanel";

const here = path.dirname(fileURLToPath(import.meta.url));
const ASSETS = path.resolve(here, "../../../../../Assets");
const GOLDEN_DIR = path.resolve(here, "../../../../../Tests/Contracts/debugpanel");
const PAGES: [string, string][] = [
  ["CluicheTest", path.join(ASSETS, "CluicheTest/Global/Presentation/UI/debug-panel.html")],
  ["CoW", path.join(ASSETS, "CoW/Global/Presentation/UI/debug-panel.html")],
];

const golden = (name: string): string => fs.readFileSync(path.join(GOLDEN_DIR, name), "utf-8");

function cmd(name: string, args?: any, extra: Record<string, any> = {}) {
  const it: any = {
    type: "command", name, description: "d", category: "c", example: "", domainId: "",
    action: "fill", actionText: name, hl: "0".repeat(name.length), ...extra,
  };
  if (args !== undefined) it.args = args;
  return it;
}
const payload = (items: any[], total = items.length) => ({
  total, counts: { all: total, commands: total, domains: 0 }, items,
});

describe.each(PAGES)("palette args (%s page)", (_label, file) => {
  let p: Panel;
  let input: HTMLInputElement;

  const rows = () => Array.from(p.doc.querySelectorAll("#pal-list .pal-row")) as HTMLElement[];
  const argLines = (row = 0) => Array.from(rows()[row].querySelectorAll(".pal-arg")) as HTMLElement[];
  const show = (data: any) => {
    p.key(p.doc.body, "p", { ctrlKey: true });
    p.win.paletteResults(typeof data === "string" ? data : JSON.stringify(data));
  };
  const showArgs = (args: any, extra: Record<string, any> = {}) => show(payload([cmd("x.cmd", args, extra)]));
  const oneArg = (a: any) => {
    showArgs([a]);
    return argLines()[0];
  };

  beforeEach(() => {
    vi.useFakeTimers();
    p = loadPanel(file);
    input = p.$("cmd-input") as HTMLInputElement;
  });
  afterEach(() => {
    p.close();
    vi.useRealTimers();
  });

  describe("false-y numbers", () => {
    it.each([
      [0, 0, "0..0"],
      [0, 10, "0..10"],
      [-1, 0, "-1..0"],
      [0.5, 10, "0.5..10"],
      [-0, 0, "0..0"],
    ])("min=%s max=%s renders %s", (mn, mx, text) => {
      const a = oneArg({ name: "r", type: "float", required: false, min: mn, max: mx });
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe(`float · optional · ${text}`);
    });
    it("e-notation numbers are shown as given", () => {
      const a = oneArg({ name: "r", type: "float", required: false, min: -1e-7, max: 1e10 });
      expect(a.querySelector(".pal-arg-meta")!.textContent).toContain("-1e-7..10000000000");
    });
    it("min without max renders no range but renders the rest", () => {
      const a = oneArg({ name: "r", type: "float", required: false, unit: "m", min: 1 });
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("float · optional · m");
      expect(a.textContent).toContain("r?");
    });
    it("max without min renders no range", () => {
      const a = oneArg({ name: "r", type: "float", required: true, max: 1 });
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("float · required");
    });
    it("null min/max (never emitted by C++) do not render the word null", () => {
      const a = oneArg({ name: "r", type: "float", min: null, max: 5 });
      expect(a.textContent).not.toContain("null");
      const b = oneArg({ name: "r", type: "float", min: null, max: null });
      expect(b.textContent).not.toContain("null");
    });
    it("the zero-valued range is not dropped next to unit and values", () => {
      const a = oneArg({ name: "r", type: "enum", unit: "s", min: 0, max: 0, values: ["a", "b"] });
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("enum · optional · s · 0..0 · a|b");
    });
  });

  describe("escaping", () => {
    const evil = ["<script>window.__pwn=1</script>", '<img src=x onerror="window.__pwn=1">', "a&amp;b &lt;i&gt;", "\"quoted\" 'single'", "</div><b>x</b>"];
    const noInjection = () => {
      const list = p.$("pal-list");
      expect(list.querySelector("script")).toBeNull();
      expect(list.querySelector("img")).toBeNull();
      expect(list.querySelector("b")).toBeNull();
      expect(list.querySelector("[onerror]")).toBeNull();
      expect(p.win.__pwn).toBeUndefined();
    };

    it.each(evil)("name %s", (s) => {
      const a = oneArg({ name: s, type: "t", required: true });
      noInjection();
      expect(a.querySelector(".pal-arg-nm")!.textContent).toBe(s);
    });
    it.each(evil)("type %s", (s) => {
      const a = oneArg({ name: "n", type: s, required: false });
      noInjection();
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe(`${s} · optional`);
    });
    it.each(evil)("description %s", (s) => {
      const a = oneArg({ name: "n", type: "t", description: s });
      noInjection();
      expect(a.querySelector(".pal-arg-ds")!.textContent).toBe(s);
    });
    it.each(evil)("unit %s", (s) => {
      const a = oneArg({ name: "n", type: "t", unit: s });
      noInjection();
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe(`t · optional · ${s}`);
    });
    it.each(evil)("enum value %s", (s) => {
      const a = oneArg({ name: "n", type: "enum", values: [s, "ok"] });
      noInjection();
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe(`enum · optional · ${s}|ok`);
    });
    it("everything evil at once, in several args and rows", () => {
      const args = evil.map((s) => ({ name: s, type: s, required: true, description: s, unit: s, min: 0, max: 1, values: [s] }));
      show(payload([cmd("a.cmd", args), cmd("b.cmd", args)]));
      noInjection();
      expect(argLines(0).length).toBe(evil.length);
      expect(argLines(1).length).toBe(evil.length);
    });
    it("a name that looks like an attribute break-out stays inert", () => {
      const a = oneArg({ name: '" class="pal-arg-nm hacked" x="', type: "t" });
      expect(p.$("pal-list").querySelector(".hacked")).toBeNull();
      expect(a.textContent).toContain('" class="pal-arg-nm hacked" x="');
    });
  });

  describe("malformed args never throw and the row still renders", () => {
    const bad: [string, any][] = [
      ["empty array", []],
      ["[null]", [null]],
      ["[undefined-like {}]", [{}]],
      ["string", "abc"],
      ["number", 5],
      ["zero", 0],
      ["true", true],
      ["null", null],
      ["plain object", { a: 1 }],
      ["array-like object", { length: 2, 0: { name: "x" }, 1: { name: "y" } }],
      ["nested arrays", [[{ name: "x" }]]],
      ["array of strings", ["abc", "def"]],
      ["array of numbers", [1, 2]],
    ];
    it.each(bad)("%s", (_n, args) => {
      expect(() => showArgs(args)).not.toThrow();
      expect(rows().length).toBe(1);
      expect(rows()[0].querySelector(".pal-nm")!.textContent).toBe("x.cmd");
      expect(rows()[0].querySelector(".pal-cat")!.textContent).toBe("c");
    });
    it("[] and non-arrays render no .pal-args block at all", () => {
      for (const args of [[], "abc", 5, 0, true, null, { a: 1 }, { length: 2 }]) {
        showArgs(args);
        expect(p.doc.querySelectorAll("#pal-list .pal-args").length, JSON.stringify(args)).toBe(0);
      }
    });
    it("[null] renders one blank optional line", () => {
      showArgs([null]);
      expect(argLines().length).toBe(1);
      expect(argLines()[0].querySelector(".pal-arg-meta")!.textContent).toBe(" · optional");
    });
    it("a bad entry between good ones does not hide the others", () => {
      showArgs([{ name: "a", type: "int" }, null, { name: "c", type: "int", required: true }]);
      const lines = argLines();
      expect(lines.length).toBe(3);
      expect(lines[0].textContent).toContain("a?");
      expect(lines[2].textContent).toContain("c");
    });
    it("missing name and type", () => {
      const a = oneArg({ required: true });
      expect(a.querySelector(".pal-arg-nm")!.textContent).toBe("");
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe(" · required");
    });
    it("non-string name/type/description values are tolerated", () => {
      const a = oneArg({ name: 42, type: 7, description: 9, unit: 3 });
      expect(a.textContent).toContain("42");
      expect(a.textContent).toContain("9");
    });
    it("args on a domain item is ignored by the data model but must not throw", () => {
      expect(() => show(payload([{ ...cmd("Dom", [{ name: "a", type: "int" }]), type: "domain", action: "reveal" }]))).not.toThrow();
      expect(rows().length).toBe(1);
    });
    it("a second render replaces the first (no stale arg lines)", () => {
      showArgs([{ name: "a", type: "int" }, { name: "b", type: "int" }]);
      expect(argLines().length).toBe(2);
      p.win.paletteResults(JSON.stringify(payload([cmd("y.cmd", [{ name: "only", type: "int" }])])));
      expect(argLines().length).toBe(1);
      expect(rows()[0].textContent).toContain("only");
      p.win.paletteResults(JSON.stringify(payload([cmd("z.cmd")])));
      expect(p.doc.querySelectorAll("#pal-list .pal-arg").length).toBe(0);
    });
  });

  describe("enum values", () => {
    it("values: [] renders no values segment", () => {
      const a = oneArg({ name: "m", type: "enum", values: [] });
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("enum · optional");
    });
    it("values that is not an array is ignored", () => {
      for (const v of ["abc", 5, { length: 2 }, true]) {
        const a = oneArg({ name: "m", type: "enum", values: v });
        expect(a.querySelector(".pal-arg-meta")!.textContent, JSON.stringify(v)).toBe("enum · optional");
      }
    });
    it("null / undefined entries render as empty, never as 'null'/'undefined'", () => {
      const a = oneArg({ name: "m", type: "enum", values: ["a", null, "b"] });
      expect(a.textContent).not.toContain("null");
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("enum · optional · a||b");
    });
    it("a long value (127 bytes, the C++ cut) and many values render in full", () => {
      const long = "q".repeat(127);
      const many = Array.from({ length: 100 }, (_, i) => `v${i}`);
      const a = oneArg({ name: "m", type: "enum", values: [long, ...many] });
      const t = a.querySelector(".pal-arg-meta")!.textContent!;
      expect(t).toContain(long);
      expect(t.endsWith("v99")).toBe(true);
      expect(t.split("|").length).toBe(101);
    });
    it("values on a non-enum type are still shown (C++ emits them)", () => {
      const a = oneArg({ name: "m", type: "string", values: ["x", "y"] });
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("string · optional · x|y");
    });
    it("numeric values are stringified", () => {
      const a = oneArg({ name: "m", type: "enum", values: [1, 2] });
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("enum · optional · 1|2");
    });
  });

  describe("required vs optional markers", () => {
    it("required: .req class, no '?', meta says required", () => {
      const a = oneArg({ name: "target", type: "string", required: true });
      const nm = a.querySelector(".pal-arg-nm")!;
      expect(nm.classList.contains("req")).toBe(true);
      expect(nm.textContent).toBe("target");
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("string · required");
    });
    it("optional: no .req class, trailing '?', meta says optional", () => {
      const a = oneArg({ name: "target", type: "string", required: false });
      const nm = a.querySelector(".pal-arg-nm")!;
      expect(nm.classList.contains("req")).toBe(false);
      expect(nm.textContent).toBe("target?");
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("string · optional");
    });
    it("required missing is optional", () => {
      const a = oneArg({ name: "t", type: "string" });
      expect(a.querySelector(".pal-arg-nm")!.classList.contains("req")).toBe(false);
    });
    it("only literal true counts as required for the .req class (truthy strings do too, C++ sends booleans)", () => {
      expect(oneArg({ name: "t", type: "s", required: true }).querySelector(".req")).not.toBeNull();
      expect(oneArg({ name: "t", type: "s", required: false }).querySelector(".req")).toBeNull();
      expect(oneArg({ name: "t", type: "s", required: 0 }).querySelector(".req")).toBeNull();
    });
    it("one line per arg in order, mixing required and optional", () => {
      showArgs([{ name: "a", type: "int", required: true }, { name: "b", type: "int" }, { name: "c", type: "int", required: true }]);
      const names = argLines().map((l) => l.querySelector(".pal-arg-nm")!.textContent);
      expect(names).toEqual(["a", "b?", "c"]);
      const reqs = argLines().map((l) => l.querySelector(".pal-arg-nm")!.classList.contains("req"));
      expect(reqs).toEqual([true, false, true]);
    });
  });

  describe("optional parts", () => {
    it("empty unit / description are omitted", () => {
      const a = oneArg({ name: "n", type: "int", unit: "", description: "" });
      expect(a.querySelector(".pal-arg-ds")).toBeNull();
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("int · optional");
    });
    it("description gets its own span", () => {
      const a = oneArg({ name: "n", type: "int", description: "does a thing" });
      expect(a.querySelector(".pal-arg-ds")!.textContent).toBe("does a thing");
    });
    it("unit before range before values", () => {
      const a = oneArg({ name: "n", type: "enum", unit: "m", min: 1, max: 2, values: ["a"] });
      expect(a.querySelector(".pal-arg-meta")!.textContent).toBe("enum · optional · m · 1..2 · a");
    });
    it("a 100-arg command renders 100 lines", () => {
      const args = Array.from({ length: 100 }, (_, i) => ({ name: `p${i}`, type: "int", required: i % 2 === 0 }));
      showArgs(args);
      expect(argLines().length).toBe(100);
    });
  });

  describe("50-row slice and keyboard selection with args rows", () => {
    const many = (n: number) =>
      Array.from({ length: n }, (_, i) =>
        cmd(`c.cmd${String(i).padStart(2, "0")}`, [{ name: `a${i}`, type: "int", required: true }, { name: "b", type: "float", min: 0, max: 1 }]),
      );
    it("60 items with args are sliced to 50 and the footer says 'showing 50 of 60'", () => {
      show(payload(many(60), 60));
      expect(rows().length).toBe(50);
      expect(p.doc.querySelectorAll("#pal-list .pal-arg").length).toBe(100);
      expect(p.$("pal-tot").textContent).toBe("showing 50 of 60");
    });
    it("total larger than shown rows (server-side cap) is reported", () => {
      show(payload(many(50), 256));
      expect(p.$("pal-tot").textContent).toBe("showing 50 of 256");
    });
    it("ArrowDown/Up move .sel across rows that contain args, with wrap", () => {
      show(payload(many(4)));
      expect(rows()[0].classList.contains("sel")).toBe(true);
      p.key(p.doc.body, "ArrowDown");
      expect(rows()[1].classList.contains("sel")).toBe(true);
      expect(rows()[0].classList.contains("sel")).toBe(false);
      p.key(p.doc.body, "ArrowUp");
      p.key(p.doc.body, "ArrowUp");
      expect(rows()[3].classList.contains("sel")).toBe(true);   // wrap up
      p.key(p.doc.body, "ArrowDown");
      expect(rows()[0].classList.contains("sel")).toBe(true);   // wrap down
      expect(rows().filter((r) => r.classList.contains("sel")).length).toBe(1);
    });
    it("changing the selection keeps every arg line in place", () => {
      show(payload(many(5)));
      for (let i = 0; i < 12; ++i) p.key(p.doc.body, "ArrowDown");
      expect(p.doc.querySelectorAll("#pal-list .pal-arg").length).toBe(10);
      for (const r of rows()) expect(r.querySelectorAll(".pal-arg").length).toBe(2);
      expect(rows().filter((r) => r.classList.contains("sel")).length).toBe(1);
    });
    it("selection can reach the 50th row; Enter then fills its actionText", () => {
      show(payload(many(60), 60));
      p.key(p.doc.body, "ArrowUp");   // wrap to last visible row
      expect(rows()[49].classList.contains("sel")).toBe(true);
      p.key(p.doc.body, "Enter");
      expect(input.value).toBe("c.cmd49");
    });
    it("hovering and clicking an argument line selects/activates its row", () => {
      show(payload(many(5)));
      const arg = rows()[3].querySelector(".pal-arg-meta")!;
      p.mousemove(arg);
      expect(rows()[3].classList.contains("sel")).toBe(true);
      p.click(arg);
      expect(input.value).toBe("c.cmd03");
      expect(p.$("pal-overlay").classList.contains("show")).toBe(false);
    });
    it("rows with and without args mix: selection indexes stay aligned to data-i", () => {
      show(payload([cmd("a.cmd"), cmd("b.cmd", [{ name: "x", type: "int" }]), cmd("c.cmd")]));
      expect(rows().map((r) => r.getAttribute("data-i"))).toEqual(["0", "1", "2"]);
      p.key(p.doc.body, "ArrowDown");
      p.key(p.doc.body, "ArrowDown");
      p.key(p.doc.body, "Enter");
      expect(input.value).toBe("c.cmd");
    });
    it("a re-render after selecting resets the selection to row 0 and keeps args", () => {
      show(payload(many(5)));
      p.key(p.doc.body, "ArrowDown");
      p.win.paletteResults(JSON.stringify(payload(many(5))));
      expect(rows()[0].classList.contains("sel")).toBe(true);
      expect(argLines(0).length).toBe(2);
    });
  });

  describe("golden files", () => {
    it("palette_results_commands_with_params.json: args rendered with golden-exact text", () => {
      show(golden("palette_results_commands_with_params.json"));
      const lines = argLines();
      expect(lines.map((l) => l.querySelector(".pal-arg-nm")!.textContent)).toEqual(["target", "radius?", "mode?"]);
      expect(lines[0].querySelector(".pal-arg-meta")!.textContent).toBe("string · required");
      expect(lines[0].querySelector(".pal-arg-ds")!.textContent).toBe("Entity <name> to spawn");
      expect(lines[1].querySelector(".pal-arg-meta")!.textContent).toBe("float · optional · m · 0.5..10");
      expect(lines[1].querySelector(".pal-arg-ds")!.textContent).toBe("Spawn radius");
      expect(lines[2].querySelector(".pal-arg-meta")!.textContent).toBe("enum · optional · fast|slow");
      expect(lines[2].querySelector(".pal-arg-ds")).toBeNull();
      expect(p.$("pal-list").querySelector("name")).toBeNull();
    });
    it("palette_results_commands_mixed_args.json: plain / params / noArgs / escaped rows", () => {
      show(golden("palette_results_commands_mixed_args.json"));
      const g = JSON.parse(golden("palette_results_commands_mixed_args.json"));
      expect(rows().length).toBe(4);
      expect(rows().map((r) => r.querySelectorAll(".pal-arg").length)).toEqual([0, 3, 0, 1]);
      expect(rows().map((r) => r.querySelector(".pal-nm")!.textContent)).toEqual(g.items.map((i: any) => i.name));
      // params row: escapes, newline/tab text, empty-token-free enum values
      const b = argLines(1);
      expect(b[0].querySelector(".pal-arg-ds")!.textContent).toBe('The "target" path C:\\x');
      expect(b[1].querySelector(".pal-arg-ds")!.textContent).toBe("Line1\nLine2\t<tab>");
      expect(b[1].querySelector(".pal-arg-meta")!.textContent).toBe("int · optional · items · 0..100");
      expect(b[2].querySelector(".pal-arg-meta")!.textContent).toBe("enum · optional · a|b&c|<d>");
      expect(rows()[1].querySelector("d")).toBeNull();
      // escaped row: control chars, quote in type, multibyte, html in description, exponent numbers
      const d = argLines(3)[0];
      expect(d.querySelector(".pal-arg-nm")!.textContent).toBe("ctl\u0001\u001f?");
      expect(d.querySelector(".pal-arg-meta")!.textContent).toBe('st"r · optional · \\ · -1e-7..10000000000');
      expect(d.querySelector(".pal-arg-ds")!.textContent).toBe("naïve ☃ </script>");
      expect(p.$("pal-list").querySelector("script")).toBeNull();
    });
    it("mixed golden: keyboard selection and Enter fill work on every row", () => {
      show(golden("palette_results_commands_mixed_args.json"));
      const g = JSON.parse(golden("palette_results_commands_mixed_args.json"));
      for (let i = 0; i < g.items.length; ++i) {
        p.key(p.doc.body, "Escape");
        show(golden("palette_results_commands_mixed_args.json"));
        for (let k = 0; k < i; ++k) p.key(p.doc.body, "ArrowDown");
        p.key(p.doc.body, "Enter");
        expect(input.value).toBe(g.items[i].actionText);
      }
    });
    it("every non-args golden still renders zero arg lines", () => {
      for (const f of ["palette_results_all.json", "palette_results_commands.json", "palette_results_domains.json"]) {
        show(golden(f));
        expect(p.doc.querySelectorAll("#pal-list .pal-args").length, f).toBe(0);
        p.key(p.doc.body, "Escape");
      }
    });
  });
});

describe("the two page copies agree on palArgsHtml", () => {
  it("palArgsHtml source is identical in both html files", () => {
    const src = (f: string) => {
      const t = fs.readFileSync(f, "utf-8").replace(/\r/g, "");
      const s = t.indexOf("function palArgsHtml");
      const e = t.indexOf("function palRender");
      return t.slice(s, e);
    };
    expect(src(PAGES[0][1]).length).toBeGreaterThan(100);
    expect(src(PAGES[1][1])).toBe(src(PAGES[0][1]));
  });
});
