import type { FC, ReactNode } from 'react';
import { theme } from './theme';

export interface EmptyStateProps {
  message: string;
  hint?: string;
  icon?: ReactNode;
}

export const EmptyState: FC<EmptyStateProps> = ({ message, hint, icon }) => (
  <div
    data-testid="empty-state"
    style={{
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      justifyContent: 'center',
      height: '100%',
      gap: 8,
    }}
  >
    <div style={{ fontSize: 24 }}>
      {icon ?? '⚙'}
    </div>
    <div style={{ fontSize: 12, color: theme.textDim }}>
      {message}
    </div>
    {hint && (
      <div style={{ fontSize: 11, color: theme.textMuted }}>
        {hint}
      </div>
    )}
  </div>
);
