import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import entriesJson from '../../../Cluiche/Tests/Contracts/valuetooltip/entries.json';
import expectedJson from '../../../Cluiche/Tests/Contracts/valuetooltip/expected.json';
import { ValueTooltip } from './ValueTooltip';
import { theme } from './theme';
import type { ValueInfoLike } from './valueInfo';

const entries = entriesJson.entries as unknown as Array<{
  id: string;
  for: string;
  entry: ValueInfoLike;
  options: { inspector: boolean; packId: string | null; resolvable: string[] };
}>;
const expected = expectedJson.cases as unknown as Record<string, { sections: unknown[] }>;

const txt = (el: Element | null) => el?.textContent ?? '';

// DOM -> expected.json shape, following the DOM contract documented in ValueTooltip.tsx.
export function extractSections(root: Element): unknown[] {
  const out: unknown[] = [];
  root.querySelectorAll(':scope > [data-vt-section]').forEach((el) => {
    const name = el.getAttribute('data-vt-section')!;
    switch (name) {
      case 'enum_values':
        out.push({
          section: name,
          values: Array.from(el.querySelectorAll('[data-vt-enum-id]')).map((v) => ({
            id: v.getAttribute('data-vt-enum-id'),
            label: txt(v.querySelector('[data-vt-enum-label]')),
            description: txt(v.querySelector('[data-vt-enum-desc]')),
          })),
        });
        break;
      case 'meta':
        out.push({
          section: name,
          items: Array.from(el.querySelectorAll('[data-vt-kind]')).map((i) => ({
            kind: i.getAttribute('data-vt-kind'),
            text: txt(i.querySelector('[data-vt-value]')),
          })),
        });
        break;
      case 'affects':
        out.push({
          section: name,
          chips: Array.from(el.querySelectorAll('[data-vt-chip]')).map((c) => c.getAttribute('data-vt-chip')),
        });
        break;
      case 'related':
        out.push({
          section: name,
          links: Array.from(el.querySelectorAll('[data-vt-key]')).map((l) => ({
            key: l.getAttribute('data-vt-key'),
            link: l.getAttribute('data-vt-link') === 'true',
          })),
        });
        break;
      case 'source':
        out.push({
          section: name,
          source: txt(el.querySelector('[data-vt-source]')),
          defined_in: txt(el.querySelector('[data-vt-defined-in]')),
        });
        break;
      default:
        out.push({ section: name, text: txt(el) });
    }
  });
  return out;
}

describe('ValueTooltip golden fixture', () => {
  it('fixture has cases', () => {
    expect(entries.length).toBeGreaterThan(5);
  });

  for (const c of entries) {
    it(`renders case ${c.id}`, () => {
      const { container } = render(
        <ValueTooltip
          entry={c.entry}
          forId={c.for}
          inspector={c.options.inspector}
          packId={c.options.packId}
          resolvable={c.options.resolvable}
        />,
      );
      const root = container.querySelector('[data-vt-tooltip]')!;
      expect(extractSections(root)).toEqual(expected[c.id].sections);
    });
  }
});

describe('ValueTooltip behaviour', () => {
  const base: ValueInfoLike = { key: 'k', label: 'Kay', description: 'Desc.' };

  it('minimal entry renders title and description only', () => {
    const { container } = render(<ValueTooltip entry={base} forId="f" />);
    expect(extractSections(container.querySelector('[data-vt-tooltip]')!)).toEqual([
      { section: 'title', text: 'Kay' },
      { section: 'description', text: 'Desc.' },
    ]);
  });

  it('related link calls onFocusKey only for resolvable keys', () => {
    const onFocusKey = vi.fn();
    render(
      <ValueTooltip entry={{ ...base, related: ['a', 'b'] }} forId="f" resolvable={new Set(['a'])} onFocusKey={onFocusKey} />,
    );
    fireEvent.click(screen.getByText('a'));
    expect(onFocusKey).toHaveBeenCalledWith('a');
    expect(screen.getByText('b').tagName).not.toBe('BUTTON');
    fireEvent.click(screen.getByText('b'));
    expect(onFocusKey).toHaveBeenCalledTimes(1);
  });

  it('uses only theme colours and no class attributes', () => {
    const { container } = render(
      <ValueTooltip
        entry={{ ...base, hint: 'h', unit: 'u', min: 0, affects: ['x'], related: ['r'], source: 's', values: [{ id: 'i', label: 'I', description: 'd' }] }}
        forId="f"
        inspector
        packId="p"
        resolvable={['r']}
      />,
    );
    expect(container.querySelectorAll('[class]')).toHaveLength(0);
    const allowed = new Set<string>(Object.values(theme).map((v) => v.toLowerCase()));
    const toHex = (s: string) => s.toLowerCase();
    container.querySelectorAll<HTMLElement>('*').forEach((el) => {
      const raw = el.getAttribute('style') ?? '';
      // jsdom normalises hex to rgb(); compare against rgb forms of theme colours too.
      const colours = raw.match(/#[0-9a-fA-F]{3,8}|rgb\([^)]*\)/g) ?? [];
      for (const col of colours) {
        const ok = allowed.has(toHex(col)) || Array.from(allowed).some((h) => hexToRgb(h) === col.replace(/\s/g, ''));
        expect(ok, `colour ${col} in ${raw}`).toBe(true);
      }
    });
  });
});

function hexToRgb(hex: string): string {
  let h = hex.replace('#', '');
  if (h.length === 3) h = h.split('').map((c) => c + c).join('');
  const n = parseInt(h, 16);
  return `rgb(${(n >> 16) & 255},${(n >> 8) & 255},${n & 255})`;
}
