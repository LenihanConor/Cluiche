import type { FC } from 'react';
import type { PipelineState } from '../state/types';

function formatDuration(ms: number): string {
    if (ms <= 0) return '—';
    const totalSec = ms / 1000;
    const min = Math.floor(totalSec / 60);
    const sec = (totalSec % 60).toFixed(1);
    return min > 0 ? `${min}:${sec.padStart(4, '0')}` : `${sec}s`;
}

interface RunSummaryProps {
    state: PipelineState;
}

export const RunSummary: FC<RunSummaryProps> = ({ state }) => {
    const counts = [];
    if (state.passCount > 0) counts.push(`${state.passCount} passed`);
    if (state.failCount > 0) counts.push(`${state.failCount} failed`);
    const countsStr = counts.join(' · ') || (state.runInProgress ? 'running' : 'no stages');

    return (
        <div style={{
            padding: '8px 12px',
            borderBottom: '1px solid #333',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            background: '#252525',
        }}>
            <div>
                <span style={{ fontWeight: 600 }}>Pipeline</span>
                {(state.target || state.config) && (
                    <span style={{ color: '#888', marginLeft: 8 }}>
                        {[state.target, state.config].filter(Boolean).join(' · ')}
                    </span>
                )}
                <span style={{ color: '#888', marginLeft: 8 }}>{countsStr}</span>
                {state.interrupted && (
                    <span style={{ color: '#e8a838', marginLeft: 8 }}>interrupted</span>
                )}
            </div>
            <span style={{ fontFamily: 'monospace', color: '#888' }}>
                {formatDuration(state.totalDurationMs)}
            </span>
        </div>
    );
};
