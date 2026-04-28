import type { FC } from 'react';
import type { StepState } from '../state/types';

const statusIcons: Record<StepState['status'], { char: string; color: string }> = {
    'not-started': { char: '○', color: '#444' },
    'running':     { char: '▶', color: '#4a9eff' },
    'passed':      { char: '✓', color: '#4ec952' },
    'failed':      { char: '✗', color: '#e05050' },
    'interrupted': { char: '⚠', color: '#e8a838' },
};

function formatDuration(ms: number): string {
    if (ms <= 0) return '—';
    const s = ms / 1000;
    return s >= 60
        ? `${Math.floor(s / 60)}:${(s % 60).toFixed(1).padStart(4, '0')}`
        : `${s.toFixed(1)}s`;
}

interface StepRowProps {
    step: StepState;
}

export const StepRow: FC<StepRowProps> = ({ step }) => {
    const icon = statusIcons[step.status] ?? statusIcons['not-started'];
    return (
        <div
            style={{
                display: 'flex',
                alignItems: 'center',
                padding: '2px 12px 2px 32px',
                gap: 8,
                fontSize: 12,
            }}
        >
            <span style={{ color: icon.color, width: 12, textAlign: 'center' }}>
                {icon.char}
            </span>
            <span style={{ flex: 1, color: '#aaa' }}>{step.name}</span>
            <span style={{ fontFamily: 'monospace', color: '#666', minWidth: 50, textAlign: 'right' }}>
                {formatDuration(step.durationMs)}
            </span>
        </div>
    );
};
