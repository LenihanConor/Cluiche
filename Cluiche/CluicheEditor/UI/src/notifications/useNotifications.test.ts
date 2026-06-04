import { describe, it, expect, vi, beforeEach } from "vitest";

// Must be declared before the store module is imported so the top-level
// setToastDispatch() call and the EditorBridge window side-effects are
// both intercepted before module evaluation.
vi.mock("../bridge/EditorBridge", () => ({
  setToastDispatch: vi.fn(),
}));

import { useNotificationStore } from "./useNotifications";

beforeEach(() => {
  useNotificationStore.setState({ toasts: [], queue: [] });
});

// ---------------------------------------------------------------------------
// addToast — default durations
// ---------------------------------------------------------------------------

describe("addToast – default durations", () => {
  it('applies default duration of 4 for level "info" when durationSeconds is omitted', () => {
    useNotificationStore.getState().addToast({
      id: "t1",
      level: "info",
      title: "Info toast",
    });

    const { toasts } = useNotificationStore.getState();
    expect(toasts).toHaveLength(1);
    expect(toasts[0].durationSeconds).toBe(4);
  });

  it('applies default duration of 3 for level "success"', () => {
    useNotificationStore.getState().addToast({
      id: "t1",
      level: "success",
      title: "Success toast",
    });

    const { toasts } = useNotificationStore.getState();
    expect(toasts[0].durationSeconds).toBe(3);
  });

  it('applies default duration of 6 for level "warning"', () => {
    useNotificationStore.getState().addToast({
      id: "t1",
      level: "warning",
      title: "Warning toast",
    });

    const { toasts } = useNotificationStore.getState();
    expect(toasts[0].durationSeconds).toBe(6);
  });

  it('applies default duration of 0 for level "error"', () => {
    useNotificationStore.getState().addToast({
      id: "t1",
      level: "error",
      title: "Error toast",
    });

    const { toasts } = useNotificationStore.getState();
    expect(toasts[0].durationSeconds).toBe(0);
  });

  it("preserves an explicit durationSeconds when provided and non-zero", () => {
    useNotificationStore.getState().addToast({
      id: "t1",
      level: "info",
      title: "Custom duration toast",
      durationSeconds: 10,
    });

    const { toasts } = useNotificationStore.getState();
    expect(toasts[0].durationSeconds).toBe(10);
  });
});

// ---------------------------------------------------------------------------
// addToast — queue overflow
// ---------------------------------------------------------------------------

describe("addToast – queue overflow", () => {
  it("fills toasts up to MAX_VISIBLE (5) with an empty queue", () => {
    for (let i = 0; i < 5; i++) {
      useNotificationStore.getState().addToast({
        id: `t${i}`,
        level: "info",
        title: `Toast ${i}`,
      });
    }

    const { toasts, queue } = useNotificationStore.getState();
    expect(toasts).toHaveLength(5);
    expect(queue).toHaveLength(0);
  });

  it("overflows the 6th toast into the queue", () => {
    for (let i = 0; i < 6; i++) {
      useNotificationStore.getState().addToast({
        id: `t${i}`,
        level: "info",
        title: `Toast ${i}`,
      });
    }

    const { toasts, queue } = useNotificationStore.getState();
    expect(toasts).toHaveLength(5);
    expect(queue).toHaveLength(1);
    expect(queue[0].id).toBe("t5");
  });
});

// ---------------------------------------------------------------------------
// removeToast — basic
// ---------------------------------------------------------------------------

describe("removeToast – basic removal", () => {
  it("removes the toast with the matching id from toasts", () => {
    useNotificationStore.getState().addToast({ id: "a", level: "info", title: "A" });
    useNotificationStore.getState().addToast({ id: "b", level: "info", title: "B" });

    useNotificationStore.getState().removeToast("a");

    const { toasts } = useNotificationStore.getState();
    expect(toasts).toHaveLength(1);
    expect(toasts[0].id).toBe("b");
  });

  it("makes no change when the id is not found", () => {
    useNotificationStore.getState().addToast({ id: "a", level: "info", title: "A" });

    useNotificationStore.getState().removeToast("unknown-id");

    const { toasts } = useNotificationStore.getState();
    expect(toasts).toHaveLength(1);
    expect(toasts[0].id).toBe("a");
  });
});

// ---------------------------------------------------------------------------
// removeToast — queue promotion
// ---------------------------------------------------------------------------

describe("removeToast – queue promotion", () => {
  it("shifts the first queued item into toasts when a toast is removed", () => {
    // Fill toasts to MAX_VISIBLE then add one more to the queue
    for (let i = 0; i < 6; i++) {
      useNotificationStore.getState().addToast({
        id: `t${i}`,
        level: "info",
        title: `Toast ${i}`,
      });
    }

    // Remove one visible toast; the queued item should promote
    useNotificationStore.getState().removeToast("t0");

    const { toasts, queue } = useNotificationStore.getState();
    expect(toasts).toHaveLength(5);
    expect(queue).toHaveLength(0);
    expect(toasts.some((t) => t.id === "t5")).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// dismissAll
// ---------------------------------------------------------------------------

describe("dismissAll", () => {
  it("clears both toasts and queue", () => {
    for (let i = 0; i < 6; i++) {
      useNotificationStore.getState().addToast({
        id: `t${i}`,
        level: "error",
        title: `Toast ${i}`,
      });
    }

    useNotificationStore.getState().dismissAll();

    const { toasts, queue } = useNotificationStore.getState();
    expect(toasts).toHaveLength(0);
    expect(queue).toHaveLength(0);
  });
});
