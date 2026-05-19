const pending = new Map();
let nextReqId = 1;
const topicListeners = new Map();
function uiLog(source, msg) {
    sendEvent("editor.ui_log", { source, msg });
}
function sendEvent(type, data) {
    if (!window.dia || !window.dia.callCpp) {
        console.warn("dia.callCpp not available; event dropped:", type);
        return;
    }
    const payload = JSON.stringify({ type, data: data ?? {} });
    window.dia.callCpp("DiaEditor_call", payload);
}
function sendRequest(type, data) {
    if (!window.dia || !window.dia.callCpp) {
        return Promise.reject(new Error("dia.callCpp not available"));
    }
    const reqId = `r${nextReqId++}`;
    const payload = JSON.stringify({ type, reqId, data: data ?? {} });
    return new Promise((resolve, reject) => {
        pending.set(reqId, { resolve: resolve, reject });
        window.dia.callCpp("DiaEditor_call", payload);
    });
}
// Requests originated inside an iframe need their response routed back to
// that iframe (CEF only delivers JS calls to the main frame).
const iframeReqOrigins = new Map();
window.DiaEditor_onResponse = (payload) => {
    try {
        const env = (typeof payload === "string"
            ? JSON.parse(payload)
            : payload);
        if (!env || !env.reqId)
            return;
        const originWindow = iframeReqOrigins.get(env.reqId);
        if (originWindow) {
            iframeReqOrigins.delete(env.reqId);
            try {
                originWindow.postMessage({
                    __diaResponse: true,
                    reqId: env.reqId,
                    result: env.result,
                }, "*");
            }
            catch { /* iframe may have unloaded */ }
            return;
        }
        const entry = pending.get(env.reqId);
        if (!entry)
            return;
        pending.delete(env.reqId);
        entry.resolve(env.result);
    }
    catch (err) {
        console.warn("DiaEditor_onResponse parse failed:", err);
    }
};
// Iframe relay: panels that live as standalone HTML (e.g. OutputConsole,
// GameConnection) send their requests via window.parent.postMessage rather
// than directly touching window.dia.callCpp, so all transport flows through
// the main frame and response routing stays centralized here.
window.addEventListener("message", (ev) => {
    const data = ev.data;
    if (!data || !data.__diaFromFrame || !data.payload)
        return;
    const p = data.payload;
    if (!p || typeof p.type !== "string")
        return;
    if (!window.dia || !window.dia.callCpp) {
        console.warn("dia.callCpp not available; iframe event dropped:", p.type);
        return;
    }
    if (p.reqId && ev.source) {
        iframeReqOrigins.set(p.reqId, ev.source);
    }
    window.dia.callCpp("DiaEditor_call", JSON.stringify(p));
});
window.DiaEditor_onDataChanged = (payload) => {
    try {
        const env = (typeof payload === "string"
            ? JSON.parse(payload)
            : payload);
        if (!env || !env.topic)
            return;
        const listeners = topicListeners.get(env.topic);
        const frames = document.querySelectorAll("iframe");
        if (listeners) {
            listeners.forEach((fn) => {
                try {
                    fn(env.data);
                }
                catch (err) {
                    uiLog("EditorBridge", `topic listener failed: ${env.topic} ${err}`);
                }
            });
        }
        // Re-broadcast to every iframe so dockable panels can subscribe too.
        frames.forEach((f) => {
            try {
                f.contentWindow?.postMessage({ __dia: true, topic: env.topic, data: env.data }, "*");
            }
            catch (err) {
                uiLog("EditorBridge", `failed to relay topic='${env.topic}' to iframe '${f.title || f.src || "(unknown)"}': ${err}`);
            }
        });
    }
    catch (err) {
        uiLog("EditorBridge", `onDataChanged parse failed: ${err}`);
    }
};
export const EditorBridge = {
    request: (type, data) => sendRequest(type, data),
    shellReady: () => sendEvent("shell_ready"),
    executeCommand: (commandId, args) => sendEvent("execute_command", { commandId, args }),
    undo: () => sendEvent("undo"),
    redo: () => sendEvent("redo"),
    getPanels: () => sendRequest("get_panels"),
    getCommands: () => sendRequest("get_commands"),
    loadLayout: () => sendRequest("load_layout"),
    saveLayout: (layout) => {
        sendEvent("save_layout", { layout });
        return Promise.resolve({});
    },
    togglePanelVisibility: (name) => sendEvent("toggle_panel_visibility", { name }),
    subscribe: (topic, listener) => {
        let set = topicListeners.get(topic);
        if (!set) {
            set = new Set();
            topicListeners.set(topic, set);
        }
        set.add(listener);
        return () => {
            const s = topicListeners.get(topic);
            if (!s)
                return;
            s.delete(listener);
            if (s.size === 0)
                topicListeners.delete(topic);
        };
    },
};
window.CluicheEditor = EditorBridge;
