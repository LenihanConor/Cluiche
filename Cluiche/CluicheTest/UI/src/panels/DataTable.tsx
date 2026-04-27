import { useEffect, useState } from "react";
import { GameBridge } from "../bridge/GameBridge";

type Column = { key: string; label: string };
type Row = Record<string, string | number>;

type TableData = { columns: Column[]; rows: Row[] };

export function DataTable() {
  const [columns, setColumns] = useState<Column[]>([]);
  const [rows, setRows] = useState<Row[]>([]);
  const [sortKey, setSortKey] = useState<string | null>(null);
  const [sortAsc, setSortAsc] = useState(true);
  const [filter, setFilter] = useState("");

  useEffect(() => {
    return GameBridge.subscribe("data_table", (data) => {
      const d = data as TableData;
      setColumns(d.columns ?? []);
      setRows(d.rows ?? []);
    });
  }, []);

  function toggleSort(key: string) {
    if (sortKey === key) setSortAsc((a) => !a);
    else { setSortKey(key); setSortAsc(true); }
    GameBridge.send("onTableSort", { key, asc: sortKey === key ? !sortAsc : true });
  }

  const filtered = filter
    ? rows.filter((r) => Object.values(r).some((v) => String(v).toLowerCase().includes(filter.toLowerCase())))
    : rows;

  const sorted = sortKey
    ? [...filtered].sort((a, b) => {
        const av = a[sortKey]; const bv = b[sortKey];
        const cmp = av < bv ? -1 : av > bv ? 1 : 0;
        return sortAsc ? cmp : -cmp;
      })
    : filtered;

  return (
    <div className="flex flex-col gap-2 p-3 bg-base-200 rounded h-full">
      <div className="flex items-center gap-2">
        <span className="text-xs font-bold uppercase tracking-widest opacity-60">Data Table</span>
        <input
          className="input input-xs input-bordered flex-1"
          placeholder="filter…"
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
        />
      </div>

      {rows.length === 0 ? (
        <p className="text-xs opacity-40">Waiting for data…</p>
      ) : (
        <div className="overflow-auto">
          <table className="table table-xs w-full">
            <thead>
              <tr>
                {columns.map((c) => (
                  <th
                    key={c.key}
                    className="cursor-pointer select-none"
                    onClick={() => toggleSort(c.key)}
                  >
                    {c.label}
                    {sortKey === c.key && (sortAsc ? " ▲" : " ▼")}
                  </th>
                ))}
              </tr>
            </thead>
            <tbody>
              {sorted.map((row, i) => (
                <tr key={i} className="hover">
                  {columns.map((c) => (
                    <td key={c.key}>{row[c.key] ?? "—"}</td>
                  ))}
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}
