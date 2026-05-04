////////////////////////////////////////////////////////////////////////////////
// debug-layers.test.ts
// Vitest (jsdom) tests for the debug-layers IIFE panel script.
//
// The script is a plain IIFE with no import/export. We load it by reading the
// file contents and calling indirect eval — (0, eval)(script) — so it runs in
// the global scope and can find the DOM elements set up in beforeAll.
//
// DOM skeleton mirrors index.html exactly so that the getElementById calls
// inside the IIFE resolve correctly on first load.
////////////////////////////////////////////////////////////////////////////////
import { describe, it, expect, vi, beforeAll } from "vitest";
import { readFileSync } from "fs";
import { join, dirname } from "path";
import { fileURLToPath } from "url";

// ---------------------------------------------------------------------------
// Resolve path to the script under test
// ---------------------------------------------------------------------------
const __dirname = dirname(fileURLToPath(import.meta.url));
const scriptPath = join(
  __dirname,
  "../../../../../Dia/DiaEditor/Plugin/Assets/debug-layers/debug-layers.js"
);
const scriptContent = readFileSync(scriptPath, "utf-8");

// ---------------------------------------------------------------------------
// Helper: dispatch a "debug.layer.state" message to window
// ---------------------------------------------------------------------------
interface LayerEntry {
  name: string;
  enabled: boolean;
  priority: number;
}

interface LayerState {
  droppedCount?: number;
  layers?: LayerEntry[];
}

function dispatchLayerState(state: LayerState): void {
  window.dispatchEvent(
    new MessageEvent("message", {
      data: { __dia: true, topic: "debug.layer.state", data: state },
    })
  );
}

// ---------------------------------------------------------------------------
// DOM setup — must happen BEFORE the script is eval'd so getElementById works
// ---------------------------------------------------------------------------
beforeAll(() => {
  // Build the DOM skeleton that debug-layers.js expects
  document.body.innerHTML = `
    <span class="badge" id="drop-badge">0 dropped</span>
    <div class="overflow-banner" id="overflow-banner"></div>
    <p class="empty-msg" id="empty-msg">No layers registered.</p>
    <ul class="layer-list" id="layer-list" style="display:none"></ul>
  `;

  // Run the IIFE in the global scope (indirect eval)
  // eslint-disable-next-line no-eval
  (0, eval)(scriptContent);
});

// ---------------------------------------------------------------------------
// Convenience element accessors (resolved after DOM is set up)
// ---------------------------------------------------------------------------
function badge(): HTMLElement {
  return document.getElementById("drop-badge") as HTMLElement;
}
function banner(): HTMLElement {
  return document.getElementById("overflow-banner") as HTMLElement;
}
function list(): HTMLElement {
  return document.getElementById("layer-list") as HTMLElement;
}
function emptyMsg(): HTMLElement {
  return document.getElementById("empty-msg") as HTMLElement;
}

// ============================================================================
// Suite: applyLayerState — empty / non-empty layer list visibility
// ============================================================================

describe("applyLayerState — visibility", () => {
  it("applyLayerState_EmptyLayers_ShowsEmptyMessage", () => {
    dispatchLayerState({ layers: [] });
    expect(emptyMsg().style.display).not.toBe("none");
    expect(list().style.display).toBe("none");
  });

  it("applyLayerState_WithLayers_HidesEmptyMessage", () => {
    dispatchLayerState({
      layers: [{ name: "layer.a", enabled: true, priority: 0 }],
    });
    expect(list().style.display).not.toBe("none");
    expect(emptyMsg().style.display).toBe("none");
  });

  it("applyLayerState_WithLayers_RendersCheckboxes", () => {
    dispatchLayerState({
      layers: [
        { name: "layer.x", enabled: true, priority: 0 },
        { name: "layer.y", enabled: false, priority: 1 },
      ],
    });
    const checkboxes = list().querySelectorAll<HTMLInputElement>(
      "input[type=checkbox]"
    );
    expect(checkboxes.length).toBe(2);
  });

  it("applyLayerState_EnabledLayer_CheckboxChecked", () => {
    dispatchLayerState({
      layers: [{ name: "enabled.layer", enabled: true, priority: 0 }],
    });
    const cb = list().querySelector<HTMLInputElement>("input[type=checkbox]");
    expect(cb).not.toBeNull();
    expect(cb!.checked).toBe(true);
  });

  it("applyLayerState_DisabledLayer_CheckboxUnchecked", () => {
    dispatchLayerState({
      layers: [{ name: "disabled.layer", enabled: false, priority: 0 }],
    });
    const cb = list().querySelector<HTMLInputElement>("input[type=checkbox]");
    expect(cb).not.toBeNull();
    expect(cb!.checked).toBe(false);
  });
});

// ============================================================================
// Suite: applyLayerState — dropped-count badge
// ============================================================================

describe("applyLayerState — dropped badge", () => {
  it("applyLayerState_DroppedCount_ShowsBadge", () => {
    dispatchLayerState({ droppedCount: 5, layers: [] });
    expect(badge().className).toContain("visible");
    expect(badge().textContent).toContain("5");
  });

  it("applyLayerState_DroppedCountZero_HidesBadge", () => {
    dispatchLayerState({ droppedCount: 0, layers: [] });
    expect(badge().className).not.toContain("visible");
  });
});

// ============================================================================
// Suite: applyLayerState — sort order
// ============================================================================

describe("applyLayerState — sort order", () => {
  it("applyLayerState_SortsByPriority", () => {
    dispatchLayerState({
      layers: [
        { name: "z", priority: 10, enabled: true },
        { name: "a", priority: 5, enabled: true },
      ],
    });
    const spans = list().querySelectorAll<HTMLSpanElement>("span.layer-name");
    expect(spans.length).toBeGreaterThanOrEqual(1);
    // priority 5 ("a") must come before priority 10 ("z")
    expect(spans[0].textContent).toBe("a");
  });

  it("applyLayerState_SamePriority_SortedAlphabetically", () => {
    dispatchLayerState({
      layers: [
        { name: "z", priority: 0, enabled: true },
        { name: "a", priority: 0, enabled: true },
      ],
    });
    const spans = list().querySelectorAll<HTMLSpanElement>("span.layer-name");
    expect(spans[0].textContent).toBe("a");
  });
});

// ============================================================================
// Suite: checkbox toggle — sends commands to window.parent
// ============================================================================

describe("checkbox toggle — postMessage commands", () => {
  it("checkbox_EnabledToggle_SendsDisableCommand", () => {
    const spy = vi.spyOn(window.parent, "postMessage");

    dispatchLayerState({
      layers: [{ name: "toggle.layer", enabled: true, priority: 0 }],
    });

    const cb = list().querySelector<HTMLInputElement>("input[type=checkbox]");
    expect(cb).not.toBeNull();

    // Simulate the user unchecking the checkbox (layer is currently enabled)
    cb!.checked = false;
    cb!.dispatchEvent(new Event("change"));

    expect(spy).toHaveBeenCalledWith(
      expect.objectContaining({
        __diaFromFrame: true,
        payload: expect.objectContaining({
          type: "debug.layer.disable",
          data: expect.objectContaining({ name: "toggle.layer" }),
        }),
      }),
      "*"
    );

    spy.mockRestore();
  });

  it("checkbox_DisabledToggle_SendsEnableCommand", () => {
    const spy = vi.spyOn(window.parent, "postMessage");

    dispatchLayerState({
      layers: [{ name: "enable.layer", enabled: false, priority: 0 }],
    });

    const cb = list().querySelector<HTMLInputElement>("input[type=checkbox]");
    expect(cb).not.toBeNull();

    // Simulate the user checking the checkbox (layer is currently disabled)
    cb!.checked = true;
    cb!.dispatchEvent(new Event("change"));

    expect(spy).toHaveBeenCalledWith(
      expect.objectContaining({
        __diaFromFrame: true,
        payload: expect.objectContaining({
          type: "debug.layer.enable",
          data: expect.objectContaining({ name: "enable.layer" }),
        }),
      }),
      "*"
    );

    spy.mockRestore();
  });
});

// ============================================================================
// Suite: message filtering — non-Dia and wrong-topic messages are ignored
// ============================================================================

describe("message filtering", () => {
  it("message_NonDiaMessage_Ignored", () => {
    // Reset list to known empty state first
    dispatchLayerState({ layers: [] });
    const before = list().innerHTML;

    // Dispatch a message that looks like layer state but __dia is false
    window.dispatchEvent(
      new MessageEvent("message", {
        data: {
          __dia: false,
          topic: "debug.layer.state",
          data: { layers: [{ name: "injected", enabled: true, priority: 0 }] },
        },
      })
    );

    // DOM must not have changed
    expect(list().innerHTML).toBe(before);
  });

  it("message_WrongTopic_Ignored", () => {
    // Reset list to known empty state first
    dispatchLayerState({ layers: [] });
    const before = list().innerHTML;

    // Dispatch a Dia-flagged message but with a different topic
    window.dispatchEvent(
      new MessageEvent("message", {
        data: {
          __dia: true,
          topic: "other.topic",
          data: { layers: [{ name: "injected", enabled: true, priority: 0 }] },
        },
      })
    );

    // DOM must not have changed
    expect(list().innerHTML).toBe(before);
  });
});
