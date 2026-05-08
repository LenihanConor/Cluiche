import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, act } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { useManifestStore } from './ManifestStore';

// Stub all child components to isolate App logic
vi.mock('./Toolbar',           () => ({ Toolbar:          () => <div data-testid="toolbar" /> }));
vi.mock('./ValidationPanel',   () => ({ ValidationPanel:  () => <div data-testid="validation-panel" /> }));
vi.mock('./TreeView',          () => ({ TreeView:         () => <div data-testid="tree-view" /> }));
vi.mock('./FlowView',          () => ({ FlowView:         () => <div data-testid="flow-view" /> }));
vi.mock('./LifecycleView',     () => ({ LifecycleView:    () => <div data-testid="lifecycle-view" /> }));
vi.mock('./ModuleInspector',   () => ({ ModuleInspector:  () => <div data-testid="module-inspector" /> }));
vi.mock('./ConflictBanner',    () => ({
    ConflictBanner:     () => <div data-testid="conflict-banner" />,
    useConflictDetection: () => ({ conflict: null, dismiss: vi.fn() }),
}));
vi.mock('./useValidation',     () => ({ useValidation:    () => {} }));

import { App } from './App';

const MANIFEST = { version: 1, processing_units: [] };

beforeEach(() => {
    vi.clearAllMocks();
    useManifestStore.setState({ manifest: null, selectedNode: null, currentView: 'tree' });
});

// ── Drop-zone (no manifest) ───────────────────────────────────────────────────

describe('App – drop zone', () => {
    it('shows drop zone when manifest is null', () => {
        render(<App />);
        expect(screen.getByText(/drop a .diaapp file/i)).toBeInTheDocument();
    });

    it('posts open_manifest with file path on drop', () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<App />);

        const dropZone = screen.getByText(/drop a .diaapp file/i).closest('div')!;
        const file = Object.assign(new File([''], 'test.diaapp'), { path: '/some/path/test.diaapp' });
        const dt = { files: [file] };

        act(() => {
            dropZone.dispatchEvent(
                Object.assign(new Event('drop', { bubbles: true }), { dataTransfer: dt, preventDefault: () => {}, stopPropagation: () => {} })
            );
        });

        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({ payload: expect.objectContaining({ type: 'open_manifest', data: { path: '/some/path/test.diaapp' } }) }),
            '*'
        );
    });

    it('does not post when dropped file has no path', () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<App />);

        const dropZone = screen.getByText(/drop a .diaapp file/i).closest('div')!;
        const file = new File([''], 'test.diaapp'); // no .path property
        const dt = { files: [file] };

        act(() => {
            dropZone.dispatchEvent(
                Object.assign(new Event('drop', { bubbles: true }), { dataTransfer: dt, preventDefault: () => {}, stopPropagation: () => {} })
            );
        });

        expect(spy).not.toHaveBeenCalled();
    });

    it('does not post when no file in drop event', () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<App />);

        const dropZone = screen.getByText(/drop a .diaapp file/i).closest('div')!;
        const dt = { files: [] };

        act(() => {
            dropZone.dispatchEvent(
                Object.assign(new Event('drop', { bubbles: true }), { dataTransfer: dt, preventDefault: () => {}, stopPropagation: () => {} })
            );
        });

        expect(spy).not.toHaveBeenCalled();
    });
});

// ── View routing ──────────────────────────────────────────────────────────────

describe('App – view routing', () => {
    it('shows TreeView when currentView is tree', () => {
        useManifestStore.setState({ manifest: MANIFEST, currentView: 'tree' });
        render(<App />);
        expect(screen.getByTestId('tree-view')).toBeInTheDocument();
    });

    it('shows FlowView when currentView is flow', () => {
        useManifestStore.setState({ manifest: MANIFEST, currentView: 'flow' });
        render(<App />);
        expect(screen.getByTestId('flow-view')).toBeInTheDocument();
    });

    it('shows LifecycleView when currentView is lifecycle', () => {
        useManifestStore.setState({ manifest: MANIFEST, currentView: 'lifecycle' });
        render(<App />);
        expect(screen.getByTestId('lifecycle-view')).toBeInTheDocument();
    });
});

// ── ModuleInspector visibility ────────────────────────────────────────────────

describe('App – inspector visibility (3-segment node ID)', () => {
    it('hides inspector when no selectedNode', () => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: null });
        render(<App />);
        expect(screen.queryByTestId('module-inspector')).not.toBeInTheDocument();
    });

    it('hides inspector when selectedNode has fewer than 3 segments', () => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'MainPU_Phase' });
        render(<App />);
        expect(screen.queryByTestId('module-inspector')).not.toBeInTheDocument();
    });

    it('shows inspector when selectedNode has exactly 3 segments', () => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'MainPU_Phase_Module' });
        render(<App />);
        expect(screen.getByTestId('module-inspector')).toBeInTheDocument();
    });

    it('shows inspector when selectedNode has more than 3 segments', () => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'MainPU_Extra_Phase_Module' });
        render(<App />);
        expect(screen.getByTestId('module-inspector')).toBeInTheDocument();
    });
});

// ── Global drag prevention ────────────────────────────────────────────────────

describe('App – global drag prevention', () => {
    it('prevents default on document dragover to stop CEF navigation', () => {
        render(<App />);
        const e = new Event('dragover', { bubbles: true, cancelable: true });
        document.dispatchEvent(e);
        expect(e.defaultPrevented).toBe(true);
    });

    it('prevents default on document drop', () => {
        render(<App />);
        const e = new Event('drop', { bubbles: true, cancelable: true });
        document.dispatchEvent(e);
        expect(e.defaultPrevented).toBe(true);
    });
});
