import { LiveConnectionButton } from './LiveConnectionButton';
import { LiveTransitionPanel } from './LiveTransitionPanel';
import { useLiveStoreV2 } from './useLiveStoreV2';

export default function AppInspector() {
  const connectionState = useLiveStoreV2((s) => s.connectionState);
  const isConnected = connectionState === 'connected';

  return (
    <div style={{ fontFamily: 'monospace', background: '#1a1a1a', color: '#ccc', height: '100%', display: 'flex', flexDirection: 'column' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '6px 10px', borderBottom: '1px solid #333' }}>
        <span style={{ fontSize: 12, color: '#888' }}>Application Flow Inspector</span>
        <div style={{ marginLeft: 'auto' }}>
          <LiveConnectionButton />
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, padding: '10px', overflow: 'auto' }}>
        {!isConnected ? (
          <div data-testid="empty-state" style={{ color: '#555', fontSize: 13, padding: '20px 0' }}>
            Not connected — use the connection button to connect to a running game.
          </div>
        ) : (
          <div data-testid="connected-content">
            <LiveTransitionPanel stages={[]} />
            {/* Phase 3 components (useInspectorStore tabs) will be added here */}
          </div>
        )}
      </div>
    </div>
  );
}
