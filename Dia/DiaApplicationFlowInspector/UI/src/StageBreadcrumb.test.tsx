import { render, screen } from '@testing-library/react';
import { act } from 'react';
import { StageBreadcrumb } from './StageBreadcrumb';
import { useInspectorStore } from './useInspectorStore';

beforeEach(() => {
    useInspectorStore.getState().clearAll();
});

describe('StageBreadcrumb', () => {
    it('shows empty state when no timeline entries', () => {
        render(<StageBreadcrumb />);
        expect(screen.getByTestId('breadcrumb-empty')).toBeInTheDocument();
    });

    it('shows single current stage with no arrows when only 1 entry', () => {
        act(() => useInspectorStore.getState().pushStageEntry({ stageName: 'Boot', enteredAtMs: 0 }));
        render(<StageBreadcrumb />);
        expect(screen.getByTestId('breadcrumb-current').textContent).toBe('Boot');
        expect(screen.queryAllByTestId('breadcrumb-past')).toHaveLength(0);
    });

    it('shows past stages grey and current stage green with 3 entries', () => {
        act(() => {
            useInspectorStore.getState().pushStageEntry({ stageName: 'Boot', enteredAtMs: 0 });
            useInspectorStore.getState().pushStageEntry({ stageName: 'Menu', enteredAtMs: 1000 });
            useInspectorStore.getState().pushStageEntry({ stageName: 'Game', enteredAtMs: 3000 });
        });
        render(<StageBreadcrumb />);
        const past = screen.queryAllByTestId('breadcrumb-past');
        expect(past).toHaveLength(2);
        expect(past[0].textContent).toBe('Boot');
        expect(past[1].textContent).toBe('Menu');
        expect(screen.getByTestId('breadcrumb-current').textContent).toBe('Game');
    });

    it('shows duration labels between entries', () => {
        act(() => {
            useInspectorStore.getState().pushStageEntry({ stageName: 'Boot', enteredAtMs: 0 });
            useInspectorStore.getState().pushStageEntry({ stageName: 'Game', enteredAtMs: 500 });
        });
        render(<StageBreadcrumb />);
        expect(screen.getByText('500ms')).toBeInTheDocument();
    });

    it('container scrolls horizontally on overflow', () => {
        act(() => {
            for (let i = 0; i < 10; i++) {
                useInspectorStore.getState().pushStageEntry({ stageName: `Stage${i}`, enteredAtMs: i * 1000 });
            }
        });
        render(<StageBreadcrumb />);
        const container = screen.getByTestId('breadcrumb-container');
        expect(container).toBeInTheDocument();
    });
});
