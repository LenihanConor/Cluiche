import { useCallback, useEffect, useRef, useState } from "react";
import { EditorBridge, PanelInfo } from "../bridge/EditorBridge";
import { ProjectContextButton } from "../toolbar/ProjectContextButton";
import { ConnectionButton } from "../toolbar/ConnectionButton";

interface ToolbarProps {
  panels: PanelInfo[];
}

const PILL_GAP = 2;
const OVERFLOW_BTN_WIDTH = 52;

export function Toolbar({ panels }: ToolbarProps) {
  const [visibleCount, setVisibleCount] = useState(panels.length);
  const [dropdownOpen, setDropdownOpen] = useState(false);

  const pillZoneRef = useRef<HTMLDivElement>(null);
  const pillWidthsRef = useRef<number[]>([]);
  const pillRefs = useRef<(HTMLButtonElement | null)[]>([]);
  const dropdownRef = useRef<HTMLDivElement>(null);

  const recompute = useCallback(() => {
    const container = pillZoneRef.current;
    if (!container) return;

    const containerWidth = container.offsetWidth;
    if (containerWidth === 0) return;

    const widths = pillWidthsRef.current;
    const total = widths.reduce((s, w, i) => s + w + (i > 0 ? PILL_GAP : 0), 0);

    if (total <= containerWidth) {
      setVisibleCount(widths.length);
      return;
    }

    const available = containerWidth - OVERFLOW_BTN_WIDTH - PILL_GAP;
    let used = 0;
    let count = 0;
    for (let i = 0; i < widths.length; i++) {
      const w = widths[i] + (i > 0 ? PILL_GAP : 0);
      if (used + w <= available) {
        used += w;
        count++;
      } else {
        break;
      }
    }
    setVisibleCount(count);
  }, []);

  // When panels change: show all first, measure widths, then recompute
  useEffect(() => {
    setVisibleCount(panels.length);
    const raf = requestAnimationFrame(() => {
      pillWidthsRef.current = pillRefs.current
        .slice(0, panels.length)
        .map(r => r?.offsetWidth ?? 0);
      recompute();
    });
    return () => cancelAnimationFrame(raf);
  }, [panels, recompute]);

  useEffect(() => {
    if (typeof ResizeObserver === "undefined") return;
    const container = pillZoneRef.current;
    if (!container) return;
    const ro = new ResizeObserver(() => recompute());
    ro.observe(container);
    return () => ro.disconnect();
  }, [recompute]);

  useEffect(() => {
    if (!dropdownOpen) return;
    const handler = (e: MouseEvent) => {
      if (!dropdownRef.current?.contains(e.target as Node)) {
        setDropdownOpen(false);
      }
    };
    document.addEventListener("mousedown", handler);
    return () => document.removeEventListener("mousedown", handler);
  }, [dropdownOpen]);

  function handleToggle(name: string) {
    EditorBridge.togglePanelVisibility(name);
  }

  function handleOverflowToggle(name: string) {
    EditorBridge.togglePanelVisibility(name);
    setDropdownOpen(false);
  }

  const overflowCount = panels.length - visibleCount;
  const overflowedPanels = panels.slice(visibleCount);

  return (
    <div
      style={{
        display: "flex",
        alignItems: "center",
        height: 28,
        background: "#252526",
        borderTop: "1px solid #3c3c3c",
        padding: "0 8px",
        flexShrink: 0,
        gap: 6,
      }}
    >
      {/* Plugin Browser button — far left, always visible */}
      <button
        onClick={() => handleToggle("Plugin Browser")}
        title="Plugin Browser"
        style={{
          height: 22,
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
          background: panels.find(p => p.name === "Plugin Browser")?.visible ? "#0e639c" : "transparent",
          color: panels.find(p => p.name === "Plugin Browser")?.visible ? "#fff" : "#808080",
          border: panels.find(p => p.name === "Plugin Browser")?.visible ? "none" : "1px solid #3c3c3c",
          cursor: "pointer",
          fontSize: 13,
          borderRadius: 2,
          padding: "0 7px",
          flexShrink: 0,
        }}
      >
        ⊞
      </button>

      {/* Pill zone — flex:1 so it fills available space, overflow hidden to clip pills */}
      <div
        ref={pillZoneRef}
        style={{ display: "flex", gap: PILL_GAP, alignItems: "center", flex: 1, overflow: "hidden" }}
      >
        {panels.map((p, i) => (
          <button
            key={p.name}
            ref={el => { pillRefs.current[i] = el; }}
            onClick={() => handleToggle(p.name)}
            title={p.name}
            style={{
              height: 22,
              display: i < visibleCount ? "flex" : "none",
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
              padding: "0 10px",
              whiteSpace: "nowrap",
              flexShrink: 0,
            }}
          >
            {p.name}
          </button>
        ))}
      </div>

      {/* Overflow button + dropdown */}
      {overflowCount > 0 && (
        <div ref={dropdownRef} style={{ position: "relative", flexShrink: 0 }}>
          <button
            onClick={() => setDropdownOpen(o => !o)}
            style={{
              height: 22,
              display: "flex",
              alignItems: "center",
              justifyContent: "center",
              background: dropdownOpen ? "#3c3c3c" : "transparent",
              color: "#808080",
              border: "1px solid #3c3c3c",
              cursor: "pointer",
              fontSize: 11,
              fontWeight: 600,
              fontFamily: "Segoe UI, system-ui, sans-serif",
              borderRadius: 2,
              padding: "0 8px",
              whiteSpace: "nowrap",
            }}
          >
            ⋯ +{overflowCount}
          </button>

          {dropdownOpen && (
            <div
              style={{
                position: "absolute",
                top: 26,
                left: 0,
                background: "#2d2d2d",
                border: "1px solid #3c3c3c",
                borderRadius: 4,
                padding: "4px 0",
                zIndex: 1000,
                minWidth: 160,
                boxShadow: "0 4px 8px rgba(0,0,0,0.4)",
              }}
            >
              {overflowedPanels.map(p => (
                <button
                  key={p.name}
                  onClick={() => handleOverflowToggle(p.name)}
                  style={{
                    display: "flex",
                    alignItems: "center",
                    gap: 8,
                    width: "100%",
                    background: "transparent",
                    border: "none",
                    cursor: "pointer",
                    color: "#cccccc",
                    fontSize: 12,
                    fontFamily: "Segoe UI, system-ui, sans-serif",
                    padding: "4px 12px",
                    textAlign: "left",
                    boxSizing: "border-box",
                  }}
                  onMouseEnter={e => { e.currentTarget.style.background = "#3c3c3c"; }}
                  onMouseLeave={e => { e.currentTarget.style.background = "transparent"; }}
                >
                  <span
                    style={{
                      display: "inline-block",
                      width: 10,
                      height: 10,
                      borderRadius: 2,
                      background: p.visible ? "#0e639c" : "transparent",
                      border: p.visible ? "none" : "1px solid #3c3c3c",
                      flexShrink: 0,
                    }}
                  />
                  {p.name}
                </button>
              ))}
            </div>
          )}
        </div>
      )}

      {/* Right side: ProjectContextButton + connection button */}
      <div style={{ display: "flex", alignItems: "center", gap: 6, flexShrink: 0 }}>
        <ProjectContextButton />
        <ConnectionButton />
      </div>
    </div>
  );
}
