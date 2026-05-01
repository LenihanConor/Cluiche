import { render, screen, act } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { DataTable } from "./DataTable";
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

const TABLE_DATA = {
  columns: [
    { key: "id",   label: "ID"   },
    { key: "name", label: "Name" },
    { key: "hp",   label: "HP"   },
  ],
  rows: [
    { id: "1", name: "Soldier", hp: 100 },
    { id: "2", name: "Archer",  hp: 75  },
  ],
};

function pushTable() {
  act(() => { subscriptions.get("data_table")?.forEach((cb) => cb(TABLE_DATA)); });
}

beforeEach(() => {
  subscriptions.clear();
  vi.mocked(GameBridge.send).mockClear();
});

describe("DataTable", () => {
  it("renders empty state before data arrives", () => {
    render(<DataTable />);
    expect(screen.getByText("Waiting for data…")).toBeInTheDocument();
  });

  it("renders columns and rows when data is pushed", () => {
    render(<DataTable />);
    pushTable();
    expect(screen.getByText("Name")).toBeInTheDocument();
    expect(screen.getByText("Soldier")).toBeInTheDocument();
    expect(screen.getByText("Archer")).toBeInTheDocument();
  });

  it("filters rows by text input", async () => {
    render(<DataTable />);
    pushTable();
    await userEvent.type(screen.getByPlaceholderText("filter…"), "Archer");
    expect(screen.queryByText("Soldier")).not.toBeInTheDocument();
    expect(screen.getByText("Archer")).toBeInTheDocument();
  });

  it("calls GameBridge.send with sort key when column header clicked", async () => {
    render(<DataTable />);
    pushTable();
    await userEvent.click(screen.getByText("Name"));
    expect(GameBridge.send).toHaveBeenCalledWith("onTableSort", expect.objectContaining({ key: "name" }));
  });

  it("unsubscribes from GameBridge on unmount", () => {
    const { unmount } = render(<DataTable />);
    expect(subscriptions.get("data_table")?.size).toBeGreaterThan(0);
    unmount();
    expect(subscriptions.get("data_table")?.size ?? 0).toBe(0);
  });
});
