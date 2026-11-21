// ValueTooltip: pure presentational tooltip for a ValueInfo-shaped entry (DiaValueSchema F3).
// Styling: inline styles from `theme` only (DEUI-001/002); no CSS files, no class names.
//
// DOM CONTRACT (shared with the debug-panel renderer; checked against the golden fixture
// Cluiche/Tests/Contracts/valuetooltip). Root has data-vt-tooltip. Each present section is a
// direct child, in fixture order, carrying data-vt-section="<name>":
//   title, description, hint, pack_badge : section node textContent is the text.
//   enum_values : children [data-vt-enum-id] each with [data-vt-enum-label] and [data-vt-enum-desc].
//   meta        : children [data-vt-kind="unit|range|default"], text in its [data-vt-value] child.
//   affects     : children [data-vt-chip="<key>"], textContent is the key.
//   related     : children [data-vt-key="<key>"][data-vt-link="true|false"] (button iff true).
//   source      : [data-vt-source] holds the source; [data-vt-defined-in] holds "<for> . <key>".
// Absent data means the section node is not rendered.
//
// Open point: placement is handled by ValueLabel (under the icon, flipping above near the
// viewport bottom); narrow-panel horizontal placement is not handled yet.
import React from 'react';
import { theme } from './theme';
import type { ResolvableKeys, ValueInfoLike } from './valueInfo';

export interface ValueTooltipProps {
  entry: ValueInfoLike;
  forId: string;
  inspector?: boolean;
  packId?: string | null;
  resolvable?: ResolvableKeys;
  onFocusKey?: (key: string) => void;
}

const FONT_FAMILY = "'Segoe UI', system-ui, sans-serif";
const isSet = (v: unknown): boolean => v !== undefined && v !== null;

export function isResolvable(resolvable: ResolvableKeys | undefined, key: string): boolean {
  if (!resolvable) return false;
  return resolvable instanceof Set ? resolvable.has(key) : (resolvable as ReadonlyArray<string>).includes(key);
}

export function formatRange(min?: number, max?: number): string | null {
  if (isSet(min) && isSet(max)) return `${String(min)} – ${String(max)}`;
  if (isSet(min)) return `≥ ${String(min)}`;
  if (isSet(max)) return `≤ ${String(max)}`;
  return null;
}

export function formatDefault(value: unknown, decimals?: number): string {
  if (typeof value === 'number' && decimals !== undefined) return value.toFixed(decimals);
  return String(value);
}

const section: React.CSSProperties = {
  marginTop: 6,
  paddingTop: 6,
  borderTop: `1px solid ${theme.border}`,
};

export const ValueTooltip: React.FC<ValueTooltipProps> = ({
  entry, forId, inspector, packId, resolvable, onFocusKey,
}) => {
  const meta: { kind: string; text: string }[] = [];
  if (entry.unit) meta.push({ kind: 'unit', text: entry.unit });
  const range = formatRange(entry.min, entry.max);
  if (range !== null) meta.push({ kind: 'range', text: range });
  if (isSet(entry.default)) meta.push({ kind: 'default', text: formatDefault(entry.default, entry.ui?.decimals) });

  return (
    <div
      role="tooltip"
      data-vt-tooltip=""
      style={{
        background: theme.bgPanel,
        border: `1px solid ${theme.borderMuted}`,
        borderRadius: 3,
        padding: '10px 12px',
        boxShadow: `0 6px 16px ${theme.bg}`,
        maxWidth: 320,
        boxSizing: 'border-box',
        fontSize: 11,
        fontFamily: FONT_FAMILY,
        color: theme.textDim,
        textAlign: 'left',
        fontWeight: 'normal',
      }}
    >
      <div data-vt-section="title" style={{ fontSize: 12, fontWeight: 600, color: theme.accentHover, marginBottom: 6 }}>
        {entry.label}
      </div>
      <div data-vt-section="description" style={{ color: theme.textDim, lineHeight: 1.4 }}>
        {entry.description}
      </div>

      {entry.values && entry.values.length > 0 && (
        <div data-vt-section="enum_values" style={{ ...section, display: 'flex', flexDirection: 'column', gap: 3 }}>
          {entry.values.map((v) => (
            <div key={v.id} data-vt-enum-id={v.id}>
              <span data-vt-enum-label="" style={{ color: theme.text, fontWeight: 600 }}>{v.label}</span>
              {' '}
              <span data-vt-enum-desc="" style={{ color: theme.textMuted }}>{v.description}</span>
            </div>
          ))}
        </div>
      )}

      {inspector && entry.hint && (
        <div data-vt-section="hint" style={{ ...section, color: theme.warning, fontStyle: 'italic' }}>
          {entry.hint}
        </div>
      )}

      {meta.length > 0 && (
        <div data-vt-section="meta" style={{ ...section, display: 'flex', flexWrap: 'wrap', gap: 8, fontSize: 10, color: theme.textMuted }}>
          {meta.map((m) => (
            <span key={m.kind} data-vt-kind={m.kind} style={{ display: 'flex', gap: 4 }}>
              <span style={{ color: theme.borderMuted }}>{m.kind}</span>
              <span data-vt-value="" style={{ color: theme.textDim }}>{m.text}</span>
            </span>
          ))}
        </div>
      )}

      {entry.affects && entry.affects.length > 0 && (
        <div data-vt-section="affects" style={{ display: 'flex', flexWrap: 'wrap', gap: 4, marginTop: 6 }}>
          {entry.affects.map((a) => (
            <span
              key={a}
              data-vt-chip={a}
              style={{
                fontSize: 9, padding: '2px 6px', background: theme.bgInput,
                border: `1px solid ${theme.border}`, color: theme.accentHover, borderRadius: 2,
              }}
            >
              {a}
            </span>
          ))}
        </div>
      )}

      {entry.related && entry.related.length > 0 && (
        <div data-vt-section="related" style={{ ...section, fontSize: 10, color: theme.textMuted }}>
          {entry.related.map((k) =>
            isResolvable(resolvable, k) ? (
              <button
                key={k}
                type="button"
                data-vt-key={k}
                data-vt-link="true"
                onClick={() => onFocusKey?.(k)}
                style={{
                  background: 'transparent', border: 'none', padding: 0, marginRight: 8,
                  color: theme.accentHover, cursor: 'pointer', fontSize: 10,
                  fontFamily: FONT_FAMILY, textDecoration: 'underline',
                }}
              >
                {k}
              </button>
            ) : (
              <span key={k} data-vt-key={k} data-vt-link="false" style={{ marginRight: 8 }}>{k}</span>
            ),
          )}
        </div>
      )}

      {entry.source && (
        <div data-vt-section="source" style={{ marginTop: 6, fontSize: 10, color: theme.textMuted, fontStyle: 'italic' }}>
          <span data-vt-source="">{entry.source}</span>
          {' · Defined in '}
          <span data-vt-defined-in="">{`${forId} · ${entry.key}`}</span>
        </div>
      )}

      {packId && (
        <div
          data-vt-section="pack_badge"
          style={{
            display: 'inline-block', marginTop: 6, fontSize: 9, padding: '2px 5px',
            background: theme.bgInput, border: `1px solid ${theme.warning}`,
            color: theme.warning, borderRadius: 2,
          }}
        >
          {`overridden by pack ${packId}`}
        </div>
      )}
    </div>
  );
};
