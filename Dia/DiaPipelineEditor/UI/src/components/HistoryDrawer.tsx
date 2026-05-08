import type { FC, Dispatch } from 'react';
import type { HistoryRun } from '../state/types';
import type { PipelineAction } from '../state/pipelineReducer';

function formatDuration(ms: number): string {
    if (ms <= 0) return '—';
    const totalSec = ms / 1000;
    const min = Math.floor(totalSec / 60);
    const sec = (totalSec % 60).toFixed(1);
    return min > 0 ? `${min}:${sec.padStart(4, '0')}` : `${sec}s`;
}

function formatTimestamp(ts: number): string {
    if (ts <= 0) return '';
    const d = new Date(ts * 1000);
    return d.toLocaleString(undefined, {
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
    });
}

interface HistoryDrawerProps {
    runs: HistoryRun[];
    viewingIndex: number | null;
    dispatch: Dispatch<PipelineAction>;
}

export const HistoryDrawer: FC<HistoryDrawerProps> = ({ runs, viewingIndex, dispatch }) => {
    if (runs.length === 0) {
        return (
            <div style={{ padding: '8px 12px', color: '#666', fontSize: 12 }}>
                No history yet
            </div>
        );
    }

    return (
        <div style={{ borderTop: '1px solid #333', maxHeight: 200, overflowY: 'auto' }}>
            <div style={{
                padding: '4px 12px',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                background: '#252525',
                borderBottom: '1px solid #333',
            }}>
                <span style={{ fontWeight: 600, fontSize: 12 }}>History</span>
                {viewingIndex !== null && (
                    <button
                        onClick={() => dispatch({ type: 'CLEAR_HISTORY_VIEW' })}
                        style={{
                            background: '#333',
                            color: '#ccc',
                            border: '1px solid #555',
                            borderRadius: 3,
                            padding: '2px 8px',
                            cursor: 'pointer',
                            fontSize: 11,
                        }}
                    >
                        Back to current
                    </button>
                )}
            </div>
            {runs.map((run, i) => {
                const isViewing = viewingIndex === i;
                const statusColor = run.interrupted ? '#e8a838' : run.failCount > 0 ? '#e85050' : '#4ec94e';
                return (
                    <div
                        key={`${run.startTimestamp}-${i}`}
                        onClick={() => dispatch({ type: 'VIEW_HISTORY', index: i })}
                        style={{
                            padding: '4px 12px',
                            cursor: 'pointer',
                            display: 'flex',
                            justifyContent: 'space-between',
                            alignItems: 'center',
                            background: isViewing ? '#333' : 'transparent',
                            borderLeft: isViewing ? '3px solid #569cd6' : '3px solid transparent',
                            fontSize: 12,
                        }}
                    >
                        <span>
                            <span style={{ color: statusColor, marginRight: 6 }}>●</span>
                            <span>{run.target}</span>
                            {run.config && <span style={{ color: '#888' }}> · {run.config}</span>}
                            <span style={{ color: '#888', marginLeft: 6 }}>
                                {run.passCount}✓ {run.failCount}✗
                            </span>
                        </span>
                        <span style={{ color: '#888', fontFamily: 'monospace', fontSize: 11 }}>
                            {formatDuration(run.totalDurationMs)}
                            <span style={{ marginLeft: 8 }}>{formatTimestamp(run.startTimestamp)}</span>
                        </span>
                    </div>
                );
            })}
        </div>
    );
};
