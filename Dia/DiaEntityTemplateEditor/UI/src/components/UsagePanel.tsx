import React, { CSSProperties } from 'react';
import { theme, EmptyState } from '@dia/editor-ui';
import type { UsageEntry } from '../types';

interface UsagePanelProps {
    usage: UsageEntry[];
    selectedId: string | null;
}

const containerStyle: CSSProperties = {
    display: 'flex',
    flexDirection: 'column',
    height: '100%',
    background: theme.bgPanel,
    color: theme.text,
    overflow: 'hidden',
};

const headerStyle: CSSProperties = {
    padding: '8px 12px',
    fontSize: '11px',
    fontWeight: 600,
    letterSpacing: '0.08em',
    textTransform: 'uppercase',
    color: theme.textMuted,
    borderBottom: `1px solid ${theme.border}`,
    flexShrink: 0,
    userSelect: 'none',
};

const listStyle: CSSProperties = {
    flex: 1,
    overflowY: 'auto',
    padding: '4px 0',
};

const itemStyle: CSSProperties = {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: '6px 12px',
    fontSize: '13px',
    color: theme.text,
    borderBottom: `1px solid ${theme.border}`,
};

const sceneIdStyle: CSSProperties = {
    overflow: 'hidden',
    textOverflow: 'ellipsis',
    whiteSpace: 'nowrap',
    flex: 1,
};

const instanceCountStyle: CSSProperties = {
    flexShrink: 0,
    marginLeft: '8px',
    color: theme.textMuted,
    fontSize: '12px',
};

export function UsagePanel({ usage, selectedId }: UsagePanelProps) {
    if (selectedId === null) {
        return <EmptyState message="No blueprint selected" />;
    }

    if (usage.length === 0) {
        return (
            <EmptyState
                message="No scene references found"
                hint="This blueprint is not placed in any scenes"
            />
        );
    }

    return (
        <div style={containerStyle}>
            <div style={headerStyle}>Usage</div>
            <div style={listStyle}>
                {usage.map((entry: UsageEntry, index: number) => (
                    <div
                        key={entry.sceneId}
                        style={itemStyle}
                        data-testid={`usage-item-${index}`}
                    >
                        <span style={sceneIdStyle}>{entry.sceneId}</span>
                        <span style={instanceCountStyle}>
                            {entry.instanceCount} instance(s)
                        </span>
                    </div>
                ))}
            </div>
        </div>
    );
}

export default UsagePanel;
