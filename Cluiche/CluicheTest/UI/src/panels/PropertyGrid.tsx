import { useEffect, useState } from "react";
import { GameBridge } from "../bridge/GameBridge";

type Property = { key: string; label: string; value: string | number | boolean; editable?: boolean };

export function PropertyGrid() {
  const [title, setTitle] = useState("Properties");
  const [props, setProps] = useState<Property[]>([]);
  const [editing, setEditing] = useState<string | null>(null);
  const [draft, setDraft] = useState("");

  useEffect(() => {
    return GameBridge.subscribe("property_grid", (data) => {
      const d = data as { title?: string; properties: Property[] };
      setTitle(d.title ?? "Properties");
      setProps(d.properties ?? []);
    });
  }, []);

  function commit(key: string) {
    setProps((prev) => prev.map((p) => (p.key === key ? { ...p, value: draft } : p)));
    GameBridge.send("onPropertyChanged", { key, value: draft });
    setEditing(null);
  }

  return (
    <div className="flex flex-col gap-1 p-3 bg-base-200 rounded">
      <span className="text-xs font-bold uppercase tracking-widest opacity-60 mb-1">{title}</span>

      {props.length === 0 ? (
        <p className="text-xs opacity-40">Waiting for data…</p>
      ) : (
        <table className="table table-xs w-full">
          <tbody>
            {props.map((p) => (
              <tr key={p.key} className="hover">
                <td className="opacity-60 w-1/2">{p.label}</td>
                <td>
                  {p.editable && editing === p.key ? (
                    <input
                      autoFocus
                      className="input input-xs input-bordered w-full"
                      value={draft}
                      onChange={(e) => setDraft(e.target.value)}
                      onBlur={() => commit(p.key)}
                      onKeyDown={(e) => { if (e.key === "Enter") commit(p.key); if (e.key === "Escape") setEditing(null); }}
                    />
                  ) : (
                    <span
                      className={`font-mono ${p.editable ? "cursor-pointer hover:text-primary" : ""}`}
                      onClick={() => { if (p.editable) { setEditing(p.key); setDraft(String(p.value)); } }}
                    >
                      {typeof p.value === "boolean" ? (p.value ? "true" : "false") : String(p.value)}
                    </span>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  );
}
