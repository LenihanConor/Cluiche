import { useEffect, useState } from "react";
import { Mosaic, MosaicWindow, MosaicNode } from "react-mosaic-component";
import "react-mosaic-component/react-mosaic-component.css";
import { EditorBridge } from "../bridge/EditorBridge";
import { OutputConsole } from "../components/OutputConsole";

type PanelId = string;

const DEFAULT_LAYOUT: MosaicNode<PanelId> = {
  direction: "column",
  first: "Output Console",
  second: "Output Console",
  splitPercentage: 70,
};

function PanelContent({ id }: { id: PanelId }) {
  if (id === "Output Console") return <OutputConsole />;
  return (
    <iframe
      src={`dia://editor/${id.toLowerCase().replace(/\s+/g, "-")}/index.html`}
      style={{ width: "100%", height: "100%", border: "none" }}
      title={id}
    />
  );
}

export function DockingManager() {
  const [layout, setLayout] = useState<MosaicNode<PanelId> | null>(DEFAULT_LAYOUT);

  useEffect(() => {
    EditorBridge.loadLayout().then((saved) => {
      if (saved && Object.keys(saved).length > 0) setLayout(saved as MosaicNode<PanelId>);
    }).catch(() => {});
  }, []);

  function handleChange(newLayout: MosaicNode<PanelId> | null) {
    setLayout(newLayout);
    if (newLayout) EditorBridge.saveLayout(newLayout as object).catch(() => {});
  }

  return (
    <Mosaic<PanelId>
      renderTile={(id, path) => (
        <MosaicWindow<PanelId> path={path} title={id} createNode={() => "Output Console"}>
          <PanelContent id={id} />
        </MosaicWindow>
      )}
      value={layout}
      onChange={handleChange}
    />
  );
}
