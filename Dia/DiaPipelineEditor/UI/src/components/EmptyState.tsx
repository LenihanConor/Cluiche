import type { FC } from 'react';

export const EmptyState: FC = () => (
    <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '100%',
        color: '#555',
        gap: 8,
    }}>
        <span style={{ fontSize: 24 }}>&#9881;</span>
        <span>No pipeline run yet</span>
        <span style={{ fontSize: 11, color: '#444' }}>Trigger a build or wait for pipeline output</span>
    </div>
);
