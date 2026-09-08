import React from 'react';

export interface RiskyChangeDialogProps {
    isOpen: boolean;
    riskCondition: string;
    commandDescription: string;
    onProceed: () => void;
    onCancel: () => void;
}

export const RiskyChangeDialog: React.FC<RiskyChangeDialogProps> = ({
    isOpen,
    riskCondition,
    commandDescription,
    onProceed,
    onCancel,
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
                data-testid="risky-dialog"
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
                    Risky Change
                </div>

                <div
                    data-testid="risk-condition-badge"
                    style={{
                        display: 'inline-block',
                        border: '1px solid #f0a030',
                        borderRadius: 12,
                        padding: '2px 10px',
                        fontSize: 11,
                        color: '#f0a030',
                        marginBottom: 12,
                    }}
                >
                    {riskCondition}
                </div>

                <div style={{ fontSize: 13, color: '#ccc', marginBottom: 20, lineHeight: 1.5 }}>
                    This change may affect the running game. {commandDescription}
                </div>

                <div style={{ display: 'flex', gap: 10, justifyContent: 'flex-end' }}>
                    <button
                        data-testid="cancel-btn"
                        onClick={onCancel}
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
                        Cancel
                    </button>
                    <button
                        data-testid="proceed-btn"
                        onClick={onProceed}
                        style={{
                            background: '#c0392b',
                            border: 'none',
                            borderRadius: 4,
                            color: '#fff',
                            cursor: 'pointer',
                            padding: '8px 16px',
                            fontSize: 13,
                            fontWeight: 600,
                        }}
                    >
                        Proceed Anyway
                    </button>
                </div>
            </div>
        </div>
    );
};
