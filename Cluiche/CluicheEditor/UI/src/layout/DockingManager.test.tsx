import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, act, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import React from "react";

// Mock EditorBridge before importing DockingManager so it doesn't touch window.dia
vi.mock("../bridge/EditorBridge", () => ({
  EditorBridge: {
    getPanels: vi.fn(),
    loadLayout: vi.fn(),
    saveLayout: vi.fn(),
    togglePanelVisibility: vi.fn(),
    subscribe: vi.fn(() => vi.fn()), // returns unsubscribe fn
  },
}));

// react-mosaic-component uses complex DOM behaviour; use a simple stub so tests
// focus on DockingManager's own logic.
vi.mock("react-mosaic-component", () => ({
  Mosaic: ({ renderTile, value }: any) => {
    const ids: string[] = [];
    function collect(node: any) {
      if (!node) return;
      if (typeof node === "string") { ids.push(node); return; }
      collect(node.first);
      collect(node.second);
    }
    collect(value);
    return (
      <div data-testid="mosaic">
        {ids.map((id) => (
          <div key={id} data-testid={`tile-${id}`}>
            {renderTile(id, [id])}
          </div>
        ))}
      </div>
    );
  },
  MosaicWindow: ({ children, title, toolbarControls }: any) => (
    <div data-testid={`window-${title}`}>
      <div data-testid={`controls-${title}`}>{toolbarControls}</div>
      {children}
    </div>
  ),
}));

// Stub Toolbar so it doesn't need EditorBridge.subscribe internally
vi.mock("./Toolbar", () => ({
  Toolbar: ({ panels }: any) => (
    <div data-testid="toolbar">
      {panels.map((p: any) => (
        <span key={p.name} data-testid={`toolbar-panel-${p.name}`} />
      ))}
    </div>
  ),
}));

import { EditorBridge } from "../bridge/EditorBridge";
import { DockingManager } from "./DockingManager";

// ── Pure algorithm tests (imported directly from module) ──────────────────────
// We can't easily re-export the private helpers, so we test them indirectly
// through the component's rendered output. Direct unit tests live below as
// integration-via-component tests.

const mockBridge = EditorBridge as unknown as {
  getPanels: ReturnType<typeof vi.fn>;
  loadLayout: ReturnType<typeof vi.fn>;
  saveLayout: ReturnType<typeof vi.fn>;
  togglePanelVisibility: ReturnType<typeof vi.fn>;
  subscribe: ReturnType<typeof vi.fn>;
};

function panelList(names: string[], allVisible = true) {
  return names.map((n) => ({ name: n, uiPath: `/panel/${n}`, visible: allVisible }));
}

function setupBridge(panels: ReturnType<typeof panelList>, savedTree?: unknown) {
  mockBridge.getPanels.mockResolvedValue({ panels });
  if (savedTree) {
    mockBridge.loadLayout.mockResolvedValue({ tree: savedTree });
  } else {
    mockBridge.loadLayout.mockResolvedValue({});
  }
  mockBridge.saveLayout.mockResolvedValue({});
}

beforeEach(() => {
  vi.clearAllMocks();
  mockBridge.subscribe.mockReturnValue(vi.fn());
});

// ── Rendering / initialisation ────────────────────────────────────────────────

describe("DockingManager – initialisation", () => {
  it("shows loading state before panels arrive", () => {
    mockBridge.getPanels.mockReturnValue(new Promise(() => {})); // never resolves
    render(<DockingManager />);
    expect(screen.getByText(/loading panels/i)).toBeInTheDocument();
  });

  it("renders mosaic with one panel when one visible panel is returned", async () => {
    setupBridge(panelList(["Console"]));
    render(<DockingManager />);
    await waitFor(() => expect(screen.getByTestId("mosaic")).toBeInTheDocument());
    expect(screen.getByTestId("tile-Console")).toBeInTheDocument();
  });

  it("renders mosaic with two panels when two visible panels are returned", async () => {
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => expect(screen.getByTestId("tile-Console")).toBeInTheDocument());
    expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument();
  });

  it("shows 'no panels' message when no visible panels", async () => {
    setupBridge(panelList(["Console"], false));
    render(<DockingManager />);
    await waitFor(() => expect(screen.getByText(/no editor panels/i)).toBeInTheDocument());
  });

  it("restores saved tree layout from loadLayout", async () => {
    const tree = { direction: "row", first: "Console", second: "Inspector", splitPercentage: 50 };
    setupBridge(panelList(["Console", "Inspector"]), tree);
    render(<DockingManager />);
    await waitFor(() => expect(screen.getByTestId("tile-Console")).toBeInTheDocument());
    expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument();
  });

  it("calls onReady after initialisation", async () => {
    const onReady = vi.fn();
    setupBridge(panelList(["Console"]));
    render(<DockingManager onReady={onReady} />);
    await waitFor(() => expect(onReady).toHaveBeenCalledTimes(1));
  });
});

// ── Panel close / fullscreen controls ────────────────────────────────────────

describe("DockingManager – panel controls", () => {
  it("close button calls togglePanelVisibility and removes tile", async () => {
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("window-Console"));

    const controls = screen.getByTestId("controls-Console");
    const closeBtn = controls.querySelector('button[title="Hide panel"]')!;
    await userEvent.click(closeBtn);

    expect(mockBridge.togglePanelVisibility).toHaveBeenCalledWith("Console");
    await waitFor(() =>
      expect(screen.queryByTestId("tile-Console")).not.toBeInTheDocument()
    );
  });

  it("fullscreen button collapses layout to single panel", async () => {
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("window-Console"));

    const controls = screen.getByTestId("controls-Console");
    const fsBtn = controls.querySelector('button[title="Fullscreen"]')!;
    await userEvent.click(fsBtn);

    // Only the fullscreened panel should be in the mosaic
    await waitFor(() =>
      expect(screen.queryByTestId("tile-Inspector")).not.toBeInTheDocument()
    );
    expect(screen.getByTestId("tile-Console")).toBeInTheDocument();
  });

  it("fullscreen exit restores previous layout", async () => {
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("window-Console"));

    // Enter fullscreen
    const controls = screen.getByTestId("controls-Console");
    const fsBtn = controls.querySelector('button[title="Fullscreen"]')!;
    await userEvent.click(fsBtn);
    await waitFor(() =>
      expect(screen.queryByTestId("tile-Inspector")).not.toBeInTheDocument()
    );

    // Exit fullscreen – button title changes to "Exit fullscreen"
    const exitBtn = screen.getByTitle("Exit fullscreen");
    await userEvent.click(exitBtn);
    await waitFor(() =>
      expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument()
    );
  });
});

// ── panels_changed subscription ───────────────────────────────────────────────

describe("DockingManager – panels_changed subscription", () => {
  it("subscribes to panels_changed on mount", async () => {
    setupBridge(panelList(["Console"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("mosaic"));
    expect(mockBridge.subscribe).toHaveBeenCalledWith("panels_changed", expect.any(Function));
  });

  it("adding a new visible panel via subscription updates the layout", async () => {
    let panelsChangedCb: ((data: unknown) => void) | undefined;
    mockBridge.subscribe.mockImplementation((topic: string, cb: (d: unknown) => void) => {
      if (topic === "panels_changed") panelsChangedCb = cb;
      return vi.fn();
    });
    setupBridge(panelList(["Console"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("tile-Console"));

    act(() => {
      panelsChangedCb!({
        panels: [
          { name: "Console", uiPath: "/console", visible: true },
          { name: "Inspector", uiPath: "/inspector", visible: true },
        ],
      });
    });

    await waitFor(() =>
      expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument()
    );
  });
});
