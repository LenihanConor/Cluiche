import { useEffect, useState } from "react";
import { GameBridge } from "../bridge/GameBridge";

type TimelineItem = { id: string; label: string; start: number; end: number; color?: string; track: number };

const TRACK_H = 24;
const LABEL_W = 80;
const PAD = 4;

export function Timeline() {
  const [items, setItems] = useState<TimelineItem[]>([]);
  const [totalTime, setTotalTime] = useState(100);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const W = 400;

  useEffect(() => {
    return GameBridge.subscribe("timeline", (data) => {
      const d = data as { items: TimelineItem[]; totalTime?: number };
      setItems(d.items ?? []);
      setTotalTime(d.totalTime ?? 100);
    });
  }, []);

  const numTracks = items.length ? Math.max(...items.map((i) => i.track)) + 1 : 1;
  const svgH = numTracks * TRACK_H + PAD * 2;

  function toX(t: number) { return LABEL_W + ((t / totalTime) * (W - LABEL_W - PAD)); }

  function select(id: string) {
    setSelectedId(id);
    GameBridge.send("onTimelineItemSelected", { id });
  }

  return (
    <div className="flex flex-col gap-2 p-3 bg-base-200 rounded overflow-auto">
      <span className="text-xs font-bold uppercase tracking-widest opacity-60">Timeline</span>

      {items.length === 0 ? (
        <p className="text-xs opacity-40">Waiting for data…</p>
      ) : (
        <svg width={W} height={svgH} className="bg-base-300 rounded font-mono text-xs">
          {Array.from({ length: numTracks }, (_, t) => (
            <line
              key={t}
              x1={LABEL_W}
              y1={PAD + t * TRACK_H + TRACK_H / 2}
              x2={W - PAD}
              y2={PAD + t * TRACK_H + TRACK_H / 2}
              stroke="#ffffff08"
              strokeWidth={TRACK_H - 2}
            />
          ))}

          {items.map((item) => {
            const x1 = toX(item.start);
            const x2 = toX(item.end);
            const y = PAD + item.track * TRACK_H + 2;
            const bw = Math.max(x2 - x1, 4);
            const isSelected = selectedId === item.id;
            return (
              <g key={item.id} onClick={() => select(item.id)} className="cursor-pointer">
                <rect
                  x={x1} y={y} width={bw} height={TRACK_H - 4}
                  rx={2}
                  fill={item.color ?? "#3b82f6"}
                  opacity={isSelected ? 1 : 0.75}
                  stroke={isSelected ? "#fff" : "none"}
                  strokeWidth={1}
                />
                {bw > 30 && (
                  <text x={x1 + 4} y={y + TRACK_H - 8} fill="#fff" fontSize={9} dominantBaseline="middle">
                    {item.label}
                  </text>
                )}
              </g>
            );
          })}

          {/* Track labels */}
          {Array.from({ length: numTracks }, (_, t) => (
            <text key={t} x={2} y={PAD + t * TRACK_H + TRACK_H / 2} fill="#ffffff50" fontSize={9} dominantBaseline="middle">
              {`Track ${t}`}
            </text>
          ))}
        </svg>
      )}
    </div>
  );
}
