declare global {
  interface Window {
    dia?: {
      callCpp: (name: string, argsJson: string) => void;
      version?: string;
    };
    CluicheEditor: typeof EditorBridge;
    DiaEditor_onDataChanged?: (json: string) => void;
  }
}

function sendEvent(type: string, data?: object): void {
  if (!window.dia || !window.dia.callCpp) {
    console.warn("dia.callCpp not available; event dropped:", type);
    return;
  }
  const payload = JSON.stringify({ type, data: data ?? {} });
  window.dia.callCpp("DiaEditor_call", payload);
}

export const EditorBridge = {
  executeCommand: (commandId: string, args?: object) =>
    sendEvent("execute_command", { commandId, args }),

  undo: () => sendEvent("undo"),

  redo: () => sendEvent("redo"),

  // Query paths (get_panels, load_layout, save_layout) are TODO: the current
  // bridge is one-way (JS -> C++). Returning values to JS requires either a
  // callback id + async JS push via CallJSFunction, or a sync CefMessageRouter.
  // Stub these so the React code can call them without crashing.
  getPanels: () =>
    Promise.resolve({ panels: [] as string[] }),

  loadLayout: () =>
    Promise.resolve({}),

  saveLayout: (layout: object) => {
    sendEvent("save_layout", { layout });
    return Promise.resolve({});
  },
};

window.CluicheEditor = EditorBridge;
