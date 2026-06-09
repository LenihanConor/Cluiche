import { render, screen, act } from '@testing-library/react';
import { useTemplateEditorStore } from './store';
import App from './App';

vi.mock('@dia/editor-ui', () => ({
    theme: { bg: '#1e1e1e', text: '#d4d4d4', border: '#3c3c3c' },
    EmptyState: ({ message }: { message: string }) => <div data-testid="empty-state">{message}</div>,
    ToastRenderer: () => null,
    injectThemeVars: () => {},
    useBridgeSubscribe: vi.fn(),
    useBridgeRequest: vi.fn(() => vi.fn().mockResolvedValue({})),
    useResizableDivider: vi.fn(() => ({ leftWidth: 240, dividerProps: { onMouseDown: vi.fn() } })),
}));

// Import mocked module for spy access
import { useBridgeSubscribe, useBridgeRequest } from '@dia/editor-ui';

const initialState = {
    projectValid: false,
    groups: [],
    selectedId: null,
    selectedPath: null,
    properties: null,
    usage: [],
    availableComponents: [],
    navigateFailedData: null,
};

beforeEach(() => {
    useTemplateEditorStore.setState(initialState);
    vi.clearAllMocks();
    // Restore default mock implementations after clearAllMocks
    vi.mocked(useBridgeRequest).mockReturnValue(vi.fn().mockResolvedValue({}));
    vi.mocked(useBridgeSubscribe).mockImplementation(() => {});
});

describe('App', () => {
    it('renders app-root without crashing', () => {
        render(<App />);
        expect(screen.getByTestId('app-root')).toBeInTheDocument();
    });

    it('shows project-overlay when projectValid is false (initial state)', () => {
        render(<App />);
        expect(screen.getByTestId('project-overlay')).toBeInTheDocument();
        expect(screen.getByTestId('empty-state')).toHaveTextContent('No project loaded');
    });

    it('hides project-overlay when store.projectValid is true', () => {
        useTemplateEditorStore.setState({ projectValid: true });
        render(<App />);
        expect(screen.queryByTestId('project-overlay')).not.toBeInTheDocument();
    });

    it('renders panel-list, panel-center, panel-right', () => {
        render(<App />);
        expect(screen.getByTestId('panel-list')).toBeInTheDocument();
        expect(screen.getByTestId('panel-center')).toBeInTheDocument();
        expect(screen.getByTestId('panel-right')).toBeInTheDocument();
    });

    it('project_changed handler updates projectValid and hides overlay', async () => {
        render(<App />);
        expect(screen.getByTestId('project-overlay')).toBeInTheDocument();

        // Grab the subscriptions registered in the last useBridgeSubscribe call
        const calls = vi.mocked(useBridgeSubscribe).mock.calls;
        expect(calls.length).toBeGreaterThan(0);
        const subs = calls[calls.length - 1][0];
        const projectChangedSub = subs.find((s: { topic: string }) => s.topic === 'entity_template_editor.project_changed');
        expect(projectChangedSub).toBeDefined();

        await act(async () => {
            projectChangedSub.handler({ isValid: true });
        });

        expect(useTemplateEditorStore.getState().projectValid).toBe(true);
        expect(screen.queryByTestId('project-overlay')).not.toBeInTheDocument();
    });

    it('navigate_failed handler calls setNavigateFailedData and updates store', async () => {
        render(<App />);

        const calls = vi.mocked(useBridgeSubscribe).mock.calls;
        const subs = calls[calls.length - 1][0];
        const navigateFailedSub = subs.find((s: { topic: string }) => s.topic === 'entity_template_editor.navigate_failed');
        expect(navigateFailedSub).toBeDefined();

        const failData = {
            instanceId: 'inst-1',
            sourcePath: 'Assets/hero.diaentitytemplate',
            error: 'Not found',
            assetType: 'diaentitytemplate',
        };

        await act(async () => {
            navigateFailedSub.handler(failData);
        });

        expect(useTemplateEditorStore.getState().navigateFailedData).toEqual(failData);
    });

    it('on mount fires get_project_state request', async () => {
        const mockRequest = vi.fn().mockResolvedValue({});
        vi.mocked(useBridgeRequest).mockReturnValue(mockRequest);

        await act(async () => {
            render(<App />);
        });

        expect(mockRequest).toHaveBeenCalledWith('entity_template_editor.get_project_state');
    });

    it('on mount fires get_list request', async () => {
        const mockRequest = vi.fn().mockResolvedValue({});
        vi.mocked(useBridgeRequest).mockReturnValue(mockRequest);

        await act(async () => {
            render(<App />);
        });

        expect(mockRequest).toHaveBeenCalledWith('entity_template_editor.get_list');
    });
});
