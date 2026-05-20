import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

vi.mock('./useManifestStoreV2', () => ({
    useManifestStoreV2: vi.fn((selector: (s: any) => any) => selector({ applyStateSnapshot: vi.fn(), manifest: null, isDirty: false, hasManifest: false, filePath: null })),
}));
vi.mock('./useUndoStoreV2', () => ({
    useUndoStoreV2: vi.fn((selector: (s: any) => any) => selector({ syncFromBackend: vi.fn(), canUndo: false, canRedo: false, count: 0, isDirty: false })),
}));
vi.mock('./useValidationStoreV2', () => ({
    useValidationStoreV2: vi.fn((selector: (s: any) => any) => selector({ setResult: vi.fn(), result: null, isExpanded: false })),
}));
vi.mock('./useLiveStoreV2', () => ({
    useLiveStoreV2: vi.fn((selector: (s: any) => any) => selector({
        setConnectionState: vi.fn(), setActiveStage: vi.fn(),
        updateModuleStates: vi.fn(), updateStreamStates: vi.fn(), clearLiveState: vi.fn(),
        connectionState: 'disconnected', activeStage: null, modules: [], streams: [],
    })),
}));

import { AppV2 } from './AppV2';

describe('AppV2', () => {
    it('renders three tab buttons', () => {
        render(<AppV2 />);
        expect(screen.getByText('Graph')).toBeTruthy();
        expect(screen.getByText('Presence')).toBeTruthy();
        expect(screen.getByText('Streams')).toBeTruthy();
    });

    it('shows graph tab content by default', () => {
        render(<AppV2 />);
        expect(screen.getByText(/Graph view/)).toBeTruthy();
    });

    it('switches to presence tab on click', () => {
        const { getByText } = render(<AppV2 />);
        fireEvent.click(getByText('Presence'));
        expect(screen.getByText(/Presence grid/)).toBeTruthy();
    });
});
