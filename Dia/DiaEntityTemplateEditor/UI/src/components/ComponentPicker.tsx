import { useState, useEffect, useCallback, CSSProperties } from 'react';
import { theme, inputStyle, buttonStyle, useToast } from '@dia/editor-ui';
import type { AvailableComponent } from '../types';

interface ComponentPickerProps {
    isOpen: boolean;
    components: AvailableComponent[];
    usageCount: number;
    sceneCount: number;
    onAdd: (typeId: string) => void;
    onClose: () => void;
}

export default function ComponentPicker({
    isOpen,
    components,
    usageCount,
    sceneCount,
    onAdd,
    onClose,
}: ComponentPickerProps) {
    const toast = useToast();
    const [query, setQuery] = useState('');
    const [focusedIndex, setFocusedIndex] = useState<number | null>(null);

    const trimmed = query.trim().toLowerCase();

    const filtered = trimmed
        ? components.filter(
              (c) =>
                  c.typeId.toLowerCase().includes(trimmed) ||
                  c.label.toLowerCase().includes(trimmed) ||
                  (c.description?.toLowerCase().includes(trimmed) ?? false)
          )
        : components;

    const confirmItem = useCallback(
        (typeId: string) => {
            if (usageCount > 0) {
                toast.push(
                    `This blueprint is used in ${usageCount} instance${usageCount !== 1 ? 's' : ''} across ${sceneCount} scene${sceneCount !== 1 ? 's' : ''}. Adding a component will affect all of them.`,
                    'warning'
                );
            }
            onAdd(typeId);
        },
        [usageCount, sceneCount, toast, onAdd]
    );

    useEffect(() => {
        if (!isOpen) return;

        const handleKeyDown = (e: KeyboardEvent) => {
            if (e.key === 'Escape') {
                onClose();
                return;
            }
            if (e.key === 'ArrowDown') {
                e.preventDefault();
                setFocusedIndex((prev) => {
                    if (filtered.length === 0) return null;
                    if (prev === null) return 0;
                    return Math.min(prev + 1, filtered.length - 1);
                });
                return;
            }
            if (e.key === 'ArrowUp') {
                e.preventDefault();
                setFocusedIndex((prev) => {
                    if (filtered.length === 0) return null;
                    if (prev === null) return 0;
                    return Math.max(prev - 1, 0);
                });
                return;
            }
            if (e.key === 'Enter') {
                if (focusedIndex !== null && filtered[focusedIndex]) {
                    confirmItem(filtered[focusedIndex].typeId);
                }
            }
        };

        document.addEventListener('keydown', handleKeyDown);
        return () => document.removeEventListener('keydown', handleKeyDown);
    }, [isOpen, filtered, focusedIndex, onClose, confirmItem]);

    // Reset state when picker opens
    useEffect(() => {
        if (isOpen) {
            setQuery('');
            setFocusedIndex(null);
        }
    }, [isOpen]);

    if (!isOpen) return null;

    const panelStyle: CSSProperties = {
        position: 'absolute',
        bottom: 50,
        left: 8,
        right: 8,
        zIndex: 50,
        background: theme.bgPanel,
        border: `1px solid ${theme.border}`,
        borderRadius: 4,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
    };

    const headerStyle: CSSProperties = {
        display: 'flex',
        alignItems: 'center',
        gap: 6,
        padding: '6px 8px',
        borderBottom: `1px solid ${theme.border}`,
        flexShrink: 0,
    };

    const countStyle: CSSProperties = {
        padding: '2px 8px',
        fontSize: '11px',
        color: theme.textMuted,
        flexShrink: 0,
    };

    const listStyle: CSSProperties = {
        overflowY: 'auto',
        maxHeight: 200,
        flex: 1,
    };

    const emptyStyle: CSSProperties = {
        padding: '12px 8px',
        textAlign: 'center',
        color: theme.textMuted,
        fontSize: '13px',
    };

    const footerStyle: CSSProperties = {
        display: 'flex',
        justifyContent: 'flex-end',
        gap: 6,
        padding: '6px 8px',
        borderTop: `1px solid ${theme.border}`,
        flexShrink: 0,
    };

    const closeBtnStyle: CSSProperties = {
        background: 'transparent',
        border: 'none',
        color: theme.textMuted,
        cursor: 'pointer',
        fontSize: '14px',
        lineHeight: 1,
        padding: '0 4px',
        flexShrink: 0,
    };

    return (
        <div style={panelStyle} data-testid="picker-panel">
            <div style={headerStyle}>
                <input
                    style={inputStyle({ flex: 1, minWidth: 0 })}
                    type="text"
                    placeholder="Search components..."
                    value={query}
                    onChange={(e) => {
                        setQuery(e.target.value);
                        setFocusedIndex(null);
                    }}
                    aria-label="Search components"
                />
                <button style={closeBtnStyle} onClick={onClose} aria-label="Close">
                    ✕
                </button>
            </div>

            <div style={countStyle} data-testid="picker-count">
                {filtered.length} of {components.length} components available
            </div>

            <div style={listStyle}>
                {filtered.length === 0 ? (
                    <div style={emptyStyle}>No components match</div>
                ) : (
                    filtered.map((component, index) => {
                        const isFocused = focusedIndex === index;
                        const itemStyle: CSSProperties = {
                            padding: '5px 10px',
                            cursor: 'pointer',
                            fontSize: '13px',
                            background: isFocused ? theme.accent : 'transparent',
                            color: isFocused ? '#fff' : theme.text,
                            userSelect: 'none',
                        };
                        return (
                            <div
                                key={component.typeId}
                                style={itemStyle}
                                data-testid={`picker-item-${index}`}
                                onClick={() => setFocusedIndex(index)}
                                onDoubleClick={() => confirmItem(component.typeId)}
                            >
                                <span style={{ fontWeight: 600 }}>{component.label}</span>
                                {component.description && (
                                    <span style={{ marginLeft: 6, color: isFocused ? '#ccc' : theme.textMuted, fontSize: '11px' }}>
                                        {component.description}
                                    </span>
                                )}
                            </div>
                        );
                    })
                )}
            </div>

            <div style={footerStyle}>
                <button
                    style={buttonStyle('primary')}
                    data-testid="add-btn"
                    disabled={focusedIndex === null}
                    onClick={() => {
                        if (focusedIndex !== null && filtered[focusedIndex]) {
                            confirmItem(filtered[focusedIndex].typeId);
                        }
                    }}
                >
                    Add
                </button>
                <button style={buttonStyle()} onClick={onClose}>
                    Cancel
                </button>
            </div>
        </div>
    );
}
