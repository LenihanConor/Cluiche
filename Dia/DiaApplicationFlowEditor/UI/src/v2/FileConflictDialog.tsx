import React from 'react';

export interface FileConflictDialogProps {
    isOpen: boolean;
    filePath: string;
    onReload: () => void;
    onKeepEdits: () => void;
}

export const FileConflictDialog: React.FC<FileConflictDialogProps> = ({
    isOpen,
    filePath,
    onReload,
    onKeepEdits,
}) => {
    if (!isOpen) return null;

    return (
        <div
            style={{
                position: 'fixed',
                top: 0, left: 0, right: 0, bottom: 0,
                background: 'rgba(0,0,0,0.7)',
                zIndex: 1000,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
            }}
        >
            <div
                data-testid="file-conflict-dialog"
                style={{
                    background: '#2d2d2d',
                    borderRadius: 8,
                    padding: 24,
                    maxWidth: 420,
                    width: '90%',
                    boxSizing: 'border-box',
                }}
            >
                <div style={{ fontSize: 16, fontWeight: 'bold', color: '#f0a030', marginBottom: 12 }}>
                    File Changed on Disk
                </div>

                <div style={{ fontSize: 13, color: '#ccc', marginBottom: 20, lineHeight: 1.5 }}>
                    The file <code style={{ background: '#1e1e1e', padding: '1px 4px', borderRadius: 3, fontSize: 12, color: '#e0c080' }}>{filePath}</code> has been modified by another process. What would you like to do?
                </div>

                <div style={{ display: 'flex', gap: 10, justifyContent: 'flex-end' }}>
                    <button
                        data-testid="keep-edits-btn"
                        onClick={onKeepEdits}
                        style={{
                            background: '#555',
                            border: 'none',
                            borderRadius: 4,
                            color: '#eee',
                            cursor: 'pointer',
                            padding: '8px 16px',
                            fontSize: 13,
                        }}
                    >
                        Keep My Edits
                    </button>
                    <button
                        data-testid="reload-btn"
                        onClick={onReload}
                        style={{
                            background: '#007acc',
                            border: 'none',
                            borderRadius: 4,
                            color: '#fff',
                            cursor: 'pointer',
                            padding: '8px 16px',
                            fontSize: 13,
                            fontWeight: 600,
                        }}
                    >
                        Reload from Disk
                    </button>
                </div>
            </div>
        </div>
    );
};
