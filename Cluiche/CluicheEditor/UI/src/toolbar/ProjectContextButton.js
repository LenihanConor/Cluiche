import { jsx as _jsx, jsxs as _jsxs, Fragment as _Fragment } from "react/jsx-runtime";
import { useEffect, useRef, useState } from "react";
import { EditorBridge } from "../bridge/EditorBridge";
const emptyProject = { name: "", diagamePath: "", source: "manual" };
export function ProjectContextButton() {
    const [project, setProject] = useState(emptyProject);
    const [dropdownOpen, setDropdownOpen] = useState(false);
    const [recentPaths, setRecentPaths] = useState([]);
    const buttonRef = useRef(null);
    useEffect(() => {
        const unsub = EditorBridge.subscribe("project_changed", (data) => {
            const d = data;
            if (!d)
                return;
            setProject({
                name: d.name ?? "",
                diagamePath: d.diagamePath ?? "",
                source: d.source === "live" ? "live" : "manual",
            });
        });
        return unsub;
    }, []);
    useEffect(() => {
        if (!dropdownOpen)
            return;
        EditorBridge.request("project.get_recent", {})
            .then((r) => setRecentPaths(r?.paths ?? []))
            .catch(() => setRecentPaths([]));
    }, [dropdownOpen]);
    useEffect(() => {
        if (!dropdownOpen)
            return;
        function handleClick(e) {
            if (buttonRef.current && !buttonRef.current.contains(e.target))
                setDropdownOpen(false);
        }
        document.addEventListener("mousedown", handleClick);
        return () => document.removeEventListener("mousedown", handleClick);
    }, [dropdownOpen]);
    const hasProject = project.diagamePath !== "";
    function handleOpenPath(path) {
        EditorBridge.request("project.open_path", { path }).catch(() => { });
        setDropdownOpen(false);
    }
    function handleClose() {
        EditorBridge.request("project.close", {}).catch(() => { });
        setDropdownOpen(false);
    }
    const buttonLabel = hasProject
        ? `${project.name || "Project"} · ${project.diagamePath.split(/[\\/]/).pop()}`
        : "No project";
    return (_jsxs("div", { style: { position: "relative" }, ref: buttonRef, children: [_jsxs("button", { onClick: () => setDropdownOpen((o) => !o), title: hasProject ? project.diagamePath : "No project open", style: {
                    display: "flex",
                    alignItems: "center",
                    gap: 5,
                    background: "transparent",
                    border: "none",
                    cursor: "pointer",
                    color: hasProject ? "#cccccc" : "#666666",
                    fontSize: 11,
                    fontFamily: "Segoe UI, system-ui, sans-serif",
                    padding: "2px 6px",
                    borderRadius: 2,
                }, children: [project.source === "live" && hasProject && (_jsx("span", { style: {
                            display: "inline-block",
                            width: 6,
                            height: 6,
                            borderRadius: "50%",
                            background: "#89d185",
                            flexShrink: 0,
                        } })), _jsx("span", { children: buttonLabel }), _jsx("span", { style: { fontSize: 9, opacity: 0.6 }, children: "\u25BE" })] }), dropdownOpen && (_jsxs("div", { style: {
                    position: "absolute",
                    bottom: "100%",
                    left: "50%",
                    transform: "translateX(-50%)",
                    marginBottom: 4,
                    background: "#2d2d30",
                    border: "1px solid #3c3c3c",
                    borderRadius: 3,
                    minWidth: 220,
                    boxShadow: "0 4px 12px rgba(0,0,0,0.5)",
                    zIndex: 1000,
                    overflow: "hidden",
                }, children: [_jsx(DropdownItem, { label: "Open .diagame\u2026", onClick: () => {
                            EditorBridge.request("project.open", {}).catch(() => { });
                            setDropdownOpen(false);
                        } }), recentPaths.length > 0 && (_jsxs(_Fragment, { children: [_jsx(Separator, {}), _jsx("div", { style: { padding: "2px 8px 2px", color: "#888", fontSize: 10 }, children: "Recent" }), recentPaths.map((p) => (_jsx(DropdownItem, { label: p, title: p, truncate: true, onClick: () => handleOpenPath(p) }, p)))] })), hasProject && (_jsxs(_Fragment, { children: [_jsx(Separator, {}), _jsx(DropdownItem, { label: "Reveal in Explorer", onClick: () => {
                                    // No-op until native shell integration is added.
                                    setDropdownOpen(false);
                                } }), _jsx(Separator, {}), _jsx(DropdownItem, { label: "Close Project", onClick: handleClose, danger: true })] }))] }))] }));
}
function DropdownItem({ label, title, onClick, danger = false, truncate = false, }) {
    return (_jsx("button", { onClick: onClick, title: title ?? label, style: {
            display: "block",
            width: "100%",
            textAlign: "left",
            background: "transparent",
            border: "none",
            cursor: "pointer",
            color: danger ? "#f48771" : "#cccccc",
            fontSize: 11,
            fontFamily: "Segoe UI, system-ui, sans-serif",
            padding: "5px 12px",
            overflow: truncate ? "hidden" : undefined,
            textOverflow: truncate ? "ellipsis" : undefined,
            whiteSpace: truncate ? "nowrap" : undefined,
            maxWidth: truncate ? 280 : undefined,
        }, onMouseEnter: (e) => { (e.currentTarget.style.background = "#094771"); }, onMouseLeave: (e) => { (e.currentTarget.style.background = "transparent"); }, children: label }));
}
function Separator() {
    return _jsx("div", { style: { height: 1, background: "#3c3c3c", margin: "2px 0" } });
}
