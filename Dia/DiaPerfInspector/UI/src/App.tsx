import { useEffect } from 'react';
import { bridgeRequest, kConnectionPrefix } from './bridge';
import { subscribePerfTiming, usePerfTimingStore } from './usePerfTimingStore';
import { useLiveConnection } from './useLiveConnection';
import { useWindowPolling } from './useWindowPolling';
import { useWindowStore } from './useWindowStore';
import { useSelectionStore } from './useSelectionStore';
import { usePauseStore } from './usePauseStore';
import { LanesView } from './components/LanesView';
import { ModuleTable } from './components/ModuleTable';
import { FrameDetail } from './components/FrameDetail';
import { TripsList } from './components/TripsList';
import { EventsRow } from './components/EventsRow';
import { ChainPanel } from './components/ChainPanel';
import { SelfCostPanel } from './components/SelfCostPanel';
import { LatencyPanel } from './components/LatencyPanel';
import { useChainStore } from './useChainStore';
import { useLatencyStore } from './useLatencyStore';

const panel: React.CSSProperties = { background: '#252525', border: '1px solid #333', padding: '6px 8px', minWidth: 0 };
const heading: React.CSSProperties = { margin: '0 0 4px', fontSize: 12, color: '#9cdcfe', fontWeight: 600 };

export default function App() {
    const connection = useLiveConnection(kConnectionPrefix, bridgeRequest);
    const setConnectionState = usePerfTimingStore((s) => s.setConnectionState);
    const unavailable = usePerfTimingStore((s) => s.timing?.unavailable);
    const selfMs = usePerfTimingStore((s) => s.timing?.selfMs);
    const latency = useLatencyStore((s) => s.actions);
    const lanes = useWindowStore((s) => s.lanes);
    const trips = useWindowStore((s) => s.trips);
    const markers = useWindowStore((s) => s.markers);
    const select = useSelectionStore((s) => s.select);
    const chain = useChainStore((s) => s.chain);
    // One budget = the tick budget of the unit that ends the chain (falls back to the first lane).
    const lastUnit = chain && chain.stages.length > 0 ? [...chain.stages].sort((a, b) => a.endMs - b.endMs)[chain.stages.length - 1].unit : null;
    const chainBudgetMs = (lanes.find((l) => l.unit === lastUnit) ?? lanes[0])?.budgetMs ?? 0;

    useEffect(() => { setConnectionState(connection); }, [connection, setConnectionState]);
    useEffect(() => subscribePerfTiming(), []);
    useWindowPolling(connection);
    useEffect(() => { useSelectionStore.getState().reconcile(lanes); }, [lanes]);
    useEffect(() => {
        if (connection !== 'connected') {
            usePauseStore.getState().reset(); // the game releases this client's pauses on disconnect
            useSelectionStore.getState().clear();
        }
    }, [connection]);

    return (
        <div className="perf-inspector">
            <div data-testid="connection-banner" data-state={connection}>
                {connection === 'connected' ? 'Connected to game' : 'Disconnected: waiting for a game connection'}
            </div>
            <section data-testid="lanes"><LanesView /></section>
            <section data-testid="events" style={{ padding: '0 8px 4px 92px' }}>
                <EventsRow markers={markers} unavailable={unavailable} />
            </section>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, minmax(0, 1fr))', gap: 8, padding: '4px 8px 8px' }}>
                <section data-testid="modules" style={panel}>
                    <h4 style={heading}>Modules</h4>
                    <ModuleTable />
                </section>
                <section data-testid="frame" style={panel}>
                    <h4 style={heading}>Selected frame</h4>
                    <FrameDetail />
                </section>
                <section data-testid="trips" style={panel}>
                    <h4 style={heading}>Trips</h4>
                    <TripsList records={trips} unavailable={unavailable} onSelect={(u, f) => { void select(u, f); }} />
                </section>
                <section data-testid="chain" style={{ ...panel, gridColumn: '1 / -1' }}>
                    <h4 style={heading}>Cross-thread chain</h4>
                    <ChainPanel budgetMs={chainBudgetMs} />
                </section>
                <section data-testid="latency" style={{ ...panel, gridColumn: '1 / span 2' }}>
                    <h4 style={heading}>Latency (end to end)</h4>
                    <LatencyPanel actions={latency} unavailable={unavailable} />
                </section>
                <section data-testid="selfcost" style={panel}>
                    <h4 style={heading}>Telemetry self-cost</h4>
                    <SelfCostPanel selfMs={selfMs} unavailable={unavailable} />
                </section>
            </div>
        </div>
    );
}
