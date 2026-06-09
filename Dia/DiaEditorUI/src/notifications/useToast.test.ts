// useToast.test.ts
// Tests for useToast hook and useToastStore.

import { describe, it, expect, beforeEach } from 'vitest';
import { act, renderHook } from '@testing-library/react';
import { useToast, useToastStore } from './useToast';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function resetStore() {
  act(() => {
    useToastStore.getState().dismissAll();
  });
}

// ---------------------------------------------------------------------------
// push — adds a toast
// ---------------------------------------------------------------------------

describe('useToast — push', () => {
  beforeEach(resetStore);

  it('adds a toast to the store', () => {
    const { result } = renderHook(() => useToast());
    act(() => {
      result.current.push('Hello world');
    });
    const toasts = useToastStore.getState().toasts;
    expect(toasts).toHaveLength(1);
    expect(toasts[0].message).toBe('Hello world');
  });

  it('default severity is info', () => {
    const { result } = renderHook(() => useToast());
    act(() => {
      result.current.push('test message');
    });
    const toasts = useToastStore.getState().toasts;
    expect(toasts[0].severity).toBe('info');
  });

  it('uses provided severity', () => {
    const { result } = renderHook(() => useToast());
    act(() => {
      result.current.push('saved', 'success');
    });
    expect(useToastStore.getState().toasts[0].severity).toBe('success');
  });

  it('assigns a unique id to each toast', () => {
    const { result } = renderHook(() => useToast());
    act(() => {
      result.current.push('first');
      result.current.push('second');
    });
    const toasts = useToastStore.getState().toasts;
    expect(toasts[0].id).toBeTruthy();
    expect(toasts[1].id).toBeTruthy();
    expect(toasts[0].id).not.toBe(toasts[1].id);
  });
});

// ---------------------------------------------------------------------------
// Default durationMs per severity
// ---------------------------------------------------------------------------

describe('useToast — durationMs defaults', () => {
  beforeEach(resetStore);

  it('info defaults to 4000ms', () => {
    const { result } = renderHook(() => useToast());
    act(() => { result.current.push('msg', 'info'); });
    expect(useToastStore.getState().toasts[0].durationMs).toBe(4000);
  });

  it('success defaults to 3000ms', () => {
    const { result } = renderHook(() => useToast());
    act(() => { result.current.push('msg', 'success'); });
    expect(useToastStore.getState().toasts[0].durationMs).toBe(3000);
  });

  it('warning defaults to 6000ms', () => {
    const { result } = renderHook(() => useToast());
    act(() => { result.current.push('msg', 'warning'); });
    expect(useToastStore.getState().toasts[0].durationMs).toBe(6000);
  });

  it('error defaults to 0 (sticky)', () => {
    const { result } = renderHook(() => useToast());
    act(() => { result.current.push('msg', 'error'); });
    expect(useToastStore.getState().toasts[0].durationMs).toBe(0);
  });

  it('explicit durationMs overrides the default', () => {
    const { result } = renderHook(() => useToast());
    act(() => { result.current.push('msg', 'info', 9999); });
    expect(useToastStore.getState().toasts[0].durationMs).toBe(9999);
  });

  it('explicit 0 overrides non-zero default (sticky override)', () => {
    const { result } = renderHook(() => useToast());
    act(() => { result.current.push('msg', 'info', 0); });
    expect(useToastStore.getState().toasts[0].durationMs).toBe(0);
  });
});

// ---------------------------------------------------------------------------
// dismiss — removes a toast
// ---------------------------------------------------------------------------

describe('useToast — dismiss', () => {
  beforeEach(resetStore);

  it('removes the toast with the given id', () => {
    const { result } = renderHook(() => useToast());
    act(() => { result.current.push('to remove'); });
    const id = useToastStore.getState().toasts[0].id;

    act(() => { result.current.dismiss(id); });
    expect(useToastStore.getState().toasts).toHaveLength(0);
  });

  it('is a no-op for an unknown id', () => {
    const { result } = renderHook(() => useToast());
    act(() => { result.current.push('stays'); });
    act(() => { result.current.dismiss('nonexistent-id'); });
    expect(useToastStore.getState().toasts).toHaveLength(1);
  });
});

// ---------------------------------------------------------------------------
// Max visible / queue
// ---------------------------------------------------------------------------

describe('useToast — max visible and queue', () => {
  beforeEach(resetStore);

  it('allows up to 5 visible toasts', () => {
    const { result } = renderHook(() => useToast());
    act(() => {
      for (let i = 0; i < 5; i++) {
        result.current.push(`toast ${i}`);
      }
    });
    expect(useToastStore.getState().toasts).toHaveLength(5);
    expect(useToastStore.getState().queue).toHaveLength(0);
  });

  it('queues toasts beyond the max visible limit', () => {
    const { result } = renderHook(() => useToast());
    act(() => {
      for (let i = 0; i < 7; i++) {
        result.current.push(`toast ${i}`);
      }
    });
    expect(useToastStore.getState().toasts).toHaveLength(5);
    expect(useToastStore.getState().queue).toHaveLength(2);
  });

  it('drains queue when a visible toast is dismissed', () => {
    const { result } = renderHook(() => useToast());
    act(() => {
      for (let i = 0; i < 6; i++) {
        result.current.push(`toast ${i}`);
      }
    });

    const firstId = useToastStore.getState().toasts[0].id;
    act(() => { result.current.dismiss(firstId); });

    expect(useToastStore.getState().toasts).toHaveLength(5);
    expect(useToastStore.getState().queue).toHaveLength(0);
  });

  it('drains queue in FIFO order', () => {
    const { result } = renderHook(() => useToast());
    act(() => {
      for (let i = 0; i < 7; i++) {
        result.current.push(`toast ${i}`);
      }
    });

    // Queue contains toast 5 and toast 6
    const queuedMsg = useToastStore.getState().queue[0].message;
    expect(queuedMsg).toBe('toast 5');

    const firstId = useToastStore.getState().toasts[0].id;
    act(() => { result.current.dismiss(firstId); });

    // toast 5 should now be visible at the end
    const visible = useToastStore.getState().toasts;
    expect(visible[visible.length - 1].message).toBe('toast 5');
    expect(useToastStore.getState().queue[0].message).toBe('toast 6');
  });
});
