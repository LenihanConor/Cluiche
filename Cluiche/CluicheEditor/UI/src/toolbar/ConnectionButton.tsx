import { useEffect, useRef, useState } from "react";
import { EditorBridge } from "../bridge/EditorBridge";
import { useConnectionHealth } from "./useConnectionHealth";
import { ConnectionHealthPopover } from "./ConnectionHealthPopover";

interface ConnectionState {
  state: "disconnected" | "connecting" | "connected";
  url?: string;
  gameName?: string;
  gameVersion?: string;
}

const defaultState: ConnectionState = { state: "disconnected" };

const DOT_COLOR: Record<ConnectionState["state"], string> = {
  disconnected: "#f48771",
  connecting: "#dcdcaa",
  connected: "#89d185",
};

const LABEL: Record<ConnectionState["state"], string> = {
  disconnected: "Disconnected",
  connecting: "Connecting...",
  connected: "Connected",
};

export function ConnectionButton() {
  const [connection, setConnection] = useState<ConnectionState>(defaultState);
  const [dropdownOpen, setDropdownOpen] = useState(false);
  const [urlInput, setUrlInput] = useState("ws://localhost:9002");
  const [healthOpen, setHealthOpen] = useState(false);
  const containerRef = useRef<HTMLDivElement>(null);

  // Subscribe to live connection state updates
  useEffect(() => {
    const unsub = EditorBridge.subscribe("game_connection", (data: unknown) => {
      const d = data as Partial<ConnectionState> | null;
      if (!d) return;
      setConnection({
        state: d.state ?? "disconnected",
        url: d.url,
        gameName: d.gameName,
        gameVersion: d.gameVersion,
      });
    });
    return unsub;
  }, []);

  // Request current state on mount
  useEffect(() => {
    EditorBridge.request<ConnectionState>("game_connection.get_state", {})
      .then((r) => {
        if (!r) return;
        setConnection({
          state: r.state ?? "disconnected",
          url: r.url,
          gameName: r.gameName,
          gameVersion: r.gameVersion,
        });
        if (r.url) setUrlInput(r.url);
      })
      .catch(() => { EditorBridge.notify({ level: "error", title: "Connection state unavailable" }); });
  }, []);

  // Outside-click-to-close
  useEffect(() => {
    if (!dropdownOpen) return;
    function handleClick(e: MouseEvent) {
      if (containerRef.current && !containerRef.current.contains(e.target as Node))
        setDropdownOpen(false);
    }
    document.addEventListener("mousedown", handleClick);
    return () => document.removeEventListener("mousedown", handleClick);
  }, [dropdownOpen]);

  function handleConnect() {
    EditorBridge.request("game_connection.connect", { url: urlInput }).catch(() => { EditorBridge.notify({ level: "error", title: "Failed to connect" }); });
    setDropdownOpen(false);
  }

  function handleDisconnect() {
    EditorBridge.request("game_connection.disconnect", {}).catch(() => { EditorBridge.notify({ level: "error", title: "Failed to disconnect" }); });
    setDropdownOpen(false);
  }

  const { state } = connection;
  const dotColor = DOT_COLOR[state];
  const label = LABEL[state];
  const health = useConnectionHealth(state === "connected");

  return (
    <div style={{ position: "relative" }} ref={containerRef}>
      <button
        onClick={() => setDropdownOpen((o) => !o)}
        title={`Game connection: ${label}`}
        style={{
          display: "flex",
          alignItems: "center",
          gap: 5,
          background: "transparent",
          border: "none",
          cursor: "pointer",
          color: "#cccccc",
          fontSize: 11,
          fontFamily: "Segoe UI, system-ui, sans-serif",
          padding: "2px 6px",
          borderRadius: 2,
        }}
      >
        <div style={{ position: "relative", width: 8, height: 8, flexShrink: 0 }}>
          <span style={{
            display: "inline-block",
            width: 8,
            height: 8,
            borderRadius: "50%",
            background: dotColor,
          }} />
          {health.badge !== "none" && (
            <span style={{
              position: "absolute",
              top: -3,
              right: -3,
              width: 6,
              height: 6,
              borderRadius: "50%",
              background: health.badge === "red" ? "#f48771" : "#dcdcaa",
              border: "1px solid #1e1e1e",
            }} />
          )}
        </div>
        <span>{label}</span>
        <span style={{ fontSize: 9, opacity: 0.6 }}>▾</span>
      </button>

      {healthOpen && (
        <ConnectionHealthPopover
          health={health}
          onClose={() => setHealthOpen(false)}
        />
      )}

      {dropdownOpen && (
        <div style={{
          position: "absolute",
          bottom: "100%",
          right: 0,
          marginBottom: 4,
          background: "#2d2d30",
          border: "1px solid #3c3c3c",
          borderRadius: 3,
          minWidth: 260,
          padding: "8px 0",
          boxShadow: "0 4px 12px rgba(0,0,0,0.5)",
          zIndex: 1000,
          overflow: "hidden",
        }}>
          {/* Status header row */}
          <div style={{
            display: "flex",
            alignItems: "center",
            gap: 8,
            padding: "4px 12px 8px",
          }}>
            <span style={{
              display: "inline-block",
              width: 8,
              height: 8,
              borderRadius: "50%",
              background: dotColor,
              flexShrink: 0,
            }} />
            <span style={{
              color: "#cccccc",
              fontSize: 11,
              fontFamily: "Segoe UI, system-ui, sans-serif",
              fontWeight: "bold",
            }}>
              {label}
            </span>
          </div>

          <Separator />

          {state !== "connected" && (
            <>
              {/* URL input row */}
              <div style={{ padding: "6px 12px" }}>
                <input
                  type="text"
                  value={urlInput}
                  onChange={(e) => setUrlInput(e.target.value)}
                  placeholder="ws://localhost:9002"
                  style={{
                    width: "100%",
                    boxSizing: "border-box",
                    background: "#1e1e1e",
                    border: "1px solid #3c3c3c",
                    borderRadius: 2,
                    color: "#cccccc",
                    fontSize: 11,
                    fontFamily: "Segoe UI, system-ui, sans-serif",
                    padding: "4px 6px",
                    outline: "none",
                  }}
                  onFocus={(e) => { e.currentTarget.style.borderColor = "#0e639c"; }}
                  onBlur={(e) => { e.currentTarget.style.borderColor = "#3c3c3c"; }}
                  onKeyDown={(e) => { if (e.key === "Enter") handleConnect(); }}
                />
              </div>

              {/* Connect action row */}
              <div style={{ padding: "6px 12px" }}>
                <button
                  onClick={handleConnect}
                  disabled={state === "connecting"}
                  style={{
                    background: state === "connecting" ? "#555" : "#0e639c",
                    color: "#fff",
                    border: "none",
                    borderRadius: 2,
                    cursor: state === "connecting" ? "default" : "pointer",
                    fontSize: 11,
                    fontFamily: "Segoe UI, system-ui, sans-serif",
                    padding: "4px 10px",
                  }}
                >
                  {state === "connecting" ? "Connecting..." : "Connect"}
                </button>
              </div>
            </>
          )}

          {state === "connected" && (
            <>
              {/* Health summary */}
              <div style={{ padding: "4px 12px 4px", display: "flex", alignItems: "center", justifyContent: "space-between" }}>
                <span style={{
                  fontSize: 10,
                  fontFamily: "Segoe UI, system-ui, sans-serif",
                  color: health.badge !== "none" ? "#dcdcaa" : "#89d185",
                }}>
                  {health.badge !== "none" ? "⚠ Dropping messages" : (health.summary || "Checking...")}
                </span>
              </div>
              <div style={{ padding: "4px 12px 8px" }}>
                <button
                  onClick={() => { setHealthOpen(true); setDropdownOpen(false); }}
                  style={{
                    background: "transparent",
                    color: "#cccccc",
                    border: "1px solid #3c3c3c",
                    borderRadius: 2,
                    cursor: "pointer",
                    fontSize: 10,
                    fontFamily: "Segoe UI, system-ui, sans-serif",
                    padding: "3px 8px",
                    width: "100%",
                  }}
                  onMouseEnter={(e) => { e.currentTarget.style.borderColor = "#0e639c"; }}
                  onMouseLeave={(e) => { e.currentTarget.style.borderColor = "#3c3c3c"; }}
                >
                  Connection Health
                </button>
              </div>
              <Separator />

              {/* Game info section */}
              <div style={{ padding: "6px 12px" }}>
                <InfoRow label="Game" value={connection.gameName ?? "—"} />
                <InfoRow label="Target URL" value={connection.url ?? "—"} />
                {connection.gameVersion && (
                  <InfoRow label="Version" value={connection.gameVersion} />
                )}
              </div>

              <Separator />

              {/* Disconnect action row */}
              <div style={{ padding: "6px 12px" }}>
                <button
                  onClick={handleDisconnect}
                  style={{
                    background: "transparent",
                    color: "#f48771",
                    border: "1px solid #f48771",
                    borderRadius: 2,
                    cursor: "pointer",
                    fontSize: 11,
                    fontFamily: "Segoe UI, system-ui, sans-serif",
                    padding: "4px 10px",
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.background = "rgba(244,135,113,0.1)";
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.background = "transparent";
                  }}
                >
                  Disconnect
                </button>
              </div>
            </>
          )}
        </div>
      )}
    </div>
  );
}

function InfoRow({ label, value }: { label: string; value: string }) {
  return (
    <div style={{ marginBottom: 4 }}>
      <span style={{
        color: "#666666",
        fontSize: 10,
        fontFamily: "Segoe UI, system-ui, sans-serif",
        display: "block",
      }}>
        {label}
      </span>
      <span style={{
        color: "#cccccc",
        fontSize: 11,
        fontFamily: "Segoe UI, system-ui, sans-serif",
        display: "block",
        wordBreak: "break-all",
      }}>
        {value}
      </span>
    </div>
  );
}

function Separator() {
  return <div style={{ height: 1, background: "#3c3c3c", margin: "2px 0" }} />;
}
