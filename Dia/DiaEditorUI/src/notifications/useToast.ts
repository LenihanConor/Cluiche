import { create } from 'zustand';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type ToastSeverity = 'info' | 'success' | 'warning' | 'error';

export interface Toast {
  id: string;
  message: string;
  severity: ToastSeverity;
  durationMs?: number; // 0 = sticky
}

// ---------------------------------------------------------------------------
// Defaults
// ---------------------------------------------------------------------------

const DEFAULT_DURATIONS: Record<ToastSeverity, number> = {
  info: 4000,
  success: 3000,
  warning: 6000,
  error: 0,
};

const MAX_VISIBLE = 5;

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

interface ToastState {
  toasts: Toast[];
  queue: Toast[];
  addToast: (message: string, severity: ToastSeverity, durationMs?: number) => void;
  removeToast: (id: string) => void;
  dismissAll: () => void;
}

export const useToastStore = create<ToastState>((set) => ({
  toasts: [],
  queue: [],

  addToast: (message, severity, durationMs) => {
    const resolvedDuration =
      durationMs !== undefined ? durationMs : DEFAULT_DURATIONS[severity];

    const toast: Toast = {
      id: `toast-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`,
      message,
      severity,
      durationMs: resolvedDuration,
    };

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

// ---------------------------------------------------------------------------
// Hook
// ---------------------------------------------------------------------------

export function useToast(): {
  push: (message: string, severity?: ToastSeverity, durationMs?: number) => void;
  dismiss: (id: string) => void;
} {
  const addToast = useToastStore((s) => s.addToast);
  const removeToast = useToastStore((s) => s.removeToast);

  return {
    push: (message, severity = 'info', durationMs) => addToast(message, severity, durationMs),
    dismiss: (id) => removeToast(id),
  };
}
