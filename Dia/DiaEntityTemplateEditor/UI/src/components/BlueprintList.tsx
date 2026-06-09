import React, { CSSProperties, useState } from 'react';
import { theme, inputStyle, EmptyState } from '@dia/editor-ui';
import type { BlueprintGroup } from '../types';

interface BlueprintListProps {
    groups: BlueprintGroup[];
    selectedId: string | null;
    onSelect: (id: string, path: string) => void;
    onRefresh: () => void;
}

const toolbarStyle: CSSProperties = {
    display: 'flex',
    alignItems: 'center',
    gap: '6px',
    padding: '6px 8px',
    borderBottom: `1px solid ${theme.border}`,
    background: theme.bgPanel,
    flexShrink: 0,
};

const bodyStyle: CSSProperties = {
    flex: 1,
    overflowY: 'auto',
    background: theme.bg,
};

const containerStyle: CSSProperties = {
    display: 'flex',
    flexDirection: 'column',
    height: '100%',
    background: theme.bg,
    color: theme.text,
};

const groupHeaderStyle: CSSProperties = {
    padding: '4px 8px 2px',
    fontSize: '11px',
    fontWeight: 600,
    textTransform: 'uppercase',
    color: theme.textMuted,
    letterSpacing: '0.06em',
    userSelect: 'none',
};

function itemStyle(selected: boolean): CSSProperties {
    return {
        padding: '5px 12px',
        cursor: 'pointer',
        fontSize: '13px',
        background: selected ? theme.accent : 'transparent',
        color: selected ? '#fff' : theme.text,
        userSelect: 'none',
        whiteSpace: 'nowrap',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
    };
}

const refreshButtonStyle: CSSProperties = {
    flexShrink: 0,
    padding: '3px 8px',
    background: 'transparent',
    border: `1px solid ${theme.border}`,
    color: theme.text,
    borderRadius: '3px',
    cursor: 'pointer',
    fontSize: '12px',
    lineHeight: 1.4,
};

export function BlueprintList({ groups, selectedId, onSelect, onRefresh }: BlueprintListProps) {
    const [query, setQuery] = useState('');
    const trimmed = query.trim().toLowerCase();

    const filteredGroups = groups
        .map(group => ({
            ...group,
            items: trimmed
                ? group.items.filter(item => item.id.toLowerCase().includes(trimmed))
                : group.items,
        }))
        .filter(group => group.items.length > 0);

    const hasContent = filteredGroups.length > 0;

    const emptyMessage = trimmed
        ? `No matches for "${query.trim()}"`
        : 'No blueprints registered.';

    return (
        <div style={containerStyle}>
            <div style={toolbarStyle}>
                <input
                    style={inputStyle({ flex: 1, minWidth: 0 })}
                    type="text"
                    placeholder="Search..."
                    value={query}
                    onChange={e => setQuery(e.target.value)}
                    aria-label="Search blueprints"
                />
                <button style={refreshButtonStyle} onClick={onRefresh} aria-label="Refresh">
                    Refresh
                </button>
            </div>
            <div style={bodyStyle}>
                {!hasContent ? (
                    <EmptyState message={emptyMessage} />
                ) : (
                    filteredGroups.map(group => (
                        <div key={group.label}>
                            <div style={groupHeaderStyle}>{group.label}</div>
                            {group.items.map(item => (
                                <div
                                    key={item.id}
                                    data-testid="blueprint-item"
                                    data-item-id={item.id}
                                    style={itemStyle(item.id === selectedId)}
                                    title={item.path}
                                    onClick={() => onSelect(item.id, item.path)}
                                >
                                    {item.label}
                                </div>
                            ))}
                        </div>
                    ))
                )}
            </div>
        </div>
    );
}

export default BlueprintList;
