import type { CSSProperties } from 'react';
import { EmptyState, theme } from '@dia/editor-ui';
import { useAssetRuntimeStore } from '../store';

const sectionStyle: CSSProperties = {
    padding: '12px 16px',
    display: 'flex',
    flexDirection: 'column',
    gap: 8,
    height: '100%',
    overflow: 'auto',
};

const rowStyle: CSSProperties = {
    display: 'flex',
    flexDirection: 'column',
    gap: 4,
};

const labelStyle: CSSProperties = {
    fontSize: 10,
    color: theme.textMuted,
    textTransform: 'uppercase',
    letterSpacing: '0.04em',
};

const valueStyle: CSSProperties = {
    fontSize: 12,
    color: theme.text,
    fontFamily: 'monospace',
};

const messageStyle: CSSProperties = {
    fontSize: 12,
    color: theme.textMuted,
    fontStyle: 'italic',
};

const headingStyle: CSSProperties = {
    fontSize: 11,
    color: theme.textMuted,
    textTransform: 'uppercase',
    letterSpacing: '0.04em',
    marginTop: 8,
    borderBottom: `1px solid ${theme.border}`,
    paddingBottom: 4,
};

const stageRefItemStyle: CSSProperties = {
    fontSize: 12,
    color: theme.text,
    fontFamily: 'monospace',
    padding: '2px 0',
};

export function RefCountInspector(): JSX.Element {
    const inspectorData = useAssetRuntimeStore((s) => s.inspectorData);

    // No selection: inspectorData null or hasSelection false
    if (inspectorData === null || !inspectorData.hasSelection) {
        const msg = inspectorData && !inspectorData.hasSelection
            ? inspectorData.message
            : 'Select an asset to inspect ref counts.';
        return <EmptyState message={msg} />;
    }

    // Missing from snapshot
    if (inspectorData.missing) {
        return (
            <div style={sectionStyle} data-testid="ref-count-inspector">
                <div style={rowStyle}>
                    <span style={labelStyle}>Asset ID</span>
                    <span style={valueStyle} data-testid="asset-id">{inspectorData.assetId}</span>
                </div>
                <div style={messageStyle} data-testid="missing-message">
                    Asset no longer present in runtime.
                </div>
            </div>
        );
    }

    // Stage-scoped asset
    if (inspectorData.stageScoped) {
        return (
            <div style={sectionStyle} data-testid="ref-count-inspector">
                <div style={rowStyle}>
                    <span style={labelStyle}>Asset ID</span>
                    <span style={valueStyle} data-testid="asset-id">{inspectorData.assetId}</span>
                </div>
                <div style={rowStyle}>
                    <span style={labelStyle}>State</span>
                    <span style={valueStyle} data-testid="asset-state">{inspectorData.state}</span>
                </div>
                <div style={rowStyle}>
                    <span style={labelStyle}>Scope</span>
                    <span style={valueStyle} data-testid="asset-scope">{inspectorData.scope}</span>
                </div>
                <div style={rowStyle}>
                    <span style={labelStyle}>Ref Count</span>
                    <span style={valueStyle} data-testid="ref-count">{inspectorData.refCount}</span>
                </div>
                <div style={messageStyle} data-testid="stage-scoped-message">
                    Stage-scoped asset — single reference from owning stage.
                </div>
            </div>
        );
    }

    // Global asset
    const { stageRefs } = inspectorData;
    return (
        <div style={sectionStyle} data-testid="ref-count-inspector">
            <div style={rowStyle}>
                <span style={labelStyle}>Asset ID</span>
                <span style={valueStyle} data-testid="asset-id">{inspectorData.assetId}</span>
            </div>
            <div style={rowStyle}>
                <span style={labelStyle}>State</span>
                <span style={valueStyle} data-testid="asset-state">{inspectorData.state}</span>
            </div>
            <div style={rowStyle}>
                <span style={labelStyle}>Scope</span>
                <span style={valueStyle} data-testid="asset-scope">{inspectorData.scope}</span>
            </div>
            <div style={rowStyle}>
                <span style={labelStyle}>Ref Count</span>
                <span style={valueStyle} data-testid="ref-count">{inspectorData.refCount}</span>
            </div>
            <div>
                <div style={headingStyle}>Stage References</div>
                {stageRefs.length === 0 ? (
                    <div style={messageStyle} data-testid="no-stage-refs">
                        No stage references found.
                    </div>
                ) : (
                    <ul style={{ margin: 0, padding: '4px 0 0 16px', listStyle: 'disc' }} data-testid="stage-refs-list">
                        {stageRefs.map((ref) => (
                            <li key={ref.stageId} style={stageRefItemStyle} data-testid="stage-ref-item">
                                {ref.stageId}
                            </li>
                        ))}
                    </ul>
                )}
            </div>
        </div>
    );
}
