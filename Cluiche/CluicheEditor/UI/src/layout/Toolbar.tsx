import { useEffect, useState } from "react";
import { EditorBridge, PanelInfo } from "../bridge/EditorBridge";

interface ToolbarProps {
  panels: PanelInfo[];
}

export function Toolbar({ panels }: ToolbarProps) {
  const [connectionState, setConnectionState] = useState("disconnected");

  useEffect(() => {
    EditorBridge.request<{ state?: string }>("game_connection.get_state", {})
      .then((result) => {
        if (result?.state) setConnectionState(result.state);
      })
      .catch(() => {});

    return EditorBridge.subscribe("game_connection", (data: unknown) => {
      const d = data as { state?: string } | null;
      if (d?.state) setConnectionState(d.state);
    });
  }, []);

  function handleToggle(name: string) {
    EditorBridge.togglePanelVisibility(name);
  }

  function handleConnectionClick() {
    EditorBridge.togglePanelVisibility("Game Connection");
  }

  const isConnected = connectionState === "connected";

  return (
    <div style={{
      display: "flex",
      alignItems: "center",
      height: 28,
      background: "#252526",
      borderTop: "1px solid #3c3c3c",
      padding: "0 8px",
      flexShrink: 0,
      gap: 2,
    }}>
      <div style={{ display: "flex", gap: 2, flex: 1 }}>
        {panels.map((p) => (
          <button
            key={p.name}
            onClick={() => handleToggle(p.name)}
            title={p.name}
            style={{
              width: 24,
              height: 22,
              display: "flex",
              alignItems: "center",
              justifyContent: "center",
              background: p.visible ? "#0e639c" : "transparent",
              color: p.visible ? "#fff" : "#808080",
              border: p.visible ? "none" : "1px solid #3c3c3c",
              cursor: "pointer",
              fontSize: 11,
              fontWeight: 600,
              fontFamily: "Segoe UI, system-ui, sans-serif",
              borderRadius: 2,
              padding: 0,
            }}
          >
            {p.name.charAt(0).toUpperCase()}
          </button>
        ))}
      </div>
      <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
        <button
          onClick={handleConnectionClick}
          title={isConnected ? "Connected to game" : "Disconnected"}
          style={{
            display: "flex",
            alignItems: "center",
            gap: 4,
            background: "transparent",
            border: "none",
            cursor: "pointer",
            color: "#808080",
            fontSize: 11,
            fontFamily: "Segoe UI, system-ui, sans-serif",
            padding: "2px 6px",
          }}
        >
          <span style={{
            display: "inline-block",
            width: 7,
            height: 7,
            borderRadius: "50%",
            background: isConnected ? "#89d185" : "#f48771",
          }} />
          {isConnected ? "Connected" : "Disconnected"}
        </button>
      </div>
    </div>
  );
}
