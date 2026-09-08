import type { CSSProperties } from 'react';

export const theme = {
  bg:          '#1e1e1e',
  bgPanel:     '#252526',
  bgInput:     '#2d2d2d',
  border:      '#3c3c3c',
  borderMuted: '#555',
  text:        '#d4d4d4',
  textMuted:   '#888',
  textDim:     '#ccc',
  accent:      '#0e639c',
  accentHover: '#007acc',
  success:     '#89d185',
  warning:     '#cca700',
  error:       '#f48771',
} as const;

export type Theme = typeof theme;

const MARKER_ATTR = 'data-dia-theme-injected';

export function injectThemeVars(): void {
  if (document.documentElement.hasAttribute(MARKER_ATTR)) {
    return;
  }

  const root = document.documentElement;

  root.style.setProperty('--dia-bg',           theme.bg);
  root.style.setProperty('--dia-bg-panel',     theme.bgPanel);
  root.style.setProperty('--dia-bg-input',     theme.bgInput);
  root.style.setProperty('--dia-border',       theme.border);
  root.style.setProperty('--dia-border-muted', theme.borderMuted);
  root.style.setProperty('--dia-text',         theme.text);
  root.style.setProperty('--dia-text-muted',   theme.textMuted);
  root.style.setProperty('--dia-text-dim',     theme.textDim);
  root.style.setProperty('--dia-accent',       theme.accent);
  root.style.setProperty('--dia-accent-hover', theme.accentHover);
  root.style.setProperty('--dia-success',      theme.success);
  root.style.setProperty('--dia-warning',      theme.warning);
  root.style.setProperty('--dia-error',        theme.error);

  root.style.setProperty('font-family', "'Segoe UI', system-ui, sans-serif");
  root.style.setProperty('font-size',   '12px');

  root.setAttribute(MARKER_ATTR, 'true');
}

const FONT_FAMILY = "'Segoe UI', system-ui, sans-serif";

export function inputStyle(overrides?: CSSProperties): CSSProperties {
  return {
    background:  theme.bgInput,
    color:       theme.text,
    border:      `1px solid ${theme.border}`,
    borderRadius: 3,
    padding:     '3px 6px',
    fontSize:    12,
    outline:     'none',
    fontFamily:  FONT_FAMILY,
    ...overrides,
  };
}

export function buttonStyle(
  variant: 'default' | 'primary' | 'ghost' = 'default',
  overrides?: CSSProperties,
): CSSProperties {
  const base: CSSProperties = {
    borderRadius: 3,
    padding:     '3px 8px',
    fontSize:    12,
    cursor:      'pointer',
    fontFamily:  FONT_FAMILY,
  };

  let variantStyle: CSSProperties;

  switch (variant) {
    case 'primary':
      variantStyle = {
        background: theme.accent,
        color:      '#fff',
        border:     `1px solid ${theme.accent}`,
      };
      break;
    case 'ghost':
      variantStyle = {
        background: 'transparent',
        border:     '1px solid transparent',
        color:      theme.textMuted,
      };
      break;
    default:
      variantStyle = {
        background: theme.bgPanel,
        color:      theme.text,
        border:     `1px solid ${theme.border}`,
      };
      break;
  }

  return { ...base, ...variantStyle, ...overrides };
}
