import React from "react";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, act, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";

vi.mock("../bridge/EditorBridge", () => ({
  EditorBridge: {
    getPanels: vi.fn(),
    loadLayout: vi.fn(),
    saveLayout: vi.fn(),
    togglePanelVisibility: vi.fn(),
    subscribe: vi.fn(() => vi.fn()),
    notify: vi.fn(),
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
        {ids.map(id => (
          <div key={id} data-testid={`tile-${id}`}>
            {renderTile(id, [id])}
          </div>
        ))}
      </div>
    );
  },
  MosaicWindow: ({ children, title, toolbarControls }: any) => {
    const titleText = typeof title === "string" ? title : title?.props?.children ?? "";
    return (
      <div data-testid={`window-${titleText}`}>
        <div data-testid={`controls-${titleText}`}>
          {title}
          {toolbarControls}
        </div>
        {children}
      </div>
    );
  },
}));

vi.mock("./Toolbar", () => ({
  Toolbar: ({ panels }: any) => (
    <div data-testid="toolbar">
      {panels.map((p: any) => <span key={p.name} data-testid={`toolbar-panel-${p.name}`} />)}
    </div>
  ),
}));

import { EditorBridge } from "../bridge/EditorBridge";
import { DockingManager } from "./DockingManager";

const mockBridge = EditorBridge as unknown as {
  getPanels: ReturnType<typeof vi.fn>;
  loadLayout: ReturnType<typeof vi.fn>;
  saveLayout: ReturnType<typeof vi.fn>;
  togglePanelVisibility: ReturnType<typeof vi.fn>;
  subscribe: ReturnType<typeof vi.fn>;
};

function panelList(names: string[], allVisible = true) {
  return names.map(n => ({ name: n, uiPath: `/panel/${n}`, visible: allVisible }));
}

function setupBridge(panels: ReturnType<typeof panelList>, savedTree?: unknown, savedTabGroups?: unknown) {
  mockBridge.getPanels.mockResolvedValue({ panels });
  if (savedTree) {
    mockBridge.loadLayout.mockResolvedValue({ tree: savedTree, tabGroups: savedTabGroups ?? {} });
  } else {
    mockBridge.loadLayout.mockResolvedValue({});
  }
  mockBridge.saveLayout.mockResolvedValue({});
}

beforeEach(() => {
  vi.clearAllMocks();
  mockBridge.subscribe.mockReturnValue(vi.fn());
});

// ── Initialisation ────────────────────────────────────────────────────────────

describe("DockingManager – initialisation", () => {
  it("shows loading state before panels arrive", () => {
    mockBridge.getPanels.mockReturnValue(new Promise(() => {}));
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

  it("restores tab groups from saved layout", async () => {
    const groupId = "tabgroup:abc123";
    const tree = { direction: "row", first: "Inspector", second: groupId, splitPercentage: 50 };
    const tg = { [groupId]: { name: "My Group", panels: ["Console", "Output"], activeId: "Console" } };
    setupBridge(panelList(["Console", "Inspector", "Output"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => expect(screen.getByTestId(`tile-${groupId}`)).toBeInTheDocument());
    expect(screen.getByText("My Group")).toBeInTheDocument();
  });

  it("calls onReady after initialisation", async () => {
    const onReady = vi.fn();
    setupBridge(panelList(["Console"]));
    render(<DockingManager onReady={onReady} />);
    await waitFor(() => expect(onReady).toHaveBeenCalledTimes(1));
  });
});

// ── Error paths ───────────────────────────────────────────────────────────────

describe("DockingManager – error paths", () => {
  it("shows empty layout when getPanels rejects", async () => {
    mockBridge.getPanels.mockRejectedValue(new Error("bridge unavailable"));
    render(<DockingManager />);
    await waitFor(() => expect(screen.getByText(/no editor panels/i)).toBeInTheDocument());
  });

  it("falls back to buildTree when loadLayout rejects", async () => {
    mockBridge.getPanels.mockResolvedValue({ panels: panelList(["Console", "Inspector"]) });
    mockBridge.loadLayout.mockRejectedValue(new Error("no saved layout"));
    mockBridge.saveLayout.mockResolvedValue({});
    render(<DockingManager />);
    await waitFor(() => expect(screen.getByTestId("tile-Console")).toBeInTheDocument());
    expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument();
  });

  it("falls back to buildTree when loadLayout returns no tree", async () => {
    mockBridge.getPanels.mockResolvedValue({ panels: panelList(["Console"]) });
    mockBridge.loadLayout.mockResolvedValue({});
    mockBridge.saveLayout.mockResolvedValue({});
    render(<DockingManager />);
    await waitFor(() => expect(screen.getByTestId("tile-Console")).toBeInTheDocument());
  });
});

// ── Panel controls ────────────────────────────────────────────────────────────

describe("DockingManager – panel controls", () => {
  it("close button calls togglePanelVisibility and removes tile", async () => {
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("window-Console"));
    const closeBtn = screen.getByTestId("controls-Console").querySelector('button[title="Hide panel"]')!;
    await userEvent.click(closeBtn);
    expect(mockBridge.togglePanelVisibility).toHaveBeenCalledWith("Console");
    await waitFor(() => expect(screen.queryByTestId("tile-Console")).not.toBeInTheDocument());
  });

  it("fullscreen button replaces mosaic with direct panel view", async () => {
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("window-Console"));
    const fsBtn = screen.getByTestId("controls-Console").querySelector('button[title="Fullscreen"]')!;
    await userEvent.click(fsBtn);
    await waitFor(() => expect(screen.queryByTestId("mosaic")).not.toBeInTheDocument());
    expect(screen.getByTitle("Exit fullscreen")).toBeInTheDocument();
    expect(screen.getByTestId("toolbar")).toBeInTheDocument();
  });

  it("fullscreen exit restores previous layout with all panels", async () => {
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("window-Console"));
    const fsBtn = screen.getByTestId("controls-Console").querySelector('button[title="Fullscreen"]')!;
    await userEvent.click(fsBtn);
    await waitFor(() => expect(screen.queryByTestId("mosaic")).not.toBeInTheDocument());
    const exitBtn = screen.getByTitle("Exit fullscreen");
    await userEvent.click(exitBtn);
    await waitFor(() => expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument());
    expect(screen.getByTestId("tile-Console")).toBeInTheDocument();
  });
});

// ── Tab group operations ──────────────────────────────────────────────────────

describe("DockingManager – tab groups", () => {
  it("right-click on panel title shows 'New tab group here' option", async () => {
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("window-Console"));
    const titleSpan = screen.getByTestId("controls-Console").closest("[data-testid^='window']")
      ?.querySelector("span") as Element;
    await userEvent.pointer({ target: titleSpan, keys: "[MouseRight]" });
    await waitFor(() => expect(screen.getByText("New tab group here")).toBeInTheDocument());
  });

  it("creating a tab group removes the panel leaf and adds a group tile", async () => {
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("tile-Console"));

    // Right-click the Console title span to open context menu
    const titleSpan = screen.getAllByText("Console")[0];
    await userEvent.pointer({ target: titleSpan, keys: "[MouseRight]" });
    await waitFor(() => screen.getByText("New tab group here"));

    await userEvent.click(screen.getByText("New tab group here"));

    // Console tile gone; a tabgroup tile appears
    await waitFor(() => expect(screen.queryByTestId("tile-Console")).not.toBeInTheDocument());
    const groupTiles = screen.getAllByTestId(/^tile-tabgroup:/);
    expect(groupTiles.length).toBeGreaterThan(0);
  });

  it("tab group tile shows the group name", async () => {
    const groupId = "tabgroup:saved1";
    const tree = { direction: "row", first: "Inspector", second: groupId, splitPercentage: 50 };
    const tg = { [groupId]: { name: "Dev Tools", panels: ["Console", "Output"], activeId: "Console" } };
    setupBridge(panelList(["Console", "Inspector", "Output"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => screen.getByText("Dev Tools"));
  });

  it("clicking + in tab group adds a loose panel into the group", async () => {
    const groupId = "tabgroup:g1";
    const tree = { direction: "row", first: groupId, second: "Inspector", splitPercentage: 50 };
    const tg = { [groupId]: { name: "Group 1", panels: ["Console"], activeId: "Console" } };
    setupBridge(panelList(["Console", "Inspector", "Output"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => screen.getByText("Group 1"));

    // Output is loose and ungrouped — should appear in + dropdown
    const addBtn = document.querySelector('button[title="Add panel to group"]')!;
    await userEvent.click(addBtn);
    await waitFor(() => screen.getByText("Output"));
    await userEvent.click(screen.getByText("Output"));

    // Output tab should now appear inside the group
    await waitFor(() => expect(screen.getAllByText("Output").length).toBeGreaterThan(0));
  });

  it("clicking × on a tab removes it from the group", async () => {
    const groupId = "tabgroup:g2";
    const tree = groupId;
    const tg = { [groupId]: { name: "Group 2", panels: ["Console", "Inspector"], activeId: "Console" } };
    setupBridge(panelList(["Console", "Inspector"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => screen.getByText("Console"));

    // find the × button next to Console tab
    const tabDivs = document.querySelectorAll('[data-testid="tile-tabgroup\\:g2"] div > div > div');
    // Click the × on the Console tab
    const consoleCross = screen.getAllByTitle("Remove from group")[0];
    await userEvent.click(consoleCross);

    // Inspector remains as active; Console is now a loose tile
    await waitFor(() => expect(screen.getByTestId("tile-Console")).toBeInTheDocument());
  });

  it("ungrouping (× on group title) returns all panels as loose tiles", async () => {
    const groupId = "tabgroup:g3";
    const tree = { direction: "row", first: groupId, second: "Inspector", splitPercentage: 50 };
    const tg = { [groupId]: { name: "Group 3", panels: ["Console", "Output"], activeId: "Console" } };
    setupBridge(panelList(["Console", "Inspector", "Output"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => screen.getByText("Group 3"));

    const ungroupBtn = screen.getByTitle("Ungroup panels");
    await userEvent.click(ungroupBtn);

    await waitFor(() => expect(screen.queryByText("Group 3")).not.toBeInTheDocument());
    await waitFor(() => expect(screen.getByTestId("tile-Console")).toBeInTheDocument());
    expect(screen.getByTestId("tile-Output")).toBeInTheDocument();
  });

  it("clicking a non-active tab switches the active iframe src", async () => {
    const groupId = "tabgroup:g4";
    const tree = groupId;
    const tg = { [groupId]: { name: "Group 4", panels: ["Console", "Inspector"], activeId: "Console" } };
    setupBridge(panelList(["Console", "Inspector"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => screen.getByTitle("Console")); // active iframe

    // Inspector tab is rendered but not active — click it
    const inspectorTab = screen.getAllByText("Inspector")
      .find(el => el.closest("div[style*='cursor: pointer']")) as Element;
    await userEvent.click(inspectorTab);

    // Inspector iframe is now active
    await waitFor(() => expect(screen.getByTitle("Inspector")).toBeInTheDocument());
  });

  it("removing the last tab in a group collapses the group tile", async () => {
    const groupId = "tabgroup:g5";
    const tree = { direction: "row", first: groupId, second: "Inspector", splitPercentage: 50 };
    const tg = { [groupId]: { name: "Group 5", panels: ["Console"], activeId: "Console" } };
    setupBridge(panelList(["Console", "Inspector"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => screen.getByText("Group 5"));

    const removeBtn = screen.getByTitle("Remove from group");
    await userEvent.click(removeBtn);

    await waitFor(() => expect(screen.queryByTestId(`tile-${groupId}`)).not.toBeInTheDocument());
    // Console returns as a loose tile
    expect(screen.getByTestId("tile-Console")).toBeInTheDocument();
  });

  it("saveLayout includes tabGroups when a group exists", async () => {
    const groupId = "tabgroup:g6";
    const tree = groupId;
    const tg = { [groupId]: { name: "Group 6", panels: ["Console"], activeId: "Console" } };
    setupBridge(panelList(["Console"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => screen.getByText("Group 6"));

    // saveLayout is called on init with the restored layout
    const calls = mockBridge.saveLayout.mock.calls;
    expect(calls.length).toBeGreaterThan(0);
    const lastCall = calls[calls.length - 1][0] as { tree: unknown; tabGroups: Record<string, unknown> };
    expect(lastCall).toHaveProperty("tabGroups");
    expect(Object.keys(lastCall.tabGroups)).toContain(groupId);
  });

  it("panels_changed removing a panel inside a group updates group membership", async () => {
    let panelsChangedCb: ((data: unknown) => void) | undefined;
    mockBridge.subscribe.mockImplementation((topic: string, cb: (d: unknown) => void) => {
      if (topic === "panels_changed") panelsChangedCb = cb;
      return vi.fn();
    });
    const groupId = "tabgroup:g7";
    const tree = groupId;
    const tg = { [groupId]: { name: "Group 7", panels: ["Console", "Output"], activeId: "Console" } };
    setupBridge(panelList(["Console", "Output"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => screen.getByText("Output")); // Output tab visible in group

    // Output panel is removed from the editor entirely
    act(() => {
      panelsChangedCb!({ panels: [{ name: "Console", uiPath: "/console", visible: true }] });
    });

    // Output tab should disappear from the group
    await waitFor(() => expect(screen.queryByText("Output")).not.toBeInTheDocument());
    // Console tab remains
    expect(screen.getAllByText("Console").length).toBeGreaterThan(0);
  });
});

  it("context menu shows 'Add to' options for every existing group", async () => {
    const groupId = "tabgroup:existing";
    const tree = { direction: "row", first: groupId, second: "Inspector", splitPercentage: 50 };
    const tg = { [groupId]: { name: "Dev Tools", panels: ["Console"], activeId: "Console" } };
    setupBridge(panelList(["Console", "Inspector"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => screen.getByText("Dev Tools"));

    // Right-click the loose Inspector panel
    const titleSpan = screen.getAllByText("Inspector")[0];
    await userEvent.pointer({ target: titleSpan, keys: "[MouseRight]" });
    await waitFor(() => screen.getByText(/Add to "Dev Tools"/));
    expect(screen.getByText("New tab group here")).toBeInTheDocument();
  });

  it("removing the active tab sets the previous tab as active", async () => {
    const groupId = "tabgroup:g8";
    const tree = groupId;
    const tg = { [groupId]: { name: "Group 8", panels: ["Console", "Inspector"], activeId: "Inspector" } };
    setupBridge(panelList(["Console", "Inspector"]), tree, tg);
    render(<DockingManager />);
    await waitFor(() => screen.getByTitle("Inspector")); // active iframe

    // Remove the active Inspector tab — it returns to the mosaic as a loose tile
    const removeBtns = screen.getAllByTitle("Remove from group");
    const inspectorRemove = removeBtns[1]; // second tab's × button
    await userEvent.click(inspectorRemove);

    // Console should now be the active iframe inside the group
    await waitFor(() => expect(screen.getByTitle("Console")).toBeInTheDocument());
    // Inspector is returned to the layout as a separate loose tile
    expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument();
  });

  it("panels_changed toggledOff removes the panel from the tree", async () => {
    let panelsChangedCb: ((data: unknown) => void) | undefined;
    mockBridge.subscribe.mockImplementation((topic: string, cb: (d: unknown) => void) => {
      if (topic === "panels_changed") panelsChangedCb = cb;
      return vi.fn();
    });
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("tile-Inspector"));

    act(() => {
      panelsChangedCb!({
        panels: [
          { name: "Console",   uiPath: "/console",   visible: true  },
          { name: "Inspector", uiPath: "/inspector", visible: false },
        ],
      });
    });

    await waitFor(() => expect(screen.queryByTestId("tile-Inspector")).not.toBeInTheDocument());
    expect(screen.getByTestId("tile-Console")).toBeInTheDocument();
  });

  it("panels_changed toggledOn adds the panel back to the tree", async () => {
    let panelsChangedCb: ((data: unknown) => void) | undefined;
    mockBridge.subscribe.mockImplementation((topic: string, cb: (d: unknown) => void) => {
      if (topic === "panels_changed") panelsChangedCb = cb;
      return vi.fn();
    });
    setupBridge(panelList(["Console", "Inspector"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("tile-Inspector"));

    // Toggle Inspector off then on
    act(() => {
      panelsChangedCb!({ panels: [
        { name: "Console",   uiPath: "/console",   visible: true  },
        { name: "Inspector", uiPath: "/inspector", visible: false },
      ]});
    });
    await waitFor(() => expect(screen.queryByTestId("tile-Inspector")).not.toBeInTheDocument());

    act(() => {
      panelsChangedCb!({ panels: [
        { name: "Console",   uiPath: "/console",   visible: true },
        { name: "Inspector", uiPath: "/inspector", visible: true },
      ]});
    });
    await waitFor(() => expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument());
  });

  it("dangling tabgroup leaf in saved tree is removed on load", async () => {
    const orphanGroupId = "tabgroup:orphan";
    // tree references a group that has no entry in tabGroups
    const tree = { direction: "row", first: "Console", second: orphanGroupId, splitPercentage: 50 };
    setupBridge(panelList(["Console"]), tree, {}); // empty tabGroups
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("tile-Console"));
    // orphan tile should not appear
    expect(screen.queryByTestId(`tile-${orphanGroupId}`)).not.toBeInTheDocument();
  });

  it("group referencing a removed panel is pruned on load", async () => {
    const groupId = "tabgroup:stale";
    // tree is just the stale group — no valid panels remain after pruning, so layout collapses
    const tree = groupId;
    const tg = { [groupId]: { name: "Stale Group", panels: ["Ghost"], activeId: "Ghost" } };
    setupBridge(panelList(["Console"]), tree, tg);
    render(<DockingManager />);
    // The stale group tile is pruned; safeTree becomes null → "no panels" message
    await waitFor(() => expect(screen.queryByText("Stale Group")).not.toBeInTheDocument());
    expect(screen.queryByTestId(`tile-${groupId}`)).not.toBeInTheDocument();
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
    await waitFor(() => expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument());
  });

  it("removing all panels via subscription collapses layout to null", async () => {
    let panelsChangedCb: ((data: unknown) => void) | undefined;
    mockBridge.subscribe.mockImplementation((topic: string, cb: (d: unknown) => void) => {
      if (topic === "panels_changed") panelsChangedCb = cb;
      return vi.fn();
    });
    setupBridge(panelList(["Console"]));
    render(<DockingManager />);
    await waitFor(() => screen.getByTestId("tile-Console"));
    act(() => { panelsChangedCb!({ panels: [] }); });
    await waitFor(() => expect(screen.queryByTestId("mosaic")).not.toBeInTheDocument());
    expect(screen.getByText(/no editor panels/i)).toBeInTheDocument();
  });
});
