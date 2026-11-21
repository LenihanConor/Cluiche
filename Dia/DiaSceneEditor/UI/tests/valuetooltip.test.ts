// DiaValueSchema Feature 7 task 9b: Scene Editor property rows adopt labels + tooltips.
// Covers: renderValueTooltip against the shared golden fixture, propRow / renderFieldsGrouped for
// described vs undescribed fields (undescribed output pinned to baseline.json, captured BEFORE the
// change), (i) icon hover/focus/Esc, old-format components, and data-to-entry mapping.
// NOTE: jsdom, not CEF: proves page logic only.
import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadScene } from './loadScene';

const here = path.dirname(fileURLToPath(import.meta.url));
const CONTRACTS = path.resolve(here, '../../../../Cluiche/Tests/Contracts');
const readJson = (p: string): any => JSON.parse(fs.readFileSync(p, 'utf-8'));
const fixtureEntries = readJson(path.join(CONTRACTS, 'valuetooltip/entries.json')).entries as any[];
const fixtureExpected = readJson(path.join(CONTRACTS, 'valuetooltip/expected.json')).cases as Record<string, any>;
const baseline = readJson(path.join(here, 'baseline.json'));

const txt = (el: Element | null) => el?.textContent ?? '';

// DOM -> expected.json shape (same extraction as Dia/DiaEditorUI/src/ValueTooltip.test.tsx).
function extractSections(root: Element): any[] {
  const out: any[] = [];
  root.querySelectorAll(':scope > [data-vt-section]').forEach((el) => {
    const name = el.getAttribute('data-vt-section')!;
    switch (name) {
      case 'enum_values':
        out.push({ section: name, values: Array.from(el.querySelectorAll('[data-vt-enum-id]')).map((v) => ({
          id: v.getAttribute('data-vt-enum-id'),
          label: txt(v.querySelector('[data-vt-enum-label]')),
          description: txt(v.querySelector('[data-vt-enum-desc]')) })) });
        break;
      case 'meta':
        out.push({ section: name, items: Array.from(el.querySelectorAll('[data-vt-kind]')).map((i) => ({
          kind: i.getAttribute('data-vt-kind'), text: txt(i.querySelector('[data-vt-value]')) })) });
        break;
      case 'affects':
        out.push({ section: name, chips: Array.from(el.querySelectorAll('[data-vt-chip]')).map((c) => c.getAttribute('data-vt-chip')) });
        break;
      case 'related':
        out.push({ section: name, links: Array.from(el.querySelectorAll('[data-vt-key]')).map((l) => ({
          key: l.getAttribute('data-vt-key'), link: l.getAttribute('data-vt-link') === 'true' })) });
        break;
      case 'source':
        out.push({ section: name, source: txt(el.querySelector('[data-vt-source]')), defined_in: txt(el.querySelector('[data-vt-defined-in]')) });
        break;
      default:
        out.push({ section: name, text: txt(el) });
    }
  });
  return out;
}

let page: ReturnType<typeof loadScene>;
beforeEach(() => { page = loadScene(); });
afterEach(() => { page.win.setFieldInfo?.({}); });

describe('renderValueTooltip vs shared golden fixture', () => {
  for (const c of fixtureEntries) {
    it(c.id, () => {
      const root = page.win.renderValueTooltip(c.entry, { inspector: c.options.inspector, packId: c.options.packId, resolvable: c.options.resolvable, forId: c.for });
      expect(extractSections(root)).toEqual(fixtureExpected[c.id].sections);
    });
  }
});

const described = { name: 'max_hp', label: 'Max HP', description: 'Hit points at full health.', unit: 'hp', hint: 'Keep above 0.', min: 1, max: 999, default: 50 };
const infoFor = (f: any) => page.win.fieldEntry('Health', f);

describe('undescribed fields render exactly as before', () => {
  it('propRow variants are byte-identical to baseline', () => {
    expect(page.win.propRow('Speed', 'float', 2, true, false)).toBe(baseline.propRowOverride);
    expect(page.win.propRow('ID', 'string', 'ent_1', false, true)).toBe(baseline.propRowReadonly);
    expect(page.win.propRow('Pos', 'vec2', [1, 2], false, false)).toBe(baseline.propRowVec);
  });
  const grouped = () => page.win.renderFieldsGrouped({ 'Health.max_hp': 50 }, { 'Health.max_hp': 50 },
    { 'Health.max_hp': 50, 'Health.invulnerable': false, 'Transform2D.position': [0, 0] }, false);
  it('renderFieldsGrouped is byte-identical to baseline', () => {
    expect(grouped()).toBe(baseline.grouped);
    expect(page.win.renderFieldsGrouped({ 'Health.max_hp': 50 }, {}, {}, true)).toBe(baseline.groupedReadonly);
  });
  it('old-format components (name/overridden/value/kind only) produce no info and no icon', () => {
    const map = page.win.collectFieldInfo({ components: [{ type: 'Health', fields: [{ name: 'max_hp', value: 50, overridden: false, kind: 'primitive' }] }] });
    expect(Object.keys(map)).toEqual([]);
    page.win.setFieldInfo(map);
    expect(grouped()).toBe(baseline.grouped);
  });
  it('a field with label/unit but NO description is still plain', () => {
    const map = page.win.collectFieldInfo({ components: [{ type: 'Health', fields: [{ name: 'max_hp', value: 50, label: 'Max HP', unit: 'hp' }] }] });
    expect(Object.keys(map)).toEqual([]);
  });
});

describe('described field', () => {
  it('propRow shows label, unit and (i) icon', () => {
    const html = page.win.propRow('max_hp', 'int', 50, false, false, infoFor(described));
    const host = page.doc.createElement('div'); host.innerHTML = html;
    expect(txt(host.querySelector('.prop-label-text'))).toBe('Max HP');
    expect(txt(host.querySelector('.prop-unit'))).toBe('hp');
    expect(host.querySelector('.info-icon')).not.toBeNull();
    expect(host.querySelector('.info-icon')!.getAttribute('tabindex')).toBe('0');
  });
  it('renderFieldsGrouped uses collected component info', () => {
    const map = page.win.collectFieldInfo({ components: [{ type: 'Health', fields: [{ ...described, value: 50, overridden: false }] }] });
    expect(Object.keys(map)).toEqual(['Health.max_hp']);
    page.win.setFieldInfo(map);
    const host = page.doc.createElement('div');
    host.innerHTML = page.win.renderFieldsGrouped({}, {}, { 'Health.max_hp': 50, 'Health.armor': 0 }, false);
    const rows = Array.from(host.querySelectorAll('.prop-row'));
    const r0 = rows.find((r) => r.getAttribute('data-field-key') === 'Health.max_hp')!;
    const r1 = rows.find((r) => r.getAttribute('data-field-key') === 'Health.armor')!;
    expect(txt(r0.querySelector('.prop-label-text'))).toBe('Max HP');
    expect(r0.querySelector('.info-icon')).not.toBeNull();
    expect(r1.querySelector('.info-icon')).toBeNull();
    expect(txt(r1.querySelector('.prop-label'))).toBe('armor');
  });
  it('tooltip opens on hover and focus, closes on Esc; content matches renderValueTooltip (AC 12)', () => {
    const entry = infoFor(described);
    page.win.setFieldInfo({ 'Health.max_hp': entry });
    const host = page.doc.createElement('div'); host.innerHTML = page.win.propRow('max_hp', 'int', 50, false, false, entry);
    page.doc.body.appendChild(host);
    const icon = host.querySelector('.info-icon')!;
    icon.dispatchEvent(new page.win.MouseEvent('mouseenter'));
    const tip = page.doc.querySelector('[data-vt-tooltip]')!;
    expect(tip).not.toBeNull();
    const expectedTip = page.win.renderValueTooltip(entry, { inspector: true, forId: 'Health', resolvable: [] });
    expect(extractSections(tip)).toEqual(extractSections(expectedTip));
    expect(extractSections(tip).map((s) => s.section)).toEqual(['title', 'description', 'hint', 'meta']);
    page.doc.dispatchEvent(new page.win.KeyboardEvent('keydown', { key: 'Escape' }));
    expect(page.doc.querySelector('[data-vt-tooltip]')).toBeNull();
    icon.dispatchEvent(new page.win.FocusEvent('focus'));
    expect(page.doc.querySelector('[data-vt-tooltip]')).not.toBeNull();
  });
  it('escapes label, unit and description', () => {
    const e = page.win.fieldEntry('Health', { ...described, label: '<b>x</b>', unit: '<i>' });
    const html = page.win.propRow('max_hp', 'int', 50, false, false, e);
    expect(html).not.toContain('<b>x</b>');
    expect(html).not.toContain('<i>');
  });
});
