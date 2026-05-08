import { useEffect, useState } from "react";
import { GameBridge } from "../bridge/GameBridge";

type Series = { label: string; color: string; points: number[] };

const W = 320;
const H = 120;
const PAD = 24;

function polyline(points: number[]): string {
  if (points.length < 2) return "";
  const min = Math.min(...points);
  const max = Math.max(...points);
  const range = max - min || 1;
  const step = (W - PAD * 2) / (points.length - 1);
  return points
    .map((v, i) => {
      const x = PAD + i * step;
      const y = PAD + (1 - (v - min) / range) * (H - PAD * 2);
      return `${x},${y}`;
    })
    .join(" ");
}

export function ChartGraph() {
  const [series, setSeries] = useState<Series[]>([]);

  useEffect(() => {
    return GameBridge.subscribe("chart_graph", (data) => {
      setSeries((data as { series: Series[] }).series ?? []);
    });
  }, []);

  const allPoints = series.flatMap((s) => s.points);
  const globalMin = allPoints.length ? Math.min(...allPoints) : 0;
  const globalMax = allPoints.length ? Math.max(...allPoints) : 1;

  return (
    <div className="flex flex-col gap-2 p-3 bg-base-200 rounded">
      <span className="text-xs font-bold uppercase tracking-widest opacity-60">Chart</span>

      {series.length === 0 ? (
        <p className="text-xs opacity-40">Waiting for data…</p>
      ) : (
        <>
          <svg width={W} height={H} className="bg-base-300 rounded">
            {/* Grid lines */}
            {[0, 0.25, 0.5, 0.75, 1].map((t) => {
              const y = PAD + t * (H - PAD * 2);
              const val = (globalMax - (globalMax - globalMin) * t).toFixed(1);
              return (
                <g key={t}>
                  <line x1={PAD} y1={y} x2={W - PAD} y2={y} stroke="#ffffff10" strokeWidth={1} />
                  <text x={2} y={y + 3} fill="#ffffff40" fontSize={8}>{val}</text>
                </g>
              );
            })}
            {series.map((s) => (
              <polyline
                key={s.label}
                points={polyline(s.points)}
                fill="none"
                stroke={s.color}
                strokeWidth={1.5}
                strokeLinejoin="round"
                strokeLinecap="round"
              />
            ))}
          </svg>

          <div className="flex gap-3 flex-wrap">
            {series.map((s) => (
              <div key={s.label} className="flex items-center gap-1 text-xs">
                <span className="w-3 h-0.5 inline-block rounded" style={{ background: s.color }} />
                <span className="opacity-70">{s.label}</span>
                <span className="font-mono opacity-50">
                  {s.points.length ? s.points[s.points.length - 1].toFixed(1) : "—"}
                </span>
              </div>
            ))}
          </div>
        </>
      )}
    </div>
  );
}
