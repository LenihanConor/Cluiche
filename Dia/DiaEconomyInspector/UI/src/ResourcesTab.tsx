import React, { useState } from 'react';
import { theme } from '@dia/editor-ui';
import { useEconomyStore } from './useEconomyStore';
import { ResourceFillBar } from './ResourceFillBar';
import { Sparkline } from './Sparkline';
import type { InstanceState, ResourceState } from './useEconomyStore';

// ---------------------------------------------------------------------------
// Stat tile
// ---------------------------------------------------------------------------

interface StatTileProps {
    label: string;
    value: number | string;
    accent?: string;
}

const StatTile: React.FC<StatTileProps> = ({ label, value, accent }) => (
    <div
        style={{
            background: theme.bgPanel,
            border: `1px solid ${theme.border}`,
            borderRadius: 4,
            padding: '6px 10px',
            minWidth: 80,
            flex: '1 1 80px',
        }}
    >
        <div style={{ fontSize: 18, fontWeight: 700, color: accent ?? theme.text }}>{value}</div>
        <div style={{ fontSize: 10, color: theme.textMuted, marginTop: 2 }}>{label}</div>
    </div>
);

// ---------------------------------------------------------------------------
// Resource card
// ---------------------------------------------------------------------------

interface ResourceCardProps {
    resource: ResourceState;
}

const ResourceCard: React.FC<ResourceCardProps> = ({ resource }) => {
    const isCapped = resource.capped_duration_s > 0;
    const isStarved = resource.starved_duration_s > 0;

    return (
        <div
            style={{
                background: theme.bgPanel,
                border: `1px solid ${theme.border}`,
                borderRadius: 4,
                padding: 8,
                marginBottom: 6,
            }}
        >
            {/* Header */}
            <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginBottom: 6 }}>
                <span style={{ fontWeight: 600, color: theme.text }}>{resource.name}</span>
                <span
                    style={{
                        fontSize: 9,
                        padding: '1px 5px',
                        borderRadius: 3,
                        background: resource.type === 'derived' ? '#9b59b6' : theme.accent,
                        color: '#fff',
                        textTransform: 'uppercase',
                    }}
                >
                    {resource.type}
                </span>
                {isCapped && (
                    <span
                        style={{
                            fontSize: 9,
                            padding: '1px 5px',
                            borderRadius: 3,
                            background: theme.warning,
                            color: '#000',
                            textTransform: 'uppercase',
                        }}
                    >
                        capped
                    </span>
                )}
                {isStarved && (
                    <span
                        style={{
                            fontSize: 9,
                            padding: '1px 5px',
                            borderRadius: 3,
                            background: theme.error,
                            color: '#fff',
                            textTransform: 'uppercase',
                        }}
                    >
                        starved
                    </span>
                )}
                <span style={{ marginLeft: 'auto', color: theme.textMuted, fontSize: 11 }}>
                    {resource.current.toFixed(1)} / {resource.cap.toFixed(1)}
                </span>
            </div>

            {/* Fill bar */}
            <ResourceFillBar value={resource.current} max={resource.cap} type={resource.type} />

            {/* Rate row */}
            <div style={{ display: 'flex', gap: 12, marginTop: 6, fontSize: 11, color: theme.textMuted }}>
                <span>
                    Net: <span style={{ color: resource.net_rate >= 0 ? theme.success : theme.error }}>
                        {resource.net_rate >= 0 ? '+' : ''}{resource.net_rate.toFixed(2)}/s
                    </span>
                </span>
                <span>In: <span style={{ color: theme.success }}>+{resource.gross_income.toFixed(2)}</span></span>
                <span>Out: <span style={{ color: theme.error }}>-{resource.gross_spend.toFixed(2)}</span></span>
            </div>

            {/* Sparkline */}
            {resource.history.length >= 2 && (
                <div style={{ marginTop: 6 }}>
                    <Sparkline history={resource.history} width={200} height={28} />
                </div>
            )}
        </div>
    );
};

// ---------------------------------------------------------------------------
// Main tab
// ---------------------------------------------------------------------------

export const ResourcesTab: React.FC = () => {
    const instances = useEconomyStore((s) => s.instances);
    const events = useEconomyStore((s) => s.events);

    const [selectedId, setSelectedId] = useState<string | null>(null);

    const selected: InstanceState | undefined =
        instances.find((i) => i.id === selectedId) ?? instances[0];

    const activeCount = instances.length;
    const resourceCount = selected?.resources.length ?? 0;
    const eventCount = events.length;
    const capWarnings = instances.reduce(
        (acc, inst) =>
            acc + inst.resources.filter((r) => r.capped_duration_s > 0).length,
        0,
    );

    return (
        <div style={{ padding: 8, height: '100%', overflow: 'auto' }}>
            {/* Instance selector chips */}
            {instances.length > 1 && (
                <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', marginBottom: 8 }}>
                    {instances.map((inst) => {
                        const isActive = inst.id === (selected?.id ?? '');
                        return (
                            <button
                                key={inst.id}
                                onClick={() => setSelectedId(inst.id)}
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

            {/* Stat tiles */}
            <div style={{ display: 'flex', gap: 8, marginBottom: 8, flexWrap: 'wrap' }}>
                <StatTile label="Active Instances" value={activeCount} />
                <StatTile label="Resources Tracked" value={resourceCount} />
                <StatTile label="Events" value={eventCount} />
                <StatTile
                    label="Cap Warnings"
                    value={capWarnings}
                    accent={capWarnings > 0 ? theme.warning : undefined}
                />
            </div>

            {/* Resource cards */}
            {selected ? (
                selected.resources.map((r) => (
                    <ResourceCard key={r.name} resource={r} />
                ))
            ) : (
                <div style={{ color: theme.textMuted, fontSize: 12 }}>No instance data</div>
            )}
        </div>
    );
};
