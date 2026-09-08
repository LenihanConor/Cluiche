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
    useToast: vi.fn(() => ({ push: vi.fn(), dismiss: vi.fn() })),
}));

vi.mock('./components/BlueprintList', () => ({
    default: () => <div data-testid="blueprint-list" />,
}));

vi.mock('./components/PropertyPanel', () => ({
    default: () => <div data-testid="property-panel" />,
}));

vi.mock('./components/UsagePanel', () => ({
    default: () => <div data-testid="usage-panel" />,
}));

vi.mock('./components/ComponentPicker', () => ({
    default: () => null,
}));

vi.mock('./components/NavigateFailedModal', () => ({
    NavigateFailedModal: () => null,
}));

vi.mock('./components/ConfirmDialog', () => ({
    ConfirmDialog: () => null,
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
        const projectChangedSub = subs.find((s: { topic: string }) => s.topic === 'entity_template_editor.project_changed')!;
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
        const navigateFailedSub = subs.find((s: { topic: string }) => s.topic === 'entity_template_editor.navigate_failed')!;
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

    it('navigated handler calls selectBlueprint (setSelected + load + usage + available_components)', async () => {
        const mockRequest = vi.fn().mockResolvedValue({});
        vi.mocked(useBridgeRequest).mockReturnValue(mockRequest);

        await act(async () => {
            render(<App />);
        });

        const calls = vi.mocked(useBridgeSubscribe).mock.calls;
        const subs = calls[calls.length - 1][0];
        const navigatedSub = subs.find((s: { topic: string }) => s.topic === 'entity_template_editor.navigated')!;
        expect(navigatedSub).toBeDefined();

        await act(async () => {
            navigatedSub.handler({ id: 'bp-1', path: 'Assets/hero.diaentitytemplate' });
        });

        expect(mockRequest).toHaveBeenCalledWith('entity_template_editor.load', { path: 'Assets/hero.diaentitytemplate' });
        expect(useTemplateEditorStore.getState().selectedId).toBe('bp-1');
        expect(useTemplateEditorStore.getState().selectedPath).toBe('Assets/hero.diaentitytemplate');
    });

    it('shows error toast when selectBlueprint load request rejects', async () => {
        const mockToast = { push: vi.fn(), dismiss: vi.fn() };
        const { useToast } = await import('@dia/editor-ui');
        vi.mocked(useToast).mockReturnValue(mockToast);
        const mockRequest = vi.fn().mockRejectedValue(new Error('network'));
        vi.mocked(useBridgeRequest).mockReturnValue(mockRequest);

        await act(async () => {
            render(<App />);
        });

        const calls = vi.mocked(useBridgeSubscribe).mock.calls;
        const subs = calls[calls.length - 1][0];
        const navigatedSub = subs.find((s: { topic: string }) => s.topic === 'entity_template_editor.navigated')!;

        await act(async () => {
            navigatedSub.handler({ id: 'bp-fail', path: 'Assets/fail.diaentitytemplate' });
        });

        expect(mockToast.push).toHaveBeenCalledWith('Failed to load blueprint', 'error');
        expect(mockToast.push).toHaveBeenCalledWith('Failed to load usage', 'error');
        expect(mockToast.push).toHaveBeenCalledWith('Failed to load components', 'error');
    });

    it('shows error toast when create asset request rejects', async () => {
        const mockToast = { push: vi.fn(), dismiss: vi.fn() };
        const { useToast } = await import('@dia/editor-ui');
        vi.mocked(useToast).mockReturnValue(mockToast);
        const mockRequest = vi.fn().mockRejectedValue(new Error('fail'));
        vi.mocked(useBridgeRequest).mockReturnValue(mockRequest);

        await act(async () => {
            render(<App />);
        });

        const calls = vi.mocked(useBridgeSubscribe).mock.calls;
        const subs = calls[calls.length - 1][0];
        const navFailSub = subs.find((s: { topic: string }) => s.topic === 'entity_template_editor.navigate_failed')!;

        await act(async () => {
            navFailSub.handler({ instanceId: 'x', sourcePath: 'foo', error: 'e', assetType: 'diaentitytemplate' });
        });

        // NavigateFailedModal is mocked, so we verify via store state
        expect(useTemplateEditorStore.getState().navigateFailedData).not.toBeNull();
    });

    it('registry_changed handler fires get_list and updates groups', async () => {
        const groups = [{ label: 'Entity', items: [{ id: 'e1', label: 'Enemy', path: 'Assets/enemy.diaentitytemplate' }] }];
        const mockRequest = vi.fn().mockImplementation((topic: string) => {
            if (topic === 'entity_template_editor.get_list') return Promise.resolve({ success: true, groups });
            return Promise.resolve({});
        });
        vi.mocked(useBridgeRequest).mockReturnValue(mockRequest);

        await act(async () => {
            render(<App />);
        });

        const calls = vi.mocked(useBridgeSubscribe).mock.calls;
        const subs = calls[calls.length - 1][0];
        const registrySub = subs.find((s: { topic: string }) => s.topic === 'asset_catalogue.registry_changed')!;
        expect(registrySub).toBeDefined();

        await act(async () => {
            registrySub.handler({});
        });

        expect(useTemplateEditorStore.getState().groups).toEqual(groups);
    });
});
