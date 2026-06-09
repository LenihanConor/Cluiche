import { useEffect, useRef, useState } from 'react';
import { Toast, useToastStore } from './useToast';

// ---------------------------------------------------------------------------
// Animation injection — idempotent (checks for existing tag by ID)
// ---------------------------------------------------------------------------

const STYLE_TAG_ID = 'dia-toast-animations';

function injectAnimations(): void {
  if (document.getElementById(STYLE_TAG_ID)) return;
  const style = document.createElement('style');
  style.id = STYLE_TAG_ID;
  style.textContent = `
    @keyframes toast-slide-in {
      from { transform: translateX(120%); opacity: 0; }
      to   { transform: translateX(0);    opacity: 1; }
    }
    @keyframes toast-fade-out {
      from { opacity: 1; }
      to   { opacity: 0; }
    }
  `;
  document.head.appendChild(style);
}

// ---------------------------------------------------------------------------
// Severity config
// ---------------------------------------------------------------------------

const SEVERITY_CONFIG = {
  info:    { color: '#3b82f6', icon: 'ℹ', ariaLive: 'polite'    as const },
  success: { color: '#22c55e', icon: '✓', ariaLive: 'polite'    as const },
  warning: { color: '#f59e0b', icon: '⚠', ariaLive: 'polite'    as const },
  error:   { color: '#ef4444', icon: '✕', ariaLive: 'assertive' as const },
};

// ---------------------------------------------------------------------------
// ToastItem
// ---------------------------------------------------------------------------

interface ToastItemProps {
  toast: Toast;
  onRemove: () => void;
}

function ToastItem({ toast, onRemove }: ToastItemProps) {
  const [exiting, setExiting] = useState(false);
  const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const config = SEVERITY_CONFIG[toast.severity];

  // Begin the exit sequence: animate then call onRemove
  const startExit = () => {
    if (exiting) return;
    setExiting(true);
    setTimeout(onRemove, 250);
  };

  // Schedule auto-dismiss (skip when durationMs === 0)
  const scheduleTimer = () => {
    if (toast.durationMs === 0) return;
    const duration = toast.durationMs ?? 4000;
    timerRef.current = setTimeout(startExit, duration);
  };

  const clearTimer = () => {
    if (timerRef.current !== null) {
      clearTimeout(timerRef.current);
      timerRef.current = null;
    }
  };

  // Mount: inject animations (idempotent) and start timer
  useEffect(() => {
    injectAnimations();
    scheduleTimer();
    return () => clearTimer();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const containerStyle: React.CSSProperties = {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'flex-start',
    gap: '10px',
    backgroundColor: '#1e1e1e',
    border: `1px solid ${config.color}`,
    borderRadius: '6px',
    padding: '10px 12px',
    minWidth: '280px',
    maxWidth: '380px',
    boxShadow: '0 4px 12px rgba(0,0,0,0.5)',
    animation: exiting
      ? 'toast-fade-out 250ms ease forwards'
      : 'toast-slide-in 250ms ease forwards',
    pointerEvents: 'auto',
  };

  const iconStyle: React.CSSProperties = {
    color: config.color,
    fontSize: '16px',
    lineHeight: '1.4',
    flexShrink: 0,
    userSelect: 'none',
  };

  const messageStyle: React.CSSProperties = {
    flex: 1,
    color: '#f0f0f0',
    fontSize: '13px',
    lineHeight: '1.4',
  };

  const dismissStyle: React.CSSProperties = {
    background: 'none',
    border: 'none',
    color: '#888',
    cursor: 'pointer',
    fontSize: '14px',
    lineHeight: '1',
    padding: '0 0 0 4px',
    flexShrink: 0,
    userSelect: 'none',
  };

  return (
    <div
      role="alert"
      aria-live={config.ariaLive}
      style={containerStyle}
      onMouseEnter={clearTimer}
      onMouseLeave={scheduleTimer}
    >
      <span style={iconStyle} aria-hidden="true">
        {config.icon}
      </span>

      <span style={messageStyle}>{toast.message}</span>

      <button
        style={dismissStyle}
        aria-label="Dismiss notification"
        onClick={startExit}
      >
        ×
      </button>
    </div>
  );
}

// ---------------------------------------------------------------------------
// ToastRenderer — mount once at app root
// ---------------------------------------------------------------------------

export function ToastRenderer() {
  const toasts = useToastStore((s) => s.toasts);
  const removeToast = useToastStore((s) => s.removeToast);

  if (toasts.length === 0) return null;

  const containerStyle: React.CSSProperties = {
    position: 'fixed',
    bottom: '20px',
    right: '20px',
    zIndex: 9999,
    display: 'flex',
    flexDirection: 'column',
    gap: '8px',
    alignItems: 'flex-end',
    pointerEvents: 'none', // container is click-through; items opt back in
  };

  return (
    <div style={containerStyle}>
      {toasts.map((toast) => (
        <ToastItem
          key={toast.id}
          toast={toast}
          onRemove={() => removeToast(toast.id)}
        />
      ))}
    </div>
  );
}
