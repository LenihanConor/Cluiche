declare global {
  interface Window {
    app?: {
      [methodName: string]: (...args: unknown[]) => void;
    };
  }
}

type MessageCallback = (data: unknown) => void;
const listeners = new Map<string, Set<MessageCallback>>();

function send(methodName: string, data?: unknown): void {
  if (!window.app || typeof window.app[methodName] !== "function") {
    console.warn(`GameBridge: window.app.${methodName} not available`);
    return;
  }
  window.app[methodName](data !== undefined ? JSON.stringify(data) : "{}");
}

export const GameBridge = {
  send,

  subscribe: (topic: string, callback: MessageCallback) => {
    let set = listeners.get(topic);
    if (!set) {
      set = new Set();
      listeners.set(topic, set);
    }
    set.add(callback);
    return () => {
      const s = listeners.get(topic);
      if (!s) return;
      s.delete(callback);
      if (s.size === 0) listeners.delete(topic);
    };
  },

  dispatch: (topic: string, data: unknown) => {
    const set = listeners.get(topic);
    if (!set) return;
    set.forEach((fn) => {
      try { fn(data); }
      catch (err) { console.warn("GameBridge listener error:", topic, err); }
    });
  },
};

// C++ calls window.GameBridge_dispatch(topic, dataJson) to push data into React
window.GameBridge_dispatch = (topic: string, dataJson: string) => {
  try {
    const data = JSON.parse(dataJson);
    GameBridge.dispatch(topic, data);
  } catch (err) {
    console.warn("GameBridge_dispatch parse error:", err);
  }
};

declare global {
  interface Window {
    GameBridge_dispatch: (topic: string, dataJson: string) => void;
  }
}
