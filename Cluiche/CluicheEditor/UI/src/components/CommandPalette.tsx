import { useEffect, useRef, useState } from "react";
import Fuse from "fuse.js";
import { EditorBridge, CommandInfo } from "../bridge/EditorBridge";

interface Props {
  onClose: () => void;
}

export function CommandPalette({ onClose }: Props) {
  const [query, setQuery] = useState("");
  const [commands, setCommands] = useState<CommandInfo[]>([]);
  const [selected, setSelected] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    inputRef.current?.focus();
    EditorBridge.getCommands()
      .then((result) => {
        setCommands(result?.commands ?? []);
      })
      .catch(() => {});
  }, []);

  const fuse = new Fuse(commands, { keys: ["label", "id"], threshold: 0.4 });
  const results = query ? fuse.search(query).map((r) => r.item) : commands;

  function run(cmd: CommandInfo) {
    EditorBridge.executeCommand(cmd.id);
    onClose();
  }

  function handleKey(e: React.KeyboardEvent) {
    if (e.key === "ArrowDown") setSelected((s) => Math.min(s + 1, results.length - 1));
    else if (e.key === "ArrowUp") setSelected((s) => Math.max(s - 1, 0));
    else if (e.key === "Enter" && results[selected]) run(results[selected]);
    else if (e.key === "Escape") onClose();
  }

  return (
    <div
      style={{
        position: "fixed", top: 0, left: 0, right: 0, bottom: 0,
        background: "rgba(0,0,0,0.5)", display: "flex",
        alignItems: "flex-start", justifyContent: "center",
        paddingTop: 80, zIndex: 1000,
      }}
      onClick={onClose}
    >
      <div
        onClick={(e) => e.stopPropagation()}
        style={{ background: "#252526", border: "1px solid #454545", width: 480, borderRadius: 4, overflow: "hidden" }}
      >
        <input
          ref={inputRef}
          value={query}
          onChange={(e) => { setQuery(e.target.value); setSelected(0); }}
          onKeyDown={handleKey}
          placeholder={commands.length === 0 ? "No commands registered" : "Search commands..."}
          style={{ width: "100%", padding: "8px 12px", background: "#3c3c3c", border: "none", color: "#d4d4d4", fontSize: 14, outline: "none", boxSizing: "border-box" }}
        />
        <div style={{ maxHeight: 300, overflow: "auto" }}>
          {results.map((cmd, i) => (
            <div
              key={cmd.id}
              onClick={() => run(cmd)}
              onMouseEnter={() => setSelected(i)}
              style={{
                padding: "6px 12px", cursor: "pointer",
                background: i === selected ? "#094771" : "transparent",
                color: "#d4d4d4", fontSize: 13,
                display: "flex", justifyContent: "space-between",
              }}
            >
              <span>{cmd.label}</span>
              <span style={{ opacity: 0.5, fontSize: 11, fontFamily: "monospace" }}>{cmd.id}</span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
