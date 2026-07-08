import { useCallback, useEffect, useMemo, useState } from "react";
import { Mosaic, MosaicWindow, MosaicNode } from "react-mosaic-component";
import "react-mosaic-component/react-mosaic-component.css";
import { EditorBridge, PanelInfo } from "../bridge/EditorBridge";
import { Toolbar } from "./Toolbar";

type PanelId = string;

function buildTree(panelNames: PanelId[]): MosaicNode<PanelId> | null {
  if (panelNames.length === 0) return null;
  if (panelNames.length === 1) return panelNames[0];

  const mid = Math.ceil(panelNames.length / 2);
  const left = buildTree(panelNames.slice(0, mid));
  const right = buildTree(panelNames.slice(mid));
  if (left == null) return right;
  if (right == null) return left;
  return {
    direction: panelNames.length > 2 ? "column" : "row",
    first: left,
    second: right,
    splitPercentage: 50,
  };
}

function collectLeaves(node: MosaicNode<PanelId> | null): PanelId[] {
  if (node == null) return [];
  if (typeof node === "string") return [node];
  return [...collectLeaves(node.first), ...collectLeaves(node.second)];
}

function dedupTree(
  node: MosaicNode<PanelId> | null,
  seen: Set<PanelId> = new Set()
): MosaicNode<PanelId> | null {
  if (node == null) return null;
  if (typeof node === "string") {
    if (seen.has(node)) return null;
    seen.add(node);
    return node;
  }
  const first = dedupTree(node.first, seen);
  const second = dedupTree(node.second, seen);
  if (first == null) return second;
  if (second == null) return first;
  return { ...node, first, second };
}

function removeFromLayout(
  node: MosaicNode<PanelId> | null,
  id: PanelId
): MosaicNode<PanelId> | null {
  if (node == null) return null;
  if (typeof node === "string") return node === id ? null : node;
  const first = removeFromLayout(node.first, id);
  const second = removeFromLayout(node.second, id);
  if (first == null) return second;
  if (second == null) return first;
  return { ...node, first, second };
}

function addToLayout(
  node: MosaicNode<PanelId> | null,
  id: PanelId
): MosaicNode<PanelId> {
  if (node == null) return id;
  return {
    direction: "row",
    first: node,
    second: id,
    splitPercentage: 70,
  };
}

interface DockingManagerProps {
  onReady?: () => void;
}

export function DockingManager({ onReady }: DockingManagerProps) {
  const [panels, setPanels] = useState<PanelInfo[]>([]);
  const [layout, setLayout] = useState<MosaicNode<PanelId> | null>(null);
  const [savedLayout, setSavedLayout] = useState<MosaicNode<PanelId> | null>(null);
  const [initialized, setInitialized] = useState(false);
  const [fullscreenPanel, setFullscreenPanel] = useState<PanelId | null>(null);

  const panelMap = useMemo(() => {
    const m = new Map<PanelId, PanelInfo>();
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
            const savedTree = (saved as { tree?: MosaicNode<PanelId> })?.tree;
            if (savedTree) {
              const clean = dedupTree(savedTree);
              setLayout(clean);
              if (clean) EditorBridge.saveLayout({ tree: clean }).catch(() => { EditorBridge.notify({ level: "warning", title: "Failed to save layout" }); });
            } else {
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
    const onMouseDown = (e: MouseEvent) => {
      if ((e.target as Element)?.closest(".mosaic-split")) {
        document.body.classList.add("is-resizing");
        const cleanup = () => { document.body.classList.remove("is-resizing"); };
        document.addEventListener("mouseup", cleanup, { once: true });
      }
    };
    document.addEventListener("mousedown", onMouseDown);
    return () => document.removeEventListener("mousedown", onMouseDown);
  }, []);

  useEffect(() => {
    return EditorBridge.subscribe("panels_changed", (data: unknown) => {
      const d = data as { panels?: PanelInfo[] } | null;
      if (!d?.panels) return;

      const newPanels = d.panels;
      setPanels((prevPanels) => {
        const prevNames = new Set(prevPanels.map((p) => p.name));
        const newNames = new Set(newPanels.map((p) => p.name));

        // Panels just added (not in prev) and visible — add to tree.
        // Panels just removed (not in new) — remove from tree.
        // Panels already known — track visibility toggles.
        const added = newPanels.filter((p) => !prevNames.has(p.name) && p.visible);
        const removed = [...prevNames].filter((n) => !newNames.has(n));
        const toggledOn = newPanels.filter((p) => {
          const prev = prevPanels.find((pp) => pp.name === p.name);
          return prev && !prev.visible && p.visible;
        });
        const toggledOff = newPanels.filter((p) => {
          const prev = prevPanels.find((pp) => pp.name === p.name);
          return prev && prev.visible && !p.visible;
        });

        if (added.length > 0 || removed.length > 0 || toggledOn.length > 0 || toggledOff.length > 0) {
          setLayout((prev) => {
            let updated = prev;
            removed.forEach((id) => { updated = removeFromLayout(updated, id); });
            toggledOff.forEach((p) => { updated = removeFromLayout(updated, p.name); });
            added.forEach((p) => { updated = addToLayout(updated, p.name); });
            toggledOn.forEach((p) => { updated = addToLayout(updated, p.name); });
            return updated;
          });
        }

        return newPanels;
      });
    });
  }, []);

  const handleChange = useCallback(
    (newLayout: MosaicNode<PanelId> | null) => {
      setLayout(newLayout);
      if (newLayout) EditorBridge.saveLayout({ tree: newLayout }).catch(() => { EditorBridge.notify({ level: "warning", title: "Failed to save layout" }); });
    },
    []
  );

  const handlePanelClose = useCallback(
    (id: PanelId) => {
      if (fullscreenPanel === id) setFullscreenPanel(null);
      EditorBridge.togglePanelVisibility(id);
      setLayout((prev) => removeFromLayout(prev, id));
    },
    [fullscreenPanel]
  );

  const handleFullscreen = useCallback(
    (id: PanelId) => {
      setFullscreenPanel((prev) => {
        if (prev === id) {
          // Exit fullscreen: restore saved layout
          setSavedLayout((saved) => { setLayout(saved); return null; });
          return null;
        } else {
          // Enter fullscreen: save current layout
          setLayout((current) => { setSavedLayout(current); return current; });
          return id;
        }
      });
    },
    []
  );

  function renderTile(id: PanelId, path: any) {
    const info = panelMap.get(id);
    const src = info?.uiPath ?? `dia://editor/${id.toLowerCase().replace(/\s+/g, "-")}/index.html`;
    const isFullscreen = fullscreenPanel === id;
    return (
      <MosaicWindow<PanelId>
        path={path}
        title={id}
        createNode={() => panels[0]?.name ?? id}
        toolbarControls={[
          <button
            key="fullscreen"
            onClick={() => handleFullscreen(id)}
            title={isFullscreen ? "Exit fullscreen" : "Fullscreen"}
            className="mosaic-default-control bp4-button bp4-minimal"
            style={{
              background: "transparent",
              border: "none",
              cursor: "pointer",
              color: "#999",
              fontSize: 12,
              lineHeight: 1,
              padding: "0 4px",
            }}
          >
            {isFullscreen ? "⊡" : "⊞"}
          </button>,
          <button
            key="close"
            onClick={() => handlePanelClose(id)}
            title="Hide panel"
            className="mosaic-default-control bp4-button bp4-minimal"
            style={{
              background: "transparent",
              border: "none",
              cursor: "pointer",
              color: "#999",
              fontSize: 14,
              lineHeight: 1,
              padding: "0 4px",
            }}
          >
            ×
          </button>,
        ]}
      >
        <iframe
          key={id}
          src={src}
          style={{ width: "100%", height: "100%", border: "none" }}
          title={id}
        />
      </MosaicWindow>
    );
  }

  if (!initialized) {
    return (
      <div style={{ padding: 16, color: "#888", fontFamily: "monospace" }}>
        Loading panels…
      </div>
    );
  }

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
      <div style={{ flex: 1, position: "relative" }}>
        {fullscreenPanel ? (
          <div style={{ width: "100%", height: "100%", display: "flex", flexDirection: "column" }}>
            <div
              style={{
                display: "flex",
                alignItems: "center",
                height: 30,
                background: "#1e1e1e",
                borderBottom: "1px solid #3c3c3c",
                padding: "0 8px",
                flexShrink: 0,
              }}
            >
              <span style={{ flex: 1, fontSize: 12, color: "#ccc", fontFamily: "Segoe UI, system-ui, sans-serif" }}>
                {fullscreenPanel}
              </span>
              <button
                onClick={() => handleFullscreen(fullscreenPanel)}
                title="Exit fullscreen"
                style={{
                  background: "transparent",
                  border: "none",
                  cursor: "pointer",
                  color: "#999",
                  fontSize: 12,
                  lineHeight: 1,
                  padding: "0 4px",
                }}
              >
                ⊡
              </button>
            </div>
            <iframe
              key={fullscreenPanel}
              src={panelMap.get(fullscreenPanel)?.uiPath ?? `dia://editor/${fullscreenPanel.toLowerCase().replace(/\s+/g, "-")}/index.html`}
              style={{ flex: 1, width: "100%", border: "none" }}
              title={fullscreenPanel}
            />
          </div>
        ) : layout ? (
          <Mosaic<PanelId>
            className="mosaic-blueprint-theme bp4-dark"
            renderTile={renderTile}
            value={layout}
            onChange={handleChange}
          />
        ) : (
          <div style={{ padding: 16, color: "#888", fontFamily: "monospace" }}>
            No editor panels visible. Use the toolbar to show a panel.
          </div>
        )}
      </div>
      <Toolbar panels={panels} />
    </div>
  );
}
