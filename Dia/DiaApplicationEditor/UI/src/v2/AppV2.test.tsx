import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

const mockState = {
    applyStateSnapshot: vi.fn(), manifest: null, isDirty: false, hasManifest: true, filePath: null,
    loadManifest: vi.fn(), saveManifest: vi.fn(), refreshState: vi.fn(), setDirty: vi.fn(),
};
const mockUndo = {
    applyUndoResponse: vi.fn(), syncFromBackend: vi.fn(), canUndo: false, canRedo: false, count: 0, isDirty: false,
    undo: vi.fn(), redo: vi.fn(),
};
const mockValidation = {
    setResult: vi.fn(), result: null, isExpanded: false, toggleExpanded: vi.fn(), runValidation: vi.fn(),
};
const mockLive = {
    setConnectionState: vi.fn(), setActiveStage: vi.fn(), connect: vi.fn(), disconnect: vi.fn(),
    updateModuleStates: vi.fn(), updateStreamStates: vi.fn(), clearLiveState: vi.fn(),
    connectionState: 'disconnected', activeStage: null, modules: [], streams: [],
};

vi.mock('./useManifestStoreV2', () => ({
    useManifestStoreV2: vi.fn((selector: (s: any) => any) => selector(mockState)),
}));
vi.mock('./useUndoStoreV2', () => ({
    useUndoStoreV2: vi.fn((selector: (s: any) => any) => selector(mockUndo)),
}));
vi.mock('./useValidationStoreV2', () => ({
    useValidationStoreV2: vi.fn((selector: (s: any) => any) => selector(mockValidation)),
}));
vi.mock('./useLiveStoreV2', () => ({
    useLiveStoreV2: vi.fn((selector: (s: any) => any) => selector(mockLive)),
}));
vi.mock('./bridge', () => ({ bridgeRequest: vi.fn() }));

// Mock child components to avoid rendering full SVG/grid in unit tests
vi.mock('./GraphView', () => ({ GraphView: () => <div>graph-view-stub</div> }));
vi.mock('./ModulePresenceGrid', () => ({ ModulePresenceGrid: () => <div>presence-grid-stub</div> }));
vi.mock('./StreamsTab', () => ({ StreamsTab: () => <div>streams-tab-stub</div> }));
vi.mock('./PUInspector', () => ({ PUInspector: () => <div>pu-inspector-stub</div> }));
vi.mock('./StageConfiguration', () => ({ StageConfiguration: () => <div>stage-config-stub</div> }));
vi.mock('./ValidationBarV2', () => ({ ValidationBarV2: () => <div data-testid="validation-bar">validation-bar-stub</div> }));
vi.mock('./LiveConnectionButton', () => ({ LiveConnectionButton: () => <div>live-button-stub</div> }));
vi.mock('./LiveTransitionPanel', () => ({ LiveTransitionPanel: () => <div>transition-panel-stub</div> }));

import { AppV2 } from './AppV2';

describe('AppV2', () => {
    it('renders three tab buttons', () => {
        render(<AppV2 />);
        expect(screen.getByText('Graph')).toBeTruthy();
        expect(screen.getByText('Presence')).toBeTruthy();
        expect(screen.getByText('Streams')).toBeTruthy();
    });

    it('shows graph view by default', () => {
        render(<AppV2 />);
        expect(screen.getByText('graph-view-stub')).toBeTruthy();
    });

    it('switches to presence tab on click', () => {
        render(<AppV2 />);
        fireEvent.click(screen.getByText('Presence'));
        expect(screen.getByText('presence-grid-stub')).toBeTruthy();
    });
});
