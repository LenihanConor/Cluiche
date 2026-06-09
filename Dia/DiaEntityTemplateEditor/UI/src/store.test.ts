import { useTemplateEditorStore } from './store';

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
});

describe('TemplateEditorStore', () => {
    it('initializes with default values', () => {
        const state = useTemplateEditorStore.getState();
        expect(state.projectValid).toBe(false);
        expect(state.groups).toEqual([]);
        expect(state.selectedId).toBeNull();
        expect(state.selectedPath).toBeNull();
        expect(state.properties).toBeNull();
        expect(state.usage).toEqual([]);
        expect(state.availableComponents).toEqual([]);
        expect(state.navigateFailedData).toBeNull();
    });

    it('setProjectValid updates projectValid to true', () => {
        useTemplateEditorStore.getState().setProjectValid(true);
        expect(useTemplateEditorStore.getState().projectValid).toBe(true);
    });

    it('setProjectValid updates projectValid to false', () => {
        useTemplateEditorStore.getState().setProjectValid(true);
        useTemplateEditorStore.getState().setProjectValid(false);
        expect(useTemplateEditorStore.getState().projectValid).toBe(false);
    });

    it('setGroups replaces groups array', () => {
        const groups = [
            { label: 'Entity', items: [{ id: 'diaentitytemplate.hero', label: 'hero', path: 'Assets/hero.diaentitytemplate' }] },
        ];
        useTemplateEditorStore.getState().setGroups(groups);
        expect(useTemplateEditorStore.getState().groups).toEqual(groups);
    });

    it('setSelected sets both selectedId and selectedPath', () => {
        useTemplateEditorStore.getState().setSelected('diaentitytemplate.hero', 'Assets/hero.diaentitytemplate');
        const state = useTemplateEditorStore.getState();
        expect(state.selectedId).toBe('diaentitytemplate.hero');
        expect(state.selectedPath).toBe('Assets/hero.diaentitytemplate');
    });

    it('clearSelected resets selectedId, selectedPath, properties, usage, and availableComponents', () => {
        useTemplateEditorStore.getState().setSelected('diaentitytemplate.hero', 'Assets/hero.diaentitytemplate');
        useTemplateEditorStore.getState().setProperties({ id: 'hero', components: [] });
        useTemplateEditorStore.getState().setUsage([{ sceneId: 'diascene.level1', instanceCount: 3 }]);
        useTemplateEditorStore.getState().setAvailableComponents([{ typeId: 'Health', label: 'Health' }]);

        useTemplateEditorStore.getState().clearSelected();

        const state = useTemplateEditorStore.getState();
        expect(state.selectedId).toBeNull();
        expect(state.selectedPath).toBeNull();
        expect(state.properties).toBeNull();
        expect(state.usage).toEqual([]);
        expect(state.availableComponents).toEqual([]);
    });

    it('setProperties stores blueprint properties', () => {
        const props = {
            id: 'hero',
            components: [
                {
                    type: 'Transform2D',
                    fields: [
                        { name: 'x', kind: 'primitive', value: 10, codeDefault: 0 },
                        { name: 'y', kind: 'primitive', codeDefault: 0 },
                    ],
                },
            ],
        };
        useTemplateEditorStore.getState().setProperties(props);
        expect(useTemplateEditorStore.getState().properties).toEqual(props);
    });

    it('setProperties accepts null', () => {
        useTemplateEditorStore.getState().setProperties({ id: 'hero', components: [] });
        useTemplateEditorStore.getState().setProperties(null);
        expect(useTemplateEditorStore.getState().properties).toBeNull();
    });

    it('setUsage replaces usage array', () => {
        const usage = [{ sceneId: 'diascene.level1', instanceCount: 3 }];
        useTemplateEditorStore.getState().setUsage(usage);
        expect(useTemplateEditorStore.getState().usage).toEqual(usage);
    });

    it('setAvailableComponents replaces availableComponents array', () => {
        const components = [
            { typeId: 'Health', label: 'Health', description: 'Health component' },
            { typeId: 'Transform2D', label: 'Transform 2D' },
        ];
        useTemplateEditorStore.getState().setAvailableComponents(components);
        expect(useTemplateEditorStore.getState().availableComponents).toEqual(components);
    });

    it('setNavigateFailedData stores navigate failure data', () => {
        const data = {
            instanceId: 'inst-1',
            sourcePath: 'Assets/hero.diaentitytemplate',
            error: 'Not found',
            assetType: 'diaentitytemplate',
        };
        useTemplateEditorStore.getState().setNavigateFailedData(data);
        expect(useTemplateEditorStore.getState().navigateFailedData).toEqual(data);
    });

    it('setNavigateFailedData accepts null to clear', () => {
        const data = {
            instanceId: 'inst-1',
            sourcePath: 'Assets/hero.diaentitytemplate',
            error: 'Not found',
            assetType: 'diaentitytemplate',
        };
        useTemplateEditorStore.getState().setNavigateFailedData(data);
        useTemplateEditorStore.getState().setNavigateFailedData(null);
        expect(useTemplateEditorStore.getState().navigateFailedData).toBeNull();
    });
});
