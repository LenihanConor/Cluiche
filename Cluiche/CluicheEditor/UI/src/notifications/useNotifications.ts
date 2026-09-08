import { create } from "zustand";
import { setToastDispatch } from "../bridge/EditorBridge";

export type ToastLevel = "info" | "success" | "warning" | "error";

export interface Toast {
  id: string;
  level: ToastLevel;
  title: string;
  message?: string;
  durationSeconds: number; // 0 = manual dismiss only
}

const DEFAULT_DURATIONS: Record<ToastLevel, number> = {
  info: 4,
  success: 3,
  warning: 6,
  error: 0,
};

const MAX_VISIBLE = 5;

interface NotificationState {
  toasts: Toast[];
  queue: Toast[];
  addToast: (toast: Omit<Toast, "durationSeconds"> & { durationSeconds?: number }) => void;
  removeToast: (id: string) => void;
  dismissAll: () => void;
}

export const useNotificationStore = create<NotificationState>((set) => ({
  toasts: [],
  queue: [],

  addToast: (incoming) => {
    const durationSeconds =
      incoming.durationSeconds != null && incoming.durationSeconds !== 0
        ? incoming.durationSeconds
        : DEFAULT_DURATIONS[incoming.level];

    const toast: Toast = { ...incoming, durationSeconds };

    set((state) => {
      if (state.toasts.length < MAX_VISIBLE) {
        return { toasts: [...state.toasts, toast] };
      }
      return { queue: [...state.queue, toast] };
    });
  },

  removeToast: (id) => {
    set((state) => {
      const toasts = state.toasts.filter((t) => t.id !== id);
      const queue = [...state.queue];
      if (queue.length > 0) {
        toasts.push(queue.shift()!);
      }
      return { toasts, queue };
    });
  },

  dismissAll: () => set({ toasts: [], queue: [] }),
}));

// Wire bridge → store so C++ pushes and EditorBridge.notify() both reach this store
setToastDispatch((raw) => {
  useNotificationStore.getState().addToast({
    id: raw.id,
    level: raw.level as ToastLevel,
    title: raw.title,
    message: raw.message,
  });
});
