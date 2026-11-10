// Loads the REAL Dia/DiaSceneEditor/UI/index.html into jsdom (no parent frame => stub bridge).
// NOTE: jsdom, not CEF: proves page logic only.
import { JSDOM } from "jsdom";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
export const PAGE = path.resolve(here, "../index.html");

export function loadScene(): { win: any; doc: Document; close: () => void } {
  const html = fs.readFileSync(PAGE, "utf-8").replace(/^﻿/, "");
  const dom = new JSDOM(html, {
    runScripts: "dangerously",
    pretendToBeVisual: true,
    url: "http://localhost/",
  });
  return { win: dom.window, doc: dom.window.document, close: () => dom.window.close() };
}
