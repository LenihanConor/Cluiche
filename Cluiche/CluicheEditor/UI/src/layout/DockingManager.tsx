import { useEffect, useMemo, useState } from "react";
import { Mosaic, MosaicWindow, MosaicNode } from "react-mosaic-component";
import "react-mosaic-component/react-mosaic-component.css";
import { EditorBridge, PanelInfo } from "../bridge/EditorBridge";
import { OutputConsole } from "../components/OutputConsole";

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

export function DockingManager() {
  const [panels, setPanels] = useState<PanelInfo[]>([]);
  const [layout, setLayout] = useState<MosaicNode<PanelId> | null>(null);
  const [initialized, setInitialized] = useState(false);

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
              setLayout(buildTree(list.map((p) => p.name)));
            }
          })
          .catch(() => setLayout(buildTree(list.map((p) => p.name))))
          .finally(() => setInitialized(true));
      })
      .catch(() => {
        setLayout(null);
        setInitialized(true);
      });
  }, []);

  function handleChange(newLayout: MosaicNode<PanelId> | null) {
    setLayout(newLayout);
    if (newLayout) EditorBridge.saveLayout({ tree: newLayout }).catch(() => {});
  }

  function renderTile(id: PanelId, path: any) {
    const info = panelMap.get(id);
    const src = info?.uiPath ?? `dia://editor/${id.toLowerCase().replace(/\s+/g, "-")}/index.html`;
    return (
      <MosaicWindow<PanelId>
        path={path}
        title={id}
        createNode={() => panels[0]?.name ?? id}
      >
        {id === "Output Console" ? (
          <OutputConsole />
        ) : (
          <iframe
            src={src}
            style={{ width: "100%", height: "100%", border: "none" }}
            title={id}
          />
        )}
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

  if (!layout) {
    return (
      <div style={{ padding: 16, color: "#888", fontFamily: "monospace" }}>
        No editor panels registered.
      </div>
    );
  }

  return <Mosaic<PanelId> renderTile={renderTile} value={layout} onChange={handleChange} />;
}
