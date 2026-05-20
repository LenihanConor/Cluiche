import React from 'react';

export type TLState = 'grey' | 'amber' | 'green' | 'red';

interface TrafficLightDotProps {
    state: TLState;
    size?: number;  // diameter in px, default 8
    title?: string;
}

const COLORS: Record<TLState, string> = {
    grey:  '#666',
    amber: '#f0a030',
    green: '#3cb370',
    red:   '#c0392b',
};

export const TrafficLightDot: React.FC<TrafficLightDotProps> = ({ state, size = 8, title }) => {
    const color = COLORS[state];

    return (
        <span
            title={title}
            data-testid="traffic-light-dot"
            data-state={state}
            style={{
                display: 'inline-block',
                width: size,
                height: size,
                borderRadius: '50%',
                background: color,
                flexShrink: 0,
            }}
        />
    );
};
