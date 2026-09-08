import React from 'react';

export interface SparklineProps {
    history: number[];
    width?: number;
    height?: number;
}

export const Sparkline: React.FC<SparklineProps> = ({ history, width = 120, height = 32 }) => {
    if (history.length < 2) {
        return (
            <svg
                data-testid="sparkline"
                width={width}
                height={height}
                viewBox={`0 0 ${width} ${height}`}
            />
        );
    }

    const min = Math.min(...history);
    const max = Math.max(...history);
    const range = max - min;

    const points = history.map((v, i) => {
        const x = (i / (history.length - 1)) * width;
        const y = range > 0
            ? height - ((v - min) / range) * height
            : height / 2;
        return `${x},${y}`;
    });

    return (
        <svg
            data-testid="sparkline"
            width={width}
            height={height}
            viewBox={`0 0 ${width} ${height}`}
        >
            <polyline
                data-testid="sparkline-line"
                points={points.join(' ')}
                fill="none"
                stroke="#0e639c"
                strokeWidth={1.5}
            />
        </svg>
    );
};
