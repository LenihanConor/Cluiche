import { render, screen, act } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { Modal } from "./Modal";
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

const MODAL_DATA = { title: "Delete Entity", message: "Are you sure?", confirmLabel: "Delete", cancelLabel: "Cancel" };

function pushModal() {
  act(() => { subscriptions.get("modal")?.forEach((cb) => cb(MODAL_DATA)); });
}

beforeEach(() => {
  subscriptions.clear();
  vi.mocked(GameBridge.send).mockClear();
});

describe("Modal", () => {
  it("modal is not visible before data arrives", () => {
    render(<Modal />);
    expect(screen.queryByText("Delete Entity")).not.toBeInTheDocument();
  });

  it("renders modal content when C++ pushes data", () => {
    render(<Modal />);
    pushModal();
    expect(screen.getByText("Delete Entity")).toBeInTheDocument();
    expect(screen.getByText("Are you sure?")).toBeInTheDocument();
  });

  it("sends onModalConfirm and closes on confirm click", async () => {
    render(<Modal />);
    pushModal();
    await userEvent.click(screen.getByText("Delete"));
    expect(GameBridge.send).toHaveBeenCalledWith("onModalConfirm", { title: "Delete Entity" });
    expect(screen.queryByText("Delete Entity")).not.toBeInTheDocument();
  });

  it("sends onModalCancel and closes on cancel click", async () => {
    render(<Modal />);
    pushModal();
    await userEvent.click(screen.getByText("Cancel"));
    expect(GameBridge.send).toHaveBeenCalledWith("onModalCancel", { title: "Delete Entity" });
    expect(screen.queryByText("Delete Entity")).not.toBeInTheDocument();
  });

  it("unsubscribes on unmount", () => {
    const { unmount } = render(<Modal />);
    expect(subscriptions.get("modal")?.size).toBeGreaterThan(0);
    unmount();
    expect(subscriptions.get("modal")?.size ?? 0).toBe(0);
  });
});
