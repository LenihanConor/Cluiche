import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, act } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { TypeSelectorModal } from './TypeSelectorModal';

function dispatch(topic: string, data: object) {
    window.dispatchEvent(new MessageEvent('message', {
        data: { __dia: true, topic, data },
    }));
}

const MODULE_TYPES = [
    { type: 'ApplicationModule', description: 'Base Dia application module' },
    { type: 'MetricsCollectorModule', description: 'Collects per-frame performance metrics' },
];

const PHASE_TYPES = [
    { type: 'ApplicationPhase', description: 'Base Dia application phase' },
];

beforeEach(() => { vi.clearAllMocks(); });

describe('TypeSelectorModal – visibility', () => {
    it('renders nothing when no show_type_selector received', () => {
        const { container } = render(<TypeSelectorModal />);
        expect(container.firstChild).toBeNull();
    });

    it('shows modal after show_type_selector message for module', () => {
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'MainPU',
                phase_id: 'UpdatePhase',
                node_type: 'module',
                available_types: MODULE_TYPES,
            });
        });
        expect(screen.getByText('Add Module')).toBeInTheDocument();
    });

    it('shows modal after show_type_selector message for phase', () => {
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'MainPU',
                node_type: 'phase',
                available_types: PHASE_TYPES,
            });
        });
        expect(screen.getByText('Add Phase')).toBeInTheDocument();
    });

    it('hides modal after Cancel click', async () => {
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'MainPU',
                phase_id: 'Update',
                node_type: 'module',
                available_types: MODULE_TYPES,
            });
        });
        await userEvent.click(screen.getByText('Cancel'));
        expect(screen.queryByText('Add Module')).not.toBeInTheDocument();
    });

    it('hides modal after Escape key', () => {
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'MainPU',
                phase_id: 'Update',
                node_type: 'module',
                available_types: MODULE_TYPES,
            });
        });
        act(() => {
            const modal = screen.getByText('Add Module').closest('div[style]')!;
            modal.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
        });
        expect(screen.queryByText('Add Module')).not.toBeInTheDocument();
    });
});

describe('TypeSelectorModal – confirm', () => {
    it('sends add_node_confirmed with correct fields for module', async () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'MainPU',
                phase_id: 'UpdatePhase',
                node_type: 'module',
                available_types: MODULE_TYPES,
            });
        });

        const input = screen.getByPlaceholderText(/e\.g\. MyModule/);
        await userEvent.type(input, 'MyNewModule');
        await userEvent.click(screen.getByText('Add'));

        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: {
                    type: 'add_node_confirmed',
                    data: {
                        pu_id: 'MainPU',
                        phase_id: 'UpdatePhase',
                        node_type: 'module',
                        type_name: 'ApplicationModule',
                        instance_id: 'MyNewModule',
                    },
                },
            }),
            '*'
        );
    });

    it('sends add_node_confirmed with correct fields for phase', async () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'MainPU',
                node_type: 'phase',
                available_types: PHASE_TYPES,
            });
        });

        const input = screen.getByPlaceholderText(/e\.g\. MyPhase/);
        await userEvent.type(input, 'RenderPhase');
        await userEvent.click(screen.getByText('Add'));

        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({
                __diaFromFrame: true,
                payload: {
                    type: 'add_node_confirmed',
                    data: {
                        pu_id: 'MainPU',
                        phase_id: undefined,
                        node_type: 'phase',
                        type_name: 'ApplicationPhase',
                        instance_id: 'RenderPhase',
                    },
                },
            }),
            '*'
        );
    });

    it('does not send when instance_id is empty', async () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'MainPU',
                phase_id: 'Update',
                node_type: 'module',
                available_types: MODULE_TYPES,
            });
        });

        await userEvent.click(screen.getByText('Add'));
        expect(spy).not.toHaveBeenCalled();
    });

    it('trims whitespace from instance_id', async () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'PU',
                phase_id: 'Phase',
                node_type: 'module',
                available_types: MODULE_TYPES,
            });
        });

        const input = screen.getByPlaceholderText(/e\.g\. MyModule/);
        await userEvent.type(input, '  Trimmed  ');
        await userEvent.click(screen.getByText('Add'));

        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({
                payload: expect.objectContaining({
                    data: expect.objectContaining({ instance_id: 'Trimmed' }),
                }),
            }),
            '*'
        );
    });
});

describe('TypeSelectorModal – error handling', () => {
    it('shows error message from add_node_error', () => {
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'PU',
                phase_id: 'Phase',
                node_type: 'module',
                available_types: MODULE_TYPES,
            });
        });
        act(() => {
            dispatch('add_node_error', { message: 'A module with this instance ID already exists' });
        });
        expect(screen.getByText('A module with this instance ID already exists')).toBeInTheDocument();
    });

    it('shows "No types available" when options list is empty', () => {
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'PU',
                phase_id: 'Phase',
                node_type: 'module',
                available_types: [],
            });
        });
        expect(screen.getByText('No types available')).toBeInTheDocument();
    });
});

describe('TypeSelectorModal – type selection', () => {
    it('allows selecting a different type from dropdown', async () => {
        const spy = vi.spyOn(window.parent, 'postMessage');
        render(<TypeSelectorModal />);
        act(() => {
            dispatch('show_type_selector', {
                pu_id: 'PU',
                phase_id: 'Phase',
                node_type: 'module',
                available_types: MODULE_TYPES,
            });
        });

        await userEvent.selectOptions(screen.getByRole('combobox'), 'MetricsCollectorModule');
        await userEvent.type(screen.getByPlaceholderText(/e\.g\. MyModule/), 'Metrics');
        await userEvent.click(screen.getByText('Add'));

        expect(spy).toHaveBeenCalledWith(
            expect.objectContaining({
                payload: expect.objectContaining({
                    data: expect.objectContaining({ type_name: 'MetricsCollectorModule' }),
                }),
            }),
            '*'
        );
    });
});
