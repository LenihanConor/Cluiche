// ValueLabel: label + unit + (i) icon that shows a ValueTooltip on hover or keyboard focus.
// Esc closes it. With no entry it renders the plain fallback label (no icon). Inline theme
// styles only (DEUI-001/002). Tooltip DOM contract: see ValueTooltip.tsx.
import React, { useEffect, useRef, useState } from 'react';
import { theme } from './theme';
import { ValueTooltip } from './ValueTooltip';
import type { ResolvableKeys, ValueInfoLike } from './valueInfo';

export interface ValueLabelProps {
  entry?: ValueInfoLike | null;
  forId: string;
  inspector?: boolean;
  packId?: string | null;
  resolvable?: ResolvableKeys;
  onFocusKey?: (key: string) => void;
  fallbackLabel?: string;
}

const FLIP_THRESHOLD_PX = 280;

export const ValueLabel: React.FC<ValueLabelProps> = ({
  entry, forId, inspector, packId, resolvable, onFocusKey, fallbackLabel,
}) => {
  const [hover, setHover] = useState(false);
  const [focus, setFocus] = useState(false);
  const [dismissed, setDismissed] = useState(false);
  const [above, setAbove] = useState(false);
  const iconRef = useRef<HTMLButtonElement>(null);
  const open = (hover || focus) && !dismissed;

  useEffect(() => {
    if (!open) return;
    const onKey = (e: KeyboardEvent) => { if (e.key === 'Escape') setDismissed(true); };
    document.addEventListener('keydown', onKey);
    return () => document.removeEventListener('keydown', onKey);
  }, [open]);

  if (!entry) return <span>{fallbackLabel ?? ''}</span>;

  // Flip above the icon when there is little room below and enough room above.
  const measure = () => {
    const r = iconRef.current?.getBoundingClientRect();
    if (r) setAbove(window.innerHeight - r.bottom < FLIP_THRESHOLD_PX && r.top > FLIP_THRESHOLD_PX);
  };

  return (
    <span
      data-vt-label=""
      style={{ position: 'relative', display: 'inline-flex', alignItems: 'center', gap: 4, color: theme.textDim }}
      onMouseLeave={() => { setHover(false); setDismissed(false); }}
    >
      <span>{entry.label}</span>
      {entry.unit && <span data-vt-unit="" style={{ color: theme.textMuted }}>{entry.unit}</span>}
      {packId && (
        <span
          data-vt-pack-marker=""
          style={{
            fontSize: 9, padding: '1px 4px', border: `1px solid ${theme.warning}`,
            color: theme.warning, borderRadius: 2,
          }}
        >
          {`pack: ${packId}`}
        </span>
      )}
      <button
        ref={iconRef}
        type="button"
        aria-label={`About ${entry.label}`}
        onMouseEnter={() => { measure(); setHover(true); }}
        onFocus={() => { measure(); setFocus(true); }}
        onBlur={() => { setFocus(false); setDismissed(false); }}
        style={{
          width: 12, height: 12, padding: 0, borderRadius: '50%',
          border: `1px solid ${open ? theme.accentHover : theme.border}`,
          background: 'transparent', color: open ? theme.accentHover : theme.textMuted,
          fontSize: 8, lineHeight: '10px', cursor: 'help',
          display: 'inline-flex', alignItems: 'center', justifyContent: 'center',
        }}
      >
        i
      </button>
      {open && (
        <span
          style={{
            position: 'absolute', left: 0, zIndex: 100, width: 320, maxWidth: '90vw',
            ...(above ? { bottom: 'calc(100% + 6px)' } : { top: 'calc(100% + 6px)' }),
          }}
        >
          <ValueTooltip
            entry={entry}
            forId={forId}
            inspector={inspector}
            packId={packId}
            resolvable={resolvable}
            onFocusKey={onFocusKey}
          />
        </span>
      )}
    </span>
  );
};
