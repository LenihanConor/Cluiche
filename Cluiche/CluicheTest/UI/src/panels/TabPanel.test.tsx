import { render, screen, act } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { TabPanel } from "./TabPanel";
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

const TABS = {
  tabs: [
    { id: "t1", label: "Overview",  content: "Overview content here" },
    { id: "t2", label: "Details",   content: "Details content here"  },
    { id: "t3", label: "Stats",     content: "Stats content here"    },
  ],
};

function pushTabs() {
  act(() => { subscriptions.get("tab_panel")?.forEach((cb) => cb(TABS)); });
}

beforeEach(() => {
  subscriptions.clear();
  vi.mocked(GameBridge.send).mockClear();
});

describe("TabPanel", () => {
  it("renders empty state before data arrives", () => {
    render(<TabPanel />);
    expect(screen.getByText("Waiting for data…")).toBeInTheDocument();
  });

  it("renders all tab labels when data pushed", () => {
    render(<TabPanel />);
    pushTabs();
    expect(screen.getByText("Overview")).toBeInTheDocument();
    expect(screen.getByText("Details")).toBeInTheDocument();
    expect(screen.getByText("Stats")).toBeInTheDocument();
  });

  it("shows first tab content by default", () => {
    render(<TabPanel />);
    pushTabs();
    expect(screen.getByText("Overview content here")).toBeInTheDocument();
  });

  it("switches content when a different tab is clicked", async () => {
    render(<TabPanel />);
    pushTabs();
    await userEvent.click(screen.getByText("Details"));
    expect(screen.getByText("Details content here")).toBeInTheDocument();
    expect(screen.queryByText("Overview content here")).not.toBeInTheDocument();
  });

  it("sends onTabSelected when a tab is clicked", async () => {
    render(<TabPanel />);
    pushTabs();
    await userEvent.click(screen.getByText("Stats"));
    expect(GameBridge.send).toHaveBeenCalledWith("onTabSelected", { id: "t3" });
  });

  it("unsubscribes on unmount", () => {
    const { unmount } = render(<TabPanel />);
    expect(subscriptions.get("tab_panel")?.size).toBeGreaterThan(0);
    unmount();
    expect(subscriptions.get("tab_panel")?.size ?? 0).toBe(0);
  });
});
