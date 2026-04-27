import { render, screen, act } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { Timeline } from "./Timeline";
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

const TIMELINE_DATA = {
  totalTime: 100,
  items: [
    { id: "a1", label: "Move",   start: 0,  end: 40, color: "#60a5fa", track: 0 },
    { id: "a2", label: "Attack", start: 50, end: 80, color: "#f87171", track: 0 },
    { id: "a3", label: "Idle",   start: 10, end: 90, color: "#34d399", track: 1 },
  ],
};

function pushTimeline() {
  act(() => { subscriptions.get("timeline")?.forEach((cb) => cb(TIMELINE_DATA)); });
}

beforeEach(() => {
  subscriptions.clear();
  vi.mocked(GameBridge.send).mockClear();
});

describe("Timeline", () => {
  it("renders empty state before data arrives", () => {
    render(<Timeline />);
    expect(screen.getByText("Waiting for data…")).toBeInTheDocument();
  });

  it("renders an SVG with rects when data pushed", () => {
    const { container } = render(<Timeline />);
    pushTimeline();
    const rects = container.querySelectorAll("rect");
    expect(rects.length).toBeGreaterThanOrEqual(3);
  });

  it("sends onTimelineItemSelected when a rect is clicked", async () => {
    const { container } = render(<Timeline />);
    pushTimeline();
    const groups = container.querySelectorAll("g[class*='cursor-pointer']");
    await userEvent.click(groups[0] as HTMLElement);
    expect(GameBridge.send).toHaveBeenCalledWith("onTimelineItemSelected", expect.objectContaining({ id: expect.any(String) }));
  });

  it("unsubscribes on unmount", () => {
    const { unmount } = render(<Timeline />);
    expect(subscriptions.get("timeline")?.size).toBeGreaterThan(0);
    unmount();
    expect(subscriptions.get("timeline")?.size ?? 0).toBe(0);
  });
});
