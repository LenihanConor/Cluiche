import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { TrafficLightDot } from './TrafficLightDot';

describe('TrafficLightDot', () => {
  it('renders dot with correct data-state', () => {
    render(<TrafficLightDot state="grey" />);
    const dot = screen.getByTestId('traffic-light-dot');
    expect(dot).toBeTruthy();
    expect(dot.getAttribute('data-state')).toBe('grey');
  });

  it('renders all states correctly', () => {
    const states = ['grey', 'amber', 'green', 'red'] as const;
    states.forEach((state) => {
      const { unmount } = render(<TrafficLightDot state={state} />);
      const dot = screen.getByTestId('traffic-light-dot');
      expect(dot.getAttribute('data-state')).toBe(state);
      unmount();
    });
  });

  it('size prop changes dimensions', () => {
    const { container } = render(<TrafficLightDot state="green" size={16} />);
    const dot = container.querySelector('[data-testid="traffic-light-dot"]') as HTMLElement;
    expect(dot.style.width).toBe('16px');
    expect(dot.style.height).toBe('16px');
  });

  it('default size is 8px', () => {
    const { container } = render(<TrafficLightDot state="green" />);
    const dot = container.querySelector('[data-testid="traffic-light-dot"]') as HTMLElement;
    expect(dot.style.width).toBe('8px');
    expect(dot.style.height).toBe('8px');
  });

  it('label renders beside dot when provided', () => {
    render(<TrafficLightDot state="amber" label="Warning" />);
    expect(screen.getByText('Warning')).toBeTruthy();
    const dot = screen.getByTestId('traffic-light-dot');
    expect(dot).toBeTruthy();
  });

  it('no label renders just the span, not a div wrapper', () => {
    const { container } = render(<TrafficLightDot state="red" />);
    const dot = screen.getByTestId('traffic-light-dot');
    // When no label, the dot should be a span, not wrapped in a div
    expect(dot.tagName).toBe('SPAN');
    // The dot should be a direct child of the container (or body), not inside a div
    const parentDiv = container.querySelector('div');
    expect(parentDiv).toBeFalsy();
  });

  it('with label, renders dot and label in flex container', () => {
    const { container } = render(<TrafficLightDot state="green" label="Connected" />);
    const divWrapper = container.querySelector('div') as HTMLElement;
    expect(divWrapper).toBeTruthy();
    expect(divWrapper.style.display).toBe('flex');
    expect(divWrapper.style.alignItems).toBe('center');
    expect(divWrapper.style.gap).toBe('5px');
  });

  it('title prop sets the HTML title attribute on the dot span', () => {
    render(<TrafficLightDot state="green" title="Service online" />);
    const dot = screen.getByTestId('traffic-light-dot');
    expect(dot.getAttribute('title')).toBe('Service online');
  });

  it('title prop absent — dot has no title attribute or empty string', () => {
    render(<TrafficLightDot state="grey" />);
    const dot = screen.getByTestId('traffic-light-dot');
    const title = dot.getAttribute('title');
    // Either null (attribute absent) or empty string are both acceptable
    expect(!title).toBe(true);
  });
});
