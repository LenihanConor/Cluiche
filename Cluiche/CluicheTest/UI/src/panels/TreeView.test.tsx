import { render, screen, act } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { TreeView } from "./TreeView";
import { GameBridge } from "../bridge/GameBridge";

type Callback = (data: unknown) => void;
const subscriptions = new Map<string, Set<Callback>>();

vi.mock("../bridge/GameBridge", () => ({
  GameBridge: {
    subscribe: vi.fn((topic: string, cb: Callback) => {
      if (!subscriptions.has(topic)) subscriptions.set(topic, new Set());
      subscriptions.get(topic)!.add(cb);
      return () => subscriptions.get(topic)?.delete(cb);
    }),
    send: vi.fn(),
  },
}));

const TREE = {
  nodes: [
    {
      id: "root",
      label: "World",
      children: [
        { id: "c1", label: "Zone A", children: [] },
        { id: "c2", label: "Zone B", children: [
          { id: "c2a", label: "Room 1" },
        ]},
      ],
    },
  ],
};

function pushTree() {
  act(() => { subscriptions.get("tree_view")?.forEach((cb) => cb(TREE)); });
}

beforeEach(() => {
  subscriptions.clear();
  vi.mocked(GameBridge.send).mockClear();
});

describe("TreeView", () => {
  it("renders empty state before data arrives", () => {
    render(<TreeView />);
    expect(screen.getByText("Waiting for data…")).toBeInTheDocument();
  });

  it("renders root and child nodes", () => {
    render(<TreeView />);
    pushTree();
    expect(screen.getByText("World")).toBeInTheDocument();
    expect(screen.getByText("Zone A")).toBeInTheDocument();
    expect(screen.getByText("Room 1")).toBeInTheDocument();
  });

  it("sends onTreeNodeSelected when a node is clicked", async () => {
    render(<TreeView />);
    pushTree();
    await userEvent.click(screen.getByText("Zone A"));
    expect(GameBridge.send).toHaveBeenCalledWith("onTreeNodeSelected", { id: "c1" });
  });

  it("unsubscribes on unmount", () => {
    const { unmount } = render(<TreeView />);
    expect(subscriptions.get("tree_view")?.size).toBeGreaterThan(0);
    unmount();
    expect(subscriptions.get("tree_view")?.size ?? 0).toBe(0);
  });
});
