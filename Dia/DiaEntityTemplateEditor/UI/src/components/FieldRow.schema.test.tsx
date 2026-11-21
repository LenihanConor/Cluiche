// DiaValueSchema Feature 7 task 9a: FieldRow adopts ValueLabel / ValueTooltip.
// Uses the real @dia/editor-ui (no mock) so the DOM contract is exercised end to end.
import { describe, it, expect } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { componentFieldToValueInfo } from '@dia/editor-ui';
import entriesJson from '../../../../../Cluiche/Tests/Contracts/valuetooltip/entries.json';
import expectedJson from '../../../../../Cluiche/Tests/Contracts/valuetooltip/expected.json';
import { FieldRow } from './FieldRow';
import type { FieldEntry } from '../types';

const COMP = 'TransformComponent';

// Captured from the pre-change FieldRow for an undescribed field.
const UNDESCRIBED_HTML =
    '<div style="display: flex; align-items: center; padding: 3px 8px; gap: 4px;" data-testid="field-row-speed"><span style="color: rgb(136, 136, 136); font-size: 12px; min-width: 100px; flex-shrink: 0; padding-right: 8px; user-select: none;">speed</span><input style="background: rgb(45, 45, 45); color: rgb(212, 212, 212); border: 1px solid rgb(78, 201, 160); border-radius: 3px; padding: 3px 6px; font-size: 12px; outline: none; font-family: &quot;Segoe UI&quot;, system-ui, sans-serif; flex: 1 1 0%; min-width: 0;" type="number" placeholder="1" data-testid="field-input-speed" value="3"><button style="background: transparent; color: rgb(136, 136, 136); cursor: pointer; font-size: 12px; padding: 0px 4px; line-height: 1; flex-shrink: 0;" data-testid="field-clear-speed" aria-label="Clear speed">✕</button></div>';

const entries = entriesJson.entries as unknown as Array<{ id: string; entry: Record<string, unknown> }>;
const expected = expectedJson.cases as unknown as Record<string, { sections: Array<{ section: string; items?: Array<{ kind: string; text: string }>; text?: string }> }>;

// Map a fixture entry into the .diaschema field shape (name/kind/label/description/unit/hint/min/max/default).
function toSchemaField(id: string): FieldEntry {
    const e = entries.find((x) => x.id === id)!.entry;
    const f: Record<string, unknown> = { name: e.key, kind: 'primitive' };
    for (const k of ['label', 'description', 'unit', 'hint', 'min', 'max', 'default']) {
        if (e[k] !== undefined) f[k] = e[k];
    }
    return f as unknown as FieldEntry;
}

function renderRow(field: FieldEntry) {
    return render(<FieldRow componentType={COMP} field={field} onChange={() => {}} />);
}

describe('FieldRow schema labels', () => {
    it('undescribed field renders exactly as before', () => {
        const { container } = renderRow({ name: 'speed', kind: 'primitive', value: 3, codeDefault: 1 });
        expect(container.innerHTML).toBe(UNDESCRIBED_HTML);
    });

    it('old-format entry (name/kind only) shows raw name and no icon', () => {
        renderRow({ name: 'speed', kind: 'primitive' });
        expect(screen.getByTestId('field-row-speed')).toHaveTextContent('speed');
        expect(screen.queryByRole('button', { name: /About/ })).toBeNull();
    });

    it('described field shows label, unit and an (i) button', () => {
        renderRow({ name: 'speed', kind: 'primitive', label: 'Move speed', description: 'How fast.', unit: 'm/s' });
        const row = screen.getByTestId('field-row-speed');
        expect(row).toHaveTextContent('Move speed');
        expect(row).toHaveTextContent('m/s');
        expect(screen.getByRole('button', { name: 'About Move speed' })).toBeInTheDocument();
        expect(screen.getByTestId('field-input-speed')).toBeInTheDocument();
    });

    it('tooltip shows description, range, default and hint (inspector) on hover', () => {
        renderRow({
            name: 'speed', kind: 'primitive', label: 'Move speed', description: 'How fast.',
            hint: 'Keep it low.', min: 0, max: 10, default: 2,
        });
        expect(document.querySelector('[data-vt-tooltip]')).toBeNull();
        fireEvent.mouseEnter(screen.getByRole('button', { name: 'About Move speed' }));
        const tip = document.querySelector('[data-vt-tooltip]')!;
        expect(tip).not.toBeNull();
        expect(tip).toHaveTextContent('How fast.');
        expect(tip.querySelector('[data-vt-kind="range"]')).not.toBeNull();
        expect(tip.querySelector('[data-vt-kind="default"]')).toHaveTextContent('2');
        expect(tip).toHaveTextContent('Keep it low.');
    });

    it('tooltip opens on keyboard focus', () => {
        renderRow({ name: 'speed', kind: 'primitive', label: 'Move speed', description: 'How fast.' });
        fireEvent.focus(screen.getByRole('button', { name: 'About Move speed' }));
        expect(document.querySelector('[data-vt-tooltip]')).not.toBeNull();
    });

    for (const id of ['numeric_no_default', 'default_zero']) {
        it(`tooltip content matches shared fixture case ${id}`, () => {
            const field = toSchemaField(id);
            // Sanity: the adapter accepts this shape.
            expect(componentFieldToValueInfo(field as never, COMP)).not.toBeNull();
            renderRow(field);
            fireEvent.mouseEnter(screen.getByRole('button', { name: /About/ }));
            const tip = document.querySelector('[data-vt-tooltip]')!;
            const meta = Array.from(tip.querySelectorAll(':scope > [data-vt-section="meta"] [data-vt-kind]')).map((i) => ({
                kind: i.getAttribute('data-vt-kind'),
                text: i.querySelector('[data-vt-value]')?.textContent ?? '',
            }));
            const exp = expected[id].sections;
            expect(tip.querySelector(':scope > [data-vt-section="title"]')?.textContent).toBe(exp.find((s) => s.section === 'title')!.text);
            expect(tip.querySelector(':scope > [data-vt-section="description"]')?.textContent).toBe(exp.find((s) => s.section === 'description')!.text);
            expect(meta).toEqual(exp.find((s) => s.section === 'meta')!.items);
        });
    }
});
