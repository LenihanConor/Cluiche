import React, { useState } from 'react';
import { theme } from '@dia/editor-ui';
import { useEconomyStore } from './useEconomyStore';
import type { ModifierInstance } from './useEconomyStore';

export const ModifiersTab: React.FC = () => {
    const modifiers = useEconomyStore((s) => s.modifiers);

    const [selectedInstanceId, setSelectedInstanceId] = useState<string | null>(null);
    const [selectedResource, setSelectedResource] = useState<string | null>(null);

    const selectedInstance: ModifierInstance | undefined =
        modifiers.find((m) => m.id === selectedInstanceId) ?? modifiers[0];

    const resourceNames = selectedInstance?.resources.map((r) => r.name) ?? [];
    const selectedResourceName = selectedResource ?? resourceNames[0] ?? null;

    const resourceData = selectedInstance?.resources.find((r) => r.name === selectedResourceName);

    return (
        <div style={{ padding: 8, height: '100%', overflow: 'auto' }}>
            {/* Instance selector */}
            {modifiers.length > 1 && (
                <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', marginBottom: 8 }}>
                    {modifiers.map((inst) => {
                        const isActive = inst.id === (selectedInstance?.id ?? '');
                        return (
                            <button
                                key={inst.id}
                                onClick={() => {
                                    setSelectedInstanceId(inst.id);
                                    setSelectedResource(null);
                                }}
                                style={{
                                    padding: '3px 8px',
                                    borderRadius: 10,
                                    border: `1px solid ${isActive ? theme.accentHover : theme.border}`,
                                    background: isActive ? theme.accent : 'transparent',
                                    color: isActive ? '#fff' : theme.textMuted,
                                    cursor: 'pointer',
                                    fontSize: 11,
                                }}
                            >
                                {inst.id}
                            </button>
                        );
                    })}
                </div>
            )}

            {/* Resource selector */}
            {resourceNames.length > 1 && (
                <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', marginBottom: 8 }}>
                    {resourceNames.map((name) => {
                        const isActive = name === selectedResourceName;
                        return (
                            <button
                                key={name}
                                onClick={() => setSelectedResource(name)}
                                style={{
                                    padding: '2px 7px',
                                    borderRadius: 10,
                                    border: `1px solid ${isActive ? theme.accentHover : theme.border}`,
                                    background: isActive ? theme.accent : 'transparent',
                                    color: isActive ? '#fff' : theme.textMuted,
                                    cursor: 'pointer',
                                    fontSize: 11,
                                }}
                            >
                                {name}
                            </button>
                        );
                    })}
                </div>
            )}

            {/* Modifier table */}
            {!resourceData ? (
                <div style={{ color: theme.textMuted, fontSize: 12 }}>No modifier data</div>
            ) : (
                <table
                    style={{
                        width: '100%',
                        borderCollapse: 'collapse',
                        fontSize: 11,
                        color: theme.text,
                    }}
                >
                    <thead>
                        <tr
                            style={{
                                background: theme.bgPanel,
                                borderBottom: `1px solid ${theme.border}`,
                            }}
                        >
                            {['Type', 'Value', 'Source', 'Condition', 'Status'].map((h) => (
                                <th
                                    key={h}
                                    style={{
                                        padding: '5px 8px',
                                        textAlign: 'left',
                                        color: theme.textMuted,
                                        fontWeight: 600,
                                    }}
                                >
                                    {h}
                                </th>
                            ))}
                        </tr>
                    </thead>
                    <tbody>
                        {resourceData.modifiers.map((entry, idx) => (
                            <tr
                                key={idx}
                                data-testid="modifier-row"
                                data-active={entry.active}
                                style={{
                                    borderBottom: `1px solid ${theme.border}22`,
                                    opacity: entry.active ? 1 : 0.45,
                                }}
                            >
                                <td style={{ padding: '3px 8px' }}>{entry.type}</td>
                                <td style={{ padding: '3px 8px', color: theme.textMuted }}>{entry.value}</td>
                                <td style={{ padding: '3px 8px' }}>{entry.source}</td>
                                <td style={{ padding: '3px 8px', color: theme.textMuted }}>
                                    {entry.condition ?? '—'}
                                </td>
                                <td style={{ padding: '3px 8px' }}>
                                    <span
                                        data-testid="modifier-active"
                                        style={{
                                            fontSize: 9,
                                            padding: '1px 5px',
                                            borderRadius: 3,
                                            background: entry.active ? '#2d6a2d' : '#3a3a3a',
                                            color: entry.active ? '#89d185' : theme.textMuted,
                                        }}
                                    >
                                        {entry.active ? 'active' : 'inactive'}
                                    </span>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            )}
        </div>
    );
};
