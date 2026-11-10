// Builds a self-contained preview of the debug panel with the value tooltips, for the visual check against
// docs/specs/applications/cow/systems/f3-worldassets/mockups/field-schema-tooltips.html (Feature 3, AC 15).
// Usage: node tools/make-tooltip-preview.mjs [outFile]
// It copies the REAL panel page, installs a stub window.app BEFORE the page scripts, then feeds the panel contract
// golden (domain_states_schema.json) and answers onValueSchemaGet like the C++ bridge would.
// Open the output in a browser; hover or Tab to the (i) icons. Add #open to the URL to auto-open the first tooltip.
// Headless screenshot: msedge --headless=new --window-size=900,700 --screenshot=<png> "file:///<out>#open"
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "../../..");
const page = fs.readFileSync(path.join(root, "Assets/CluicheTest/Global/Presentation/UI/debug-panel.html"), "utf-8").replace(/^\uFEFF/, "");
const golden = fs.readFileSync(path.join(root, "Tests/Contracts/debugpanel/domain_states_schema.json"), "utf-8");
const out = process.argv[2] || path.join(root, "out/preview/panel-tooltip-preview.html");

const result = {
  version: 7,
  entries: [
    { for: "cow.ecology_params", key: "grazing.rate", kind: "parameter", label: "Grazing rate",
      description: "How much vegetation herbivores eat each season, relative to normal.",
      hint: "Below 0.5 herbivores barely dent the producers; above 1.5 they strip cells bare.",
      unit: "× per season", type: "float", min: 0, max: 2, default: 1.0,
      affects: ["producers_total", "group.herbivore"], related: ["growth.rate"],
      source: "A1 step 6 (producers)", ui: { group: "Herbivory", step: 0.05, decimals: 2 } },
    { for: "cow.ecology_params", key: "growth.rate", kind: "parameter", label: "Growth rate",
      description: "How fast plants regrow after being eaten.", type: "float", min: 0, max: 1,
      ui: { step: 0.01, decimals: 2 } },
  ],
};

const stub = `<script>
window.app = {
  onConsoleLine: function(){}, onConsoleComplete: function(){}, onConsoleHistory: function(){},
  onPaletteOpen: function(){}, onPaletteSearch: function(){}, onCommand: function(){},
  PushInputMode: function(){}, PopInputMode: function(){},
  onValueSchemaGet: function(prefix) {
    var r = ${JSON.stringify(result)};
    setTimeout(function(){ window.valueSchemaResult(r); }, 0);
  }
};
</script>`;

const boot = `<script>
window.addEventListener("load", function() {
  window.updateDomainState(${golden});
  var hdr = document.querySelector(".domain-hdr");
  if (hdr) { hdr.click(); }
  setTimeout(function() {
    var icon = document.querySelector(".info-icon");
    if (icon && location.hash === "#open") { icon.dispatchEvent(new MouseEvent("mouseenter")); }
  }, 100);
});
</script>`;

let html = page.replace(/<head[^>]*>/i, (m) => m + stub);
html = html.replace(/<\/body>/i, boot + "</body>");
fs.mkdirSync(path.dirname(out), { recursive: true });
fs.writeFileSync(out, html);
console.log("wrote " + out);
