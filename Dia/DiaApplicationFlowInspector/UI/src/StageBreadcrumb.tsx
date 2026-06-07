import React from 'react';
import { useInspectorStore } from './useInspectorStore';

function formatDuration(ms: number): string {
    if (ms < 1000) return `${ms}ms`;
    if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
    return `${Math.floor(ms / 60000)}m${Math.floor((ms % 60000) / 1000)}s`;
}

export const StageBreadcrumb: React.FC = () => {
    const timeline = useInspectorStore((s) => s.timeline);

    if (timeline.length === 0) {
        return (
            <div data-testid="breadcrumb-empty" style={{ color: '#555', fontSize: 12, padding: '4px 8px' }}>
                No stage history
            </div>
        );
    }

    return (
        <div
            data-testid="breadcrumb-container"
            style={{
                display: 'flex',
                alignItems: 'center',
                gap: 4,
                overflowX: 'auto',
                padding: '4px 8px',
                fontSize: 12,
                whiteSpace: 'nowrap',
            }}
        >
            {timeline.map((entry, i) => {
                const isCurrent = i === timeline.length - 1;
                const next = timeline[i + 1];
                const duration = next ? next.enteredAtMs - entry.enteredAtMs : null;

                return (
                    <React.Fragment key={`${entry.stageName}-${entry.enteredAtMs}`}>
                        <span
                            data-testid={isCurrent ? 'breadcrumb-current' : 'breadcrumb-past'}
                            style={{
                                color: isCurrent ? '#4caf50' : '#888',
                                fontWeight: isCurrent ? 600 : 400,
                            }}
                        >
                            {entry.stageName}
                        </span>
                        {duration !== null && (
                            <>
                                <span style={{ color: '#555', fontSize: 10 }}>
                                    {formatDuration(duration)}
                                </span>
                                <span style={{ color: '#555' }}>→</span>
                            </>
                        )}
                    </React.Fragment>
                );
            })}
        </div>
    );
};
