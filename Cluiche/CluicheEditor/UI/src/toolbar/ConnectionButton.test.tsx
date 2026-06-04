import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, act, fireEvent } from "@testing-library/react";
import userEvent from "@testing-library/user-event";

let gameConnectionCallback: ((data: unknown) => void) | null = null;
let mockRequestResolve: (value: unknown) => void = () => {};

vi.mock("../bridge/EditorBridge", () => ({
  EditorBridge: {
    subscribe: vi.fn((topic: string, cb: (d: unknown) => void) => {
      if (topic === "game_connection") gameConnectionCallback = cb;
      return vi.fn();
    }),
    request: vi.fn(() => new Promise((resolve) => { mockRequestResolve = resolve; })),
  },
}));

import { EditorBridge } from "../bridge/EditorBridge";
import { ConnectionButton } from "./ConnectionButton";

beforeEach(() => {
  vi.clearAllMocks();
  gameConnectionCallback = null;
  mockRequestResolve = () => {};
  (EditorBridge.subscribe as ReturnType<typeof vi.fn>).mockImplementation(
    (topic: string, cb: (d: unknown) => void) => {
      if (topic === "game_connection") gameConnectionCallback = cb;
      return vi.fn();
    }
  );
  (EditorBridge.request as ReturnType<typeof vi.fn>).mockResolvedValue(null);
});

describe("ConnectionButton – initial state", () => {
  it("shows Disconnected by default", () => {
    render(<ConnectionButton />);
    expect(screen.getByText("Disconnected")).toBeInTheDocument();
  });

  it("requests get_state on mount", () => {
    render(<ConnectionButton />);
    expect(EditorBridge.request).toHaveBeenCalledWith("game_connection.get_state", {});
  });

  it("subscribes to game_connection topic on mount", () => {
    render(<ConnectionButton />);
    expect(EditorBridge.subscribe).toHaveBeenCalledWith("game_connection", expect.any(Function));
  });
});

describe("ConnectionButton – state transitions", () => {
  it("shows Connected when topic fires with state=connected", () => {
    render(<ConnectionButton />);
    act(() => { gameConnectionCallback!({ state: "connected" }); });
    expect(screen.getByText("Connected")).toBeInTheDocument();
  });

  it("shows Connecting... when topic fires with state=connecting", () => {
    render(<ConnectionButton />);
    act(() => { gameConnectionCallback!({ state: "connecting" }); });
    expect(screen.getByText("Connecting...")).toBeInTheDocument();
  });

  it("shows Disconnected when topic fires with state=disconnected", () => {
    render(<ConnectionButton />);
    act(() => { gameConnectionCallback!({ state: "connected" }); });
    act(() => { gameConnectionCallback!({ state: "disconnected" }); });
    expect(screen.getByText("Disconnected")).toBeInTheDocument();
  });
});

describe("ConnectionButton – dropdown", () => {
  it("dropdown is closed by default", () => {
    render(<ConnectionButton />);
    expect(screen.queryByPlaceholderText("ws://localhost:9002")).not.toBeInTheDocument();
  });

  it("opens dropdown on button click", async () => {
    render(<ConnectionButton />);
    await userEvent.click(screen.getByTitle(/Game connection/i));
    expect(screen.getByPlaceholderText("ws://localhost:9002")).toBeInTheDocument();
  });

  it("shows Connect button in disconnected dropdown", async () => {
    render(<ConnectionButton />);
    await userEvent.click(screen.getByTitle(/Game connection/i));
    expect(screen.getByText("Connect")).toBeInTheDocument();
  });

  it("shows Disconnect button when connected", async () => {
    render(<ConnectionButton />);
    act(() => { gameConnectionCallback!({ state: "connected" }); });
    await userEvent.click(screen.getByTitle(/Game connection/i));
    expect(screen.getByText("Disconnect")).toBeInTheDocument();
  });
});

describe("ConnectionButton – connect/disconnect actions", () => {
  it("sends connect request with URL when Connect clicked", async () => {
    render(<ConnectionButton />);
    await userEvent.click(screen.getByTitle(/Game connection/i));
    fireEvent.click(screen.getByText("Connect"));
    expect(EditorBridge.request).toHaveBeenCalledWith(
      "game_connection.connect",
      expect.objectContaining({ url: "ws://localhost:9002" })
    );
  });

  it("sends disconnect request when Disconnect clicked", async () => {
    render(<ConnectionButton />);
    act(() => { gameConnectionCallback!({ state: "connected" }); });
    await userEvent.click(screen.getByTitle(/Game connection/i));
    fireEvent.click(screen.getByText("Disconnect"));
    expect(EditorBridge.request).toHaveBeenCalledWith("game_connection.disconnect", {});
  });

  it("closes dropdown after Connect", async () => {
    render(<ConnectionButton />);
    await userEvent.click(screen.getByTitle(/Game connection/i));
    fireEvent.click(screen.getByText("Connect"));
    expect(screen.queryByPlaceholderText("ws://localhost:9002")).not.toBeInTheDocument();
  });

  it("closes dropdown after Disconnect", async () => {
    render(<ConnectionButton />);
    act(() => { gameConnectionCallback!({ state: "connected" }); });
    await userEvent.click(screen.getByTitle(/Game connection/i));
    fireEvent.click(screen.getByText("Disconnect"));
    expect(screen.queryByText("Disconnect")).not.toBeInTheDocument();
  });
});
