import { render, screen, fireEvent } from '@testing-library/react';
import { FieldRow } from './FieldRow';

vi.mock('@dia/editor-ui', () => ({
    theme: { textMuted: '#888', text: '#d4d4d4', border: '#3c3c3c' },
    inputStyle: (o?: object) => ({ ...o }),
}));

const COMP = 'TransformComponent';

function makeField(overrides: Partial<Parameters<typeof FieldRow>[0]['field']> = {}) {
    return {
        name: 'speed',
        kind: 'primitive',
        ...overrides,
    };
}

describe('FieldRow', () => {
    it('renders field name', () => {
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'speed' })}
                onChange={vi.fn()}
            />
        );
        expect(screen.getByTestId('field-row-speed')).toHaveTextContent('speed');
    });

    it('shows value when field.value is set', () => {
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'speed', value: 42 })}
                onChange={vi.fn()}
            />
        );
        const input = screen.getByTestId('field-input-speed') as HTMLInputElement;
        expect(input.value).toBe('42');
    });

    it('shows placeholder (codeDefault) when no value', () => {
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'speed', codeDefault: 10 })}
                onChange={vi.fn()}
            />
        );
        const input = screen.getByTestId('field-input-speed') as HTMLInputElement;
        expect(input.value).toBe('');
        expect(input.placeholder).toBe('10');
    });

    it('shows green border when value is overridden', () => {
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'speed', value: 99 })}
                onChange={vi.fn()}
            />
        );
        const input = screen.getByTestId('field-input-speed') as HTMLInputElement;
        // jsdom normalizes hex to rgb; both forms confirm the override color is applied
        const border = input.style.border;
        expect(border).toMatch(/#4ec9a0|rgb\(78,\s*201,\s*160\)/);
    });

    it('no clear button when no value', () => {
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'speed', codeDefault: 10 })}
                onChange={vi.fn()}
            />
        );
        expect(screen.queryByTestId('field-clear-speed')).not.toBeInTheDocument();
    });

    it('clear button shown when value set', () => {
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'speed', value: 5 })}
                onChange={vi.fn()}
            />
        );
        expect(screen.getByTestId('field-clear-speed')).toBeInTheDocument();
    });

    it('clicking clear calls onChange with null', () => {
        const onChange = vi.fn();
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'speed', value: 5 })}
                onChange={onChange}
            />
        );
        fireEvent.click(screen.getByTestId('field-clear-speed'));
        expect(onChange).toHaveBeenCalledWith(COMP, 'speed', null);
    });

    it('numeric field (kind=primitive, value is number) renders input type=number', () => {
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'mass', kind: 'primitive', value: 1.5 })}
                onChange={vi.fn()}
            />
        );
        const input = screen.getByTestId('field-input-mass') as HTMLInputElement;
        expect(input.type).toBe('number');
    });

    it('text field renders input type=text', () => {
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'tag', kind: 'string', value: 'hero' })}
                onChange={vi.fn()}
            />
        );
        const input = screen.getByTestId('field-input-tag') as HTMLInputElement;
        expect(input.type).toBe('text');
    });

    it('changing input value calls onChange with parsed value on blur', () => {
        const onChange = vi.fn();
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'speed', kind: 'primitive', codeDefault: 1 })}
                onChange={onChange}
            />
        );
        const input = screen.getByTestId('field-input-speed');
        fireEvent.change(input, { target: { value: '7' } });
        fireEvent.blur(input);
        expect(onChange).toHaveBeenCalledWith(COMP, 'speed', 7);
    });

    it('clearing to empty string calls onChange with null', () => {
        const onChange = vi.fn();
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'speed', kind: 'primitive', value: 5, codeDefault: 1 })}
                onChange={onChange}
            />
        );
        const input = screen.getByTestId('field-input-speed');
        fireEvent.change(input, { target: { value: '' } });
        fireEvent.blur(input);
        expect(onChange).toHaveBeenCalledWith(COMP, 'speed', null);
    });

    it('non-parseable numeric input ("abc") calls onChange with null', () => {
        const onChange = vi.fn();
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'mass', kind: 'primitive', codeDefault: 1 })}
                onChange={onChange}
            />
        );
        const input = screen.getByTestId('field-input-mass');
        fireEvent.change(input, { target: { value: 'abc' } });
        fireEvent.blur(input);
        expect(onChange).toHaveBeenCalledWith(COMP, 'mass', null);
    });

    it('entering codeDefault value calls onChange with null (clears override)', () => {
        const onChange = vi.fn();
        render(
            <FieldRow
                componentType={COMP}
                field={makeField({ name: 'speed', kind: 'primitive', codeDefault: 10 })}
                onChange={onChange}
            />
        );
        const input = screen.getByTestId('field-input-speed');
        fireEvent.change(input, { target: { value: '10' } });
        fireEvent.blur(input);
        expect(onChange).toHaveBeenCalledWith(COMP, 'speed', null);
    });
});
