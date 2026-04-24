import { describe, it, expect, beforeEach } from 'vitest';
import { useManifestStore } from './ManifestStore';

// Reset store to initial state between tests
beforeEach(() => {
    useManifestStore.setState({
        manifestVersion: 0,
        manifest: null,
        isDirty: false,
        currentView: 'tree',
        validationResult: null,
        selectedNode: null,
    });
});

describe('ManifestStore – manifest', () => {
    it('starts with null manifest', () => {
        expect(useManifestStore.getState().manifest).toBeNull();
    });

    it('setManifest stores the manifest', () => {
        const m = { version: 1, processing_units: [] };
        useManifestStore.getState().setManifest(m);
        expect(useManifestStore.getState().manifest).toEqual(m);
    });

    it('setManifest(null) clears the manifest', () => {
        useManifestStore.getState().setManifest({ version: 1, processing_units: [] });
        useManifestStore.getState().setManifest(null);
        expect(useManifestStore.getState().manifest).toBeNull();
    });
});

describe('ManifestStore – manifestVersion', () => {
    it('starts at 0', () => {
        expect(useManifestStore.getState().manifestVersion).toBe(0);
    });

    it('bumpManifestVersion increments by 1 each call', () => {
        useManifestStore.getState().bumpManifestVersion();
        expect(useManifestStore.getState().manifestVersion).toBe(1);
        useManifestStore.getState().bumpManifestVersion();
        expect(useManifestStore.getState().manifestVersion).toBe(2);
    });
});

describe('ManifestStore – isDirty', () => {
    it('starts clean', () => {
        expect(useManifestStore.getState().isDirty).toBe(false);
    });

    it('setDirty(true) marks dirty', () => {
        useManifestStore.getState().setDirty(true);
        expect(useManifestStore.getState().isDirty).toBe(true);
    });

    it('setDirty(false) clears dirty', () => {
        useManifestStore.getState().setDirty(true);
        useManifestStore.getState().setDirty(false);
        expect(useManifestStore.getState().isDirty).toBe(false);
    });
});

describe('ManifestStore – view mode', () => {
    it('starts on tree view', () => {
        expect(useManifestStore.getState().currentView).toBe('tree');
    });

    it('setView changes to the requested view', () => {
        useManifestStore.getState().setView('flow');
        expect(useManifestStore.getState().currentView).toBe('flow');
        useManifestStore.getState().setView('lifecycle');
        expect(useManifestStore.getState().currentView).toBe('lifecycle');
    });

    it('toggleView cycles tree → flow → lifecycle → tree', () => {
        // starts at tree
        useManifestStore.getState().toggleView();
        expect(useManifestStore.getState().currentView).toBe('flow');
        useManifestStore.getState().toggleView();
        expect(useManifestStore.getState().currentView).toBe('lifecycle');
        useManifestStore.getState().toggleView();
        expect(useManifestStore.getState().currentView).toBe('tree');
    });
});

describe('ManifestStore – validation', () => {
    it('starts with null validationResult', () => {
        expect(useManifestStore.getState().validationResult).toBeNull();
    });

    it('setValidationResult stores the result', () => {
        const result = { is_valid: true, errors: [] };
        useManifestStore.getState().setValidationResult(result);
        expect(useManifestStore.getState().validationResult).toEqual(result);
    });
});

describe('ManifestStore – selectedNode', () => {
    it('starts with no selection', () => {
        expect(useManifestStore.getState().selectedNode).toBeNull();
    });

    it('setSelectedNode stores node id', () => {
        useManifestStore.getState().setSelectedNode('pu_phase_mod');
        expect(useManifestStore.getState().selectedNode).toBe('pu_phase_mod');
    });

    it('setSelectedNode(null) clears selection', () => {
        useManifestStore.getState().setSelectedNode('pu_phase_mod');
        useManifestStore.getState().setSelectedNode(null);
        expect(useManifestStore.getState().selectedNode).toBeNull();
    });
});
