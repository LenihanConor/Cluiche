import React from 'react';

export type DotState = 'grey' | 'amber' | 'green' | 'red';

export interface TrafficLightDotProps {
  state: DotState;
  size?: number;    // diameter in px, default 8
  label?: string;   // optional text beside dot
  title?: string;   // HTML tooltip on the dot span
}

const COLORS: Record<DotState, string> = {
  grey:  '#666',
  amber: '#f0a030',
  green: '#3cb370',
  red:   '#c0392b',
};

export const TrafficLightDot: React.FC<TrafficLightDotProps> = ({ state, size = 8, label, title }) => {
  const color = COLORS[state];

  const dot = (
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

  if (label) {
    return (
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: 5,
        }}
      >
        {dot}
        <span>{label}</span>
      </div>
    );
  }

  return dot;
};
