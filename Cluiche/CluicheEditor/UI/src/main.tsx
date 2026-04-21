import { useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import { DockingManager } from "./layout/DockingManager";
import { CommandPalette } from "./components/CommandPalette";
import { EditorBridge } from "./bridge/EditorBridge";

function App() {
  const [paletteOpen, setPaletteOpen] = useState(false);
  const [lastAction, setLastAction] = useState<string>("");

  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      if (e.key === "F1" || (e.ctrlKey && e.shiftKey && e.key === "P")) {
        e.preventDefault();
        setPaletteOpen(true);
        return;
      }

      if (e.ctrlKey && !e.shiftKey && (e.key === "z" || e.key === "Z")) {
        e.preventDefault();
        EditorBridge.undo();
        setLastAction("undo");
        return;
      }

      if (e.ctrlKey && (e.key === "y" || e.key === "Y" ||
          (e.shiftKey && (e.key === "z" || e.key === "Z")))) {
        e.preventDefault();
        EditorBridge.redo();
        setLastAction("redo");
        return;
      }
    }

    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, []);

  return (
    <>
      <DockingManager />
      {paletteOpen && <CommandPalette onClose={() => setPaletteOpen(false)} />}
      {lastAction && (
        <div style={{
          position: "fixed", bottom: 8, right: 8, padding: "4px 10px",
          background: "rgba(0,0,0,0.7)", color: "#fff", borderRadius: 4,
          fontFamily: "monospace", fontSize: 12, pointerEvents: "none",
        }}>
          last: {lastAction}
        </div>
      )}
    </>
  );
}

createRoot(document.getElementById("root")!).render(<App />);
