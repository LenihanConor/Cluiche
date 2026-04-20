import { useEffect, useRef, useState } from "react";

interface ConsoleEntry {
  id: number;
  level: "info" | "warning" | "error";
  message: string;
  timestamp: string;
}

export function OutputConsole() {
  const [entries, setEntries] = useState<ConsoleEntry[]>([]);
  const bottomRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    (window as unknown as Record<string, unknown>)["__dia_console_push"] = (entry: ConsoleEntry) => {
      setEntries((prev) => [...prev.slice(-999), entry]);
    };
    return () => {
      delete (window as unknown as Record<string, unknown>)["__dia_console_push"];
    };
  }, []);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [entries]);

  const levelColor = { info: "#d4d4d4", warning: "#dcdcaa", error: "#f44747" };

  return (
    <div style={{ height: "100%", overflow: "auto", padding: "4px 8px", background: "#1e1e1e" }}>
      {entries.map((e) => (
        <div key={e.id} style={{ color: levelColor[e.level], fontSize: 12, lineHeight: "1.4" }}>
          <span style={{ opacity: 0.5 }}>[{e.timestamp}] </span>
          {e.message}
        </div>
      ))}
      <div ref={bottomRef} />
    </div>
  );
}
