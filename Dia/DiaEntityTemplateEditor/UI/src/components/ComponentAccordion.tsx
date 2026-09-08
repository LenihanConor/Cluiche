import { useState, CSSProperties, ReactNode } from 'react';
import { theme, buttonStyle } from '@dia/editor-ui';
import type { ComponentEntry } from '../types';

interface ComponentAccordionProps {
    component: ComponentEntry;
    onRemove: (type: string) => void;
    children?: ReactNode;
}

export default function ComponentAccordion({ component, onRemove, children }: ComponentAccordionProps) {
    const [open, setOpen] = useState(true);

    const wrapperStyle: CSSProperties = {
        background: theme.bgPanel,
        border: `1px solid ${theme.border}`,
        borderRadius: 4,
        marginBottom: 4,
        overflow: 'hidden',
    };

    const headerStyle: CSSProperties = {
        display: 'flex',
        alignItems: 'center',
        gap: 6,
        padding: '6px 8px',
        cursor: 'pointer',
        userSelect: 'none',
        color: theme.text,
        fontWeight: 600,
    };

    const chevronStyle: CSSProperties = {
        display: 'inline-block',
        transform: open ? 'rotate(90deg)' : 'rotate(0deg)',
        transition: 'transform 0.15s ease',
        fontSize: 10,
        lineHeight: 1,
    };

    const bodyStyle: CSSProperties = {
        display: open ? 'block' : 'none',
        padding: '4px 8px 8px',
        borderTop: `1px solid ${theme.border}`,
    };

    const removeRowStyle: CSSProperties = {
        display: 'flex',
        justifyContent: 'flex-end',
        marginTop: 6,
    };

    const removeButtonStyle: CSSProperties = buttonStyle('ghost', { color: theme.error });

    return (
        <div style={wrapperStyle} data-testid={`accordion-${component.type}`}>
            <div
                style={headerStyle}
                data-testid={`accordion-header-${component.type}`}
                onClick={() => setOpen((prev) => !prev)}
            >
                <span style={chevronStyle}>▶</span>
                <span>{component.type}</span>
            </div>
            <div style={bodyStyle} data-testid={`accordion-body-${component.type}`}>
                {children}
                <div style={removeRowStyle}>
                    <button
                        style={removeButtonStyle}
                        data-testid={`remove-btn-${component.type}`}
                        onClick={() => onRemove(component.type)}
                    >
                        Remove
                    </button>
                </div>
            </div>
        </div>
    );
}
