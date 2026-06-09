// ToastRenderer.test.tsx
// Comprehensive tests for the ToastRenderer component.

import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import { render, screen, fireEvent, act } from '@testing-library/react';
import { ToastRenderer } from './ToastRenderer';
import { useToastStore } from './useToast';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function resetStore() {
  useToastStore.setState({ toasts: [], queue: [] });
}

function addToast(
  message: string,
  severity: 'info' | 'success' | 'warning' | 'error' = 'info',
  durationMs?: number,
) {
  act(() => {
    useToastStore.getState().addToast(message, severity, durationMs);
  });
}

// ---------------------------------------------------------------------------
// 1. Returns null when toast store is empty
// ---------------------------------------------------------------------------

describe('ToastRenderer — empty store', () => {
  beforeEach(resetStore);

  it('renders nothing when there are no toasts', () => {
    const { container } = render(<ToastRenderer />);
    expect(container.firstChild).toBeNull();
  });
});

// ---------------------------------------------------------------------------
// 2. Renders a container when toasts are present
// ---------------------------------------------------------------------------

describe('ToastRenderer — container', () => {
  beforeEach(resetStore);

  it('renders a container div when at least one toast is present', () => {
    addToast('Hello');
    const { container } = render(<ToastRenderer />);
    expect(container.firstChild).not.toBeNull();
    expect(container.querySelector('div')).toBeTruthy();
  });
});

// ---------------------------------------------------------------------------
// 3. Severity icons
// ---------------------------------------------------------------------------

describe('ToastRenderer — severity icons', () => {
  beforeEach(resetStore);

  it('renders ℹ icon for info severity', () => {
    addToast('info message', 'info', 0);
    render(<ToastRenderer />);
    expect(screen.getByText('ℹ')).toBeTruthy();
  });

  it('renders ✓ icon for success severity', () => {
    addToast('success message', 'success', 0);
    render(<ToastRenderer />);
    expect(screen.getByText('✓')).toBeTruthy();
  });

  it('renders ⚠ icon for warning severity', () => {
    addToast('warning message', 'warning', 0);
    render(<ToastRenderer />);
    expect(screen.getByText('⚠')).toBeTruthy();
  });

  it('renders ✕ icon for error severity', () => {
    addToast('error message', 'error', 0);
    render(<ToastRenderer />);
    expect(screen.getByText('✕')).toBeTruthy();
  });
});

// ---------------------------------------------------------------------------
// 4. aria-live attribute
// ---------------------------------------------------------------------------

describe('ToastRenderer — aria-live', () => {
  beforeEach(resetStore);

  it('uses aria-live="polite" for info', () => {
    addToast('info', 'info', 0);
    render(<ToastRenderer />);
    const alert = screen.getByRole('alert');
    expect(alert.getAttribute('aria-live')).toBe('polite');
  });

  it('uses aria-live="polite" for success', () => {
    addToast('success', 'success', 0);
    render(<ToastRenderer />);
    const alert = screen.getByRole('alert');
    expect(alert.getAttribute('aria-live')).toBe('polite');
  });

  it('uses aria-live="polite" for warning', () => {
    addToast('warning', 'warning', 0);
    render(<ToastRenderer />);
    const alert = screen.getByRole('alert');
    expect(alert.getAttribute('aria-live')).toBe('polite');
  });

  it('uses aria-live="assertive" for error', () => {
    addToast('error', 'error', 0);
    render(<ToastRenderer />);
    const alert = screen.getByRole('alert');
    expect(alert.getAttribute('aria-live')).toBe('assertive');
  });
});

// ---------------------------------------------------------------------------
// 5. Close button dismisses the toast
// ---------------------------------------------------------------------------

describe('ToastRenderer — close button', () => {
  beforeEach(() => {
    resetStore();
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('clicking × begins the dismiss sequence', () => {
    addToast('dismiss me', 'info', 0);
    render(<ToastRenderer />);

    const button = screen.getByRole('button', { name: /dismiss notification/i });
    expect(button).toBeTruthy();
    expect(button.textContent).toBe('×');

    fireEvent.click(button);

    // After 250ms animation the onRemove callback fires, removing from store
    act(() => { vi.advanceTimersByTime(300); });

    expect(useToastStore.getState().toasts).toHaveLength(0);
  });
});

// ---------------------------------------------------------------------------
// 6. Toast message text renders
// ---------------------------------------------------------------------------

describe('ToastRenderer — message text', () => {
  beforeEach(resetStore);

  it('displays the toast message', () => {
    addToast('Build succeeded!', 'success', 0);
    render(<ToastRenderer />);
    expect(screen.getByText('Build succeeded!')).toBeTruthy();
  });
});

// ---------------------------------------------------------------------------
// 7. Auto-dismiss: toast removed after durationMs
// ---------------------------------------------------------------------------

describe('ToastRenderer — auto-dismiss', () => {
  beforeEach(() => {
    resetStore();
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('removes the toast after durationMs elapses', () => {
    addToast('auto dismiss', 'info', 1000);
    render(<ToastRenderer />);

    expect(useToastStore.getState().toasts).toHaveLength(1);

    // Advance past duration (1000ms) + exit animation (250ms)
    act(() => { vi.advanceTimersByTime(1300); });

    expect(useToastStore.getState().toasts).toHaveLength(0);
  });
});

// ---------------------------------------------------------------------------
// 8. Sticky toast (durationMs=0) does NOT auto-dismiss
// ---------------------------------------------------------------------------

describe('ToastRenderer — sticky toast', () => {
  beforeEach(() => {
    resetStore();
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('does not auto-dismiss when durationMs is 0', () => {
    addToast('sticky toast', 'error', 0);
    render(<ToastRenderer />);

    act(() => { vi.advanceTimersByTime(60000); });

    expect(useToastStore.getState().toasts).toHaveLength(1);
  });
});

// ---------------------------------------------------------------------------
// 9. Hover pauses auto-dismiss; mouse-leave resumes it
// ---------------------------------------------------------------------------

describe('ToastRenderer — hover pauses timer', () => {
  beforeEach(() => {
    resetStore();
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('hovering prevents auto-dismiss from firing', () => {
    addToast('hover me', 'info', 1000);
    render(<ToastRenderer />);

    const alert = screen.getByRole('alert');

    // Advance partway, then hover
    act(() => { vi.advanceTimersByTime(500); });
    fireEvent.mouseEnter(alert);

    // Advance well past original deadline — should still be alive
    act(() => { vi.advanceTimersByTime(2000); });

    expect(useToastStore.getState().toasts).toHaveLength(1);
  });

  it('mouse-leave resumes the timer and toast eventually dismisses', () => {
    addToast('hover resume', 'info', 1000);
    render(<ToastRenderer />);

    const alert = screen.getByRole('alert');

    // Hover immediately
    fireEvent.mouseEnter(alert);
    act(() => { vi.advanceTimersByTime(2000); });

    // Still alive while hovering
    expect(useToastStore.getState().toasts).toHaveLength(1);

    // Release hover — a fresh 1000ms timer starts
    fireEvent.mouseLeave(alert);
    act(() => { vi.advanceTimersByTime(1300); });

    expect(useToastStore.getState().toasts).toHaveLength(0);
  });
});

// ---------------------------------------------------------------------------
// 10. Animation injection is idempotent
// ---------------------------------------------------------------------------

describe('ToastRenderer — animation injection idempotency', () => {
  beforeEach(() => {
    resetStore();
    // Remove any pre-existing style tag so each test starts clean
    const existing = document.getElementById('dia-toast-animations');
    if (existing) existing.remove();
  });

  it('injects exactly one <style id="dia-toast-animations"> even when rendered twice', () => {
    addToast('first render', 'info', 0);

    const { unmount } = render(<ToastRenderer />);
    unmount();

    // Re-render a second time
    render(<ToastRenderer />);

    const tags = document.querySelectorAll('#dia-toast-animations');
    expect(tags.length).toBe(1);
  });
});

// ---------------------------------------------------------------------------
// 11. Multiple toasts render multiple items
// ---------------------------------------------------------------------------

describe('ToastRenderer — multiple toasts', () => {
  beforeEach(resetStore);

  it('renders one item per toast', () => {
    addToast('first',  'info',    0);
    addToast('second', 'success', 0);
    addToast('third',  'warning', 0);
    render(<ToastRenderer />);

    const alerts = screen.getAllByRole('alert');
    expect(alerts).toHaveLength(3);
  });
});

// ---------------------------------------------------------------------------
// 12. Each toast item has role="alert"
// ---------------------------------------------------------------------------

describe('ToastRenderer — role="alert"', () => {
  beforeEach(resetStore);

  it('each toast item carries role="alert"', () => {
    addToast('alpha', 'info',    0);
    addToast('beta',  'error',   0);
    render(<ToastRenderer />);

    const alerts = screen.getAllByRole('alert');
    expect(alerts.length).toBeGreaterThanOrEqual(2);
    alerts.forEach((el) => {
      expect(el.getAttribute('role')).toBe('alert');
    });
  });
});
