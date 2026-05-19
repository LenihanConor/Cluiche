import { jsx as _jsx } from "react/jsx-runtime";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, act } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
let subscribeCallback = null;
vi.mock("../bridge/EditorBridge", () => ({
    EditorBridge: {
        togglePanelVisibility: vi.fn(),
        subscribe: vi.fn((topic, cb) => {
            if (topic === "game_connection")
                subscribeCallback = cb;
            return vi.fn();
        }),
        request: vi.fn(() => Promise.resolve(null)),
    },
}));
import { EditorBridge } from "../bridge/EditorBridge";
import { Toolbar } from "./Toolbar";
const mockToggle = EditorBridge.togglePanelVisibility;
function panelList(names, visible = true) {
    return names.map((n) => ({ name: n, uiPath: `/panel/${n}`, visible }));
}
beforeEach(() => {
    vi.clearAllMocks();
    subscribeCallback = null;
    EditorBridge.subscribe.mockImplementation((topic, cb) => {
        if (topic === "game_connection")
            subscribeCallback = cb;
        return vi.fn();
    });
    EditorBridge.request.mockResolvedValue(null);
});
describe("Toolbar – panel buttons", () => {
    it("renders a button for each panel", () => {
        render(_jsx(Toolbar, { panels: panelList(["Console", "Inspector", "Hierarchy"]) }));
        // Each button shows the first letter of the panel name
        expect(screen.getByTitle("Console")).toBeInTheDocument();
        expect(screen.getByTitle("Inspector")).toBeInTheDocument();
        expect(screen.getByTitle("Hierarchy")).toBeInTheDocument();
    });
    it("renders no panel buttons when panels list is empty", () => {
        render(_jsx(Toolbar, { panels: [] }));
        // Project context button + connection button; no panel toggle buttons.
        expect(screen.queryAllByRole("button")).toHaveLength(2);
    });
    it("visible panel button has active background colour", () => {
        render(_jsx(Toolbar, { panels: panelList(["Console"], true) }));
        const btn = screen.getByTitle("Console");
        expect(btn).toHaveStyle({ background: "#0e639c" });
    });
    it("hidden panel button has transparent background", () => {
        render(_jsx(Toolbar, { panels: panelList(["Console"], false) }));
        const btn = screen.getByTitle("Console");
        expect(btn).toHaveStyle({ background: "transparent" });
    });
    it("clicking a panel button calls togglePanelVisibility with the panel name", async () => {
        render(_jsx(Toolbar, { panels: panelList(["Console"]) }));
        await userEvent.click(screen.getByTitle("Console"));
        expect(mockToggle).toHaveBeenCalledWith("Console");
    });
});
describe("Toolbar – connection status", () => {
    it("shows Disconnected by default", () => {
        render(_jsx(Toolbar, { panels: [] }));
        expect(screen.getByText("Disconnected")).toBeInTheDocument();
    });
    it("shows Connected when game_connection topic fires with state=connected", () => {
        render(_jsx(Toolbar, { panels: [] }));
        act(() => { subscribeCallback({ state: "connected" }); });
        expect(screen.getByText("Connected")).toBeInTheDocument();
    });
    it("status indicator is green when connected", () => {
        render(_jsx(Toolbar, { panels: [] }));
        act(() => { subscribeCallback({ state: "connected" }); });
        // The dot span is the first child of the connection button
        const btn = screen.getByTitle("Connected to game");
        const dot = btn.querySelector("span");
        expect(dot).toHaveStyle({ background: "#89d185" });
    });
    it("status indicator is red when disconnected", () => {
        render(_jsx(Toolbar, { panels: [] }));
        const btn = screen.getByTitle("Disconnected");
        const dot = btn.querySelector("span");
        expect(dot).toHaveStyle({ background: "#f48771" });
    });
    it("clicking the connection button toggles Game Connection panel", async () => {
        render(_jsx(Toolbar, { panels: [] }));
        await userEvent.click(screen.getByText("Disconnected"));
        expect(mockToggle).toHaveBeenCalledWith("Game Connection");
    });
});
