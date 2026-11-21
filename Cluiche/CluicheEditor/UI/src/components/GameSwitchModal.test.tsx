import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, act, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";

let changedCb: ((d: unknown) => void) | null = null;
const unsub = vi.fn();

vi.mock("../bridge/EditorBridge", () => ({
  EditorBridge: {
    subscribe: vi.fn(),
    request: vi.fn(),
  },
}));

import { EditorBridge } from "../bridge/EditorBridge";
import { GameSwitchModal } from "./GameSwitchModal";

const mockRequest = EditorBridge.request as ReturnType<typeof vi.fn>;
const mockSubscribe = EditorBridge.subscribe as ReturnType<typeof vi.fn>;

const base = {
  pending: false, prompt: "none", targetPath: "", targetName: "",
  currentName: "", isClose: false, dirty: [], lastOutcome: "none",
};
const confirmState = { ...base, pending: true, prompt: "confirm_switch",
  targetPath: "C:/g/cow.diagame", targetName: "cow", currentName: "cluichetest" };
const unsavedState = { ...base, pending: true, prompt: "unsaved",
  targetPath: "C:/g/cluichetest.diagame", targetName: "cluichetest", currentName: "cow",
  dirty: [
    { typeId: "a", name: "CoW World Data Editor", saved: false, saveFailed: false },
    { typeId: "b", name: "CoW World Sim Inspector", saved: false, saveFailed: false },
  ] };
const failedState = { ...unsavedState, prompt: "save_failed",
  dirty: [
    { typeId: "a", name: "CoW World Data Editor", saved: true, saveFailed: false },
    { typeId: "b", name: "CoW World Sim Inspector", saved: false, saveFailed: true },
  ] };

function setup(initial: unknown) {
  mockRequest.mockImplementation((type: string) =>
    Promise.resolve(type === "game_switch.get_state" ? initial : { ok: true }));
  mockSubscribe.mockImplementation((topic: string, cb: (d: unknown) => void) => {
    if (topic === "game_switch_changed") changedCb = cb;
    return unsub;
  });
  return render(<GameSwitchModal />);
}

beforeEach(() => {
  vi.clearAllMocks();
  changedCb = null;
});

describe("GameSwitchModal", () => {
  it("requests state on load and subscribes to the topic", async () => {
    setup(base);
    await waitFor(() => expect(mockRequest).toHaveBeenCalledWith("game_switch.get_state", {}));
    expect(mockSubscribe).toHaveBeenCalledWith("game_switch_changed", expect.any(Function));
  });

  it("is hidden when nothing is pending", async () => {
    const { container } = setup(base);
    await waitFor(() => expect(mockRequest).toHaveBeenCalled());
    expect(container.firstElementChild).toBeNull();
  });

  it("renders confirm state from get_state", async () => {
    setup(confirmState);
    expect(await screen.findByText("Switch to cow.diagame?")).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Switch to cow" })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Stay on cluichetest" })).toBeInTheDocument();
  });

  it("shows and hides via the topic", async () => {
    const { container } = setup(base);
    await waitFor(() => expect(changedCb).not.toBeNull());
    act(() => changedCb!(confirmState));
    expect(await screen.findByText("Switch to cow.diagame?")).toBeInTheDocument();
    act(() => changedCb!(base));
    expect(container.firstElementChild).toBeNull();
  });

  it("renders unsaved state with dirty list and buttons", async () => {
    setup(unsavedState);
    expect(await screen.findByText("Switch to cluichetest.diagame?")).toBeInTheDocument();
    expect(screen.getByText("CoW World Data Editor")).toBeInTheDocument();
    expect(screen.getByText("CoW World Sim Inspector")).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Cancel" })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Discard" })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Save all and switch" })).toBeInTheDocument();
  });

  it("renders save-failed state with markers and Retry save", async () => {
    setup(failedState);
    expect(await screen.findByText(/nothing was closed/)).toBeInTheDocument();
    expect(screen.getByText("saved")).toBeInTheDocument();
    expect(screen.getByText("save failed")).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Retry save" })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Discard" })).toBeInTheDocument();
  });

  it.each([
    [confirmState, "Switch to cow", "confirm"],
    [confirmState, "Stay on cluichetest", "cancel"],
    [unsavedState, "Cancel", "cancel"],
    [unsavedState, "Discard", "discard"],
    [unsavedState, "Save all and switch", "save"],
    [failedState, "Retry save", "save"],
    [failedState, "Discard", "discard"],
    [failedState, "Cancel", "cancel"],
  ])("button %# '%s' sends choice %s", async (state, label, choice) => {
    setup(state);
    const btn = await screen.findByRole("button", { name: label as string });
    await userEvent.click(btn);
    expect(mockRequest).toHaveBeenCalledWith("game_switch.respond", { choice });
  });

  it("unsubscribes on unmount", async () => {
    const { unmount } = setup(base);
    await waitFor(() => expect(mockSubscribe).toHaveBeenCalled());
    unmount();
    expect(unsub).toHaveBeenCalled();
  });
});
