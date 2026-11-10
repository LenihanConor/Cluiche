import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { ValueLabel } from './ValueLabel';
import type { ValueInfoLike } from './valueInfo';

const entry: ValueInfoLike = {
  key: 'grazing.rate',
  label: 'Grazing rate',
  description: 'How much herbivores eat.',
  unit: 'x per season',
  related: ['regrowth.forest', 'other'],
};

describe('ValueLabel', () => {
  it('renders label, unit and an (i) button', () => {
    render(<ValueLabel entry={entry} forId="cow.p" />);
    expect(screen.getByText('Grazing rate')).toBeTruthy();
    expect(screen.getByText('x per season')).toBeTruthy();
    expect(screen.getByRole('button', { name: 'About Grazing rate' })).toBeTruthy();
    expect(screen.queryByRole('tooltip')).toBeNull();
  });

  it('hover shows the tooltip and mouseleave closes it', () => {
    const { container } = render(<ValueLabel entry={entry} forId="cow.p" />);
    const icon = screen.getByRole('button', { name: 'About Grazing rate' });
    fireEvent.mouseEnter(icon);
    expect(screen.getByRole('tooltip')).toBeTruthy();
    fireEvent.mouseLeave(container.querySelector('[data-vt-label]')!);
    expect(screen.queryByRole('tooltip')).toBeNull();
  });

  it('keyboard focus shows the tooltip, Esc closes it, blur closes it', () => {
    render(<ValueLabel entry={entry} forId="cow.p" />);
    const icon = screen.getByRole('button', { name: 'About Grazing rate' });
    fireEvent.focus(icon);
    expect(screen.getByRole('tooltip')).toBeTruthy();
    fireEvent.keyDown(icon, { key: 'Escape' });
    expect(screen.queryByRole('tooltip')).toBeNull();
    fireEvent.blur(icon);
    fireEvent.focus(icon);
    expect(screen.getByRole('tooltip')).toBeTruthy();
    fireEvent.blur(icon);
    expect(screen.queryByRole('tooltip')).toBeNull();
  });

  it('shows pack marker when packId is set', () => {
    render(<ValueLabel entry={entry} forId="cow.p" packId="lush" />);
    expect(screen.getByText('pack: lush')).toBeTruthy();
  });

  it('omits pack marker without packId', () => {
    render(<ValueLabel entry={entry} forId="cow.p" />);
    expect(screen.queryByText(/pack:/)).toBeNull();
  });

  it('tooltip link click calls onFocusKey only for resolvable keys', () => {
    const onFocusKey = vi.fn();
    render(<ValueLabel entry={entry} forId="cow.p" resolvable={['regrowth.forest']} onFocusKey={onFocusKey} />);
    fireEvent.focus(screen.getByRole('button', { name: 'About Grazing rate' }));
    fireEvent.click(screen.getByText('regrowth.forest'));
    fireEvent.click(screen.getByText('other'));
    expect(onFocusKey).toHaveBeenCalledTimes(1);
    expect(onFocusKey).toHaveBeenCalledWith('regrowth.forest');
  });

  it('without an entry renders the plain fallback label and no icon', () => {
    const { container } = render(<ValueLabel entry={undefined} forId="cow.p" fallbackLabel="Plain" />);
    expect(container.textContent).toBe('Plain');
    expect(screen.queryByRole('button')).toBeNull();
  });

  it('has no class attributes', () => {
    const { container } = render(<ValueLabel entry={entry} forId="cow.p" packId="x" />);
    fireEvent.focus(screen.getByRole('button', { name: 'About Grazing rate' }));
    expect(container.querySelectorAll('[class]')).toHaveLength(0);
  });
});
