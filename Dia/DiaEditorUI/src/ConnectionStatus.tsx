import React from 'react';
import { TrafficLightDot } from './TrafficLightDot';
import type { DotState } from './TrafficLightDot';
import { theme } from './theme';

export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'error';

export interface ConnectionStatusProps {
  state: ConnectionState;
  label?: string;
  onConnect?: () => void;
  onDisconnect?: () => void;
  compact?: boolean;
}

const DOT_STATE: Record<ConnectionState, DotState> = {
  connected:    'green',
  connecting:   'amber',
  disconnected: 'grey',
  error:        'red',
};

const BG: Record<ConnectionState, string> = {
  connected:    '#1a3a1a',
  connecting:   '#333',
  disconnected: '#333',
  error:        '#3a1a1a',
};

const DEFAULT_LABEL: Record<ConnectionState, string> = {
  connected:    'Connected',
  connecting:   'Connecting...',
  disconnected: 'Disconnected',
  error:        'Error',
};

const ACTION_BTN_STYLE: React.CSSProperties = {
  background: 'transparent',
  border: `1px solid ${theme.borderMuted}`,
  borderRadius: 3,
  color: theme.textMuted,
  cursor: 'pointer',
  fontSize: 11,
  fontFamily: "'Segoe UI', system-ui, sans-serif",
  padding: '1px 5px',
  lineHeight: 1,
  marginLeft: 4,
};

export const ConnectionStatus: React.FC<ConnectionStatusProps> = ({
  state,
  label,
  onConnect,
  onDisconnect,
  compact = false,
}) => {
  if (compact) {
    return <TrafficLightDot state={DOT_STATE[state]} size={8} />;
  }

  const displayLabel = label ?? DEFAULT_LABEL[state];

  return (
    <div
      data-testid="connection-status"
      data-connection-state={state}
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: 6,
        padding: '4px 10px',
        background: BG[state],
        border: `1px solid ${theme.borderMuted}`,
        borderRadius: 4,
        fontSize: 12,
        userSelect: 'none',
        fontFamily: "'Segoe UI', system-ui, sans-serif",
        color: theme.text,
      }}
    >
      <TrafficLightDot state={DOT_STATE[state]} size={8} />
      <span>{displayLabel}</span>
      {state === 'disconnected' && onConnect && (
        <button
          data-testid="connect-btn"
          onClick={onConnect}
          style={ACTION_BTN_STYLE}
        >
          Connect
        </button>
      )}
      {state === 'connected' && onDisconnect && (
        <button
          data-testid="disconnect-btn"
          onClick={onDisconnect}
          style={ACTION_BTN_STYLE}
        >
          ×
        </button>
      )}
    </div>
  );
};
