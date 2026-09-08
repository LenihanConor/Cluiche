import { render, screen } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import { EmptyState } from './EmptyState';

describe('EmptyState', () => {
  it('renders with just message', () => {
    render(<EmptyState message="No data available" />);
    expect(screen.getByText('No data available')).toBeInTheDocument();
    expect(screen.getByTestId('empty-state')).toBeInTheDocument();
  });

  it('renders hint when provided', () => {
    render(
      <EmptyState
        message="No data available"
        hint="Try refreshing the page"
      />
    );
    expect(screen.getByText('No data available')).toBeInTheDocument();
    expect(screen.getByText('Try refreshing the page')).toBeInTheDocument();
  });

  it('renders custom icon when provided', () => {
    render(
      <EmptyState
        message="No data available"
        icon="📦"
      />
    );
    expect(screen.getByText('📦')).toBeInTheDocument();
  });

  it('uses default icon when no icon prop', () => {
    render(<EmptyState message="No data available" />);
    const emptyState = screen.getByTestId('empty-state');
    expect(emptyState.textContent).toContain('⚙');
  });
});
