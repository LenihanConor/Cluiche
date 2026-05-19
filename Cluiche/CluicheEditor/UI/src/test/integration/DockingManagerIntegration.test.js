import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, act, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
// ── Shared mock state for the bridge ────────────────────────────────────────
let panelsChangedCb = null;
vi.mock("../../bridge/EditorBridge", () => ({
    EditorBridge: {
        getPanels: vi.fn(),
        loadLayout: vi.fn(),
        saveLayout: vi.fn().mockResolvedValue({}),
        togglePanelVisibility: vi.fn(),
        subscribe: vi.fn((topic, cb) => {
            if (topic === "panels_changed")
                panelsChangedCb = cb;
            return vi.fn();
        }),
        request: vi.fn(() => Promise.resolve(null)),
    },
}));
vi.mock("react-mosaic-component", () => ({
    Mosaic: ({ renderTile, value }) => {
        const ids = [];
        function collect(node) {
            if (!node)
                return;
            if (typeof node === "string") {
                ids.push(node);
                return;
            }
            collect(node.first);
            collect(node.second);
        }
        collect(value);
        return (_jsx("div", { "data-testid": "mosaic", children: ids.map((id) => (_jsx("div", { "data-testid": `tile-${id}`, children: renderTile(id, [id]) }, id))) }));
    },
    MosaicWindow: ({ children, title, toolbarControls }) => (_jsxs("div", { "data-testid": `window-${title}`, children: [_jsx("div", { "data-testid": `controls-${title}`, children: toolbarControls }), children] })),
}));
import { EditorBridge } from "../../bridge/EditorBridge";
import { DockingManager } from "../../layout/DockingManager";
const mockBridge = EditorBridge;
function makePanel(name, visible = true) {
    return { name, uiPath: `/panel/${name.toLowerCase()}`, visible };
}
beforeEach(() => {
    vi.clearAllMocks();
    panelsChangedCb = null;
    mockBridge.subscribe.mockImplementation((topic, cb) => {
        if (topic === "panels_changed")
            panelsChangedCb = cb;
        return vi.fn();
    });
    mockBridge.loadLayout.mockResolvedValue({});
    mockBridge.saveLayout.mockResolvedValue({});
});
describe("DockingManager + Toolbar – integration", () => {
    it("renders toolbar panel buttons matching loaded panels", async () => {
        mockBridge.getPanels.mockResolvedValue({
            panels: [makePanel("Console"), makePanel("Inspector")],
        });
        render(_jsx(DockingManager, {}));
        await waitFor(() => screen.getByTestId("mosaic"));
        // iframes also carry a title attribute, so use getAllByTitle
        expect(screen.getAllByTitle("Console").length).toBeGreaterThan(0);
        expect(screen.getAllByTitle("Inspector").length).toBeGreaterThan(0);
    });
    it("clicking toolbar toggle calls togglePanelVisibility", async () => {
        mockBridge.getPanels.mockResolvedValue({
            panels: [makePanel("Console"), makePanel("Inspector")],
        });
        render(_jsx(DockingManager, {}));
        await waitFor(() => screen.getAllByTitle("Console"));
        // Target the button (role=button) specifically to avoid matching the iframe
        const toolbarBtn = screen
            .getAllByTitle("Console")
            .find((el) => el.tagName === "BUTTON");
        await userEvent.click(toolbarBtn);
        expect(mockBridge.togglePanelVisibility).toHaveBeenCalledWith("Console");
    });
    it("panels_changed subscription adds a new panel to the mosaic and toolbar", async () => {
        mockBridge.getPanels.mockResolvedValue({
            panels: [makePanel("Console")],
        });
        render(_jsx(DockingManager, {}));
        await waitFor(() => screen.getByTestId("tile-Console"));
        act(() => {
            panelsChangedCb({
                panels: [makePanel("Console"), makePanel("Inspector")],
            });
        });
        await waitFor(() => expect(screen.getByTestId("tile-Inspector")).toBeInTheDocument());
        // Toolbar button also reflects the new panel (iframes also get a title so use getAllByTitle)
        expect(screen.getAllByTitle("Inspector").length).toBeGreaterThan(0);
    });
    it("panels_changed removes a panel that is no longer in the list", async () => {
        mockBridge.getPanels.mockResolvedValue({
            panels: [makePanel("Console"), makePanel("Inspector")],
        });
        render(_jsx(DockingManager, {}));
        await waitFor(() => screen.getByTestId("tile-Inspector"));
        act(() => {
            panelsChangedCb({ panels: [makePanel("Console")] });
        });
        await waitFor(() => expect(screen.queryByTestId("tile-Inspector")).not.toBeInTheDocument());
    });
    it("fullscreen via mosaic toolbar hides other panels", async () => {
        mockBridge.getPanels.mockResolvedValue({
            panels: [makePanel("Console"), makePanel("Inspector")],
        });
        render(_jsx(DockingManager, {}));
        await waitFor(() => screen.getByTestId("window-Console"));
        const fsBtn = screen.getByTestId("controls-Console")
            .querySelector('button[title="Fullscreen"]');
        await userEvent.click(fsBtn);
        await waitFor(() => expect(screen.queryByTestId("tile-Inspector")).not.toBeInTheDocument());
        expect(screen.getByTestId("tile-Console")).toBeInTheDocument();
    });
});
