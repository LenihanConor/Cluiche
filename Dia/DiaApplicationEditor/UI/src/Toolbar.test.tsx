import { describe, it, expect, beforeEach, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { Toolbar } from './Toolbar';
import { useManifestStore } from './ManifestStore';

beforeEach(() => {
    useManifestStore.setState({ manifest: null, isDirty: false, currentView: 'tree' });
});

describe('Toolbar – title', () => {
    it('shows "No manifest" when no manifest loaded', () => {
        render(<Toolbar />);
        expect(screen.getByText(/no manifest/i)).toBeInTheDocument();
    });

    it('shows "Manifest Editor" when manifest is loaded', () => {
        useManifestStore.setState({ manifest: { version: 1, processing_units: [] } });
        render(<Toolbar />);
        expect(screen.getByText(/manifest editor/i)).toBeInTheDocument();
    });

    it('shows dirty indicator "*" when isDirty', () => {
        useManifestStore.setState({ manifest: { version: 1, processing_units: [] }, isDirty: true });
        render(<Toolbar />);
        expect(screen.getByText('*')).toBeInTheDocument();
    });

    it('does not show "*" when clean', () => {
        useManifestStore.setState({ manifest: { version: 1, processing_units: [] }, isDirty: false });
        render(<Toolbar />);
        expect(screen.queryByText('*')).not.toBeInTheDocument();
    });
});

describe('Toolbar – buttons', () => {
    it('Open button is always enabled', () => {
        render(<Toolbar />);
        expect(screen.getByTitle('Open .diaapp file')).not.toBeDisabled();
    });

    it('Save button is disabled when not dirty', () => {
        useManifestStore.setState({ manifest: { version: 1, processing_units: [] }, isDirty: false });
        render(<Toolbar />);
        expect(screen.getByTitle('Save (Ctrl+S)')).toBeDisabled();
    });

    it('Save button is enabled when dirty', () => {
        useManifestStore.setState({ manifest: { version: 1, processing_units: [] }, isDirty: true });
        render(<Toolbar />);
        expect(screen.getByTitle('Save (Ctrl+S)')).not.toBeDisabled();
    });

    it('Open button posts open_manifest message', async () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<Toolbar />);
        await userEvent.click(screen.getByTitle('Open .diaapp file'));
        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({ payload: expect.objectContaining({ type: 'open_manifest' }) }),
            '*'
        );
    });

    it('Save button posts save_manifest message when dirty', async () => {
        useManifestStore.setState({ manifest: { version: 1, processing_units: [] }, isDirty: true });
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<Toolbar />);
        await userEvent.click(screen.getByTitle('Save (Ctrl+S)'));
        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({ payload: expect.objectContaining({ type: 'save_manifest' }) }),
            '*'
        );
    });
});

describe('Toolbar – view switcher', () => {
    beforeEach(() => {
        useManifestStore.setState({ manifest: { version: 1, processing_units: [] }, currentView: 'tree' });
    });

    it('renders Tree, Lifecycle and Flow buttons', () => {
        render(<Toolbar />);
        expect(screen.getByTitle('Tree view')).toBeInTheDocument();
        expect(screen.getByTitle('Lifecycle view')).toBeInTheDocument();
        expect(screen.getByTitle('Flow view')).toBeInTheDocument();
    });

    it('view buttons are disabled when no manifest', () => {
        useManifestStore.setState({ manifest: null });
        render(<Toolbar />);
        expect(screen.getByTitle('Flow view')).toBeDisabled();
    });

    it('clicking Flow sets currentView to flow', async () => {
        render(<Toolbar />);
        await userEvent.click(screen.getByTitle('Flow view'));
        expect(useManifestStore.getState().currentView).toBe('flow');
    });

    it('clicking Lifecycle sets currentView to lifecycle', async () => {
        render(<Toolbar />);
        await userEvent.click(screen.getByTitle('Lifecycle view'));
        expect(useManifestStore.getState().currentView).toBe('lifecycle');
    });
});
