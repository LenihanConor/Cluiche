import type { CSSProperties } from 'react';
import { useAssetRuntimeStore } from '../store';
import type { AssetState } from '../types';

// ─── State dot colours ───────────────────────────────────────────────────────

const STATE_COLORS: Record<AssetState, string> = {
    Loaded:   '#4ec9a0',
    Loading:  '#dcdcaa',
    Staged:   '#808080',
    Failed:   '#f44747',
    Unloaded: '#ce9178',
    Null:     '#555555',
};

// ─── Bridge helper ───────────────────────────────────────────────────────────

function sendBridgeRequest(type: string, data: unknown) {
    window.parent.postMessage(
        { __diaFromFrame: true, payload: { type, reqId: null, data } },
        '*',
    );
}

// ─── Sub-components ──────────────────────────────────────────────────────────

interface StateDotProps {
    state: AssetState;
}

function StateDot({ state }: StateDotProps) {
    const style: CSSProperties = {
        display: 'inline-block',
        width: 8,
        height: 8,
        borderRadius: '50%',
        background: STATE_COLORS[state],
        flexShrink: 0,
    };
    return <span data-testid="state-dot" style={style} />;
}

// ─── StageAssetTree ──────────────────────────────────────────────────────────

export function StageAssetTree(): JSX.Element {
    const treeNodes      = useAssetRuntimeStore((s) => s.treeNodes);
    const globalAssets   = useAssetRuntimeStore((s) => s.globalAssets);
    const selectedAssetId = useAssetRuntimeStore((s) => s.selectedAssetId);
    const stageChildren  = useAssetRuntimeStore((s) => s.stageChildren);

    const containerStyle: CSSProperties = {
        height: '100%',
        overflowY: 'auto',
        padding: '4px 0',
        fontSize: 12,
    };

    const stageHeaderStyle: CSSProperties = {
        display: 'flex',
        alignItems: 'center',
        gap: 4,
        padding: '2px 8px',
        cursor: 'pointer',
        userSelect: 'none',
        fontWeight: 600,
        color: '#d4d4d4',
    };

    const assetRowBase: CSSProperties = {
        display: 'flex',
        alignItems: 'center',
        gap: 6,
        padding: '1px 8px 1px 24px',
        cursor: 'pointer',
        userSelect: 'none',
    };

    const selectedStyle: CSSProperties = {
        background: 'rgba(14, 99, 156, 0.4)',
    };

    const badgeStyle: CSSProperties = {
        fontSize: 10,
        color: '#888',
        marginLeft: 4,
    };

    return (
        <div data-testid="stage-asset-tree" style={containerStyle}>
            {treeNodes.map((node) => {
                const isGlobal = node.stageId === '[Global]';

                const handleToggle = () => {
                    if (node.expanded) {
                        sendBridgeRequest('asset_runtime_inspector.collapse_stage', { stageId: node.stageId });
                    } else {
                        sendBridgeRequest('asset_runtime_inspector.expand_stage', { stageId: node.stageId });
                    }
                };

                return (
                    <div key={node.stageId} data-testid={`stage-node-${node.stageId}`}>
                        {/* Stage header row */}
                        <div
                            style={stageHeaderStyle}
                            onClick={handleToggle}
                            data-testid={`stage-header-${node.stageId}`}
                        >
                            <span data-testid={`toggle-${node.stageId}`}>
                                {node.expanded ? '▼' : '▶'}
                            </span>
                            <span>{node.stageId}</span>
                            <span style={{ color: '#888', fontWeight: 400 }}>
                                ({node.assetCount})
                            </span>
                        </div>

                        {/* Children rows */}
                        {node.expanded && (
                            <div data-testid={`stage-children-${node.stageId}`}>
                                {isGlobal
                                    ? globalAssets.map((asset) => (
                                        <div
                                            key={asset.id}
                                            data-testid={`asset-row-${asset.id}`}
                                            style={{
                                                ...assetRowBase,
                                                ...(selectedAssetId === asset.id ? selectedStyle : {}),
                                            }}
                                            onClick={() =>
                                                sendBridgeRequest(
                                                    'asset_runtime_inspector.tree_select_asset',
                                                    { assetId: asset.id },
                                                )
                                            }
                                        >
                                            <StateDot state={asset.state} />
                                            <span>{asset.id}</span>
                                            <span
                                                data-testid={`ref-badge-${asset.id}`}
                                                style={badgeStyle}
                                            >
                                                ref:{asset.refCount}
                                            </span>
                                        </div>
                                    ))
                                    : (stageChildren[node.stageId] ?? []).map((assetId) => (
                                        <div
                                            key={assetId}
                                            data-testid={`asset-row-${assetId}`}
                                            style={{
                                                ...assetRowBase,
                                                ...(selectedAssetId === assetId ? selectedStyle : {}),
                                            }}
                                            onClick={() =>
                                                sendBridgeRequest(
                                                    'asset_runtime_inspector.tree_select_asset',
                                                    { assetId },
                                                )
                                            }
                                        >
                                            <span>{assetId}</span>
                                        </div>
                                    ))}
                            </div>
                        )}
                    </div>
                );
            })}
        </div>
    );
}
