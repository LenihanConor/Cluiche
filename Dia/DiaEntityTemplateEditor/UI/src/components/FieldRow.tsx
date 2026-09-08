import { CSSProperties, useState, useEffect, ChangeEvent, FocusEvent } from 'react';
import { theme, inputStyle } from '@dia/editor-ui';
import type { FieldEntry } from '../types';

interface FieldRowProps {
    componentType: string;
    field: FieldEntry;
    onChange: (componentType: string, fieldName: string, value: string | number | null) => void;
}

const OVERRIDE_BORDER_COLOR = '#4ec9a0';

function isNumericField(field: FieldEntry): boolean {
    return (
        field.kind === 'primitive' &&
        (typeof field.value === 'number' || typeof field.codeDefault === 'number')
    );
}

const labelStyle: CSSProperties = {
    color: theme.textMuted,
    fontSize: '12px',
    minWidth: '100px',
    flexShrink: 0,
    paddingRight: '8px',
    userSelect: 'none',
};

const rowStyle: CSSProperties = {
    display: 'flex',
    alignItems: 'center',
    padding: '3px 8px',
    gap: '4px',
};

const clearButtonStyle: CSSProperties = {
    background: 'transparent',
    border: 'none',
    color: theme.textMuted,
    cursor: 'pointer',
    fontSize: '12px',
    padding: '0 4px',
    lineHeight: 1,
    flexShrink: 0,
};

export function FieldRow({ componentType, field, onChange }: FieldRowProps) {
    const hasValue = field.value !== undefined && field.value !== null;
    const isNumeric = isNumericField(field);

    const valueAsString = hasValue ? String(field.value) : '';
    const [inputVal, setInputVal] = useState(valueAsString);

    // Keep local state in sync when prop changes externally
    useEffect(() => {
        setInputVal(hasValue ? String(field.value) : '');
    }, [field.value, hasValue]);

    const inputType = isNumeric ? 'number' : 'text';

    const overrideStyle: CSSProperties = hasValue
        ? { border: `1px solid ${OVERRIDE_BORDER_COLOR}` }
        : {};

    const computedInputStyle = inputStyle({ flex: 1, minWidth: 0, ...overrideStyle });

    function handleChange(e: ChangeEvent<HTMLInputElement>) {
        const raw = e.target.value;
        setInputVal(raw);
    }

    function handleBlur(e: FocusEvent<HTMLInputElement>) {
        const raw = e.target.value;
        commit(raw);
    }

    function commit(raw: string) {
        if (raw === '') {
            onChange(componentType, field.name, null);
            return;
        }
        const codeDefaultStr =
            field.codeDefault !== undefined && field.codeDefault !== null
                ? String(field.codeDefault)
                : undefined;

        if (codeDefaultStr !== undefined && raw === codeDefaultStr) {
            onChange(componentType, field.name, null);
            return;
        }

        if (isNumeric) {
            const parsed = parseFloat(raw);
            onChange(componentType, field.name, isNaN(parsed) ? null : parsed);
        } else {
            onChange(componentType, field.name, raw);
        }
    }

    function handleClear() {
        setInputVal('');
        onChange(componentType, field.name, null);
    }

    const placeholder =
        field.codeDefault !== undefined && field.codeDefault !== null
            ? String(field.codeDefault)
            : undefined;

    return (
        <div style={rowStyle} data-testid={`field-row-${field.name}`}>
            <span style={labelStyle}>{field.name}</span>
            <input
                style={computedInputStyle}
                type={inputType}
                value={inputVal}
                placeholder={placeholder}
                onChange={handleChange}
                onBlur={handleBlur}
                data-testid={`field-input-${field.name}`}
            />
            {hasValue && (
                <button
                    style={clearButtonStyle}
                    onClick={handleClear}
                    data-testid={`field-clear-${field.name}`}
                    aria-label={`Clear ${field.name}`}
                >
                    ✕
                </button>
            )}
        </div>
    );
}

export default FieldRow;
