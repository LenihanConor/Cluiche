import type { FC } from 'react';
import type { LogLine, StepState } from '../state/types';
import { StepRow } from './StepRow';

const levelColors: Record<string, string> = {
    info: '#ccc',
    debug: '#888',
    warn: '#e8a838',
    error: '#e05050',
};

interface StageDetailProps {
    steps: StepState[];
    logLines: LogLine[];
}

export const StageDetail: FC<StageDetailProps> = ({ steps, logLines }) => {
    const hasContent = steps.length > 0 || logLines.length > 0;

    if (!hasContent) {
        return (
            <div style={{ padding: '2px 0 2px 28px', color: '#555', fontSize: 12 }}>
                No log output
            </div>
        );
    }

    return (
        <div style={{ padding: '0 0 4px 16px' }}>
            {steps.map(step => (
                <StepRow key={step.name} step={step} />
            ))}
            {logLines.map((line, i) => (
                <div
                    key={i}
                    style={{
                        display: 'flex',
                        gap: 6,
                        padding: '1px 0 1px 12px',
                        fontSize: 12,
                        fontFamily: 'monospace',
                        color: levelColors[line.level] ?? '#ccc',
                        borderLeft: '2px solid #333',
                        cursor: 'pointer',
                    }}
                    onClick={() => navigator.clipboard?.writeText(line.message)}
                    title="Click to copy"
                >
                    <span style={{ color: '#555', minWidth: 40, textAlign: 'right' }}>
                        [{line.level}]
                    </span>
                    <span style={{ wordBreak: 'break-all' }}>{line.message}</span>
                </div>
            ))}
        </div>
    );
};
