import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
import { useEffect, useState } from "react";
import { EditorBridge } from "../bridge/EditorBridge";
import { ProjectContextButton } from "../toolbar/ProjectContextButton";
export function Toolbar({ panels }) {
    const [connectionState, setConnectionState] = useState("disconnected");
    useEffect(() => {
        EditorBridge.request("game_connection.get_state", {})
            .then((result) => {
            if (result?.state)
                setConnectionState(result.state);
        })
            .catch(() => { });
        return EditorBridge.subscribe("game_connection", (data) => {
            const d = data;
            if (d?.state)
                setConnectionState(d.state);
        });
    }, []);
    function handleToggle(name) {
        EditorBridge.togglePanelVisibility(name);
    }
    function handleConnectionClick() {
        EditorBridge.togglePanelVisibility("Game Connection");
    }
    const isConnected = connectionState === "connected";
    return (_jsxs("div", { style: {
            display: "flex",
            alignItems: "center",
            height: 28,
            background: "#252526",
            borderTop: "1px solid #3c3c3c",
            padding: "0 8px",
            flexShrink: 0,
            gap: 2,
        }, children: [_jsx("div", { style: { display: "flex", gap: 2 }, children: panels.map((p) => (_jsx("button", { onClick: () => handleToggle(p.name), title: p.name, style: {
                        width: 24,
                        height: 22,
                        display: "flex",
                        alignItems: "center",
                        justifyContent: "center",
                        background: p.visible ? "#0e639c" : "transparent",
                        color: p.visible ? "#fff" : "#808080",
                        border: p.visible ? "none" : "1px solid #3c3c3c",
                        cursor: "pointer",
                        fontSize: 11,
                        fontWeight: 600,
                        fontFamily: "Segoe UI, system-ui, sans-serif",
                        borderRadius: 2,
                        padding: 0,
                    }, children: p.name.charAt(0).toUpperCase() }, p.name))) }), _jsx("div", { style: { flex: 1, display: "flex", justifyContent: "center", alignItems: "center" }, children: _jsx(ProjectContextButton, {}) }), _jsx("div", { style: { display: "flex", alignItems: "center", gap: 6 }, children: _jsxs("button", { onClick: handleConnectionClick, title: isConnected ? "Connected to game" : "Disconnected", style: {
                        display: "flex",
                        alignItems: "center",
                        gap: 4,
                        background: "transparent",
                        border: "none",
                        cursor: "pointer",
                        color: "#808080",
                        fontSize: 11,
                        fontFamily: "Segoe UI, system-ui, sans-serif",
                        padding: "2px 6px",
                    }, children: [_jsx("span", { style: {
                                display: "inline-block",
                                width: 7,
                                height: 7,
                                borderRadius: "50%",
                                background: isConnected ? "#89d185" : "#f48771",
                            } }), isConnected ? "Connected" : "Disconnected"] }) })] }));
}
