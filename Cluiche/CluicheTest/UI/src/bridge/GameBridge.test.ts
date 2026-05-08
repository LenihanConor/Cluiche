import { describe, it, expect, vi, beforeEach } from "vitest";
import { GameBridge } from "./GameBridge";

// GameBridge.ts registers window.GameBridge_dispatch as a side effect on import.
// Tests rely on that registration being present — do not reset modules between tests.

beforeEach(() => {
  // Reset window.app to a clean state before each test
  (window as Window & { app?: object }).app = undefined;
});

describe("GameBridge — bridge contract tests", () => {
  it("Test 1: dispatches C++ data to React subscribers (push pattern)", () => {
    const received: unknown[] = [];
    const unsub = GameBridge.subscribe("entities", (data) => received.push(data));

    GameBridge.dispatch("entities", [{ id: "e1", name: "Soldier", pos: "10,20", state: "idle" }]);

    expect(received).toHaveLength(1);
    expect((received[0] as { id: string }[])[0].id).toBe("e1");
    unsub();
  });

  it("Test 2: sends message to C++ via window.app (JS → C++)", () => {
    const mockMethod = vi.fn();
    (window as Window & { app: object }).app = { onEntitySelected: mockMethod };

    GameBridge.send("onEntitySelected", { id: "e1" });

    expect(mockMethod).toHaveBeenCalledOnce();
    expect(mockMethod).toHaveBeenCalledWith(JSON.stringify({ id: "e1" }));
  });

  it("Test 3: window.GameBridge_dispatch parses JSON and routes to subscribers (C++ → React)", () => {
    const received: unknown[] = [];
    const unsub = GameBridge.subscribe("fps", (data) => received.push(data));

    window.GameBridge_dispatch("fps", JSON.stringify({ value: 60 }));

    expect(received).toHaveLength(1);
    expect((received[0] as { value: number }).value).toBe(60);
    unsub();
  });

  it("Test 4: subscriber unsubscribe stops receiving updates", () => {
    const received: unknown[] = [];
    const unsub = GameBridge.subscribe("test_topic", (data) => received.push(data));

    GameBridge.dispatch("test_topic", { x: 1 });
    unsub();
    GameBridge.dispatch("test_topic", { x: 2 });

    expect(received).toHaveLength(1);
  });

  it("Test 5: send warns and does not throw when window.app is unavailable", () => {
    const warn = vi.spyOn(console, "warn").mockImplementation(() => {});

    expect(() => GameBridge.send("onEntitySelected", { id: "e1" })).not.toThrow();
    expect(warn).toHaveBeenCalled();

    warn.mockRestore();
  });

  it("Test 6: GameBridge_dispatch handles malformed JSON without throwing", () => {
    const warn = vi.spyOn(console, "warn").mockImplementation(() => {});

    expect(() => window.GameBridge_dispatch("fps", "not valid json{{")).not.toThrow();
    expect(warn).toHaveBeenCalled();

    warn.mockRestore();
  });
});
