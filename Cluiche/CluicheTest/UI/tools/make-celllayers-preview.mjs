// Builds a self-contained preview of the debug panel with the DiaCellLayers viewer card, for the visual check against
// docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html (Registry-and-viewer, AC 17).
// Usage: node tools/make-celllayers-preview.mjs [outFile] [--cow]
// It copies the REAL panel page (CluicheTest, or the CoW fork with --cow), installs a stub window.app BEFORE the page
// scripts, then feeds a panel contract golden of the real CellLayerViewerDomain. Clicks in the picker send commands to
// the stub, which only logs them (the console shows nothing; this is a static look-and-feel check).
// Open the output in a browser. URL hash picks the state: (none) float base, #category (multi-grid + category base),
// #empty (empty state), #nodata (view source not live, no data), #selection (a 4-cell selection, Pan tool active),
// #inspector (frozen pin A + a single cell: stats rows, A / delta columns, histogram, cell details), #inspector-multi (the same
// without the cell block, with a category layer and an "updating" mark), #toolarge (the same state with the "Selection too large to outline" banner, for the selection visual check). Headless screenshot:
//   msedge --headless=new --window-size=700,1000 --screenshot=<png> "file:///<out>#category"
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "../../..");
const cow = process.argv.includes("--cow");
const args = process.argv.slice(2).filter((a) => !a.startsWith("--"));
const pagePath = cow
  ? "Assets/CoW/Global/Presentation/UI/debug-panel.html"
  : "Assets/CluicheTest/Global/Presentation/UI/debug-panel.html";
const page = fs.readFileSync(path.join(root, pagePath), "utf-8").replace(/^﻿/, "");
const g = (n) => fs.readFileSync(path.join(root, "Tests/Contracts/debugpanel", n), "utf-8");
const out = args[0] || path.join(root, `out/preview/panel-celllayers-preview${cow ? "-cow" : ""}.html`);

const stub = `<script>
window.app = {
  onConsoleLine: function(){}, onConsoleComplete: function(){}, onConsoleHistory: function(){},
  onPaletteOpen: function(){}, onPaletteSearch: function(){},
  onCommand: function(){ console.log("command", Array.prototype.slice.call(arguments)); },
  onValueSchemaGet: function(){},
  PushInputMode: function(){}, PopInputMode: function(){}
};
</script>`;

const boot = `<script>
window.addEventListener("load", function() {
  var states = {
    "": ${g("domain_states_celllayers.json")},
    "#category": ${g("domain_states_celllayers_category.json")},
    "#empty": ${g("domain_states_celllayers_empty.json")},
    "#nodata": ${g("domain_states_celllayers_nodata.json")},
    "#selection": ${g("domain_states_celllayers_selection.json")},
    "#inspector": ${g("domain_states_celllayers_inspector.json")},
    "#inspector-multi": (function () {
      var s = ${g("domain_states_celllayers_inspector.json")};
      var i = s["Cell Layers"].inspector;
      delete i.cell; i.updating = true; i.expandedGroups.push("Terrain");
      i.layers.unshift({ id: "biome", schemaKey: "cells/biome", label: "Biome", unit: "", tooltip: "Dominant biome", kind: "category", group: "Terrain",
        hasData: true, count: 4, nanCount: 0, min: null, mean: null, max: null, other: 0,
        categories: [{ id: "grass", label: "Grass", count: 3 }, { id: "water", label: "Water", count: 1 }], a: { notFrozen: true } });
      return s;
    })(),
    "#toolarge": (function () {
      var s = ${g("domain_states_celllayers_selection.json")};
      var sel = s["Cell Layers"].selection;
      sel.count = 1120; sel.tool = "select"; sel.tooLarge = true; sel.tooLargeText = "Selection too large to outline (1120 cells)";
      return s;
    })()
  };
  window.updateDomainState(states[location.hash] || states[""]);
  var hdr = document.querySelector(".domain-hdr");
  if (hdr && !document.querySelector("[data-role=cell-layer-viewer]")) { hdr.click(); }
});
window.addEventListener("hashchange", function() { location.reload(); });
</script>`;

let html = page.replace(/<head[^>]*>/i, (m) => m + stub);
html = html.replace(/<\/body>/i, boot + "</body>");
fs.mkdirSync(path.dirname(out), { recursive: true });
fs.writeFileSync(out, html);
console.log("wrote " + out);
