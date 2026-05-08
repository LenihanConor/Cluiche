import { render, screen, act } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { Accordion } from "./Accordion";
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

const ACCORDION_DATA = {
  sections: [
    { id: "s1", title: "Movement",  content: "Move speed and direction settings" },
    { id: "s2", title: "Combat",    content: "Attack, defence, and ability stats" },
    { id: "s3", title: "Inventory", content: "Equipped items and carrying capacity" },
  ],
};

function pushAccordion() {
  act(() => { subscriptions.get("accordion")?.forEach((cb) => cb(ACCORDION_DATA)); });
}

beforeEach(() => {
  subscriptions.clear();
  vi.mocked(GameBridge.send).mockClear();
});

describe("Accordion", () => {
  it("renders empty state before data arrives", () => {
    render(<Accordion />);
    expect(screen.getByText("Waiting for data…")).toBeInTheDocument();
  });

  it("renders all section titles when data pushed", () => {
    render(<Accordion />);
    pushAccordion();
    expect(screen.getByText("Movement")).toBeInTheDocument();
    expect(screen.getByText("Combat")).toBeInTheDocument();
    expect(screen.getByText("Inventory")).toBeInTheDocument();
  });

  it("sends onAccordionToggled when a section is opened", async () => {
    render(<Accordion />);
    pushAccordion();
    const radios = document.querySelectorAll('input[name="accordion"]');
    await userEvent.click(radios[0] as HTMLElement);
    expect(GameBridge.send).toHaveBeenCalledWith("onAccordionToggled", expect.objectContaining({ id: "s1" }));
  });

  it("unsubscribes on unmount", () => {
    const { unmount } = render(<Accordion />);
    expect(subscriptions.get("accordion")?.size).toBeGreaterThan(0);
    unmount();
    expect(subscriptions.get("accordion")?.size ?? 0).toBe(0);
  });
});
