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

window.DiaEditor_onResponse = (payload: unknown) => {
  try {
    const env = (typeof payload === "string"
      ? JSON.parse(payload)
      : payload) as { reqId?: string; result?: unknown };
    if (!env || !env.reqId) return;
    const entry = pending.get(env.reqId);
    if (!entry) return;
    pending.delete(env.reqId);
    entry.resolve(env.result);
  } catch (err) {
    console.warn("DiaEditor_onResponse parse failed:", err);
  }
};

export type PanelInfo = { name: string; uiPath: string; visible: boolean };

export const EditorBridge = {
  executeCommand: (commandId: string, args?: object) =>
    sendEvent("execute_command", { commandId, args }),

  undo: () => sendEvent("undo"),

  redo: () => sendEvent("redo"),

  getPanels: () =>
    sendRequest<{ panels: PanelInfo[] }>("get_panels"),

  loadLayout: () =>
    sendRequest<object>("load_layout"),

  saveLayout: (layout: object) => {
    sendEvent("save_layout", { layout });
    return Promise.resolve({});
  },
};

window.CluicheEditor = EditorBridge;
