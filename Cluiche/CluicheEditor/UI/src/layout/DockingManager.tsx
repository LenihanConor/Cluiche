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
              setLayout(savedTree);
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
    return EditorBridge.subscribe("panels_changed", (data: unknown) => {
      const d = data as { panels?: PanelInfo[] } | null;
      if (!d?.panels) return;

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

  const handleChange = useCallback(
    (newLayout: MosaicNode<PanelId> | null) => {
      setLayout(newLayout);
      if (newLayout) EditorBridge.saveLayout({ tree: newLayout }).catch(() => {});
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
      setFullscreenPanel((prev) => (prev === id ? null : id));
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

  const fullscreenInfo = fullscreenPanel ? panelMap.get(fullscreenPanel) : null;
  const fullscreenSrc = fullscreenInfo?.uiPath
    ?? (fullscreenPanel ? `dia://editor/${fullscreenPanel.toLowerCase().replace(/\s+/g, "-")}/index.html` : "");

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
      <div style={{ flex: 1, position: "relative" }}>
        {fullscreenPanel ? (
          <div style={{ display: "flex", flexDirection: "column", height: "100%", background: "#1e1e1e" }}>
            <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", padding: "4px 8px", background: "#252526", borderBottom: "1px solid #3a3a3a" }}>
              <span style={{ color: "#ccc", fontFamily: "monospace", fontSize: 13 }}>{fullscreenPanel}</span>
              <div style={{ display: "flex", gap: 4 }}>
                <button
                  onClick={() => setFullscreenPanel(null)}
                  title="Exit fullscreen"
                  style={{ background: "transparent", border: "none", cursor: "pointer", color: "#999", fontSize: 12, lineHeight: 1, padding: "0 4px" }}
                >
                  ⊡
                </button>
                <button
                  onClick={() => handlePanelClose(fullscreenPanel)}
                  title="Hide panel"
                  style={{ background: "transparent", border: "none", cursor: "pointer", color: "#999", fontSize: 14, lineHeight: 1, padding: "0 4px" }}
                >
                  ×
                </button>
              </div>
            </div>
            <iframe
              src={fullscreenSrc}
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
