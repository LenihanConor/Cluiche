import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { TabBar } from './TabBar';
import type { Tab } from './TabBar';

const TABS: Tab[] = [
  { id: 'alpha', label: 'Alpha' },
  { id: 'beta',  label: 'Beta' },
  { id: 'gamma', label: 'Gamma' },
];

describe('TabBar', () => {
  it('renders all tab labels', () => {
    render(<TabBar tabs={TABS} activeTab="alpha" onTabChange={vi.fn()} />);
    expect(screen.getByText('Alpha')).toBeTruthy();
    expect(screen.getByText('Beta')).toBeTruthy();
    expect(screen.getByText('Gamma')).toBeTruthy();
  });

  it('active tab has aria-selected="true"', () => {
    render(<TabBar tabs={TABS} activeTab="beta" onTabChange={vi.fn()} />);
    const activeBtn = screen.getByRole('tab', { name: 'Beta' });
    expect(activeBtn.getAttribute('aria-selected')).toBe('true');
  });

  it('inactive tabs have aria-selected="false"', () => {
    render(<TabBar tabs={TABS} activeTab="beta" onTabChange={vi.fn()} />);
    expect(screen.getByRole('tab', { name: 'Alpha' }).getAttribute('aria-selected')).toBe('false');
    expect(screen.getByRole('tab', { name: 'Gamma' }).getAttribute('aria-selected')).toBe('false');
  });

  it('clicking inactive tab calls onTabChange with correct id', () => {
    const onTabChange = vi.fn();
    render(<TabBar tabs={TABS} activeTab="alpha" onTabChange={onTabChange} />);
    fireEvent.click(screen.getByRole('tab', { name: 'Beta' }));
    expect(onTabChange).toHaveBeenCalledWith('beta');
  });

  it('renders count as "Label (3)" when count is provided', () => {
    const tabs: Tab[] = [
      { id: 'errors', label: 'Errors', count: 3 },
      { id: 'info',   label: 'Info' },
    ];
    render(<TabBar tabs={tabs} activeTab="errors" onTabChange={vi.fn()} />);
    expect(screen.getByText('Errors (3)')).toBeTruthy();
    expect(screen.getByText('Info')).toBeTruthy();
  });

  it('data-tab-id is set on each button', () => {
    render(<TabBar tabs={TABS} activeTab="alpha" onTabChange={vi.fn()} />);
    const buttons = screen.getAllByRole('tab');
    expect(buttons[0].getAttribute('data-tab-id')).toBe('alpha');
    expect(buttons[1].getAttribute('data-tab-id')).toBe('beta');
    expect(buttons[2].getAttribute('data-tab-id')).toBe('gamma');
  });

  it('container has data-testid="tab-bar"', () => {
    render(<TabBar tabs={TABS} activeTab="alpha" onTabChange={vi.fn()} />);
    expect(screen.getByTestId('tab-bar')).toBeTruthy();
  });

  it('container has role="tablist"', () => {
    render(<TabBar tabs={TABS} activeTab="alpha" onTabChange={vi.fn()} />);
    expect(screen.getByRole('tablist')).toBeTruthy();
  });

  it('hover state: mouseEnter on inactive tab changes color; mouseLeave reverts', () => {
    const { container } = render(<TabBar tabs={TABS} activeTab="alpha" onTabChange={vi.fn()} />);
    const betaBtn = screen.getByRole('tab', { name: 'Beta' }) as HTMLElement;

    // Before hover, inactive tab should have muted color
    const colorBefore = betaBtn.style.color;

    fireEvent.mouseEnter(betaBtn);
    const colorHovered = betaBtn.style.color;

    fireEvent.mouseLeave(betaBtn);
    const colorAfterLeave = betaBtn.style.color;

    // Hovered color should differ from muted (pre-hover) color
    expect(colorHovered).not.toBe(colorBefore);
    // After mouse leave, color should revert to the muted value
    expect(colorAfterLeave).toBe(colorBefore);
    // Suppress unused variable warning
    void container;
  });

  it('active tab button has borderBottom containing "2px solid"', () => {
    render(<TabBar tabs={TABS} activeTab="beta" onTabChange={vi.fn()} />);
    const activeBtn = screen.getByRole('tab', { name: 'Beta' }) as HTMLElement;
    expect(activeBtn.style.borderBottom).toContain('2px solid');
  });

  it('count={0} renders as "Label (0)"', () => {
    const tabs: Tab[] = [{ id: 'zero', label: 'Zero', count: 0 }];
    render(<TabBar tabs={tabs} activeTab="zero" onTabChange={vi.fn()} />);
    expect(screen.getByText('Zero (0)')).toBeTruthy();
  });

  it('empty tabs=[] renders the container with no buttons', () => {
    render(<TabBar tabs={[]} activeTab="" onTabChange={vi.fn()} />);
    expect(screen.getByTestId('tab-bar')).toBeTruthy();
    expect(screen.queryAllByRole('tab')).toHaveLength(0);
  });
});
