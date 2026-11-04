// DiaCellHistory Feature 2 (AC 3, 5, 6, 7, 8, 10, 11): the golden state of the REAL CellHistoryDomain
// (Cluiche/Tests/Contracts/debugpanel/domain_states_cellhistory*.json) merged with the real CellLayerViewerDomain golden, rendered by BOTH pages
// (CluicheTest and CoW). Layout reference: docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html.
// NOTE: runs in jsdom, not Ultralight's JavaScriptCore: proves page logic only.
import { describe, it, expect, beforeEach, afterEach, vi } from "vitest";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadPanel, type Panel } from "./loadPanel";

const here = path.dirname(fileURLToPath(import.meta.url));
const GOLDEN_DIR = path.resolve(here, "../../../../../Tests/Contracts/debugpanel");
const goldenJson = (name: string): any => JSON.parse(fs.readFileSync(path.join(GOLDEN_DIR, name), "utf-8"));
const PAGES: Record<string, string> = {
  CluicheTest: path.resolve(here, "../../../../../Assets/CluicheTest/Global/Presentation/UI/debug-panel.html"),
  CoW: path.resolve(here, "../../../../../Assets/CoW/Global/Presentation/UI/debug-panel.html"),
};

const HIST = "Cell History";

describe("contract: domain_states_cellhistory*.json shape", () => {
  for (const f of ["live", "scrubbed", "untracked"]) {
    it(`${f} carries the keys the card reads`, () => {
      const s = goldenJson(`domain_states_cellhistory_${f}.json`)[HIST];
      expect(s._id).toBe("cellhistory");
      expect(s._displayName).toBe(HIST);
      for (const k of ["available", "baseLayer", "banner", "history", "timeline"]) expect(s).toHaveProperty(k);
      for (const k of ["visible", "state", "layer", "sample", "label", "firstRetained"]) expect(s.banner).toHaveProperty(k);
      for (const k of ["range", "position", "liveMarker", "buckets", "bandTicks", "gaps", "preWindowDim", "sparkline"]) expect(s.timeline).toHaveProperty(k);
    });
  }
});

for (const [pageName, pageFile] of Object.entries(PAGES)) {
  describe(`history card on the ${pageName} page`, () => {
    let p: Panel;
    beforeEach(() => {
      vi.useFakeTimers();
      p = loadPanel(pageFile);
    });
    afterEach(() => {
      p.close();
      vi.useRealTimers();
    });

    const merged = (file: string, patch?: (h: any) => void): string => {
      const h = goldenJson(file);
      if (patch) patch(h[HIST]);
      return JSON.stringify({ ...goldenJson("domain_states_celllayers.json"), ...h });
    };
    const push = (file: string, patch?: (h: any) => void) => p.win.updateDomainState(merged(file, patch));
    const hcard = () => p.doc.querySelector(`.domain[data-domain="${HIST}"]`) as HTMLElement;
    const vroot = () => p.doc.querySelector("[data-role=cell-layer-viewer]") as HTMLElement;
    const tl = () => hcard().querySelector("[data-role=history-timeline]") as HTMLElement;
    const banner = () => vroot().querySelector("[data-role=history-banner]") as HTMLElement | null;
    const notice = () => vroot().querySelector("[data-role=history-notice]") as HTMLElement | null;
    const txt = (e: Element | null) => e?.textContent ?? "";
    const commands = () => p.calls.filter((c) => c[0] === "command");
    const act = (a: string) => tl().querySelector(`[data-act="${a}"]`) as HTMLButtonElement;

    describe("LIVE", () => {
      beforeEach(() => push("domain_states_cellhistory_live.json"));

      it("renders the timeline card beside the viewer card, open by default", () => {
        expect(hcard()).not.toBeNull();
        expect(hcard().classList.contains("open")).toBe(true);
        expect(tl()).not.toBeNull();
        expect(txt(tl().querySelector("[data-role=tl-layer]"))).toBe("temp");
      });

      it("shows no banner and no notice", () => {
        expect(banner()).toBeNull();
        expect(notice()).toBeNull();
        expect(vroot().querySelector(".sample-badge")).toBeNull();
      });

      it("puts the thumb on the live head and marks the card live", () => {
        expect(tl().classList.contains("is-live")).toBe(true);
        expect(txt(tl().querySelector("[data-role=tl-thumb-tag]"))).toBe("LIVE 5");
        expect((tl().querySelector("[data-role=tl-thumb]") as HTMLElement).style.left).toBe("100%");
        expect(txt(tl().querySelector("[data-role=tl-pos]"))).toContain("Following sim");
        expect(txt(hcard().querySelector("[data-role=headline]"))).toBe("live");
      });

      it("draws one coloured tick per event bucket from the bucket colour", () => {
        const ev = tl().querySelectorAll("[data-role=tl-event]") as NodeListOf<HTMLElement>;
        expect(ev.length).toBe(2);
        // golden colour 2661195519 = 0x9E9E9EFF -> rgb(158, 158, 158)
        expect(ev[0].style.background).toContain("158");
      });

      it("draws the band tick with its name, the sparkline path and the window line", () => {
        expect(tl().querySelectorAll("[data-role=tl-band]").length).toBe(1);
        expect(txt(tl().querySelector(".tl-band-label"))).toBe("Year");
        expect(tl().querySelector("[data-role=tl-spark] path")).not.toBeNull();
        expect(txt(tl().querySelector("[data-role=tl-info]"))).toContain("sparkline: 6 points");
      });

      it("has all seven controls and Back to live", () => {
        for (const a of ["step-band-back", "step-back", "step-fwd", "step-band-fwd", "event-prev", "event-next", "live"]) expect(act(a)).not.toBeNull();
      });

      it("rebuilds the timeline data only when it changes, not on every frame", () => {
        const before = tl().querySelector("[data-role=tl-event]");
        push("domain_states_cellhistory_live.json");
        expect(tl().querySelector("[data-role=tl-event]")).toBe(before);
      });
    });

    describe("controls send the history commands to the history domain id", () => {
      beforeEach(() => push("domain_states_cellhistory_scrubbed.json"));

      it("step buttons", () => {
        p.click(act("step-back"));
        p.click(act("step-fwd"));
        expect(commands()).toEqual([
          ["command", "cellhistory", "step", JSON.stringify({ n: -1 })],
          ["command", "cellhistory", "step", JSON.stringify({ n: 1 })],
        ]);
      });

      it("band buttons use the coarsest band index found on the timeline", () => {
        p.click(act("step-band-back"));
        p.click(act("step-band-fwd"));
        expect(commands()).toEqual([
          ["command", "cellhistory", "step", JSON.stringify({ n: -1, band: 1 })],
          ["command", "cellhistory", "step", JSON.stringify({ n: 1, band: 1 })],
        ]);
      });

      it("band buttons are disabled when the timeline has no band ticks", () => {
        p.win.updateDomainState(merged("domain_states_cellhistory_scrubbed.json", (h) => { h.timeline.bandTicks = []; }));
        expect(act("step-band-back").disabled).toBe(true);
        expect(act("step-fwd").disabled).toBe(false);
      });

      it("event buttons", () => {
        p.click(act("event-prev"));
        p.click(act("event-next"));
        expect(commands()).toEqual([
          ["command", "cellhistory", "event", JSON.stringify({ direction: "prev" })],
          ["command", "cellhistory", "event", JSON.stringify({ direction: "next" })],
        ]);
      });

      it("Back to live", () => {
        p.click(act("live"));
        expect(commands()).toEqual([["command", "cellhistory", "live", "{}"]]);
      });

      it("dragging the plot runs view, once per distinct sample", () => {
        const plot = tl().querySelector("[data-role=tl-plot]") as HTMLElement;
        (plot as any).getBoundingClientRect = () => ({ left: 100, width: 500, top: 0, height: 96, right: 600, bottom: 96 });
        const ev = (type: string, x: number) => plot.dispatchEvent(new p.win.MouseEvent(type, { bubbles: true, clientX: x }));
        ev("mousedown", 100);            // sample 0
        ev("mousemove", 101);            // still sample 0: not resent
        ev("mousemove", 350);            // half: sample 3 (round(2.5) = 3)
        ev("mouseup", 350);
        ev("mousemove", 600);            // not dragging: ignored
        expect(commands()).toEqual([
          ["command", "cellhistory", "view", JSON.stringify({ sample: 0 })],
          ["command", "cellhistory", "view", JSON.stringify({ sample: 3 })],
        ]);
      });
    });

    describe("SCRUBBED", () => {
      beforeEach(() => push("domain_states_cellhistory_scrubbed.json"));

      it("shows the banner with the sample, label and a Back to live button on the viewer card", () => {
        expect(banner()).not.toBeNull();
        expect(txt(banner())).toContain("Viewing sample 3");
        expect(txt(banner())).toContain("Y1 W4");
        expect(txt(banner())).toContain("not live");
        expect(notice()).toBeNull();
        p.click(banner()!.querySelector("[data-act=live]")!);
        expect(commands()).toEqual([["command", "cellhistory", "live", "{}"]]);
      });

      it("moves the thumb and describes the position", () => {
        expect(tl().classList.contains("is-live")).toBe(false);
        expect(txt(tl().querySelector("[data-role=tl-thumb-tag]"))).toBe("3");
        expect((tl().querySelector("[data-role=tl-thumb]") as HTMLElement).style.left).toBe("60%");
        expect(txt(tl().querySelector("[data-role=tl-pos]"))).toContain("Sample 3");
        expect(txt(hcard().querySelector("[data-role=headline]"))).toBe("sample 3");
      });

      it("puts the sample badge on the hover readout", () => {
        expect(txt(vroot().querySelector(".sample-badge"))).toBe("sample 3");
      });

      it("returns to live: the banner and the badge go away", () => {
        push("domain_states_cellhistory_live.json");
        expect(banner()).toBeNull();
        expect(vroot().querySelector(".sample-badge")).toBeNull();
      });
    });

    describe("NO PER-CELL HISTORY", () => {
      beforeEach(() => push("domain_states_cellhistory_untracked.json"));

      it("shows the banner extra and the explanation with a Track this layer button", () => {
        expect(txt(banner())).toContain("temp has no per-cell history");
        expect(notice()!.classList.contains("untracked")).toBe(true);
        expect(txt(notice())).toContain("No per-cell history for temp");
        expect(txt(notice())).toContain("Live values are never shown while scrubbed");
        expect(txt(notice())).toContain("runs: cells.history.track temp");
      });

      it("Track this layer runs track for the layer", () => {
        p.click(notice()!.querySelector("[data-act=track]")!);
        expect(commands()).toEqual([["command", "cellhistory", "track", JSON.stringify({ layer: "temp" })]]);
      });

      it("the notice goes away once the layer is tracked", () => {
        push("domain_states_cellhistory_scrubbed.json");
        expect(notice()).toBeNull();
        expect(banner()).not.toBeNull();
      });
    });

    describe("EVICTED", () => {
      beforeEach(() => push("domain_states_cellhistory_scrubbed.json", (h) => {
        h.banner.state = "evicted";
        h.banner.sample = 2;
        h.banner.firstRetained = 5;
        h.timeline.position.sample = 2;
        h.timeline.preWindowDim = { first: 0, last: 4 };
      }));

      it("shows the evicted notice with the window start and no Track button", () => {
        expect(notice()!.classList.contains("evicted")).toBe(true);
        expect(txt(notice())).toContain("Per-cell data evicted");
        expect(txt(notice())).toContain("window, which now starts at sample 5");
        expect(notice()!.querySelector("[data-act=track]")).toBeNull();
        expect(txt(banner())).toContain("per-cell data evicted");
      });

      it("dims the region before the per-cell window", () => {
        const dim = tl().querySelector("[data-role=tl-dim]") as HTMLElement;
        expect(dim).not.toBeNull();
        expect(dim.style.width).toBe("100%");
        expect(txt(dim)).toContain("no per-cell data before sample 5");
      });
    });

    describe("gaps", () => {
      it("draws aggregate and event gap bands with labels", () => {
        push("domain_states_cellhistory_scrubbed.json", (h) => {
          h.timeline.gaps = { aggregate: [{ first: 1, last: 2 }], cell: [], event: [{ first: 0, last: 1 }] };
        });
        expect(tl().querySelector("[data-role=tl-gap-aggregate]")).not.toBeNull();
        expect(tl().querySelector("[data-role=tl-gap-event]")).not.toBeNull();
        expect(txt(tl().querySelector("[data-role=tl-under]"))).toContain("aggregates evicted");
        expect(txt(tl().querySelector("[data-role=tl-under]"))).toContain("events evicted");
      });

      it("breaks the sparkline path at a missing value", () => {
        push("domain_states_cellhistory_scrubbed.json", (h) => {
          h.timeline.sparkline[2].value = null;
          h.timeline.sparkline[3].value = 41;
        });
        const d = tl().querySelector("[data-role=tl-spark] path")!.getAttribute("d")!;
        expect(d.split("M").length - 1).toBe(2);
      });
    });

    describe("no history domain", () => {
      it("shows no timeline and no banner", () => {
        p.win.updateDomainState(JSON.stringify(goldenJson("domain_states_celllayers.json")));
        expect(p.doc.querySelector(`.domain[data-domain="${HIST}"]`)).toBeNull();
        expect(banner()).toBeNull();
      });

      it("an unavailable history state (no timeline yet) renders as a plain card without throwing", () => {
        const s = goldenJson("domain_states_cellhistory_live.json");
        s[HIST] = { _id: "cellhistory", _displayName: HIST, _group: "Spatial", _description: "d", _accent: "#06b6d4", available: false };
        p.win.updateDomainState(JSON.stringify({ ...goldenJson("domain_states_celllayers.json"), ...s }));
        expect(tl()).toBeNull();
        expect(banner()).toBeNull();
      });
    });
  });
}
