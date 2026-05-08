import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useManifestStore } from './ManifestStore';
import { useValidation } from './useValidation';

beforeEach(() => {
    vi.useFakeTimers();
    useManifestStore.setState({ validationResult: null, manifest: null, isDirty: false });
});

afterEach(() => {
    vi.useRealTimers();
    vi.restoreAllMocks();
});

describe('useValidation – validate trigger', () => {
    it('posts validate message after 500ms debounce when manifestVersion changes', () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        renderHook(() => useValidation(1));

        expect(spy).not.toHaveBeenCalled();
        act(() => { vi.advanceTimersByTime(500); });

        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({ payload: expect.objectContaining({ type: 'validate' }) }),
            '*'
        );
    });

    it('does not post before 500ms', () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        renderHook(() => useValidation(1));

        act(() => { vi.advanceTimersByTime(499); });
        expect(spy).not.toHaveBeenCalled();
    });
});

describe('useValidation – message handling', () => {
    it('validation_complete sets validationResult in store', () => {
        renderHook(() => useValidation(0));
        const result = { is_valid: true, errors: [] };

        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { __dia: true, topic: 'validation_complete', data: result },
            }));
        });

        expect(useManifestStore.getState().validationResult).toEqual(result);
    });

    it('manifest_loaded sets manifest and clears dirty', () => {
        useManifestStore.setState({ isDirty: true });
        renderHook(() => useValidation(0));
        const m = { version: 1, processing_units: [] };

        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { __dia: true, topic: 'manifest_loaded', data: { manifest: m, is_dirty: false } },
            }));
        });

        expect(useManifestStore.getState().manifest).toEqual(m);
        expect(useManifestStore.getState().isDirty).toBe(false);
    });

    it('manifest_updated sets manifest and marks dirty', () => {
        renderHook(() => useValidation(0));
        const m = { version: 2, processing_units: [] };

        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { __dia: true, topic: 'manifest_updated', data: { manifest: m } },
            }));
        });

        expect(useManifestStore.getState().manifest).toEqual(m);
        expect(useManifestStore.getState().isDirty).toBe(true);
    });

    it('manifest_saved clears dirty flag', () => {
        useManifestStore.setState({ isDirty: true });
        renderHook(() => useValidation(0));

        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { __dia: true, topic: 'manifest_saved' },
            }));
        });

        expect(useManifestStore.getState().isDirty).toBe(false);
    });

    it('manifest_closed clears manifest and dirty', () => {
        useManifestStore.setState({ manifest: { version: 1, processing_units: [] }, isDirty: true });
        renderHook(() => useValidation(0));

        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { __dia: true, topic: 'manifest_closed' },
            }));
        });

        expect(useManifestStore.getState().manifest).toBeNull();
        expect(useManifestStore.getState().isDirty).toBe(false);
    });

    it('ignores messages without __dia flag', () => {
        renderHook(() => useValidation(0));

        act(() => {
            window.dispatchEvent(new MessageEvent('message', {
                data: { topic: 'validation_complete', data: { is_valid: false, errors: [] } },
            }));
        });

        expect(useManifestStore.getState().validationResult).toBeNull();
    });
});
