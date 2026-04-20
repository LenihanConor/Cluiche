type CefQueryCallback = (response: string) => void;

interface CefQueryRequest {
  request: string;
  onSuccess: CefQueryCallback;
  onFailure: (errorCode: number, errorMessage: string) => void;
}

declare global {
  interface Window {
    cefQuery?: (req: CefQueryRequest) => void;
    CluicheEditor: typeof EditorBridge;
  }
}

function sendRequest(request: object): Promise<unknown> {
  return new Promise((resolve, reject) => {
    if (!window.cefQuery) {
      reject(new Error("cefQuery not available"));
      return;
    }
    window.cefQuery({
      request: JSON.stringify(request),
      onSuccess: (response) => resolve(JSON.parse(response)),
      onFailure: (_code, msg) => reject(new Error(msg)),
    });
  });
}

export const EditorBridge = {
  executeCommand: (commandId: string, args?: object) =>
    sendRequest({ type: "DiaEditor_execute_command", commandId, args }),

  undo: () => sendRequest({ type: "DiaEditor_undo" }),

  redo: () => sendRequest({ type: "DiaEditor_redo" }),

  getPanels: () =>
    sendRequest({ type: "DiaEditor_get_panels" }) as Promise<{ panels: string[] }>,

  loadLayout: () =>
    sendRequest({ type: "DiaEditor_load_layout" }) as Promise<object>,

  saveLayout: (layout: object) =>
    sendRequest({ type: "DiaEditor_save_layout", layout }),
};

window.CluicheEditor = EditorBridge;
