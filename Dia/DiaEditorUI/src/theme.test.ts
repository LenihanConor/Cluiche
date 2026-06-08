import { describe, it, expect, beforeEach } from 'vitest';
import { theme, inputStyle, buttonStyle, injectThemeVars } from './theme';

describe('theme', () => {
  it('has all 13 colour keys', () => {
    const keys = Object.keys(theme);
    expect(keys).toHaveLength(13);
    expect(keys).toEqual(expect.arrayContaining([
      'bg', 'bgPanel', 'bgInput', 'border', 'borderMuted',
      'text', 'textMuted', 'textDim', 'accent', 'accentHover',
      'success', 'warning', 'error',
    ]));
  });
});

describe('inputStyle', () => {
  it('returns background equal to theme.bgInput', () => {
    const style = inputStyle();
    expect(style.background).toBe(theme.bgInput);
  });

  it('merges overrides last so color can be overridden', () => {
    const style = inputStyle({ color: 'red' });
    expect(style.color).toBe('red');
  });
});

describe('buttonStyle', () => {
  it('default variant has background equal to theme.bgPanel', () => {
    const style = buttonStyle('default');
    expect(style.background).toBe(theme.bgPanel);
  });

  it('primary variant has background equal to theme.accent', () => {
    const style = buttonStyle('primary');
    expect(style.background).toBe(theme.accent);
  });

  it('ghost variant has background: transparent', () => {
    const style = buttonStyle('ghost');
    expect(style.background).toBe('transparent');
  });

  it('defaults to default variant when no variant provided', () => {
    const style = buttonStyle();
    expect(style.background).toBe(theme.bgPanel);
  });
});

describe('injectThemeVars', () => {
  beforeEach(() => {
    // Remove the idempotency marker so each test starts fresh
    document.documentElement.removeAttribute('data-dia-theme-injected');
    // Clear any previously set CSS custom properties
    document.documentElement.style.removeProperty('--dia-bg');
  });

  it('sets --dia-bg to theme.bg (#1e1e1e)', () => {
    injectThemeVars();
    expect(
      document.documentElement.style.getPropertyValue('--dia-bg'),
    ).toBe(theme.bg);
  });

  it('is idempotent — calling twice does not throw and keeps the value', () => {
    injectThemeVars();
    injectThemeVars();
    expect(
      document.documentElement.style.getPropertyValue('--dia-bg'),
    ).toBe(theme.bg);
  });
});
