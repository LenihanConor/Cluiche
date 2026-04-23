declare global {
  interface Window {
    dia?: {
      callCpp: (name: string, argsJson: string) => void;
      version?: string;
    };
    CluicheEditor: typeof EditorBridge;
    DiaEditor_onDataChanged?: (payload: unknown) => void;
    DiaEditor_onResponse?: (payload: unknown) => void;
  }
}

type Pending = {
  resolve: (value: unknown) => void;
  reject: (reason: unknown) => void;
};

const pending = new Map<string, Pending>();
let nextReqId = 1;

type TopicListener = (data: unknown) => void;
const topicListeners = new Map<string, Set<TopicListener>>();

function sendEvent(type: string, data?: object): void {
  if (!window.dia || !window.dia.callCpp) {
    console.warn("dia.callCpp not available; event dropped:", type);
    return;
  }
  const payload = JSON.stringify({ type, data: data ?? {} });
  window.dia.callCpp("DiaEditor_call", payload);
}

function sendRequest<T>(type: string, data?: object): Promise<T> {
  if (!window.dia || !window.dia.callCpp) {
    return Promise.reject(new Error("dia.callCpp not available"));
  }

  const reqId = `r${nextReqId++}`;
  const payload = JSON.stringify({ type, reqId, data: data ?? {} });

  return new Promise<T>((resolve, reject) => {
    pending.set(reqId, { resolve: resolve as (v: unknown) => void, reject });
    window.dia!.callCpp("DiaEditor_call", payload);
  });
}

// Requests originated inside an iframe need their response routed back to
// that iframe (CEF only delivers JS calls to the main frame).
const iframeReqOrigins = new Map<string, Window>();

window.DiaEditor_onResponse = (payload: unknown) => {
  try {
    const env = (typeof payload === "string"
      ? JSON.parse(payload)
      : payload) as { reqId?: string; result?: unknown };
    if (!env || !env.reqId) return;

    const originWindow = iframeReqOrigins.get(env.reqId);
    if (originWindow) {
      iframeReqOrigins.delete(env.reqId);
      try {
        originWindow.postMessage({
          __diaResponse: true,
          reqId: env.reqId,
          result: env.result,
        }, "*");
      } catch { /* iframe may have unloaded */ }
      return;
    }

    const entry = pending.get(env.reqId);
    if (!entry) return;
    pending.delete(env.reqId);
    entry.resolve(env.result);
  } catch (err) {
    console.warn("DiaEditor_onResponse parse failed:", err);
  }
};

// Iframe relay: panels that live as standalone HTML (e.g. OutputConsole,
// GameConnection) send their requests via window.parent.postMessage rather
// than directly touching window.dia.callCpp, so all transport flows through
// the main frame and response routing stays centralized here.
window.addEventListener("message", (ev: MessageEvent) => {
  const data = ev.data as { __diaFromFrame?: boolean; payload?: unknown } | null;
  if (!data || !data.__diaFromFrame || !data.payload) return;

  const p = data.payload as { type?: string; reqId?: string; data?: unknown };
  if (!p || typeof p.type !== "string") return;

  if (!window.dia || !window.dia.callCpp) {
    console.warn("dia.callCpp not available; iframe event dropped:", p.type);
    return;
  }

  if (p.reqId && ev.source) {
    iframeReqOrigins.set(p.reqId, ev.source as Window);
  }

  window.dia.callCpp("DiaEditor_call", JSON.stringify(p));
});

window.DiaEditor_onDataChanged = (payload: unknown) => {
  try {
    const env = (typeof payload === "string"
      ? JSON.parse(payload)
      : payload) as { topic?: string; data?: unknown };
    if (!env || !env.topic) return;

    const listeners = topicListeners.get(env.topic);
    if (listeners) {
      listeners.forEach((fn) => {
        try { fn(env.data); }
        catch (err) { console.warn("topic listener failed:", env.topic, err); }
      });
    }

    // Re-broadcast to every iframe so dockable panels can subscribe too.
    const frames = document.querySelectorAll("iframe");
    frames.forEach((f) => {
      try { f.contentWindow?.postMessage({ __dia: true, topic: env.topic, data: env.data }, "*"); }
      catch { /* frame not ready or cross-origin, ignore */ }
    });
  } catch (err) {
    console.warn("DiaEditor_onDataChanged parse failed:", err);
  }
};

export type PanelInfo = { name: string; uiPath: string; visible: boolean };
export type CommandInfo = { id: string; label: string };

export const EditorBridge = {
  shellReady: () => sendEvent("shell_ready"),

  executeCommand: (commandId: string, args?: object) =>
    sendEvent("execute_command", { commandId, args }),

  undo: () => sendEvent("undo"),

  redo: () => sendEvent("redo"),

  getPanels: () =>
    sendRequest<{ panels: PanelInfo[] }>("get_panels"),

  getCommands: () =>
    sendRequest<{ commands: CommandInfo[] }>("get_commands"),

  loadLayout: () =>
    sendRequest<object>("load_layout"),

  saveLayout: (layout: object) => {
    sendEvent("save_layout", { layout });
    return Promise.resolve({});
  },

  togglePanelVisibility: (name: string) =>
    sendEvent("toggle_panel_visibility", { name }),

  subscribe: (topic: string, listener: TopicListener) => {
    let set = topicListeners.get(topic);
    if (!set) {
      set = new Set();
      topicListeners.set(topic, set);
    }
    set.add(listener);
    return () => {
      const s = topicListeners.get(topic);
      if (!s) return;
      s.delete(listener);
      if (s.size === 0) topicListeners.delete(topic);
    };
  },
};

window.CluicheEditor = EditorBridge;
