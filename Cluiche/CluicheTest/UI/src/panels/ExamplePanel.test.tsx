import { render, screen, act } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { ExamplePanel } from "./ExamplePanel";
import { GameBridge } from "../bridge/GameBridge";

// Capture subscribe callbacks so tests can push data in.
// Multiple components may subscribe to the same topic — store all of them.
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

const ENTITIES = [
  { id: "e1", name: "Soldier", pos: "10,20", state: "idle" },
  { id: "e2", name: "Archer",  pos: "30,40", state: "moving" },
];

function pushEntities() {
  act(() => {
    subscriptions.get("entities")?.forEach((cb) => cb(ENTITIES));
  });
}

beforeEach(() => {
  subscriptions.clear();
  vi.mocked(GameBridge.send).mockClear();
});

describe("ExamplePanel", () => {
  it("renders empty entity list before C++ data arrives", () => {
    render(<ExamplePanel />);
    expect(screen.queryByText("Soldier")).not.toBeInTheDocument();
    expect(screen.getByText("Nothing selected")).toBeInTheDocument();
  });

  it("renders entities when C++ pushes data via GameBridge", () => {
    render(<ExamplePanel />);
    pushEntities();
    expect(screen.getByText("Soldier")).toBeInTheDocument();
    expect(screen.getByText("Archer")).toBeInTheDocument();
  });

  it("DetailPanel shows 'Nothing selected' when no entity is clicked", () => {
    render(<ExamplePanel />);
    pushEntities();
    expect(screen.getByText("Nothing selected")).toBeInTheDocument();
  });

  it("clicking an entity calls GameBridge.send with the correct payload", async () => {
    render(<ExamplePanel />);
    pushEntities();

    await userEvent.click(screen.getByText("Soldier"));

    expect(GameBridge.send).toHaveBeenCalledOnce();
    expect(GameBridge.send).toHaveBeenCalledWith("onEntitySelected", { id: "e1" });
  });

  it("DetailPanel shows selected entity properties after click — cross-panel state test", async () => {
    render(<ExamplePanel />);
    pushEntities();

    await userEvent.click(screen.getByText("Soldier"));

    expect(screen.getByText("ID: e1")).toBeInTheDocument();
    expect(screen.getByText("Position: 10,20")).toBeInTheDocument();
    expect(screen.getByText("State: idle")).toBeInTheDocument();
    expect(screen.queryByText("Nothing selected")).not.toBeInTheDocument();
  });

  it("selecting a different entity updates DetailPanel — context re-render test", async () => {
    render(<ExamplePanel />);
    pushEntities();

    await userEvent.click(screen.getByText("Soldier"));
    expect(screen.getByText("ID: e1")).toBeInTheDocument();

    await userEvent.click(screen.getByText("Archer"));
    expect(screen.getByText("ID: e2")).toBeInTheDocument();
    expect(screen.getByText("Position: 30,40")).toBeInTheDocument();
    expect(screen.queryByText("ID: e1")).not.toBeInTheDocument();
  });

  it("unsubscribes from GameBridge on unmount", () => {
    const { unmount } = render(<ExamplePanel />);
    expect(subscriptions.get("entities")?.size).toBeGreaterThan(0);
    unmount();
    expect(subscriptions.get("entities")?.size ?? 0).toBe(0);
  });
});
