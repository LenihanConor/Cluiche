// Shared helper for the debug-panel Vitest suites.
// Loads the REAL Assets/CluicheTest/Global/Presentation/UI/debug-panel.html into
// jsdom (runScripts: 'dangerously') with a recording fake `window.app` installed
// BEFORE the page scripts run. NOTE: jsdom is not Ultralight's JavaScriptCore;
// these tests prove page logic only.
//
// Dev knob: set env PANEL_HTML_OVERRIDE=<path to html> to load a modified copy
// of the page instead of the real file (used for mutation sanity checks).
//
// jsdom stubs installed here:
//  - window.setTimeout/clearTimeout delegate to the Node globals so vi.useFakeTimers() controls the page debounces
//  - Element.prototype.scrollIntoView (no-op, jsdom lacks it)
//  - HTMLElement clientHeight/offsetHeight/offsetTop return 0 (jsdom has no layout; scrollTop is settable natively)
import { JSDOM } from "jsdom";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const REAL_PAGE = path.resolve(
  here,
  "../../../../../Assets/CluicheTest/Global/Presentation/UI/debug-panel.html",
);

export type Call = [string, ...any[]];

export interface Panel {
  win: any;
  doc: Document;
  calls: Call[];
  $: (id: string) => HTMLElement;
  key: (el: EventTarget, k: string, extra?: Record<string, any>) => KeyboardEvent;
  typeInto: (el: HTMLInputElement, text: string) => void;
  click: (el: Element) => void;
  mousemove: (el: Element) => void;
  last: (name: string) => Call | null;
  count: (name: string) => number;
  close: () => void;
}

export function loadPanel(): Panel {
  const file = process.env.PANEL_HTML_OVERRIDE || REAL_PAGE;
  const html = fs.readFileSync(file, "utf-8").replace(/^﻿/, "");
  const calls: Call[] = [];

  const dom = new JSDOM(html, {
    runScripts: "dangerously",
    pretendToBeVisual: true,
    url: "http://localhost/",
    beforeParse(window: any) {
      window.app = {
        onConsoleLine: (...a: any[]) => calls.push(["line", ...a]),
        onConsoleComplete: (t: string) => calls.push(["complete", t]),
        onConsoleHistory: (d: string) => calls.push(["hist", d]),
        onPaletteOpen: () => calls.push(["open"]),
        onPaletteSearch: (q: string, m: string) => calls.push(["search", q, m]),
        onCommand: (...a: any[]) => calls.push(["command", ...a]),
        PushInputMode: (m: string) => calls.push(["push", m]),
        PopInputMode: () => calls.push(["pop"]),
      };
      window.setTimeout = (...a: any[]) => (globalThis.setTimeout as any)(...a);
      window.clearTimeout = (id: any) => globalThis.clearTimeout(id);
      window.Element.prototype.scrollIntoView = function () {};
      for (const p of ["clientHeight", "offsetHeight", "offsetTop"]) {
        Object.defineProperty(window.HTMLElement.prototype, p, { configurable: true, get: () => 0 });
      }
    },
  });
  const win: any = dom.window;
  const doc: Document = win.document;

  return {
    win,
    doc,
    calls,
    $: (id) => doc.getElementById(id) as HTMLElement,
    key(el, k, extra = {}) {
      const ev = new win.KeyboardEvent("keydown", { key: k, bubbles: true, cancelable: true, ...extra });
      el.dispatchEvent(ev);
      return ev;
    },
    typeInto(el, text) {
      el.value = text;
      el.dispatchEvent(new win.Event("input", { bubbles: true }));
    },
    click: (el) => void el.dispatchEvent(new win.MouseEvent("click", { bubbles: true })),
    mousemove: (el) => void el.dispatchEvent(new win.MouseEvent("mousemove", { bubbles: true })),
    last(name) {
      for (let i = calls.length - 1; i >= 0; --i) if (calls[i][0] === name) return calls[i];
      return null;
    },
    count: (name) => calls.filter((c) => c[0] === name).length,
    close: () => win.close(),
  };
}
