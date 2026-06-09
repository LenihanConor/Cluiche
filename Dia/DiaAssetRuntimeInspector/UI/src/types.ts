export type AssetState = 'Null' | 'Staged' | 'Loading' | 'Loaded' | 'Failed' | 'Unloaded';
export type AssetScope = 'Global' | 'Stage';

export interface AssetRow {
    id: string;       // StringCRC as string
    state: AssetState;
    scope: AssetScope;
    refCount: number;
    deployPath: string;
}

export interface StageNode {
    stageId: string;
    assetCount: number;
    expanded: boolean;
    childrenLoaded: boolean;
}

export interface GlobalAsset {
    id: string;
    state: AssetState;
    refCount: number;
}

// Inspector data — discriminated union based on selection state
export type InspectorData =
    | { hasSelection: false; message: string }
    | { hasSelection: true; missing: true; message: string; assetId: string }
    | { hasSelection: true; missing: false; assetId: string; state: AssetState; scope: 'Stage'; refCount: number; stageScoped: true; message: string }
    | { hasSelection: true; missing: false; assetId: string; state: AssetState; scope: 'Global'; refCount: number; stageScoped: false; stageRefs: Array<{ stageId: string }> };

export interface LogEntry {
    timestamp: number;
    type: 'transition' | 'disconnect' | 'reconnect';
    assetId?: string;
    oldState?: AssetState;
    newState?: AssetState;
}

// Snapshot payload: asset_runtime_inspector.snapshot
export interface SnapshotPayload {
    assets: AssetRow[];
    total: number;
}

// Connection state payload: asset_runtime_inspector.connection_state (and panel variants)
export interface ConnectionStatePayload {
    connected: boolean;
}

// Table filters payload: asset_runtime_inspector.table_filters
export interface TableFiltersPayload {
    stateFilter: string;
    idSearch: string;
}

// Tree data payload: asset_runtime_inspector.tree_data
export interface TreeDataPayload {
    stages: StageNode[];
    globalAssets: GlobalAsset[];
    selectedAssetId: string;
}

// Stage children payload: asset_runtime_inspector.stage_children
export interface StageChildrenPayload {
    stageId: string;
    assets: unknown[];
}

// Log data payload: asset_runtime_inspector.log_data
export interface LogDataPayload {
    entries: LogEntry[];
    total: number;
    paused: boolean;
    maxEntries: number;
}

// Log entry payload: asset_runtime_inspector.log_entry
export interface LogEntryPayload {
    entry: LogEntry;
    total: number;
    paused: boolean;
}
