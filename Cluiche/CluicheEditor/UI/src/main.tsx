import { useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import { DockingManager } from "./layout/DockingManager";
import { CommandPalette } from "./components/CommandPalette";
import "./bridge/EditorBridge";

function App() {
  const [paletteOpen, setPaletteOpen] = useState(false);

  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      if (e.key === "F1" || (e.ctrlKey && e.shiftKey && e.key === "P")) {
        e.preventDefault();
        setPaletteOpen(true);
      }
    }
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, []);

  return (
    <>
      <DockingManager />
      {paletteOpen && <CommandPalette onClose={() => setPaletteOpen(false)} />}
    </>
  );
}

createRoot(document.getElementById("root")!).render(<App />);
