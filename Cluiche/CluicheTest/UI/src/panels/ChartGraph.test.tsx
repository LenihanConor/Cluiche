import { render, screen, act } from "@testing-library/react";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { ChartGraph } from "./ChartGraph";

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

const CHART_DATA = {
  series: [
    { label: "FPS",    color: "#60a5fa", points: [60, 59, 61, 58, 62] },
    { label: "Memory", color: "#34d399", points: [120, 121, 119, 122, 120] },
  ],
};

function pushChart() {
  act(() => { subscriptions.get("chart_graph")?.forEach((cb) => cb(CHART_DATA)); });
}

beforeEach(() => {
  subscriptions.clear();
});

describe("ChartGraph", () => {
  it("renders empty state before data arrives", () => {
    render(<ChartGraph />);
    expect(screen.getByText("Waiting for data…")).toBeInTheDocument();
  });

  it("renders series labels when data pushed", () => {
    render(<ChartGraph />);
    pushChart();
    expect(screen.getByText("FPS")).toBeInTheDocument();
    expect(screen.getByText("Memory")).toBeInTheDocument();
  });

  it("renders an SVG polyline per series", () => {
    const { container } = render(<ChartGraph />);
    pushChart();
    const polylines = container.querySelectorAll("polyline");
    expect(polylines.length).toBe(2);
  });

  it("shows latest value for each series", () => {
    render(<ChartGraph />);
    pushChart();
    expect(screen.getByText("62.0")).toBeInTheDocument();
    expect(screen.getByText("120.0")).toBeInTheDocument();
  });

  it("unsubscribes on unmount", () => {
    const { unmount } = render(<ChartGraph />);
    expect(subscriptions.get("chart_graph")?.size).toBeGreaterThan(0);
    unmount();
    expect(subscriptions.get("chart_graph")?.size ?? 0).toBe(0);
  });
});
