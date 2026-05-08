import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, act, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import React from "react";

// ── Shared mock state for the bridge ────────────────────────────────────────
let panelsChangedCb: ((data: unknown) => void) | null = null;

vi.mock("../../bridge/EditorBridge", () => ({
  EditorBridge: {
    getPanels: vi.fn(),
    loadLayout: vi.fn(),
    saveLayout: vi.fn().mockResolvedValue({}),
    togglePanelVisibility: vi.fn(),
    subscribe: vi.fn((topic: string, cb: (d: unknown) => void) => {
      if (topic === "panels_changed") panelsChangedCb = cb;
      return vi.fn();
    }),
  },
}));

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

import { EditorBridge } from "../../bridge/EditorBridge";
import { DockingManager } from "../../layout/DockingManager";

const mockBridge = EditorBridge as unknown as {
  getPanels: ReturnType<typeof vi.fn>;
  loadLayout: ReturnType<typeof vi.fn>;
  saveLayout: ReturnType<typeof vi.fn>;
  togglePanelVisibility: ReturnType<typeof vi.fn>;
  subscribe: ReturnType<typeof vi.fn>;
};

function makePanel(name: string, visible = true) {
  return { name, uiPath: `/panel/${name.toLowerCase()}`, visible };
}

beforeEach(() => {
  vi.clearAllMocks();
  panelsChangedCb = null;
  mockBridge.subscribe.mockImplementation((topic: string, cb: (d: unknown) => void) => {
    if (topic === "panels_changed") panelsChangedCb = cb;
    return vi.fn();
  });
  mockBridge.loadLayout.mockResolvedValue({});
  mockBridge.saveLayout.mockResolvedValue({});
});

describe("DockingManager + Toolbar – integration", () => {
  it("renders toolbar panel buttons matching loaded panels", async () => {
    mockBridge.getPanels.mockResolvedValue({
      panels: [makePanel("Console"), makePanel("Inspector")],
    });
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("mosaic"));
    // iframes also carry a title attribute, so use getAllByTitle
    expect(screen.getAllByTitle("Console").length).toBeGreaterThan(0);
    expect(screen.getAllByTitle("Inspector").length).toBeGreaterThan(0);
  });

  it("clicking toolbar toggle calls togglePanelVisibility", async () => {
    mockBridge.getPanels.mockResolvedValue({
      panels: [makePanel("Console"), makePanel("Inspector")],
    });
    render(<DockingManager />);
    await waitFor(() => screen.getAllByTitle("Console"));

    // Target the button (role=button) specifically to avoid matching the iframe
    const toolbarBtn = screen
      .getAllByTitle("Console")
      .find((el) => el.tagName === "BUTTON")!;
    await userEvent.click(toolbarBtn);
    expect(mockBridge.togglePanelVisibility).toHaveBeenCalledWith("Console");
  });

  it("panels_changed subscription adds a new panel to the mosaic and toolbar", async () => {
    mockBridge.getPanels.mockResolvedValue({
      panels: [makePanel("Console")],
    });
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("tile-Console"));

    act(() => {
      panelsChangedCb!({
        panels: [makePanel("Console"), makePanel("Inspector")],
      });
    });

    await waitFor(() =>
      expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument()
    );
    // Toolbar button also reflects the new panel (iframes also get a title so use getAllByTitle)
    expect(screen.getAllByTitle("Inspector").length).toBeGreaterThan(0);
  });

  it("panels_changed removes a panel that is no longer in the list", async () => {
    mockBridge.getPanels.mockResolvedValue({
      panels: [makePanel("Console"), makePanel("Inspector")],
    });
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("tile-Inspector"));

    act(() => {
      panelsChangedCb!({ panels: [makePanel("Console")] });
    });

    await waitFor(() =>
      expect(screen.queryByTestId("tile-Inspector")).not.toBeInTheDocument()
    );
  });

  it("fullscreen via mosaic toolbar hides other panels", async () => {
    mockBridge.getPanels.mockResolvedValue({
      panels: [makePanel("Console"), makePanel("Inspector")],
    });
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("window-Console"));

    const fsBtn = screen.getByTestId("controls-Console")
      .querySelector('button[title="Fullscreen"]')!;
    await userEvent.click(fsBtn);

    await waitFor(() =>
      expect(screen.queryByTestId("tile-Inspector")).not.toBeInTheDocument()
    );
    expect(screen.getByTestId("tile-Console")).toBeInTheDocument();
  });
});
