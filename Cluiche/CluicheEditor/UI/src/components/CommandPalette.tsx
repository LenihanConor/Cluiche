import { useEffect, useRef, useState } from "react";
import Fuse from "fuse.js";
import { EditorBridge } from "../bridge/EditorBridge";

interface Command {
  id: string;
  label: string;
}

interface Props {
  onClose: () => void;
}

export function CommandPalette({ onClose }: Props) {
  const [query, setQuery] = useState("");
  const [commands, setCommands] = useState<Command[]>([]);
  const [selected, setSelected] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    inputRef.current?.focus();
    EditorBridge.getPanels().then((result) => {
      const panels = (result as { panels: string[] }).panels ?? [];
      setCommands(panels.map((p) => ({ id: p, label: p })));
    }).catch(() => {});
  }, []);

  const fuse = new Fuse(commands, { keys: ["label"], threshold: 0.4 });
  const results = query ? fuse.search(query).map((r) => r.item) : commands;

  function handleKey(e: React.KeyboardEvent) {
    if (e.key === "ArrowDown") setSelected((s) => Math.min(s + 1, results.length - 1));
    else if (e.key === "ArrowUp") setSelected((s) => Math.max(s - 1, 0));
    else if (e.key === "Enter" && results[selected]) {
      EditorBridge.executeCommand(results[selected].id).catch(() => {});
      onClose();
    } else if (e.key === "Escape") onClose();
  }

  return (
    <div style={{ position: "fixed", top: 0, left: 0, right: 0, bottom: 0, background: "rgba(0,0,0,0.5)", display: "flex", alignItems: "flex-start", justifyContent: "center", paddingTop: 80, zIndex: 1000 }}>
      <div style={{ background: "#252526", border: "1px solid #454545", width: 480, borderRadius: 4, overflow: "hidden" }}>
        <input
          ref={inputRef}
          value={query}
          onChange={(e) => { setQuery(e.target.value); setSelected(0); }}
          onKeyDown={handleKey}
          placeholder="Search commands..."
          style={{ width: "100%", padding: "8px 12px", background: "#3c3c3c", border: "none", color: "#d4d4d4", fontSize: 14, outline: "none" }}
        />
        <div style={{ maxHeight: 300, overflow: "auto" }}>
          {results.map((cmd, i) => (
            <div key={cmd.id} onClick={() => { EditorBridge.executeCommand(cmd.id).catch(() => {}); onClose(); }}
              style={{ padding: "6px 12px", cursor: "pointer", background: i === selected ? "#094771" : "transparent", color: "#d4d4d4", fontSize: 13 }}>
              {cmd.label}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
