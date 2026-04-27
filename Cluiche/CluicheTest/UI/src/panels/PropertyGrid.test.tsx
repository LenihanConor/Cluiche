import { render, screen, act } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { PropertyGrid } from "./PropertyGrid";
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

const PROPS_DATA = {
  title: "Entity Properties",
  properties: [
    { key: "health",   label: "Health",   value: 100,     editable: true  },
    { key: "name",     label: "Name",     value: "Soldier", editable: true },
    { key: "faction",  label: "Faction",  value: "Blue",  editable: false },
  ],
};

function pushProps() {
  act(() => { subscriptions.get("property_grid")?.forEach((cb) => cb(PROPS_DATA)); });
}

beforeEach(() => {
  subscriptions.clear();
  vi.mocked(GameBridge.send).mockClear();
});

describe("PropertyGrid", () => {
  it("renders empty state before data arrives", () => {
    render(<PropertyGrid />);
    expect(screen.getByText("Waiting for data…")).toBeInTheDocument();
  });

  it("renders property labels and values", () => {
    render(<PropertyGrid />);
    pushProps();
    expect(screen.getByText("Health")).toBeInTheDocument();
    expect(screen.getByText("100")).toBeInTheDocument();
    expect(screen.getByText("Soldier")).toBeInTheDocument();
  });

  it("shows custom title from data", () => {
    render(<PropertyGrid />);
    pushProps();
    expect(screen.getByText("Entity Properties")).toBeInTheDocument();
  });

  it("clicking editable value opens an input", async () => {
    render(<PropertyGrid />);
    pushProps();
    await userEvent.click(screen.getByText("100"));
    expect(screen.getByDisplayValue("100")).toBeInTheDocument();
  });

  it("committing an edit sends onPropertyChanged", async () => {
    render(<PropertyGrid />);
    pushProps();
    await userEvent.click(screen.getByText("100"));
    const input = screen.getByDisplayValue("100");
    await userEvent.clear(input);
    await userEvent.type(input, "80");
    await userEvent.keyboard("{Enter}");
    expect(GameBridge.send).toHaveBeenCalledWith("onPropertyChanged", { key: "health", value: "80" });
  });

  it("unsubscribes on unmount", () => {
    const { unmount } = render(<PropertyGrid />);
    expect(subscriptions.get("property_grid")?.size).toBeGreaterThan(0);
    unmount();
    expect(subscriptions.get("property_grid")?.size ?? 0).toBe(0);
  });
});
