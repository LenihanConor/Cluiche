// DiaValueSchema Feature 7 task 10: FieldsTab adopts ValueLabel / ValueTooltip.
// Uses the real @dia/editor-ui (no mock) so the DOM contract is exercised end to end.
import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent, act } from '@testing-library/react';
import entriesJson from '../../../../../Cluiche/Tests/Contracts/valuetooltip/entries.json';
import expectedJson from '../../../../../Cluiche/Tests/Contracts/valuetooltip/expected.json';
import { FieldsTab } from './FieldsTab';
import { SchemaCache } from '../schemaCache';
import type { EntityEntry } from '../types';

// Captured from the pre-change FieldsTab (undescribed field, no schema).
const UNDESCRIBED_HTML =
    '<div data-testid="component-header-Transform" style="background: rgb(37, 37, 38); padding: 5px 10px; display: flex; align-items: center; cursor: pointer; gap: 6px; user-select: none;"><span style="color: rgb(136, 136, 136); font-size: 9px; transition: transform 0.15s; transform: rotate(90deg); width: 10px; display: inline-block;">▶</span><span style="font-weight: 600; color: rgb(156, 220, 254); font-size: 12px;">Transform</span></div><div><table style="width: 100%; border-collapse: collapse;"><tbody><tr data-testid="field-row-Transform-speed"><td style="padding: 3px 10px; border-bottom: 1px solid rgb(85, 85, 85); width: 150px; color: rgb(136, 136, 136); font-size: 11px; vertical-align: middle;">speed</td><td style="padding: 3px 10px; border-bottom: 1px solid rgb(85, 85, 85); width: 70px; color: rgb(136, 136, 136); font-size: 10px; font-family: monospace; vertical-align: middle;">f32</td><td style="padding: 3px 10px; border-bottom: 1px solid rgb(85, 85, 85); font-family: monospace; font-size: 11px; color: rgb(144, 200, 144); vertical-align: middle;">3</td><td style="padding: 3px 10px; border-bottom: 1px solid rgb(85, 85, 85); vertical-align: middle; text-align: center; width: 28px;"><span data-testid="field-edit-btn-Transform-speed" title="Edit field" style="color: rgb(136, 136, 136); cursor: pointer; font-size: 11px; padding: 1px 4px; border-radius: 2px;">✎</span></td></tr></tbody></table></div>';

const TID = 'transform-component';
const PREFIX = `${TID}/`;

function entity(id: string | undefined, fieldName = 'speed'): EntityEntry {
    return {
        i: 0, g: 1, n: 'P', t: ['T'], d: 0,
        components: [{ name: 'Transform', id, fields: [{ n: fieldName, v: '3', t: 'float', dt: 'f32' }] }],
    };
}

function tab(e: EntityEntry, cache?: SchemaCache) {
    return <FieldsTab entity={e} allEntities={[e]} onNavigate={() => {}} onWriteField={() => {}} schemaCache={cache} />;
}

const SPEED = {
    for: TID, key: 'speed', kind: 'component_field', label: 'Move speed', description: 'How fast.',
    hint: 'Keep it low.', unit: 'm/s', min: 0, max: 10, default: 2,
};

describe('FieldsTab schema labels', () => {
    it('without a schema cache renders exactly as before', () => {
        render(tab(entity(TID)));
        expect(screen.getByTestId('component-section-Transform').innerHTML).toBe(UNDESCRIBED_HTML);
    });

    it('with a cache but no entry for the field renders exactly as before and requests the prefix once', () => {
        const request = vi.fn();
        const cache = new SchemaCache(request);
        const { rerender } = render(tab(entity(TID), cache));
        expect(screen.getByTestId('component-section-Transform').innerHTML).toBe(UNDESCRIBED_HTML);
        rerender(tab(entity(TID), cache));
        expect(request).toHaveBeenCalledTimes(1);
        expect(request).toHaveBeenCalledWith(PREFIX);
        act(() => cache.receive(PREFIX, true, []));
        expect(screen.getByTestId('component-section-Transform').innerHTML).toBe(UNDESCRIBED_HTML);
        expect(request).toHaveBeenCalledTimes(1);
    });

    it('a component without a type id is never queried', () => {
        const request = vi.fn();
        render(tab(entity(undefined), new SchemaCache(request)));
        expect(request).not.toHaveBeenCalled();
    });

    it('a failed request leaves rows as before and does not throw', () => {
        const cache = new SchemaCache(() => { throw new Error('offline'); });
        render(tab(entity(TID), cache));
        expect(screen.getByTestId('component-section-Transform').innerHTML).toBe(UNDESCRIBED_HTML);
    });

    it('described field shows label, unit and an (i) button once the schema arrives', () => {
        const cache = new SchemaCache(vi.fn());
        render(tab(entity(TID), cache));
        act(() => cache.receive(PREFIX, true, [SPEED]));
        const row = screen.getByTestId('field-row-Transform-speed');
        expect(row).toHaveTextContent('Move speed');
        expect(row).toHaveTextContent('m/s');
        expect(screen.getByRole('button', { name: 'About Move speed' })).toBeInTheDocument();
        expect(screen.getByTestId('field-edit-btn-Transform-speed')).toBeInTheDocument();
    });

    it('tooltip shows description, range, default and hint on hover and opens on focus', () => {
        const cache = new SchemaCache(vi.fn());
        render(tab(entity(TID), cache));
        act(() => cache.receive(PREFIX, true, [SPEED]));
        expect(document.querySelector('[data-vt-tooltip]')).toBeNull();
        const icon = screen.getByRole('button', { name: 'About Move speed' });
        fireEvent.mouseEnter(icon);
        const tip = document.querySelector('[data-vt-tooltip]')!;
        expect(tip).toHaveTextContent('How fast.');
        expect(tip.querySelector('[data-vt-kind="range"]')).not.toBeNull();
        expect(tip.querySelector('[data-vt-kind="default"]')).toHaveTextContent('2');
        expect(tip).toHaveTextContent('Keep it low.');
    });

    it('tooltip opens on keyboard focus', () => {
        const cache = new SchemaCache(vi.fn());
        render(tab(entity(TID), cache));
        act(() => cache.receive(PREFIX, true, [SPEED]));
        fireEvent.focus(screen.getByRole('button', { name: 'About Move speed' }));
        expect(document.querySelector('[data-vt-tooltip]')).not.toBeNull();
    });

    it('a field in the schema without a description stays a raw name', () => {
        const cache = new SchemaCache(vi.fn());
        render(tab(entity(TID), cache));
        act(() => cache.receive(PREFIX, true, [{ for: TID, key: 'speed', kind: 'component_field', label: 'Speed' }]));
        expect(screen.getByTestId('component-section-Transform').innerHTML).toBe(UNDESCRIBED_HTML);
    });
});

// ---- Shared golden fixture parity (Feature 3) ----
type Sec = { section: string; text?: string; items?: unknown; chips?: string[]; links?: unknown; source?: string; defined_in?: string };
const fixtureEntries = entriesJson.entries as unknown as Array<{ id: string; for: string; entry: Record<string, unknown>; options: { inspector: boolean } }>;
const expected = expectedJson.cases as unknown as Record<string, { sections: Sec[] }>;

function domToSections(tip: Element): Sec[] {
    const out: Sec[] = [];
    for (const el of Array.from(tip.children)) {
        const section = el.getAttribute('data-vt-section')!;
        if (section === 'title' || section === 'description' || section === 'hint') out.push({ section, text: el.textContent ?? '' });
        else if (section === 'meta') out.push({ section, items: Array.from(el.querySelectorAll('[data-vt-kind]')).map((i) => ({ kind: i.getAttribute('data-vt-kind'), text: i.querySelector('[data-vt-value]')?.textContent ?? '' })) });
        else if (section === 'affects') out.push({ section, chips: Array.from(el.querySelectorAll('[data-vt-chip]')).map((c) => c.getAttribute('data-vt-chip')!) });
        else if (section === 'related') out.push({ section, links: Array.from(el.querySelectorAll('[data-vt-key]')).map((k) => ({ key: k.getAttribute('data-vt-key'), link: k.getAttribute('data-vt-link') === 'true' })) });
        else if (section === 'source') out.push({ section, source: el.querySelector('[data-vt-source]')?.textContent ?? '', defined_in: el.querySelector('[data-vt-defined-in]')?.textContent ?? '' });
    }
    return out;
}

describe('FieldsTab tooltip matches the shared golden fixture', () => {
    // Inspector rows always pass inspector: true, so compare against the inspector-true case.
    it('parameter_inspector', () => {
        const c = fixtureEntries.find((x) => x.id === 'parameter_inspector')!;
        expect(c.options.inspector).toBe(true);
        const e: EntityEntry = {
            i: 0, g: 1, n: 'P', t: ['T'], d: 0,
            components: [{ name: 'Eco', id: c.for, fields: [{ n: String(c.entry.key), v: '1', t: 'float' }] }],
        };
        const cache = new SchemaCache(vi.fn());
        render(tab(e, cache));
        act(() => cache.receive(`${c.for}/`, true, [c.entry as never]));
        fireEvent.mouseEnter(screen.getByRole('button', { name: /About/ }));
        const tip = document.querySelector('[data-vt-tooltip]')!;
        expect(domToSections(tip)).toEqual(expected['parameter_inspector'].sections);
    });
});
