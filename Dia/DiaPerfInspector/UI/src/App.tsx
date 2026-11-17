import { useEffect } from 'react';
import { bridgeRequest, kConnectionPrefix } from './bridge';
import { subscribePerfTiming, usePerfTimingStore } from './usePerfTimingStore';
import { useLiveConnection } from './useLiveConnection';
import { useWindowPolling } from './useWindowPolling';
import { LanesView } from './components/LanesView';

export default function App() {
    const connection = useLiveConnection(kConnectionPrefix, bridgeRequest);
    const setConnectionState = usePerfTimingStore((s) => s.setConnectionState);

    useEffect(() => { setConnectionState(connection); }, [connection, setConnectionState]);
    useEffect(() => subscribePerfTiming(), []);
    useWindowPolling(connection);

    return (
        <div className="perf-inspector">
            <div data-testid="connection-banner" data-state={connection}>
                {connection === 'connected' ? 'Connected to game' : 'Disconnected: waiting for a game connection'}
            </div>
            <section data-testid="lanes"><LanesView /></section>
        </div>
    );
}
