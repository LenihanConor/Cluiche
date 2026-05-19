import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
import { useCallback, useEffect, useMemo, useState } from "react";
import { Mosaic, MosaicWindow } from "react-mosaic-component";
import "react-mosaic-component/react-mosaic-component.css";
import { EditorBridge } from "../bridge/EditorBridge";
import { Toolbar } from "./Toolbar";
function buildTree(panelNames) {
    if (panelNames.length === 0)
        return null;
    if (panelNames.length === 1)
        return panelNames[0];
    const mid = Math.ceil(panelNames.length / 2);
    const left = buildTree(panelNames.slice(0, mid));
    const right = buildTree(panelNames.slice(mid));
    if (left == null)
        return right;
    if (right == null)
        return left;
    return {
        direction: panelNames.length > 2 ? "column" : "row",
        first: left,
        second: right,
        splitPercentage: 50,
    };
}
function collectLeaves(node) {
    if (node == null)
        return [];
    if (typeof node === "string")
        return [node];
    return [...collectLeaves(node.first), ...collectLeaves(node.second)];
}
function dedupTree(node, seen = new Set()) {
    if (node == null)
        return null;
    if (typeof node === "string") {
        if (seen.has(node))
            return null;
        seen.add(node);
        return node;
    }
    const first = dedupTree(node.first, seen);
    const second = dedupTree(node.second, seen);
    if (first == null)
        return second;
    if (second == null)
        return first;
    return { ...node, first, second };
}
function removeFromLayout(node, id) {
    if (node == null)
        return null;
    if (typeof node === "string")
        return node === id ? null : node;
    const first = removeFromLayout(node.first, id);
    const second = removeFromLayout(node.second, id);
    if (first == null)
        return second;
    if (second == null)
        return first;
    return { ...node, first, second };
}
function addToLayout(node, id) {
    if (node == null)
        return id;
    return {
        direction: "row",
        first: node,
        second: id,
        splitPercentage: 70,
    };
}
export function DockingManager({ onReady }) {
    const [panels, setPanels] = useState([]);
    const [layout, setLayout] = useState(null);
    const [savedLayout, setSavedLayout] = useState(null);
    const [initialized, setInitialized] = useState(false);
    const [fullscreenPanel, setFullscreenPanel] = useState(null);
    const panelMap = useMemo(() => {
        const m = new Map();
        panels.forEach((p) => m.set(p.name, p));
        return m;
    }, [panels]);
    useEffect(() => {
        EditorBridge.getPanels()
            .then((res) => {
            const list = res?.panels ?? [];
            setPanels(list);
            EditorBridge.loadLayout()
                .then((saved) => {
                const savedTree = saved?.tree;
                if (savedTree) {
                    const clean = dedupTree(savedTree);
                    setLayout(clean);
                    if (clean)
                        EditorBridge.saveLayout({ tree: clean }).catch(() => { });
                }
                else {
                    const visible = list.filter((p) => p.visible).map((p) => p.name);
                    setLayout(buildTree(visible));
                }
            })
                .catch(() => {
                const visible = list.filter((p) => p.visible).map((p) => p.name);
                setLayout(buildTree(visible));
            })
                .finally(() => { setInitialized(true); onReady?.(); });
        })
            .catch(() => {
            setLayout(null);
            setInitialized(true);
            onReady?.();
        });
    }, []);
    useEffect(() => {
        return EditorBridge.subscribe("panels_changed", (data) => {
            const d = data;
            if (!d?.panels)
                return;
            const newPanels = d.panels;
            setPanels(newPanels);
            setLayout((prev) => {
                const currentIds = new Set(collectLeaves(prev));
                const newPanelNames = new Set(newPanels.map((p) => p.name));
                let updated = prev;
                currentIds.forEach((id) => {
                    if (!newPanelNames.has(id)) {
                        updated = removeFromLayout(updated, id);
                    }
                });
                newPanels.forEach((p) => {
                    if (p.visible && !currentIds.has(p.name)) {
                        updated = addToLayout(updated, p.name);
                    }
                });
                return updated;
            });
        });
    }, []);
    const handleChange = useCallback((newLayout) => {
        setLayout(newLayout);
        if (newLayout)
            EditorBridge.saveLayout({ tree: newLayout }).catch(() => { });
    }, []);
    const handlePanelClose = useCallback((id) => {
        if (fullscreenPanel === id)
            setFullscreenPanel(null);
        EditorBridge.togglePanelVisibility(id);
        setLayout((prev) => removeFromLayout(prev, id));
    }, [fullscreenPanel]);
    const handleFullscreen = useCallback((id) => {
        setFullscreenPanel((prev) => {
            if (prev === id) {
                // Exit fullscreen: restore saved layout
                setLayout(savedLayout);
                setSavedLayout(null);
                return null;
            }
            else {
                // Enter fullscreen: save current layout and collapse to single panel
                setLayout((current) => { setSavedLayout(current); return id; });
                return id;
            }
        });
    }, [savedLayout]);
    function renderTile(id, path) {
        const info = panelMap.get(id);
        const src = info?.uiPath ?? `dia://editor/${id.toLowerCase().replace(/\s+/g, "-")}/index.html`;
        const isFullscreen = fullscreenPanel === id;
        return (_jsx(MosaicWindow, { path: path, title: id, createNode: () => panels[0]?.name ?? id, toolbarControls: [
                _jsx("button", { onClick: () => handleFullscreen(id), title: isFullscreen ? "Exit fullscreen" : "Fullscreen", className: "mosaic-default-control bp4-button bp4-minimal", style: {
                        background: "transparent",
                        border: "none",
                        cursor: "pointer",
                        color: "#999",
                        fontSize: 12,
                        lineHeight: 1,
                        padding: "0 4px",
                    }, children: isFullscreen ? "⊡" : "⊞" }, "fullscreen"),
                _jsx("button", { onClick: () => handlePanelClose(id), title: "Hide panel", className: "mosaic-default-control bp4-button bp4-minimal", style: {
                        background: "transparent",
                        border: "none",
                        cursor: "pointer",
                        color: "#999",
                        fontSize: 14,
                        lineHeight: 1,
                        padding: "0 4px",
                    }, children: "\u00D7" }, "close"),
            ], children: _jsx("iframe", { src: src, style: { width: "100%", height: "100%", border: "none" }, title: id }, id) }));
    }
    if (!initialized) {
        return (_jsx("div", { style: { padding: 16, color: "#888", fontFamily: "monospace" }, children: "Loading panels\u2026" }));
    }
    return (_jsxs("div", { style: { display: "flex", flexDirection: "column", height: "100%" }, children: [_jsx("div", { style: { flex: 1, position: "relative" }, children: layout ? (_jsx(Mosaic, { className: "mosaic-blueprint-theme bp4-dark", renderTile: renderTile, value: layout, onChange: handleChange })) : (_jsx("div", { style: { padding: 16, color: "#888", fontFamily: "monospace" }, children: "No editor panels visible. Use the toolbar to show a panel." })) }), _jsx(Toolbar, { panels: panels })] }));
}
