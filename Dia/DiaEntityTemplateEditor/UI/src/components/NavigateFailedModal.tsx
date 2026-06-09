import { CSSProperties } from 'react';
import { theme, buttonStyle, buildNavigateFailedContext } from '@dia/editor-ui';
import type { NavigateFailedContext } from '@dia/editor-ui';
import type { NavigateFailedData } from '../types';

interface NavigateFailedModalProps {
    data: NavigateFailedData | null;
    onCreateFile: (context: NavigateFailedContext) => void;
    onRemoveEntry: (context: NavigateFailedContext) => void;
    onDismiss: () => void;
}

const overlayStyle: CSSProperties = {
    position: 'fixed',
    top: 0,
    left: 0,
    width: '100%',
    height: '100%',
    zIndex: 200,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: 'rgba(0, 0, 0, 0.6)',
};

const dialogStyle: CSSProperties = {
    background: theme.bgPanel,
    border: `1px solid ${theme.border}`,
    borderRadius: '4px',
    padding: '24px',
    maxWidth: '480px',
    width: '100%',
    color: theme.text,
    display: 'flex',
    flexDirection: 'column',
    gap: '12px',
};

const titleStyle: CSSProperties = {
    fontSize: '15px',
    fontWeight: 700,
    color: theme.error,
    margin: 0,
};

const messageStyle: CSSProperties = {
    fontSize: '13px',
    color: theme.text,
    lineHeight: 1.5,
};

const pathStyle: CSSProperties = {
    fontSize: '12px',
    color: theme.text,
    opacity: 0.7,
    fontFamily: 'monospace',
    wordBreak: 'break-all',
};

const buttonRowStyle: CSSProperties = {
    display: 'flex',
    gap: '8px',
    justifyContent: 'flex-end',
    marginTop: '8px',
};

export function NavigateFailedModal({
    data,
    onCreateFile,
    onRemoveEntry,
    onDismiss,
}: NavigateFailedModalProps) {
    if (data === null) {
        return null;
    }

    const context = buildNavigateFailedContext(data);
    if (context === null) {
        return null;
    }

    return (
        <div style={overlayStyle}>
            <div style={dialogStyle} data-testid="navigate-failed-modal">
                <p style={titleStyle}>Blueprint Not Found</p>
                <p style={messageStyle}>
                    Blueprint <strong>{context.instanceId}</strong> could not be loaded from{' '}
                    <strong>{context.sourcePath}</strong>.<br />
                    {data.error}
                </p>
                <p style={pathStyle}>Expected path: {context.expectedPath}</p>
                <div style={buttonRowStyle}>
                    <button
                        style={buttonStyle('primary')}
                        data-testid="create-file-btn"
                        onClick={() => onCreateFile(context)}
                    >
                        Create File
                    </button>
                    <button
                        style={buttonStyle('default')}
                        data-testid="remove-entry-btn"
                        onClick={() => onRemoveEntry(context)}
                    >
                        Remove Entry
                    </button>
                    <button
                        style={buttonStyle('ghost')}
                        data-testid="dismiss-btn"
                        onClick={onDismiss}
                    >
                        Dismiss
                    </button>
                </div>
            </div>
        </div>
    );
}

export default NavigateFailedModal;
