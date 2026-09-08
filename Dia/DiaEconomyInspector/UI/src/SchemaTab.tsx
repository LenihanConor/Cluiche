import React, { useState } from 'react';
import { theme, inputStyle } from '@dia/editor-ui';
import { useEconomyStore } from './useEconomyStore';

export const SchemaTab: React.FC = () => {
    const schema = useEconomyStore((s) => s.schema);
    const [search, setSearch] = useState('');

    if (!schema) {
        return (
            <div style={{ padding: 8, color: theme.textMuted, fontSize: 12 }}>
                No schema data received yet.
            </div>
        );
    }

    const filteredResources = schema.resources.filter((r) =>
        r.name.toLowerCase().includes(search.toLowerCase()),
    );

    // Derive cost table columns from all actions
    const costColumns = schema.cost_table.length > 0
        ? Object.keys(schema.cost_table[0]).filter((k) => k !== 'action')
        : [];

    return (
        <div style={{ padding: 8, height: '100%', overflow: 'auto' }}>
            {/* Search bar */}
            <input
                type="text"
                placeholder="Search resources..."
                value={search}
                onChange={(e) => setSearch(e.target.value)}
                style={{ ...inputStyle(), width: '100%', marginBottom: 10 }}
            />

            {/* Resources table */}
            <div style={{ marginBottom: 4, fontSize: 11, color: theme.textMuted, fontWeight: 600 }}>
                Resources
            </div>
            <table
                style={{
                    width: '100%',
                    borderCollapse: 'collapse',
                    fontSize: 11,
                    color: theme.text,
                    marginBottom: 16,
                }}
            >
                <thead>
                    <tr style={{ background: theme.bgPanel, borderBottom: `1px solid ${theme.border}` }}>
                        {['Name', 'Type', 'Cap', 'Income Rule'].map((h) => (
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
                    {filteredResources.length === 0 ? (
                        <tr>
                            <td colSpan={4} style={{ padding: 8, color: theme.textMuted, textAlign: 'center' }}>
                                No resources match search
                            </td>
                        </tr>
                    ) : (
                        filteredResources.map((r) => (
                            <tr
                                key={r.name}
                                data-testid="schema-resource-row"
                                style={{ borderBottom: `1px solid ${theme.border}22` }}
                            >
                                <td style={{ padding: '3px 8px', fontWeight: 500 }}>{r.name}</td>
                                <td style={{ padding: '3px 8px' }}>
                                    <span
                                        style={{
                                            fontSize: 9,
                                            padding: '1px 5px',
                                            borderRadius: 3,
                                            background: r.type === 'derived' ? '#9b59b6' : theme.accent,
                                            color: '#fff',
                                            textTransform: 'uppercase',
                                        }}
                                    >
                                        {r.type}
                                    </span>
                                </td>
                                <td style={{ padding: '3px 8px', color: theme.textMuted }}>{r.base_cap}</td>
                                <td style={{ padding: '3px 8px', color: theme.textMuted }}>{r.income_rule}</td>
                            </tr>
                        ))
                    )}
                </tbody>
            </table>

            {/* Cost table */}
            {schema.cost_table.length > 0 && (
                <>
                    <div style={{ marginBottom: 4, fontSize: 11, color: theme.textMuted, fontWeight: 600 }}>
                        Cost Table
                    </div>
                    <table
                        style={{
                            width: '100%',
                            borderCollapse: 'collapse',
                            fontSize: 11,
                            color: theme.text,
                        }}
                    >
                        <thead>
                            <tr style={{ background: theme.bgPanel, borderBottom: `1px solid ${theme.border}` }}>
                                <th style={{ padding: '5px 8px', textAlign: 'left', color: theme.textMuted, fontWeight: 600 }}>
                                    Action
                                </th>
                                {costColumns.map((col) => (
                                    <th
                                        key={col}
                                        style={{
                                            padding: '5px 8px',
                                            textAlign: 'left',
                                            color: theme.textMuted,
                                            fontWeight: 600,
                                        }}
                                    >
                                        {col}
                                    </th>
                                ))}
                            </tr>
                        </thead>
                        <tbody>
                            {schema.cost_table.map((row, idx) => (
                                <tr
                                    key={idx}
                                    data-testid="schema-cost-row"
                                    style={{ borderBottom: `1px solid ${theme.border}22` }}
                                >
                                    <td style={{ padding: '3px 8px', fontWeight: 500 }}>{row.action}</td>
                                    {costColumns.map((col) => (
                                        <td key={col} style={{ padding: '3px 8px', color: theme.textMuted }}>
                                            {row[col] ?? '—'}
                                        </td>
                                    ))}
                                </tr>
                            ))}
                        </tbody>
                    </table>
                </>
            )}
        </div>
    );
};
