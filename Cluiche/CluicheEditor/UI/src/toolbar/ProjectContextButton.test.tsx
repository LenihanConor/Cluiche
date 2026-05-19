import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, act, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";

// Capture callbacks registered via EditorBridge.subscribe / request.
let projectChangedCallback: ((data: unknown) => void) | null = null;
let pendingRequestResolve: ((val: unknown) => void) | null = null;

vi.mock("../bridge/EditorBridge", () => ({
  EditorBridge: {
    subscribe: vi.fn((topic: string, cb: (d: unknown) => void) => {
      if (topic === "project_changed") projectChangedCallback = cb;
      return vi.fn();
    }),
    request: vi.fn(() =>
      new Promise((resolve) => {
        pendingRequestResolve = resolve;
      })
    ),
  },
}));

import { EditorBridge } from "../bridge/EditorBridge";
import { ProjectContextButton } from "./ProjectContextButton";

const mockRequest = EditorBridge.request as ReturnType<typeof vi.fn>;

beforeEach(() => {
  vi.clearAllMocks();
  projectChangedCallback = null;
  pendingRequestResolve = null;

  (EditorBridge.subscribe as ReturnType<typeof vi.fn>).mockImplementation(
    (topic: string, cb: (d: unknown) => void) => {
      if (topic === "project_changed") projectChangedCallback = cb;
      return vi.fn();
    }
  );

  mockRequest.mockImplementation(
    () =>
      new Promise((resolve) => {
        pendingRequestResolve = resolve;
      })
  );
});

// ---------------------------------------------------------------------------
// No-project state
// ---------------------------------------------------------------------------

describe("ProjectContextButton – no project", () => {
  it("shows 'No project' when no project_changed event has fired", () => {
    render(<ProjectContextButton />);
    expect(screen.getByText("No project")).toBeInTheDocument();
  });

  it("button is dimmed (colour #666666) when no project is open", () => {
    render(<ProjectContextButton />);
    const btn = screen.getByTitle("No project open");
    expect(btn).toHaveStyle({ color: "#666666" });
  });

  it("does not render a green dot when no project is open", () => {
    render(<ProjectContextButton />);
    const btn = screen.getByTitle("No project open");
    // No span with the live-dot colour should appear inside the button.
    const spans = btn.querySelectorAll("span");
    const hasGreenDot = Array.from(spans).some(
      (s) => (s as HTMLElement).style.background === "rgb(137, 209, 133)"
    );
    expect(hasGreenDot).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// project_changed — with valid project
// ---------------------------------------------------------------------------

describe("ProjectContextButton – project loaded (manual)", () => {
  it("shows project name and filename after project_changed fires", () => {
    render(<ProjectContextButton />);
    act(() => {
      projectChangedCallback!({
        name: "CluicheTest",
        diagamePath: "C:/projects/cluichetest.diagame",
        source: "manual",
      });
    });
    expect(screen.getByText(/CluicheTest/)).toBeInTheDocument();
    expect(screen.getByText(/cluichetest\.diagame/)).toBeInTheDocument();
  });

  it("button colour changes to #cccccc when project is open", () => {
    render(<ProjectContextButton />);
    act(() => {
      projectChangedCallback!({
        name: "MyGame",
        diagamePath: "my/game.diagame",
        source: "manual",
      });
    });
    const btn = screen.getByTitle("my/game.diagame");
    expect(btn).toHaveStyle({ color: "#cccccc" });
  });

  it("does not show green dot for manual source", () => {
    render(<ProjectContextButton />);
    act(() => {
      projectChangedCallback!({
        name: "G",
        diagamePath: "g.diagame",
        source: "manual",
      });
    });
    const btn = screen.getByTitle("g.diagame");
    const spans = btn.querySelectorAll("span");
    const hasGreenDot = Array.from(spans).some(
      (s) => (s as HTMLElement).style.background === "rgb(137, 209, 133)"
    );
    expect(hasGreenDot).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// project_changed — live source (green dot)
// ---------------------------------------------------------------------------

describe("ProjectContextButton – live-connected project", () => {
  it("shows green dot when source is 'live'", () => {
    render(<ProjectContextButton />);
    act(() => {
      projectChangedCallback!({
        name: "LiveGame",
        diagamePath: "live/game.diagame",
        source: "live",
      });
    });
    const btn = screen.getByTitle("live/game.diagame");
    const spans = btn.querySelectorAll("span");
    const hasGreenDot = Array.from(spans).some(
      (s) => (s as HTMLElement).style.background === "rgb(137, 209, 133)"
    );
    expect(hasGreenDot).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Close Project clears button back to "No project"
// ---------------------------------------------------------------------------

describe("ProjectContextButton – close project", () => {
  it("returns to 'No project' when project_changed fires with empty diagamePath", () => {
    render(<ProjectContextButton />);
    act(() => {
      projectChangedCallback!({
        name: "Game",
        diagamePath: "game.diagame",
        source: "manual",
      });
    });
    act(() => {
      projectChangedCallback!({ name: "", diagamePath: "", source: "manual" });
    });
    expect(screen.getByText("No project")).toBeInTheDocument();
  });
});

// ---------------------------------------------------------------------------
// Dropdown — Recent list populates
// ---------------------------------------------------------------------------

describe("ProjectContextButton – dropdown recent list", () => {
  it("requests project.get_recent when dropdown is opened", async () => {
    render(<ProjectContextButton />);

    // Open dropdown by clicking the button.
    await userEvent.click(screen.getByTitle("No project open"));

    expect(mockRequest).toHaveBeenCalledWith("project.get_recent", {});
  });

  it("shows recent paths after project.get_recent resolves", async () => {
    render(<ProjectContextButton />);
    await userEvent.click(screen.getByTitle("No project open"));

    // Resolve the pending request.
    await act(async () => {
      pendingRequestResolve!({ paths: ["proj/a.diagame", "proj/b.diagame"] });
      await Promise.resolve();
    });

    await waitFor(() => {
      expect(screen.getByText("proj/a.diagame")).toBeInTheDocument();
      expect(screen.getByText("proj/b.diagame")).toBeInTheDocument();
    });
  });

  it("shows no recent section when list is empty", async () => {
    render(<ProjectContextButton />);
    await userEvent.click(screen.getByTitle("No project open"));

    await act(async () => {
      pendingRequestResolve!({ paths: [] });
      await Promise.resolve();
    });

    await waitFor(() => {
      expect(screen.queryByText("Recent")).not.toBeInTheDocument();
    });
  });
});

// ---------------------------------------------------------------------------
// Dropdown — Close Project sends project.close
// ---------------------------------------------------------------------------

describe("ProjectContextButton – close via dropdown", () => {
  it("sends project.close request when Close Project is clicked", async () => {
    render(<ProjectContextButton />);

    act(() => {
      projectChangedCallback!({
        name: "G",
        diagamePath: "g.diagame",
        source: "manual",
      });
    });

    await userEvent.click(screen.getByTitle("g.diagame"));

    // Resolve the get_recent request so the dropdown renders fully.
    await act(async () => {
      pendingRequestResolve!({ paths: [] });
      await Promise.resolve();
    });

    // Now a second request call happens when we click "Close Project".
    mockRequest.mockResolvedValueOnce({});
    const closeBtn = await screen.findByText("Close Project");
    await userEvent.click(closeBtn);

    expect(mockRequest).toHaveBeenCalledWith("project.close", {});
  });
});

// ---------------------------------------------------------------------------
// Dropdown — Open .diagame… sends project.open
// ---------------------------------------------------------------------------

describe("ProjectContextButton – open via dropdown", () => {
  it("sends project.open request when Open .diagame… is clicked", async () => {
    render(<ProjectContextButton />);

    await userEvent.click(screen.getByTitle("No project open"));

    await act(async () => {
      pendingRequestResolve!({ paths: [] });
      await Promise.resolve();
    });

    mockRequest.mockResolvedValueOnce({ ok: false, error: "not implemented" });
    const openBtn = await screen.findByText("Open .diagame…");
    await userEvent.click(openBtn);

    expect(mockRequest).toHaveBeenCalledWith("project.open", {});
  });
});

// ---------------------------------------------------------------------------
// Dropdown — clicking a recent path sends project.open_path with that path
// ---------------------------------------------------------------------------

describe("ProjectContextButton – recent path click", () => {
  it("sends project.open_path with the clicked recent path", async () => {
    render(<ProjectContextButton />);

    await userEvent.click(screen.getByTitle("No project open"));

    await act(async () => {
      pendingRequestResolve!({ paths: ["recent/alpha.diagame", "recent/beta.diagame"] });
      await Promise.resolve();
    });

    const recentBtn = await screen.findByText("recent/alpha.diagame");
    mockRequest.mockResolvedValueOnce({ ok: true });
    await userEvent.click(recentBtn);

    expect(mockRequest).toHaveBeenCalledWith("project.open_path", {
      path: "recent/alpha.diagame",
    });
  });

  it("closes dropdown after clicking a recent path", async () => {
    render(<ProjectContextButton />);

    await userEvent.click(screen.getByTitle("No project open"));

    await act(async () => {
      pendingRequestResolve!({ paths: ["recent/alpha.diagame"] });
      await Promise.resolve();
    });

    mockRequest.mockResolvedValueOnce({ ok: true });
    await userEvent.click(await screen.findByText("recent/alpha.diagame"));

    await waitFor(() => {
      expect(screen.queryByText("recent/alpha.diagame")).not.toBeInTheDocument();
    });
  });
});

// ---------------------------------------------------------------------------
// Dropdown — outside click closes the dropdown
// ---------------------------------------------------------------------------

describe("ProjectContextButton – outside click", () => {
  it("closes dropdown when clicking outside the button", async () => {
    render(
      <div>
        <ProjectContextButton />
        <div data-testid="outside">outside</div>
      </div>
    );

    await userEvent.click(screen.getByTitle("No project open"));

    await act(async () => {
      pendingRequestResolve!({ paths: [] });
      await Promise.resolve();
    });

    // Dropdown is open — "Open .diagame…" is visible.
    expect(await screen.findByText("Open .diagame…")).toBeInTheDocument();

    // Click outside.
    await userEvent.click(screen.getByTestId("outside"));

    await waitFor(() => {
      expect(screen.queryByText("Open .diagame…")).not.toBeInTheDocument();
    });
  });
});
