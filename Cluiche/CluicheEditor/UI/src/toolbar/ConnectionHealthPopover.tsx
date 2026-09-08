import { useEffect, useRef } from "react";
import { ConnectionHealth, SubscriptionAck, TopicHealth } from "./useConnectionHealth";

interface Props {
  health: ConnectionHealth;
  onClose: () => void;
}

export function ConnectionHealthPopover({ health, onClose }: Props) {
  const ref = useRef<HTMLDivElement>(null);

  // Click-away to dismiss
  useEffect(() => {
    function handleClick(e: MouseEvent) {
      if (ref.current && !ref.current.contains(e.target as Node))
        onClose();
    }
    document.addEventListener("mousedown", handleClick);
    return () => document.removeEventListener("mousedown", handleClick);
  }, [onClose]);

  // Escape to dismiss
  useEffect(() => {
    function handleKey(e: KeyboardEvent) {
      if (e.key === "Escape") onClose();
    }
    document.addEventListener("keydown", handleKey);
    return () => document.removeEventListener("keydown", handleKey);
  }, [onClose]);

  return (
    <div
      ref={ref}
      style={{
        position: "absolute",
        bottom: "calc(100% + 8px)",
        right: 0,
        width: 380,
        background: "#2d2d30",
        border: "1px solid #3c3c3c",
        borderRadius: 4,
        boxShadow: "0 8px 24px rgba(0,0,0,0.6)",
        zIndex: 2000,
        fontFamily: "Segoe UI, system-ui, sans-serif",
        fontSize: 11,
        color: "#cccccc",
        overflow: "hidden",
      }}
    >
      {/* Header */}
      <div style={{
        display: "flex",
        alignItems: "center",
        justifyContent: "space-between",
        padding: "8px 12px",
        borderBottom: "1px solid #3c3c3c",
        fontWeight: "bold",
      }}>
        <span>Connection Health</span>
        <button
          onClick={onClose}
          style={{
            background: "transparent",
            border: "none",
            color: "#888",
            cursor: "pointer",
            fontSize: 14,
            lineHeight: 1,
            padding: "0 2px",
          }}
          onMouseEnter={(e) => { e.currentTarget.style.color = "#cccccc"; }}
          onMouseLeave={(e) => { e.currentTarget.style.color = "#888"; }}
        >
          ✕
        </button>
      </div>

      {/* Topics table */}
      <div style={{ padding: "8px 12px 4px" }}>
        <div style={{ color: "#888", fontSize: 10, marginBottom: 4, textTransform: "uppercase", letterSpacing: "0.05em" }}>Topics</div>
        {health.topics.length === 0 ? (
          <div style={{ color: "#666", fontStyle: "italic", padding: "4px 0" }}>No active subscriptions</div>
        ) : (
          <table style={{ width: "100%", borderCollapse: "collapse" }}>
            <thead>
              <tr style={{ color: "#888", fontSize: 10 }}>
                <th style={{ textAlign: "left", padding: "2px 4px 4px 0", fontWeight: "normal" }}>Topic</th>
                <th style={{ textAlign: "right", padding: "2px 4px 4px", fontWeight: "normal" }}>Received</th>
                <th style={{ textAlign: "right", padding: "2px 4px 4px", fontWeight: "normal" }}>Dropped</th>
                <th style={{ textAlign: "right", padding: "2px 0 4px 4px", fontWeight: "normal" }}>Rate</th>
              </tr>
            </thead>
            <tbody>
              {health.topics.map((t) => (
                <TopicRow key={t.topic} topic={t} />
              ))}
            </tbody>
          </table>
        )}
      </div>

      <div style={{ height: 1, background: "#3c3c3c", margin: "4px 0" }} />

      {/* Subscriptions section */}
      <div style={{ padding: "4px 12px 10px" }}>
        <div style={{ color: "#888", fontSize: 10, marginBottom: 4, textTransform: "uppercase", letterSpacing: "0.05em" }}>Subscriptions</div>
        {health.subscriptions.length === 0 ? (
          <div style={{ color: "#666", fontStyle: "italic" }}>No subscriptions</div>
        ) : (
          health.subscriptions.map((s) => (
            <SubscriptionRow key={s.topic} ack={s} />
          ))
        )}
      </div>
    </div>
  );
}

function topicRowColor(dropRate: number): string {
  if (dropRate <= 0) return "#89d185";      // green
  if (dropRate < 5) return "#dcdcaa";       // yellow
  return "#f48771";                          // red
}

function TopicRow({ topic }: { topic: TopicHealth }) {
  const color = topicRowColor(topic.dropRate);
  return (
    <tr>
      <td style={{ padding: "2px 4px 2px 0", color: "#cccccc", maxWidth: 140, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
        {topic.topic}
      </td>
      <td style={{ textAlign: "right", padding: "2px 4px", color: "#cccccc" }}>
        {topic.sent.toLocaleString()}
      </td>
      <td style={{ textAlign: "right", padding: "2px 4px", color: topic.dropped > 0 ? color : "#cccccc" }}>
        {topic.dropped.toLocaleString()}
      </td>
      <td style={{ textAlign: "right", padding: "2px 0 2px 4px" }}>
        <span style={{ color, display: "inline-flex", alignItems: "center", gap: 4 }}>
          {topic.dropRate.toFixed(1)}%
          <span style={{ display: "inline-block", width: 6, height: 6, borderRadius: "50%", background: color }} />
        </span>
      </td>
    </tr>
  );
}

function SubscriptionRow({ ack }: { ack: SubscriptionAck }) {
  let statusText: string;
  let statusColor: string;

  switch (ack.status) {
    case "acked":
      statusText = `✓ ACK (${Math.round(ack.latencyMs ?? 0)}ms)`;
      statusColor = "#89d185";
      break;
    case "pending":
      statusText = `⏳ Pending (${Math.round(ack.elapsedMs ?? 0)}ms)`;
      statusColor = "#dcdcaa";
      break;
    case "timeout":
      statusText = "⚠ No ACK (timeout 3s)";
      statusColor = "#f48771";
      break;
  }

  return (
    <div style={{ display: "flex", justifyContent: "space-between", padding: "2px 0", color: "#cccccc" }}>
      <span style={{ maxWidth: 200, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>{ack.topic}</span>
      <span style={{ color: statusColor, fontSize: 10 }}>{statusText}</span>
    </div>
  );
}
