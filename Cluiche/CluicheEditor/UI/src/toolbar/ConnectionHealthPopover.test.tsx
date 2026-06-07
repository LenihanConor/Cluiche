import { describe, it, expect, vi } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { ConnectionHealthPopover } from "./ConnectionHealthPopover";
import { ConnectionHealth } from "./useConnectionHealth";

function makeHealth(overrides: Partial<ConnectionHealth> = {}): ConnectionHealth {
  return {
    badge: "none",
    summary: "",
    topics: [],
    subscriptions: [],
    totalDropsInWindow: 0,
    ...overrides,
  };
}

describe("ConnectionHealthPopover", () => {
  it("renders header", () => {
    render(<ConnectionHealthPopover health={makeHealth()} onClose={vi.fn()} />);
    expect(screen.getByText("Connection Health")).toBeInTheDocument();
  });

  it("shows 'No active subscriptions' when topics empty", () => {
    render(<ConnectionHealthPopover health={makeHealth()} onClose={vi.fn()} />);
    expect(screen.getByText("No active subscriptions")).toBeInTheDocument();
  });

  it("shows 'No subscriptions' when subscriptions empty", () => {
    render(<ConnectionHealthPopover health={makeHealth()} onClose={vi.fn()} />);
    expect(screen.getByText("No subscriptions")).toBeInTheDocument();
  });

  it("renders topic rows with data", () => {
    const health = makeHealth({
      topics: [
        { topic: "entity.inspect", sent: 142, dropped: 0, dropRate: 0 },
        { topic: "observation.metric", sent: 4012, dropped: 87, dropRate: 2.12 },
      ],
    });

    render(<ConnectionHealthPopover health={health} onClose={vi.fn()} />);

    expect(screen.getByText("entity.inspect")).toBeInTheDocument();
    expect(screen.getByText("observation.metric")).toBeInTheDocument();
    expect(screen.getByText("142")).toBeInTheDocument();
    expect(screen.getByText("4,012")).toBeInTheDocument();
    expect(screen.getByText("87")).toBeInTheDocument();
    expect(screen.getByText("0.0%")).toBeInTheDocument();
    expect(screen.getByText("2.1%")).toBeInTheDocument();
  });

  // --- Subscription ACK display ---

  it("renders acked subscription with latency", () => {
    const health = makeHealth({
      subscriptions: [
        { topic: "entity.inspect", status: "acked", latencyMs: 12 },
      ],
    });

    render(<ConnectionHealthPopover health={health} onClose={vi.fn()} />);

    expect(screen.getByText("entity.inspect")).toBeInTheDocument();
    expect(screen.getByText(/ACK \(12ms\)/)).toBeInTheDocument();
  });

  it("renders pending subscription", () => {
    const health = makeHealth({
      subscriptions: [
        { topic: "observation.log", status: "pending", elapsedMs: 1500 },
      ],
    });

    render(<ConnectionHealthPopover health={health} onClose={vi.fn()} />);

    expect(screen.getByText("observation.log")).toBeInTheDocument();
    expect(screen.getByText(/Pending \(1500ms\)/)).toBeInTheDocument();
  });

  it("renders timeout subscription", () => {
    const health = makeHealth({
      subscriptions: [
        { topic: "observation.trace", status: "timeout", elapsedMs: 3500 },
      ],
    });

    render(<ConnectionHealthPopover health={health} onClose={vi.fn()} />);

    expect(screen.getByText("observation.trace")).toBeInTheDocument();
    expect(screen.getByText(/No ACK \(timeout 3s\)/)).toBeInTheDocument();
  });

  // --- Dismiss behavior ---

  it("calls onClose when close button clicked", () => {
    const onClose = vi.fn();
    render(<ConnectionHealthPopover health={makeHealth()} onClose={onClose} />);

    fireEvent.click(screen.getByText("✕"));
    expect(onClose).toHaveBeenCalledTimes(1);
  });

  it("calls onClose on Escape key", () => {
    const onClose = vi.fn();
    render(<ConnectionHealthPopover health={makeHealth()} onClose={onClose} />);

    fireEvent.keyDown(document, { key: "Escape" });
    expect(onClose).toHaveBeenCalledTimes(1);
  });

  it("calls onClose on click outside", () => {
    const onClose = vi.fn();
    render(
      <div>
        <div data-testid="outside">Outside</div>
        <ConnectionHealthPopover health={makeHealth()} onClose={onClose} />
      </div>
    );

    fireEvent.mouseDown(screen.getByTestId("outside"));
    expect(onClose).toHaveBeenCalledTimes(1);
  });

  it("does NOT call onClose on click inside", () => {
    const onClose = vi.fn();
    render(<ConnectionHealthPopover health={makeHealth()} onClose={onClose} />);

    fireEvent.mouseDown(screen.getByText("Connection Health"));
    expect(onClose).not.toHaveBeenCalled();
  });
});
