import { useState, useEffect } from 'react';
import type { FC } from 'react';
import type { StageState } from '../state/types';
import { StageDetail } from './StageDetail';

const statusIcons: Record<StageState['status'], { char: string; color: string }> = {
    'not-started': { char: '○', color: '#555' },
    'running':     { char: '▶', color: '#4a9eff' },
    'passed':      { char: '✓', color: '#4ec952' },
    'failed':      { char: '✗', color: '#e05050' },
    'interrupted': { char: '⚠', color: '#e8a838' },
};

function formatDuration(ms: number): string {
    if (ms <= 0) return '—';
    const totalSec = ms / 1000;
    return totalSec >= 60
        ? `${Math.floor(totalSec / 60)}:${(totalSec % 60).toFixed(1).padStart(4, '0')}`
        : `${totalSec.toFixed(1)}s`;
}

interface StageRowProps {
    stage: StageState;
    onToggle: () => void;
}

export const StageRow: FC<StageRowProps> = ({ stage, onToggle }) => {
    const icon = statusIcons[stage.status] ?? statusIcons['not-started'];
    const [elapsed, setElapsed] = useState(0);

    useEffect(() => {
        if (stage.status !== 'running' || stage.startTimestamp <= 0) {
            setElapsed(0);
            return;
        }
        const tick = () => setElapsed(Date.now() / 1000 - stage.startTimestamp);
        tick();
        const id = setInterval(tick, 100);
        return () => clearInterval(id);
    }, [stage.status, stage.startTimestamp]);

    const displayDuration = stage.status === 'running'
        ? formatDuration(elapsed * 1000)
        : formatDuration(stage.durationMs);

    return (
        <div>
            <div
                onClick={onToggle}
                style={{
                    display: 'flex',
                    alignItems: 'center',
                    padding: '4px 12px',
                    cursor: 'pointer',
                    gap: 8,
                    userSelect: 'none',
                }}
                role="button"
                aria-expanded={stage.expanded}
            >
                <span style={{ color: icon.color, width: 16, textAlign: 'center' }}>
                    {icon.char}
                </span>
                <span style={{ flex: 1 }}>{stage.name}</span>
                <span style={{ fontFamily: 'monospace', color: '#888', minWidth: 50, textAlign: 'right' }}>
                    {displayDuration}
                </span>
            </div>
            {stage.expanded && <StageDetail logLines={stage.logLines} />}
        </div>
    );
};
