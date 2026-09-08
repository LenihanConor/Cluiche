import React from 'react';
import { theme } from '@dia/editor-ui';

export interface ResourceFillBarProps {
    value: number;
    max: number;
    type: 'base' | 'derived';
}

function getFillColor(type: 'base' | 'derived', value: number, max: number): string {
    if (type === 'derived') {
        return '#9b59b6'; // purple for derived
    }
    if (value <= 0) {
        return theme.error;
    }
    if (max > 0 && value >= max) {
        return theme.warning;
    }
    return theme.accent;
}

export const ResourceFillBar: React.FC<ResourceFillBarProps> = ({ value, max, type }) => {
    const fillPercent = max > 0 ? Math.min(100, (value / max) * 100) : 0;
    const fillColor = getFillColor(type, value, max);

    return (
        <div
            data-testid="fill-bar"
            style={{
                width: '100%',
                height: 8,
                background: '#333',
                borderRadius: 4,
                overflow: 'hidden',
            }}
        >
            <div
                data-testid="fill-bar-inner"
                style={{
                    width: `${fillPercent}%`,
                    height: '100%',
                    background: fillColor,
                    borderRadius: 4,
                    transition: 'width 0.1s ease',
                }}
            />
        </div>
    );
};
