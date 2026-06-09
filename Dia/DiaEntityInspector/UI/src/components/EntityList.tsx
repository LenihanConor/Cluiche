import { CSSProperties } from 'react';
import { theme, inputStyle } from '@dia/editor-ui';
import type { EntityEntry } from '../types';

interface EntityListProps {
    entities: EntityEntry[];
    selectedIdx: number | null;
    onSelect: (idx: number) => void;
    searchText: string;
    onSearchChange: (v: string) => void;
    activeFilter: string;
    onFilterChange: (v: string) => void;
}

// Component tag colours — domain-semantic, keep hardcoded
const TAG_STYLES: Record<string, CSSProperties> = {
    T: { borderColor: '#3a6a8a', color: '#6aa8c8', background: '#151e28' },
    P: { borderColor: '#3a8a4a', color: '#6ac47a', background: '#15201a' },
    R: { borderColor: '#8a5a3a', color: '#c4906a', background: '#201515' },
    A: { borderColor: '#8a8a3a', color: '#c4c46a', background: '#201e15' },
    S: { borderColor: '#6a3a8a', color: '#a46ac4', background: '#1a1525' },
    H: { borderColor: '#8a3a3a', color: '#c46a6a', background: '#201515' },
    C: { borderColor: '#3a8a8a', color: '#6ac4c4', background: '#151e1e' },
};

const FILTERS = ['All', 'T', 'P', 'R', 'A', 'S', 'H', 'C'];

export function EntityList({
    entities,
    selectedIdx,
    onSelect,
    searchText,
    onSearchChange,
    activeFilter,
    onFilterChange,
}: EntityListProps) {
    const search = searchText.toLowerCase();
    const filterKey = activeFilter === 'All' ? 'ALL' : activeFilter;

    const visible = entities.filter((e) => {
        if (search && !e.n.toLowerCase().includes(search)) return false;
        if (filterKey !== 'ALL' && !e.t.includes(filterKey)) return false;
        return true;
    });

    const tagStyle = (tag: string): CSSProperties => {
        const base = TAG_STYLES[tag];
        return {
            fontFamily: 'monospace',
            fontSize: 8.5,
            border: `1px solid ${base?.borderColor ?? theme.border}`,
            borderRadius: 2,
            padding: '1px 2px',
            color: base?.color ?? theme.textMuted,
            background: base?.background ?? theme.bgPanel,
        };
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', overflow: 'hidden', height: '100%' }} data-testid="entity-list">
            {/* Header */}
            <div style={{ background: theme.bgPanel, borderBottom: `1px solid ${theme.border}`, padding: '5px 8px', display: 'flex', alignItems: 'center', gap: 5, flexShrink: 0 }}>
                <span style={{ fontSize: 11, color: theme.textMuted, textTransform: 'uppercase', letterSpacing: 0.5 }}>Entities</span>
                <span style={{ fontSize: 10, color: theme.textMuted, marginLeft: 'auto' }}>{visible.length}</span>
            </div>

            {/* Search + filters */}
            <div style={{ background: theme.bg, borderBottom: `1px solid ${theme.borderMuted}`, padding: '5px 6px', display: 'flex', flexDirection: 'column', gap: 4, flexShrink: 0 }}>
                <input
                    style={inputStyle({ width: '100%', fontSize: 11 })}
                    type="text"
                    placeholder="filter..."
                    value={searchText}
                    onChange={(e) => onSearchChange(e.target.value)}
                    data-testid="entity-search"
                />
                <div style={{ display: 'flex', gap: 3, flexWrap: 'wrap' }}>
                    {FILTERS.map((f) => {
                        const key = f === 'All' ? 'ALL' : f;
                        const isActive = filterKey === key;
                        const chipTag = TAG_STYLES[f];
                        return (
                            <button
                                key={f}
                                onClick={() => onFilterChange(f === 'All' ? 'All' : f)}
                                data-testid={`filter-chip-${f}`}
                                style={{
                                    background: isActive ? (chipTag ? chipTag.background : theme.accent) : theme.bgPanel,
                                    border: `1px solid ${isActive ? (chipTag ? chipTag.borderColor : theme.accent) : theme.border}`,
                                    borderRadius: 10,
                                    padding: '1px 7px',
                                    fontSize: 9.5,
                                    color: isActive ? (chipTag ? chipTag.color : '#fff') : theme.textMuted,
                                    cursor: 'pointer',
                                    whiteSpace: 'nowrap',
                                }}
                            >
                                {f}
                            </button>
                        );
                    })}
                </div>
            </div>

            {/* Entity rows */}
            <div style={{ overflowY: 'auto', flex: 1 }}>
                {visible.map((e) => {
                    const isSelected = selectedIdx === e.i;
                    return (
                        <div
                            key={e.i}
                            data-testid={`entity-row-${e.i}`}
                            data-selected={isSelected ? 'true' : 'false'}
                            onClick={() => onSelect(e.i)}
                            style={{
                                display: 'flex',
                                alignItems: 'center',
                                paddingRight: 6,
                                height: 29,
                                cursor: 'pointer',
                                borderBottom: `1px solid ${theme.borderMuted}`,
                                gap: 4,
                                borderLeft: isSelected ? `2px solid ${theme.accent}` : '2px solid transparent',
                                background: isSelected ? '#094771' : 'transparent',
                                paddingLeft: e.d * 12,
                            }}
                        >
                            <span style={{ fontFamily: 'monospace', fontSize: 9.5, color: theme.textMuted, width: 26, textAlign: 'right', flexShrink: 0 }}>
                                {String(e.i).padStart(3, '0')}
                            </span>
                            <span style={{ flex: 1, fontSize: 11.5, color: isSelected ? '#fff' : theme.text, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                                {e.n}
                            </span>
                            <div style={{ display: 'flex', gap: 2, flexShrink: 0 }}>
                                {e.t.map((tag) => (
                                    <span key={tag} style={tagStyle(tag)}>{tag}</span>
                                ))}
                            </div>
                        </div>
                    );
                })}
            </div>
        </div>
    );
}
