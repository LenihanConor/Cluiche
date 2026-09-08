import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { renderHook, act } from "@testing-library/react";

let mockStatsResponse: unknown = { topics: [], editor_side: { messages_dropped: 0 } };
let mockAckResponse: unknown = { records: [], pending: [] };

vi.mock("../bridge/EditorBridge", () => ({
  EditorBridge: {
    request: vi.fn((cmd: string) => {
      if (cmd === "get_server_stats") return Promise.resolve(mockStatsResponse);
      if (cmd === "game_connection.get_ack_records") return Promise.resolve(mockAckResponse);
      return Promise.resolve(null);
    }),
  },
}));

import { useConnectionHealth, HealthBadge } from "./useConnectionHealth";

beforeEach(() => {
  vi.useFakeTimers();
  mockStatsResponse = { topics: [], editor_side: { messages_dropped: 0 } };
  mockAckResponse = { records: [], pending: [] };
});

afterEach(() => {
  vi.useRealTimers();
  vi.clearAllMocks();
});

describe("useConnectionHealth", () => {
  it("returns empty state when disconnected", () => {
    const { result } = renderHook(() => useConnectionHealth(false));
    expect(result.current.badge).toBe("none");
    expect(result.current.summary).toBe("");
    expect(result.current.topics).toEqual([]);
    expect(result.current.subscriptions).toEqual([]);
    expect(result.current.totalDropsInWindow).toBe(0);
  });

  it("polls immediately on connect", async () => {
    const { EditorBridge } = await import("../bridge/EditorBridge");
    const { result } = renderHook(() => useConnectionHealth(true));

    await act(async () => {
      await vi.advanceTimersByTimeAsync(0);
    });

    expect(EditorBridge.request).toHaveBeenCalledWith("get_server_stats", {});
    expect(EditorBridge.request).toHaveBeenCalledWith("game_connection.get_ack_records", {});
  });

  it("shows 'No active subscriptions' with empty topics", async () => {
    const { result } = renderHook(() => useConnectionHealth(true));

    await act(async () => {
      await vi.advanceTimersByTimeAsync(0);
    });

    expect(result.current.summary).toBe("No active subscriptions");
    expect(result.current.badge).toBe("none");
  });

  it("computes healthy topic count", async () => {
    mockStatsResponse = {
      topics: [
        { topic: "a", sent: 100, dropped: 0, drop_rate: 0 },
        { topic: "b", sent: 50, dropped: 0, drop_rate: 0 },
        { topic: "c", sent: 200, dropped: 5, drop_rate: 2.4 },
      ],
      editor_side: { messages_dropped: 0 },
    };

    const { result } = renderHook(() => useConnectionHealth(true));

    await act(async () => {
      await vi.advanceTimersByTimeAsync(0);
    });

    expect(result.current.summary).toBe("2 of 3 topics healthy");
    expect(result.current.topics).toHaveLength(3);
  });

  // --- Badge thresholds ---

  it("badge is none when drops < 10 in window", async () => {
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 5 } };

    const { result } = renderHook(() => useConnectionHealth(true));

    // First poll seeds prevTotalDrops (no delta on first poll)
    await act(async () => { await vi.advanceTimersByTimeAsync(0); });

    // Second poll: 5 new drops (total 10, delta = 5)
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 10 } };
    await act(async () => { await vi.advanceTimersByTimeAsync(2000); });

    expect(result.current.badge).toBe("none");
    expect(result.current.totalDropsInWindow).toBe(5);
  });

  it("badge is yellow when drops >= 10 in window", async () => {
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 0 } };

    const { result } = renderHook(() => useConnectionHealth(true));

    // First poll: seed
    await act(async () => { await vi.advanceTimersByTimeAsync(0); });

    // Second poll: 15 new drops
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 15 } };
    await act(async () => { await vi.advanceTimersByTimeAsync(2000); });

    expect(result.current.badge).toBe("yellow");
    expect(result.current.summary).toBe("Dropping messages");
  });

  it("badge is red when drops >= 100 in window", async () => {
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 0 } };

    const { result } = renderHook(() => useConnectionHealth(true));

    // First poll: seed
    await act(async () => { await vi.advanceTimersByTimeAsync(0); });

    // Second poll: 150 new drops
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 150 } };
    await act(async () => { await vi.advanceTimersByTimeAsync(2000); });

    expect(result.current.badge).toBe("red");
    expect(result.current.summary).toBe("Dropping messages");
  });

  // --- First-poll seeding (no badge flash) ---

  it("first poll after connect does not cause badge flash", async () => {
    // Game already has 500 lifetime drops when we connect
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 500 } };

    const { result } = renderHook(() => useConnectionHealth(true));

    await act(async () => { await vi.advanceTimersByTimeAsync(0); });

    // First poll should seed without computing a delta
    expect(result.current.badge).toBe("none");
    expect(result.current.totalDropsInWindow).toBe(0);
  });

  it("second poll after reconnect computes delta correctly", async () => {
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 500 } };

    const { result } = renderHook(() => useConnectionHealth(true));

    // First poll: seed at 500
    await act(async () => { await vi.advanceTimersByTimeAsync(0); });
    expect(result.current.totalDropsInWindow).toBe(0);

    // Second poll: 505 → delta = 5
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 505 } };
    await act(async () => { await vi.advanceTimersByTimeAsync(2000); });

    expect(result.current.totalDropsInWindow).toBe(5);
    expect(result.current.badge).toBe("none");
  });

  // --- Sliding window expiry ---

  it("drops expire from window after 5s", async () => {
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 0 } };

    const { result } = renderHook(() => useConnectionHealth(true));

    // First poll: seed
    await act(async () => { await vi.advanceTimersByTimeAsync(0); });

    // Second poll at 2s: 50 new drops → yellow
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 50 } };
    await act(async () => { await vi.advanceTimersByTimeAsync(2000); });
    expect(result.current.badge).toBe("yellow");

    // No more drops, wait 6s total (3 more polls at 2s intervals)
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 50 } };
    await act(async () => { await vi.advanceTimersByTimeAsync(2000); });
    await act(async () => { await vi.advanceTimersByTimeAsync(2000); });

    // The sample from t=2s should have expired from the 5s window by t=8s
    expect(result.current.totalDropsInWindow).toBe(0);
    expect(result.current.badge).toBe("none");
  });

  // --- Disconnect resets state ---

  it("resets state on disconnect", async () => {
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 0 } };

    const { result, rerender } = renderHook(
      ({ connected }) => useConnectionHealth(connected),
      { initialProps: { connected: true } }
    );

    // Get some state
    await act(async () => { await vi.advanceTimersByTimeAsync(0); });
    mockStatsResponse = { topics: [], editor_side: { messages_dropped: 20 } };
    await act(async () => { await vi.advanceTimersByTimeAsync(2000); });
    expect(result.current.badge).toBe("yellow");

    // Disconnect
    rerender({ connected: false });
    expect(result.current.badge).toBe("none");
    expect(result.current.summary).toBe("");
    expect(result.current.totalDropsInWindow).toBe(0);
  });

  // --- Subscription ACK data ---

  it("includes acked subscriptions", async () => {
    mockAckResponse = {
      records: [
        { topic: "entity.inspect", latency_ms: 12 },
        { topic: "observation.log", latency_ms: 8 },
      ],
      pending: [],
    };

    const { result } = renderHook(() => useConnectionHealth(true));

    await act(async () => { await vi.advanceTimersByTimeAsync(0); });

    expect(result.current.subscriptions).toHaveLength(2);
    expect(result.current.subscriptions[0]).toEqual({
      topic: "entity.inspect",
      status: "acked",
      latencyMs: 12,
    });
    expect(result.current.subscriptions[1]).toEqual({
      topic: "observation.log",
      status: "acked",
      latencyMs: 8,
    });
  });

  it("includes pending subscriptions", async () => {
    mockAckResponse = {
      records: [],
      pending: [{ topic: "observation.metric", elapsed_ms: 1500 }],
    };

    const { result } = renderHook(() => useConnectionHealth(true));

    await act(async () => { await vi.advanceTimersByTimeAsync(0); });

    expect(result.current.subscriptions).toHaveLength(1);
    expect(result.current.subscriptions[0]).toEqual({
      topic: "observation.metric",
      status: "pending",
      elapsedMs: 1500,
    });
  });

  it("marks timed-out subscriptions", async () => {
    mockAckResponse = {
      records: [],
      pending: [{ topic: "observation.trace", elapsed_ms: 3500 }],
    };

    const { result } = renderHook(() => useConnectionHealth(true));

    await act(async () => { await vi.advanceTimersByTimeAsync(0); });

    expect(result.current.subscriptions).toHaveLength(1);
    expect(result.current.subscriptions[0].status).toBe("timeout");
  });

  it("gracefully handles ack endpoint failure", async () => {
    mockAckResponse = null; // Will cause the .catch to fire

    const { EditorBridge } = await import("../bridge/EditorBridge");
    (EditorBridge.request as ReturnType<typeof vi.fn>).mockImplementation((cmd: string) => {
      if (cmd === "get_server_stats") return Promise.resolve({ topics: [], editor_side: { messages_dropped: 0 } });
      if (cmd === "game_connection.get_ack_records") return Promise.reject(new Error("not available"));
      return Promise.resolve(null);
    });

    const { result } = renderHook(() => useConnectionHealth(true));

    await act(async () => { await vi.advanceTimersByTimeAsync(0); });

    // Should still work — subscriptions just empty
    expect(result.current.subscriptions).toEqual([]);
    expect(result.current.badge).toBe("none");
  });
});
