import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { TrafficLightDot } from './TrafficLightDot';

describe('TrafficLightDot', () => {
    it('renders grey state', () => {
        render(<TrafficLightDot state="grey" />);
        const dot = screen.getByTestId('traffic-light-dot');
        expect(dot).toBeTruthy();
        expect(dot.getAttribute('data-state')).toBe('grey');
    });

    it('renders amber state', () => {
        render(<TrafficLightDot state="amber" />);
        const dot = screen.getByTestId('traffic-light-dot');
        expect(dot.getAttribute('data-state')).toBe('amber');
    });

    it('renders green state with pulse animation', () => {
        const { container } = render(<TrafficLightDot state="green" />);
        const dot = container.querySelector('[data-testid="traffic-light-dot"]') as HTMLElement;
        expect(dot.style.animation).toContain('tlPulse');
    });

    it('renders red state without pulse', () => {
        const { container } = render(<TrafficLightDot state="red" />);
        const dot = container.querySelector('[data-testid="traffic-light-dot"]') as HTMLElement;
        expect(dot.style.animation ?? '').toBe('');
    });

    it('applies custom size', () => {
        const { container } = render(<TrafficLightDot state="grey" size={12} />);
        const dot = container.querySelector('[data-testid="traffic-light-dot"]') as HTMLElement;
        expect(dot.style.width).toBe('12px');
    });

    it('shows title as tooltip', () => {
        render(<TrafficLightDot state="green" title="Connected" />);
        const dot = screen.getByTitle('Connected');
        expect(dot).toBeTruthy();
    });
});
