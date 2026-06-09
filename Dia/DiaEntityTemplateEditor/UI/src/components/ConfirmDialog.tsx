import React, { CSSProperties } from 'react';
import { theme, buttonStyle } from '@dia/editor-ui';

export interface ConfirmDialogProps {
    isOpen: boolean;
    title: string;
    message: string;
    confirmLabel?: string;
    cancelLabel?: string;
    variant?: 'default' | 'danger';
    onConfirm: () => void;
    onCancel: () => void;
}

const overlayStyle: CSSProperties = {
    position: 'fixed',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    background: 'rgba(0,0,0,0.7)',
    zIndex: 150,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
};

const dialogStyle: CSSProperties = {
    background: theme.bgPanel,
    border: `1px solid ${theme.border}`,
    borderRadius: '4px',
    padding: '20px 24px',
    minWidth: '320px',
    maxWidth: '480px',
    display: 'flex',
    flexDirection: 'column',
    gap: '12px',
};

const messageStyle: CSSProperties = {
    color: theme.text,
    fontSize: '13px',
    lineHeight: '1.5',
};

const buttonRowStyle: CSSProperties = {
    display: 'flex',
    justifyContent: 'flex-end',
    gap: '8px',
    marginTop: '4px',
};

function ConfirmDialog({
    isOpen,
    title,
    message,
    confirmLabel,
    cancelLabel,
    variant = 'default',
    onConfirm,
    onCancel,
}: ConfirmDialogProps) {
    if (!isOpen) {
        return null;
    }

    const titleStyle: CSSProperties = {
        color: variant === 'danger' ? theme.error : theme.text,
        fontSize: '14px',
        fontWeight: 600,
        margin: 0,
    };

    const confirmBtnStyle = buttonStyle(
        'primary',
        variant === 'danger' ? { background: theme.error, borderColor: theme.error } : {}
    );

    return (
        <div style={overlayStyle}>
            <div style={dialogStyle} data-testid="confirm-dialog">
                <div style={titleStyle} data-testid="confirm-title">
                    {title}
                </div>
                <div style={messageStyle}>
                    {message}
                </div>
                <div style={buttonRowStyle}>
                    <button
                        style={buttonStyle('default')}
                        data-testid="cancel-btn"
                        onClick={onCancel}
                    >
                        {cancelLabel || 'Cancel'}
                    </button>
                    <button
                        style={confirmBtnStyle}
                        data-testid="confirm-btn"
                        onClick={onConfirm}
                    >
                        {confirmLabel || 'Confirm'}
                    </button>
                </div>
            </div>
        </div>
    );
}

export { ConfirmDialog };
