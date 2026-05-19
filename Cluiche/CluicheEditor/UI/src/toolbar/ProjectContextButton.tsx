import { useEffect, useRef, useState } from "react";
import { EditorBridge } from "../bridge/EditorBridge";

interface ProjectState {
  name: string;
  diagamePath: string;
  source: "live" | "manual";
}

const emptyProject: ProjectState = { name: "", diagamePath: "", source: "manual" };

export function ProjectContextButton() {
  const [project, setProject] = useState<ProjectState>(emptyProject);
  const [dropdownOpen, setDropdownOpen] = useState(false);
  const [recentPaths, setRecentPaths] = useState<string[]>([]);
  const buttonRef = useRef<HTMLButtonElement>(null);

  useEffect(() => {
    const unsub = EditorBridge.subscribe("project_changed", (data: unknown) => {
      const d = data as Partial<ProjectState> | null;
      if (!d) return;
      setProject({
        name: d.name ?? "",
        diagamePath: d.diagamePath ?? "",
        source: d.source === "live" ? "live" : "manual",
      });
    });
    return unsub;
  }, []);

  useEffect(() => {
    if (!dropdownOpen) return;
    EditorBridge.request<{ paths: string[] }>("project.get_recent", {})
      .then((r) => setRecentPaths(r?.paths ?? []))
      .catch(() => setRecentPaths([]));
  }, [dropdownOpen]);

  useEffect(() => {
    if (!dropdownOpen) return;
    function handleClick(e: MouseEvent) {
      if (buttonRef.current && !buttonRef.current.contains(e.target as Node))
        setDropdownOpen(false);
    }
    document.addEventListener("mousedown", handleClick);
    return () => document.removeEventListener("mousedown", handleClick);
  }, [dropdownOpen]);

  const hasProject = project.diagamePath !== "";

  function handleOpenPath(path: string) {
    EditorBridge.request("project.open_path", { path }).catch(() => {});
    setDropdownOpen(false);
  }

  function handleClose() {
    EditorBridge.request("project.close", {}).catch(() => {});
    setDropdownOpen(false);
  }

  const buttonLabel = hasProject
    ? `${project.name || "Project"} · ${project.diagamePath.split(/[\\/]/).pop()}`
    : "No project";

  return (
    <div style={{ position: "relative" }} ref={buttonRef as unknown as React.RefObject<HTMLDivElement>}>
      <button
        onClick={() => setDropdownOpen((o) => !o)}
        title={hasProject ? project.diagamePath : "No project open"}
        style={{
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
        }}
      >
        {project.source === "live" && hasProject && (
          <span style={{
            display: "inline-block",
            width: 6,
            height: 6,
            borderRadius: "50%",
            background: "#89d185",
            flexShrink: 0,
          }} />
        )}
        <span>{buttonLabel}</span>
        <span style={{ fontSize: 9, opacity: 0.6 }}>▾</span>
      </button>

      {dropdownOpen && (
        <div style={{
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
        }}>
          <DropdownItem label="Open .diagame…" onClick={() => {
            EditorBridge.request("project.open", {}).catch(() => {});
            setDropdownOpen(false);
          }} />

          {recentPaths.length > 0 && (
            <>
              <Separator />
              <div style={{ padding: "2px 8px 2px", color: "#888", fontSize: 10 }}>Recent</div>
              {recentPaths.map((p) => (
                <DropdownItem
                  key={p}
                  label={p}
                  title={p}
                  truncate
                  onClick={() => handleOpenPath(p)}
                />
              ))}
            </>
          )}

          {hasProject && (
            <>
              <Separator />
              <DropdownItem label="Reveal in Explorer" onClick={() => {
                // No-op until native shell integration is added.
                setDropdownOpen(false);
              }} />
              <Separator />
              <DropdownItem label="Close Project" onClick={handleClose} danger />
            </>
          )}
        </div>
      )}
    </div>
  );
}

function DropdownItem({
  label, title, onClick, danger = false, truncate = false,
}: {
  label: string;
  title?: string;
  onClick: () => void;
  danger?: boolean;
  truncate?: boolean;
}) {
  return (
    <button
      onClick={onClick}
      title={title ?? label}
      style={{
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
      }}
      onMouseEnter={(e) => { (e.currentTarget.style.background = "#094771"); }}
      onMouseLeave={(e) => { (e.currentTarget.style.background = "transparent"); }}
    >
      {label}
    </button>
  );
}

function Separator() {
  return <div style={{ height: 1, background: "#3c3c3c", margin: "2px 0" }} />;
}
