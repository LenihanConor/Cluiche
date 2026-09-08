import { useState } from 'react';
import { TabBar, ConnectionStatus, EmptyState, useBridgeSubscribe, theme } from '@dia/editor-ui';
import type { Tab } from '@dia/editor-ui';
import { bridgeRequest } from './bridge';
import { useLiveConnection } from './useLiveConnection';
import { useEconomyStore } from './useEconomyStore';
import { ResourcesTab } from './ResourcesTab';
import { EventsTab } from './EventsTab';
import { ModifiersTab } from './ModifiersTab';
import { SchemaTab } from './SchemaTab';
import type { InstanceState, ModifierInstance, EconomyEvent } from './useEconomyStore';

type EconomyTab = 'resources' | 'events' | 'modifiers' | 'schema';

const ECONOMY_TABS: Tab[] = [
    { id: 'resources', label: 'Resources' },
    { id: 'events',    label: 'Events' },
    { id: 'modifiers', label: 'Modifiers' },
    { id: 'schema',    label: 'Schema' },
];

export default function EconomyInspector() {
    const [activeTab, setActiveTab] = useState<EconomyTab>('resources');

    const connectionState = useLiveConnection('economy_inspector', bridgeRequest);
    const isConnected = connectionState === 'connected';

    const setSchema     = useEconomyStore((s) => s.setSchema);
    const setInstances  = useEconomyStore((s) => s.setInstances);
    const setModifiers  = useEconomyStore((s) => s.setModifiers);
    const appendEvents  = useEconomyStore((s) => s.appendEvents);
    const clearAll      = useEconomyStore((s) => s.clearAll);

    // Clear store when disconnected
    if (!isConnected) {
        // Only call this outside render — handled by the subscribe lifecycle below
    }

    useBridgeSubscribe([
        {
            topic: 'economy.schema',
            handler: (data) => {
                const d = data as { resources: unknown[]; cost_table: unknown[] };
                setSchema(d as Parameters<typeof setSchema>[0]);
            },
        },
        {
            topic: 'economy.instances',
            handler: (data) => {
                const d = data as { frame: number; instances: InstanceState[] };
                setInstances(d.instances ?? []);
            },
        },
        {
            topic: 'economy.modifiers',
            handler: (data) => {
                const d = data as { frame: number; instances: ModifierInstance[] };
                setModifiers(d.instances ?? []);
            },
        },
        {
            topic: 'economy.events',
            handler: (data) => {
                const d = data as { full_ring: boolean; events: EconomyEvent[] };
                appendEvents({ full_ring: d.full_ring ?? false, events: d.events ?? [] });
            },
        },
        {
            // Disconnect notification resets the store
            topic: 'economy_inspector.connection_state',
            handler: (data) => {
                const d = data as { connected?: boolean };
                if (!d?.connected) {
                    clearAll();
                }
            },
        },
    ]);

    return (
        <div
            style={{
                fontFamily: "'Segoe UI', system-ui, sans-serif",
                background: theme.bg,
                color: theme.text,
                height: '100%',
                display: 'flex',
                flexDirection: 'column',
                fontSize: 12,
            }}
        >
            {/* Titlebar */}
            <div
                style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: 8,
                    padding: '5px 10px',
                    background: theme.bgPanel,
                    borderBottom: `1px solid ${theme.border}`,
                    flexShrink: 0,
                    userSelect: 'none',
                }}
            >
                <ConnectionStatus state={isConnected ? 'connected' : 'disconnected'} compact />
                <span style={{ fontWeight: 600, color: theme.text }}>Economy Inspector</span>
                <span style={{ color: theme.textMuted, fontSize: 10, marginLeft: 4 }}>
                    DiaEconomyInspectorPlugin v1.0
                </span>
            </div>

            {!isConnected ? (
                <EmptyState
                    message="No game connected"
                    hint="Use the Game Connection panel in the toolbar to connect."
                />
            ) : (
                <>
                    <TabBar
                        tabs={ECONOMY_TABS}
                        activeTab={activeTab}
                        onTabChange={(id) => setActiveTab(id as EconomyTab)}
                    />
                    <div
                        data-testid="connected-content"
                        style={{ flex: 1, overflow: 'auto', display: 'flex', flexDirection: 'column' }}
                    >
                        {activeTab === 'resources'  && <div data-testid="tab-content-resources"  style={{ flex: 1 }}><ResourcesTab /></div>}
                        {activeTab === 'events'     && <div data-testid="tab-content-events"     style={{ flex: 1 }}><EventsTab /></div>}
                        {activeTab === 'modifiers'  && <div data-testid="tab-content-modifiers"  style={{ flex: 1 }}><ModifiersTab /></div>}
                        {activeTab === 'schema'     && <div data-testid="tab-content-schema"     style={{ flex: 1 }}><SchemaTab /></div>}
                    </div>
                </>
            )}
        </div>
    );
}
