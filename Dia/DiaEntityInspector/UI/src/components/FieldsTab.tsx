import { CSSProperties, useState, useCallback } from 'react';
import { theme } from '@dia/editor-ui';
import type { EntityEntry } from '../types';

interface FieldsTabProps {
    entity: EntityEntry | null;
    allEntities: EntityEntry[];
    onNavigate: (idx: number) => void;
    onWriteField: (entityIdx: number, comp: string, field: string, value: string) => void;
}

interface EditState {
    compName: string;
    fieldName: string;
    value: string;
}

function valueColor(typeClass: string, value: string): string {
    if (typeClass === 'bool') return value === 'true' ? '#70c470' : '#c47070';
    if (typeClass === 'num' || typeClass === 'float') return '#90c890';
    if (typeClass === 'vec') return '#70a8e8';
    if (typeClass === 'str') return '#e8b860';
    return '#ccc';
}

export function FieldsTab({ entity, allEntities, onNavigate, onWriteField }: FieldsTabProps) {
    // Track which components are expanded: key = `${entityIdx}:${compName}`
    const [expanded, setExpanded] = useState<Record<string, boolean>>({});
    const [editing, setEditing] = useState<EditState | null>(null);
    const [editValue, setEditValue] = useState('');

    const toggleComp = useCallback((entityIdx: number, compName: string, compIdx: number) => {
        const key = `${entityIdx}:${compName}`;
        setExpanded((prev) => {
            const current = key in prev ? prev[key] : compIdx < 2;
            return { ...prev, [key]: !current };
        });
    }, []);

    const isExpanded = (entityIdx: number, compName: string, compIdx: number): boolean => {
        const key = `${entityIdx}:${compName}`;
        if (key in expanded) return expanded[key];
        return compIdx < 2; // first 2 open by default
    };

    const startEdit = (compName: string, fieldName: string, currentValue: string) => {
        setEditing({ compName, fieldName, value: currentValue });
        setEditValue(currentValue);
    };

    const commitEdit = () => {
        if (!editing || !entity) return;
        onWriteField(entity.i, editing.compName, editing.fieldName, editValue);
        setEditing(null);
    };

    const cancelEdit = () => {
        setEditing(null);
        setEditValue('');
    };

    if (!entity) {
        return (
            <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', flexDirection: 'column', gap: 6, color: theme.borderMuted }}>
                <span style={{ fontSize: 28 }}>←</span>
                <span style={{ fontSize: 11 }}>Select an entity</span>
            </div>
        );
    }

    const hasParent = entity.pi !== undefined && entity.pi >= 0;
    const children = allEntities.filter((e) => e.pi === entity.i);
    const showHierNav = hasParent || children.length > 0;

    const parentEntity = hasParent ? allEntities.find((e) => e.i === entity.pi) : null;

    const hierNavStyle: CSSProperties = {
        margin: '0 6px 6px',
        padding: '6px 10px',
        background: theme.bgPanel,
        border: `1px solid ${theme.border}`,
        borderRadius: 4,
        fontSize: 11,
    };

    const linkStyle: CSSProperties = {
        color: '#1177bb',
        cursor: 'pointer',
        textDecoration: 'underline',
        fontFamily: 'monospace',
        fontSize: 11,
    };

    const footerStyle: CSSProperties = {
        background: theme.bgPanel,
        borderTop: `1px solid ${theme.border}`,
        padding: '5px 10px',
        display: 'flex',
        gap: 6,
        flexShrink: 0,
    };

    const disabledBtnStyle: CSSProperties = {
        background: theme.bg,
        border: `1px solid ${theme.border}`,
        borderRadius: 3,
        color: theme.textMuted,
        padding: '3px 10px',
        fontSize: 11,
        cursor: 'not-allowed',
        opacity: 0.5,
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', flex: 1, overflow: 'hidden' }} data-testid="fields-tab">
            <div style={{ overflowY: 'auto', flex: 1, padding: 6 }}>
                {/* Hierarchy nav */}
                {showHierNav && (
                    <div style={hierNavStyle} data-testid="hierarchy-nav">
                        <div style={{ color: theme.textMuted, fontSize: 10, textTransform: 'uppercase', letterSpacing: 0.4, marginBottom: 4 }}>
                            Hierarchy
                        </div>
                        {hasParent && (
                            <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginBottom: 2 }}>
                                <span style={{ color: theme.textMuted }}>Parent:</span>
                                <span
                                    style={linkStyle}
                                    data-testid="parent-link"
                                    onClick={() => entity.pi !== undefined && entity.pi >= 0 && onNavigate(entity.pi)}
                                >
                                    {parentEntity ? parentEntity.n : `[${entity.pi}]`}
                                </span>
                            </div>
                        )}
                        {children.length > 0 && (
                            <div style={{ display: 'flex', alignItems: 'center', gap: 6, flexWrap: 'wrap' }}>
                                <span style={{ color: theme.textMuted }}>Children ({children.length}):</span>
                                {children.map((c) => (
                                    <span
                                        key={c.i}
                                        style={linkStyle}
                                        data-testid={`child-link-${c.i}`}
                                        onClick={() => onNavigate(c.i)}
                                    >
                                        {c.n || `[${c.i}]`}
                                    </span>
                                ))}
                            </div>
                        )}
                    </div>
                )}

                {/* Component accordions */}
                {(entity.components ?? []).map((comp, ti) => {
                    const open = isExpanded(entity.i, comp.name, ti);
                    return (
                        <div
                            key={comp.name}
                            data-testid={`component-section-${comp.name}`}
                            style={{ marginBottom: 4, border: `1px solid ${theme.border}`, borderRadius: 4, overflow: 'hidden' }}
                        >
                            {/* Header */}
                            <div
                                data-testid={`component-header-${comp.name}`}
                                onClick={() => toggleComp(entity.i, comp.name, ti)}
                                style={{ background: theme.bgPanel, padding: '5px 10px', display: 'flex', alignItems: 'center', cursor: 'pointer', gap: 6, userSelect: 'none' }}
                            >
                                <span style={{ color: theme.textMuted, fontSize: 9, transition: 'transform 0.15s', transform: open ? 'rotate(90deg)' : 'none', width: 10, display: 'inline-block' }}>▶</span>
                                <span style={{ fontWeight: 600, color: '#9cdcfe', fontSize: 12 }}>{comp.name}</span>
                                {comp.tier && (
                                    <span style={{ marginLeft: 'auto', background: theme.bg, border: `1px solid ${theme.border}`, borderRadius: 2, padding: '1px 6px', fontSize: 9.5, color: theme.textMuted, fontFamily: 'monospace' }}>
                                        Tier: {comp.tier}
                                    </span>
                                )}
                            </div>

                            {/* Body */}
                            {open && (
                                <div>
                                    <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                                        <tbody>
                                            {(comp.fields ?? []).map((f) => {
                                                const isEditingThis = editing?.compName === comp.name && editing?.fieldName === f.n;
                                                return (
                                                    <tr key={f.n} data-testid={`field-row-${comp.name}-${f.n}`}>
                                                        <td style={{ padding: '3px 10px', borderBottom: `1px solid ${theme.borderMuted}`, width: 150, color: theme.textMuted, fontSize: 11, verticalAlign: 'middle' }}>
                                                            {f.n}
                                                        </td>
                                                        <td style={{ padding: '3px 10px', borderBottom: `1px solid ${theme.borderMuted}`, width: 70, color: theme.textMuted, fontSize: 10, fontFamily: 'monospace', verticalAlign: 'middle' }}>
                                                            {f.dt ?? ''}
                                                        </td>
                                                        <td style={{ padding: '3px 10px', borderBottom: `1px solid ${theme.borderMuted}`, fontFamily: 'monospace', fontSize: 11, color: valueColor(f.t, f.v), verticalAlign: 'middle' }}>
                                                            {isEditingThis ? (
                                                                <input
                                                                    data-testid={`field-edit-input-${comp.name}-${f.n}`}
                                                                    autoFocus
                                                                    value={editValue}
                                                                    onChange={(e) => setEditValue(e.target.value)}
                                                                    onKeyDown={(e) => {
                                                                        if (e.key === 'Enter') commitEdit();
                                                                        if (e.key === 'Escape') cancelEdit();
                                                                    }}
                                                                    onBlur={commitEdit}
                                                                    style={{ background: theme.bgInput, border: `1px solid ${theme.accent}`, borderRadius: 2, color: theme.text, fontFamily: 'monospace', fontSize: 11, padding: '1px 4px', width: '100%' }}
                                                                />
                                                            ) : (
                                                                f.v
                                                            )}
                                                        </td>
                                                        <td style={{ padding: '3px 10px', borderBottom: `1px solid ${theme.borderMuted}`, verticalAlign: 'middle', textAlign: 'center', width: 28 }}>
                                                            {!isEditingThis && (
                                                                <span
                                                                    data-testid={`field-edit-btn-${comp.name}-${f.n}`}
                                                                    title="Edit field"
                                                                    onClick={() => startEdit(comp.name, f.n, f.v)}
                                                                    style={{ color: theme.textMuted, cursor: 'pointer', fontSize: 11, padding: '1px 4px', borderRadius: 2 }}
                                                                >
                                                                    ✎
                                                                </span>
                                                            )}
                                                        </td>
                                                    </tr>
                                                );
                                            })}
                                        </tbody>
                                    </table>
                                </div>
                            )}
                        </div>
                    );
                })}
            </div>

            {/* Footer */}
            <div style={footerStyle} data-testid="fields-footer">
                <button disabled style={disabledBtnStyle} title="Not available in v1">+ Add Component</button>
                <button disabled style={disabledBtnStyle} title="Not available in v1">− Remove Component</button>
                <button disabled style={disabledBtnStyle} title="Not available in v1">⎡ Destroy Entity</button>
            </div>
        </div>
    );
}
