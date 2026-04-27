import type { FC } from 'react';
import type { HistoryRun } from '../state/types';

function formatDuration(ms: number): string {
    if (ms <= 0) return '—';
    const totalSec = ms / 1000;
    const min = Math.floor(totalSec / 60);
    const sec = (totalSec % 60).toFixed(1);
    return min > 0 ? `${min}:${sec.padStart(4, '0')}` : `${sec}s`;
}

function formatTimestamp(ts: number): string {
    if (ts <= 0) return '';
    return new Date(ts * 1000).toLocaleString();
}

interface HistorySummaryProps {
    run: HistoryRun;
}

export const HistorySummary: FC<HistorySummaryProps> = ({ run }) => {
    const statusColor = run.interrupted ? '#e8a838' : run.failCount > 0 ? '#e85050' : '#4ec94e';

    return (
        <div style={{ padding: '16px' }}>
            <div style={{ marginBottom: 12 }}>
                <span style={{ fontSize: 14, fontWeight: 600 }}>Past Run</span>
                <span style={{ color: '#888', marginLeft: 8, fontSize: 12 }}>
                    {formatTimestamp(run.startTimestamp)}
                </span>
            </div>
            <div style={{
                background: '#252525',
                borderRadius: 4,
                padding: '12px 16px',
                border: '1px solid #333',
            }}>
                <div style={{ marginBottom: 8 }}>
                    <span style={{ color: statusColor, marginRight: 8 }}>●</span>
                    <span style={{ fontWeight: 600 }}>{run.target}</span>
                    {run.config && <span style={{ color: '#888' }}> · {run.config}</span>}
                    {run.interrupted && <span style={{ color: '#e8a838', marginLeft: 8 }}>interrupted</span>}
                </div>
                <div style={{ display: 'flex', gap: 24, fontSize: 13 }}>
                    <span><span style={{ color: '#4ec94e' }}>{run.passCount}</span> passed</span>
                    <span><span style={{ color: '#e85050' }}>{run.failCount}</span> failed</span>
                    <span style={{ color: '#888', fontFamily: 'monospace' }}>{formatDuration(run.totalDurationMs)}</span>
                </div>
            </div>
        </div>
    );
};
