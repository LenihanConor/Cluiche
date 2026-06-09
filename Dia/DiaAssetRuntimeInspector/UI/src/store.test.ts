import { describe, it, expect, beforeEach } from 'vitest';
import { useAssetRuntimeStore } from './store';
import type { AssetRow, StageNode, GlobalAsset, InspectorData, LogEntry } from './types';

beforeEach(() => {
    useAssetRuntimeStore.setState({
        connected: false,
        assets: [],
        total: 0,
        stateFilter: '',
        idSearch: '',
        treeNodes: [],
        globalAssets: [],
        selectedAssetId: '',
        stageChildren: {},
        inspectorData: null,
        logEntries: [],
        logTotal: 0,
        logPaused: false,
        logMaxEntries: 0,
        activeTab: 'table',
    });
});

describe('AssetRuntimeStore', () => {
    it('initializes with default values', () => {
        const state = useAssetRuntimeStore.getState();
        expect(state.connected).toBe(false);
        expect(state.assets).toEqual([]);
        expect(state.logEntries).toEqual([]);
        expect(state.activeTab).toBe('table');
        expect(state.total).toBe(0);
        expect(state.stateFilter).toBe('');
        expect(state.idSearch).toBe('');
        expect(state.treeNodes).toEqual([]);
        expect(state.globalAssets).toEqual([]);
        expect(state.selectedAssetId).toBe('');
        expect(state.stageChildren).toEqual({});
        expect(state.inspectorData).toBeNull();
        expect(state.logTotal).toBe(0);
        expect(state.logPaused).toBe(false);
        expect(state.logMaxEntries).toBe(0);
    });

    it('setConnected(true) sets connected to true', () => {
        useAssetRuntimeStore.getState().setConnected(true);
        expect(useAssetRuntimeStore.getState().connected).toBe(true);
    });

    it('setConnected(false) sets connected to false', () => {
        useAssetRuntimeStore.getState().setConnected(true);
        useAssetRuntimeStore.getState().setConnected(false);
        expect(useAssetRuntimeStore.getState().connected).toBe(false);
    });

    it('setSnapshot stores assets array and total correctly', () => {
        const assets: AssetRow[] = [
            { id: 'crc-001', state: 'Loaded', scope: 'Global', refCount: 3, deployPath: 'textures/hero.png' },
            { id: 'crc-002', state: 'Loading', scope: 'Stage', refCount: 1, deployPath: 'audio/sfx.wav' },
        ];
        useAssetRuntimeStore.getState().setSnapshot(assets, 42);
        const state = useAssetRuntimeStore.getState();
        expect(state.assets).toEqual(assets);
        expect(state.total).toBe(42);
    });

    it('setTableFilters stores stateFilter and idSearch', () => {
        useAssetRuntimeStore.getState().setTableFilters('Loaded', 'hero');
        const state = useAssetRuntimeStore.getState();
        expect(state.stateFilter).toBe('Loaded');
        expect(state.idSearch).toBe('hero');
    });

    it('setTreeData stores treeNodes, globalAssets and selectedAssetId', () => {
        const nodes: StageNode[] = [
            { stageId: 'stage-01', assetCount: 5, expanded: true, childrenLoaded: true },
        ];
        const globals: GlobalAsset[] = [
            { id: 'crc-global', state: 'Loaded', refCount: 2 },
        ];
        useAssetRuntimeStore.getState().setTreeData(nodes, globals, 'crc-global');
        const state = useAssetRuntimeStore.getState();
        expect(state.treeNodes).toEqual(nodes);
        expect(state.globalAssets).toEqual(globals);
        expect(state.selectedAssetId).toBe('crc-global');
    });

    it('setStageChildren merges children into the correct stageId key', () => {
        useAssetRuntimeStore.getState().setStageChildren('stage-01', ['crc-a', 'crc-b']);
        useAssetRuntimeStore.getState().setStageChildren('stage-02', ['crc-c']);
        const state = useAssetRuntimeStore.getState();
        expect(state.stageChildren['stage-01']).toEqual(['crc-a', 'crc-b']);
        expect(state.stageChildren['stage-02']).toEqual(['crc-c']);
    });

    it('setStageChildren overwrites existing entry for same stageId', () => {
        useAssetRuntimeStore.getState().setStageChildren('stage-01', ['crc-a']);
        useAssetRuntimeStore.getState().setStageChildren('stage-01', ['crc-b', 'crc-c']);
        expect(useAssetRuntimeStore.getState().stageChildren['stage-01']).toEqual(['crc-b', 'crc-c']);
    });

    it('setInspectorData stores inspector data', () => {
        const data: InspectorData = {
            hasSelection: true,
            missing: false,
            assetId: 'crc-001',
            state: 'Loaded',
            scope: 'Global',
            refCount: 3,
            stageScoped: false,
            stageRefs: [{ stageId: 'stage-01' }],
        };
        useAssetRuntimeStore.getState().setInspectorData(data);
        expect(useAssetRuntimeStore.getState().inspectorData).toEqual(data);
    });

    it('setInspectorData(null) clears inspector data', () => {
        const data: InspectorData = { hasSelection: false, message: 'Select an asset' };
        useAssetRuntimeStore.getState().setInspectorData(data);
        useAssetRuntimeStore.getState().setInspectorData(null);
        expect(useAssetRuntimeStore.getState().inspectorData).toBeNull();
    });

    it('setLogData stores full log with all fields', () => {
        const entries: LogEntry[] = [
            { timestamp: 1000, type: 'transition', assetId: 'crc-001', oldState: 'Loading', newState: 'Loaded' },
            { timestamp: 2000, type: 'disconnect' },
        ];
        useAssetRuntimeStore.getState().setLogData(entries, 100, true, 500);
        const state = useAssetRuntimeStore.getState();
        expect(state.logEntries).toEqual(entries);
        expect(state.logTotal).toBe(100);
        expect(state.logPaused).toBe(true);
        expect(state.logMaxEntries).toBe(500);
    });

    it('appendLogEntry prepends entry (newest-first) and updates total/paused', () => {
        const first: LogEntry = { timestamp: 1000, type: 'disconnect' };
        const second: LogEntry = { timestamp: 2000, type: 'reconnect' };
        useAssetRuntimeStore.getState().appendLogEntry(first, 1, false);
        useAssetRuntimeStore.getState().appendLogEntry(second, 2, true);
        const state = useAssetRuntimeStore.getState();
        expect(state.logEntries[0]).toEqual(second);
        expect(state.logEntries[1]).toEqual(first);
        expect(state.logTotal).toBe(2);
        expect(state.logPaused).toBe(true);
    });

    it('setActiveTab updates the active tab', () => {
        useAssetRuntimeStore.getState().setActiveTab('log');
        expect(useAssetRuntimeStore.getState().activeTab).toBe('log');
        useAssetRuntimeStore.getState().setActiveTab('inspector');
        expect(useAssetRuntimeStore.getState().activeTab).toBe('inspector');
    });
});
