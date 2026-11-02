import { useEffect, useState } from "react";
import type { CSSProperties } from "react";
import { EditorBridge } from "../bridge/EditorBridge";

type Choice = "cancel" | "confirm" | "discard" | "save";

interface DirtyEntry {
  typeId: string;
  name: string;
  saved: boolean;
  saveFailed: boolean;
}

interface GameSwitchState {
  pending: boolean;
  prompt: "none" | "confirm_switch" | "unsaved" | "save_failed";
  targetPath: string;
  targetName: string;
  currentName: string;
  isClose: boolean;
  dirty: DirtyEntry[];
  lastOutcome: string;
}

const colors = {
  bg: "#1e1e2e", surface: "#313150", border: "#44446a", text: "#cdd6f4",
  dim: "#7f849c", accent: "#89b4fa", green: "#a6e3a1", red: "#f38ba8", orange: "#fab387",
};

const btnBase: CSSProperties = {
  padding: "5px 14px", borderRadius: 3, fontSize: 12, fontFamily: "inherit",
  fontWeight: 600, cursor: "pointer", border: `1px solid ${colors.border}`,
  background: "transparent", color: colors.text,
};
const btnPrimary: CSSProperties = { ...btnBase, background: colors.accent, borderColor: colors.accent, color: colors.bg };
const btnDanger: CSSProperties = { ...btnBase, color: colors.red, borderColor: "rgba(243,139,168,0.5)" };

function respond(choice: Choice) {
  EditorBridge.request("game_switch.respond", { choice }).catch(() => {
    EditorBridge.notify({ level: "error", title: "Game switch response failed" });
  });
}

function normalize(d: Partial<GameSwitchState> | null | undefined): GameSwitchState | null {
  if (!d) return null;
  return {
    pending: !!d.pending,
    prompt: d.prompt ?? "none",
    targetPath: d.targetPath ?? "",
    targetName: d.targetName ?? "",
    currentName: d.currentName ?? "",
    isClose: !!d.isClose,
    dirty: Array.isArray(d.dirty) ? d.dirty : [],
    lastOutcome: d.lastOutcome ?? "none",
  };
}

export function GameSwitchModal() {
  const [state, setState] = useState<GameSwitchState | null>(null);

  useEffect(() => {
    const unsub = EditorBridge.subscribe("game_switch_changed", (data: unknown) => {
      setState(normalize(data as Partial<GameSwitchState>));
    });
    EditorBridge.request<Partial<GameSwitchState>>("game_switch.get_state", {})
      .then((d) => {
        // A topic push that arrived first is newer than this snapshot only if
        // state is already set; keep it in that case.
        setState((prev) => prev ?? normalize(d));
      })
      .catch(() => {});
    return unsub;
  }, []);

  if (!state || !state.pending || state.prompt === "none") return null;

  const { prompt, targetName, currentName, isClose, dirty } = state;
  const title = isClose
    ? `Close ${currentName}.diagame?`
    : `Switch to ${targetName}.diagame?`;

  return (
    <div
      style={{
        position: "fixed", inset: 0, zIndex: 10000, display: "flex",
        alignItems: "center", justifyContent: "center",
        background: "rgba(10,10,15,0.62)",
      }}
    >
      <div
        role="dialog"
        aria-label={title}
        style={{
          width: 360, background: colors.surface, border: `1px solid ${colors.border}`,
          borderRadius: 6, boxShadow: "0 8px 24px rgba(0,0,0,0.55)", padding: 16,
          color: colors.text, fontFamily: "inherit",
        }}
      >
        <div style={{ fontSize: 14, fontWeight: 700, marginBottom: 8 }}>{title}</div>

        {prompt === "confirm_switch" && (
          <>
            <div style={{ fontSize: 12, color: colors.dim, lineHeight: 1.6, marginBottom: 12 }}>
              The running game is <strong style={{ color: colors.text }}>{targetName}</strong>, but the editor
              has <strong style={{ color: colors.text }}>{currentName}.diagame</strong> loaded.
              Switching makes {targetName}'s editor plugins available and closes any {currentName}-only plugins.
            </div>
            <Buttons>
              <button style={btnBase} onClick={() => respond("cancel")}>Stay on {currentName}</button>
              <button style={btnPrimary} onClick={() => respond("confirm")}>Switch to {targetName}</button>
            </Buttons>
          </>
        )}

        {(prompt === "unsaved" || prompt === "save_failed") && (
          <>
            <div style={{ fontSize: 12, color: colors.dim, lineHeight: 1.6, marginBottom: 12 }}>
              {prompt === "unsaved" ? (
                <>These <strong style={{ color: colors.text }}>{currentName}.diagame</strong> plugins have unsaved changes and will close:</>
              ) : (
                <>Could not save every plugin, so nothing was closed:</>
              )}
            </div>
            <div
              style={{
                background: colors.bg, border: `1px solid ${colors.border}`,
                borderRadius: 3, padding: "6px 8px", marginBottom: 14,
              }}
            >
              {dirty.map((e) => (
                <DirtyRow key={e.typeId} entry={e} showStatus={prompt === "save_failed"} />
              ))}
            </div>
            <Buttons>
              <button style={btnBase} onClick={() => respond("cancel")}>Cancel</button>
              <button style={btnDanger} onClick={() => respond("discard")}>Discard</button>
              <button style={btnPrimary} onClick={() => respond("save")}>
                {prompt === "unsaved" ? "Save all and switch" : "Retry save"}
              </button>
            </Buttons>
            {prompt === "unsaved" && (
              <div style={{ fontSize: 10, color: colors.dim, marginTop: 10, lineHeight: 1.5 }}>
                Cancel keeps {currentName}.diagame loaded. Discard closes the plugins without saving.
                If a save fails the dialog stays open and the failed plugin is marked.
              </div>
            )}
          </>
        )}
      </div>
    </div>
  );
}

function Buttons({ children }: { children: React.ReactNode }) {
  return <div style={{ display: "flex", gap: 8, justifyContent: "flex-end" }}>{children}</div>;
}

function DirtyRow({ entry, showStatus }: { entry: DirtyEntry; showStatus: boolean }) {
  const dot = entry.saveFailed ? colors.red : entry.saved ? colors.green : colors.orange;
  const note = !showStatus ? "" : entry.saveFailed ? "save failed" : entry.saved ? "saved" : "";
  return (
    <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "3px 0", fontSize: 12 }}>
      <span style={{ width: 7, height: 7, borderRadius: "50%", background: dot, flexShrink: 0 }} />
      {entry.name}
      {note && (
        <span
          style={{
            marginLeft: "auto", fontSize: 10,
            color: entry.saveFailed ? colors.red : colors.dim,
            fontWeight: entry.saveFailed ? 600 : 400,
          }}
        >
          {note}
        </span>
      )}
    </div>
  );
}
