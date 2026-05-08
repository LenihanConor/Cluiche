import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, act, fireEvent } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { useManifestStore } from './ManifestStore';

vi.mock('@uiw/react-codemirror', () => ({
    default: ({ value, onChange }: any) => (
        <textarea data-testid="codemirror" value={value} onChange={e => onChange(e.target.value)} />
    ),
}));
vi.mock('@codemirror/lang-json', () => ({ json: () => [] }));
vi.mock('@codemirror/lint', () => ({ linter: () => [] }));

import { ModuleInspector } from './ModuleInspector';

const MANIFEST = {
    version: 1,
    processing_units: [{
        instance_id: 'MainPU',
        type: 'MainProcessingUnit',
        phases: [{ instance_id: 'UpdatePhase', type: 'UpdatePhase' }],
        modules: [{
            instance_id: 'RenderModule',
            type: 'RenderModule',
            phases: ['UpdatePhase'],
            config: { fps: 60, vsync: true },
        }],
        transitions: [],
    }],
};

beforeEach(() => {
    vi.clearAllMocks();
    useManifestStore.setState({ manifest: null, selectedNode: null, isDirty: false });
});

// ── findModule ────────────────────────────────────────────────────────────────

describe('ModuleInspector – findModule / node selection', () => {
    it('shows placeholder when no node selected', () => {
        useManifestStore.setState({ manifest: MANIFEST });
        render(<ModuleInspector />);
        expect(screen.getByText(/select a node/i)).toBeInTheDocument();
    });

    it('shows "select a module" when a non-module node is selected', () => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'MainPU_UpdatePhase' });
        render(<ModuleInspector />);
        expect(screen.getByText(/select a module node/i)).toBeInTheDocument();
    });

    it('shows module instance_id in header when a valid 3-segment node is selected', () => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'MainPU_UpdatePhase_RenderModule' });
        render(<ModuleInspector />);
        // The bold instance_id span (font-weight 600) is one of the matches
        const matches = screen.getAllByText('RenderModule');
        expect(matches.length).toBeGreaterThanOrEqual(1);
    });

    it('handles node IDs with extra segments — last segment is moduleId', () => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'MainPU_Extra_UpdatePhase_RenderModule' });
        render(<ModuleInspector />);
        expect(screen.getAllByText('RenderModule').length).toBeGreaterThanOrEqual(1);
    });

    it('shows placeholder when puId does not match', () => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'UnknownPU_UpdatePhase_RenderModule' });
        render(<ModuleInspector />);
        expect(screen.getByText(/select a module node/i)).toBeInTheDocument();
    });

    it('shows placeholder when moduleId does not match', () => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'MainPU_UpdatePhase_NoSuchModule' });
        render(<ModuleInspector />);
        expect(screen.getByText(/select a module node/i)).toBeInTheDocument();
    });
});

// ── Config display ────────────────────────────────────────────────────────────

describe('ModuleInspector – config display', () => {
    beforeEach(() => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'MainPU_UpdatePhase_RenderModule' });
    });

    it('populates CodeMirror with the module config JSON', async () => {
        render(<ModuleInspector />);
        await userEvent.click(screen.getByText('JSON'));
        const ta = screen.getByTestId('codemirror') as HTMLTextAreaElement;
        expect(JSON.parse(ta.value)).toEqual({ fps: 60, vsync: true });
    });

    it('shows {} in CodeMirror when module has no config', async () => {
        const noConfigManifest = {
            ...MANIFEST,
            processing_units: [{
                ...MANIFEST.processing_units[0],
                modules: [{ instance_id: 'RenderModule', type: 'RenderModule', phases: [], config: undefined as any }],
            }],
        };
        useManifestStore.setState({ manifest: noConfigManifest });
        render(<ModuleInspector />);
        await userEvent.click(screen.getByText('JSON'));
        const ta = screen.getByTestId('codemirror') as HTMLTextAreaElement;
        expect(ta.value).toBe('{}');
    });
});

// ── applyConfig / debounce ────────────────────────────────────────────────────

describe('ModuleInspector – applyConfig debounce', () => {
    beforeEach(() => {
        vi.useFakeTimers({ shouldAdvanceTime: false });
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'MainPU_UpdatePhase_RenderModule' });
    });

    afterEach(() => {
        vi.useRealTimers();
    });

    it('does not post before 500ms', () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<ModuleInspector />);
        act(() => { screen.getByText('JSON').click(); });
        const ta = screen.getByTestId('codemirror');
        act(() => { fireEvent.change(ta, { target: { value: '{"fps":30}' } }); });
        act(() => { vi.advanceTimersByTime(499); });
        expect(spy).not.toHaveBeenCalled();
    });

    it('posts module_config_changed after 500ms with parsed config', () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<ModuleInspector />);
        act(() => { screen.getByText('JSON').click(); });
        const ta = screen.getByTestId('codemirror');
        act(() => { fireEvent.change(ta, { target: { value: '{"fps":30}' } }); });
        act(() => { vi.advanceTimersByTime(500); });
        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({
                payload: expect.objectContaining({
                    type: 'module_config_changed',
                    data: expect.objectContaining({ config: { fps: 30 } }),
                }),
            }),
            '*'
        );
    });

    it('does not post when JSON is invalid', () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<ModuleInspector />);
        act(() => { screen.getByText('JSON').click(); });
        const ta = screen.getByTestId('codemirror');
        act(() => { fireEvent.change(ta, { target: { value: '{invalid}' } }); });
        act(() => { vi.advanceTimersByTime(500); });
        expect(spy).not.toHaveBeenCalled();
    });

    it('marks store dirty after posting', () => {
        render(<ModuleInspector />);
        act(() => { screen.getByText('JSON').click(); });
        const ta = screen.getByTestId('codemirror');
        act(() => { fireEvent.change(ta, { target: { value: '{"fps":30}' } }); });
        act(() => { vi.advanceTimersByTime(500); });
        expect(useManifestStore.getState().isDirty).toBe(true);
    });
});

// ── Form/JSON tab toggle ──────────────────────────────────────────────────────

describe('ModuleInspector – tab toggle', () => {
    beforeEach(() => {
        useManifestStore.setState({ manifest: MANIFEST, selectedNode: 'MainPU_UpdatePhase_RenderModule' });
    });

    it('shows Form view by default with config field labels', () => {
        render(<ModuleInspector />);
        expect(screen.getByText('fps')).toBeInTheDocument();
        expect(screen.queryByTestId('codemirror')).not.toBeInTheDocument();
    });

    it('switches to JSON view when JSON tab is clicked', async () => {
        render(<ModuleInspector />);
        await userEvent.click(screen.getByText('JSON'));
        expect(screen.getByTestId('codemirror')).toBeInTheDocument();
    });
});
