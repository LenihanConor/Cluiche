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
});
