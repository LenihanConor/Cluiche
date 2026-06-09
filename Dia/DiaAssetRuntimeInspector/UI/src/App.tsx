import { useEffect, CSSProperties } from 'react';
import { theme, ConnectionStatus, TabBar } from '@dia/editor-ui';
import type { Tab } from '@dia/editor-ui';
import { useAssetRuntimeStore } from './store';
import type { ActiveTab } from './store';
import { AssetStateTable } from './components/AssetStateTable';
import { StageAssetTree } from './components/StageAssetTree';
import { RefCountInspector } from './components/RefCountInspector';
import { StateTransitionLog } from './components/StateTransitionLog';

const TABS: Tab[] = [
    { id: 'table',     label: 'Asset State Table' },
    { id: 'tree',      label: 'Stage Tree' },
    { id: 'inspector', label: 'Ref Count' },
    { id: 'log',       label: 'Transition Log' },
];

export default function App() {
    const connected   = useAssetRuntimeStore((s) => s.connected);
    const activeTab   = useAssetRuntimeStore((s) => s.activeTab);

    const setConnected    = useAssetRuntimeStore((s) => s.setConnected);
    const setSnapshot     = useAssetRuntimeStore((s) => s.setSnapshot);
    const setTableFilters = useAssetRuntimeStore((s) => s.setTableFilters);
    const setTreeData     = useAssetRuntimeStore((s) => s.setTreeData);
    const setStageChildren = useAssetRuntimeStore((s) => s.setStageChildren);
    const setInspectorData = useAssetRuntimeStore((s) => s.setInspectorData);
    const setLogData       = useAssetRuntimeStore((s) => s.setLogData);
    const appendLogEntry   = useAssetRuntimeStore((s) => s.appendLogEntry);
    const setActiveTab     = useAssetRuntimeStore((s) => s.setActiveTab);

    // Bridge wiring — EIRM-003 pattern
    useEffect(() => {
        const dispatch = (topic: string, data: unknown) => {
            const d = data as any;
            switch (topic) {
                case 'asset_runtime_inspector.connection_state':
                    if (d?.connected !== undefined) setConnected(d.connected);
                    break;
                case 'asset_runtime_inspector.snapshot':
                    if (d?.assets !== undefined) setSnapshot(d.assets, d.total);
                    break;
                case 'asset_runtime_inspector.table_filters':
                    setTableFilters(d?.stateFilter ?? '', d?.idSearch ?? '');
                    break;
                case 'asset_runtime_inspector.tree_data':
                    setTreeData(d?.stages ?? [], d?.globalAssets ?? [], d?.selectedAssetId ?? '');
                    break;
                case 'asset_runtime_inspector.stage_children':
                    if (d?.stageId !== undefined) setStageChildren(d.stageId, d.assets ?? []);
                    break;
                case 'asset_runtime_inspector.inspector_data':
                    setInspectorData(d ?? null);
                    break;
                case 'asset_runtime_inspector.log_data':
                    setLogData(d?.entries ?? [], d?.total ?? 0, d?.paused ?? false, d?.maxEntries ?? 0);
                    break;
                case 'asset_runtime_inspector.log_entry':
                    if (d?.entry !== undefined) appendLogEntry(d.entry, d.total ?? 0, d.paused ?? false);
                    break;
                // Per-panel connection topics — acknowledged, no store update needed
                case 'table_connection_state':
                case 'tree_connection_state':
                case 'inspector_connection_state':
                case 'log_connection_state':
                    break;
                default:
                    break;
            }
        };

        (window as any).DiaEditor_onDataChanged = (msg: { topic: string; data: unknown }) => {
            dispatch(msg.topic, msg.data);
        };

        const onMessage = (e: MessageEvent) => {
            const env = e.data;
            if (env && env.__dia === true && typeof env.topic === 'string') {
                dispatch(env.topic, env.data);
            }
        };
        window.addEventListener('message', onMessage);
        return () => window.removeEventListener('message', onMessage);
    }, [
        setConnected, setSnapshot, setTableFilters, setTreeData,
        setStageChildren, setInspectorData, setLogData, appendLogEntry,
    ]);

    // Bootstrap: request current connection state on mount
    useEffect(() => {
        if (window.parent && window.parent !== window) {
            window.parent.postMessage({
                __diaFromFrame: true,
                payload: { type: 'asset_runtime_inspector.get_connection_state', reqId: null, data: {} },
            }, '*');
        }
    }, []);

    const rootStyle: CSSProperties = {
        fontFamily: "'Segoe UI', system-ui, sans-serif",
        background: theme.bg,
        color: theme.text,
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        position: 'relative',
        overflow: 'hidden',
        fontSize: 12,
    };

    const overlayStyle: CSSProperties = {
        position: 'absolute',
        top: 0, left: 0, right: 0, bottom: 0,
        background: 'rgba(0,0,0,0.6)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 100,
        flexDirection: 'column',
        gap: 8,
    };

    return (
        <div style={rootStyle}>
            {/* Disconnect overlay */}
            {!connected && (
                <div style={overlayStyle} data-testid="disconnect-overlay">
                    <div style={{ fontSize: 14, color: '#f44747' }}>No game connected</div>
                    <div style={{ fontSize: 11, color: theme.textMuted }}>Use the Game Connection panel to connect to a running game</div>
                </div>
            )}

            {/* Title bar */}
            <div style={{ background: theme.bgPanel, borderBottom: `1px solid ${theme.border}`, padding: '5px 10px', display: 'flex', alignItems: 'center', gap: 8, flexShrink: 0, userSelect: 'none' }}>
                <ConnectionStatus state={connected ? 'connected' : 'disconnected'} compact />
                <span style={{ fontWeight: 600, color: theme.text }}>Asset Runtime Inspector</span>
            </div>

            {/* Tab bar */}
            <TabBar
                tabs={TABS}
                activeTab={activeTab}
                onTabChange={(id) => setActiveTab(id as ActiveTab)}
            />

            {/* Tab content (placeholders — replaced in Tasks 5–8) */}
            <div style={{ flex: 1, overflow: 'hidden' }}>
                {activeTab === 'table' && (
                    <div data-testid="tab-table" style={{ height: '100%' }}>
                        <AssetStateTable />
                    </div>
                )}
                {activeTab === 'tree' && (
                    <div data-testid="tab-tree" style={{ height: '100%' }}>
                        <StageAssetTree />
                    </div>
                )}
                {activeTab === 'inspector' && (
                    <div data-testid="tab-inspector" style={{ height: '100%' }}>
                        <RefCountInspector />
                    </div>
                )}
                {activeTab === 'log' && (
                    <div data-testid="tab-log" style={{ height: '100%' }}>
                        <StateTransitionLog />
                    </div>
                )}
            </div>
        </div>
    );
}
