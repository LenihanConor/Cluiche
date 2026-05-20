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

const PULSE_STATES: TLState[] = ['amber', 'green'];

if (typeof document !== 'undefined') {
    const styleId = 'tl-pulse-keyframe';
    if (!document.getElementById(styleId)) {
        const style = document.createElement('style');
        style.id = styleId;
        style.textContent = `@keyframes tlPulse { 0%,100%{box-shadow:0 0 0 0 currentColor;opacity:1} 50%{box-shadow:0 0 0 4px transparent;opacity:0.8} }`;
        document.head.appendChild(style);
    }
}

export const TrafficLightDot: React.FC<TrafficLightDotProps> = ({ state, size = 8, title }) => {
    const color = COLORS[state];
    const pulse = PULSE_STATES.includes(state);

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
                boxShadow: pulse ? `0 0 0 0 ${color}` : undefined,
                animation: pulse ? 'tlPulse 1.4s ease-in-out infinite' : undefined,
                flexShrink: 0,
            }}
        />
    );
};
