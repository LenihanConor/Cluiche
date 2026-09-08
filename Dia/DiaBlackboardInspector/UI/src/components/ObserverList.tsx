import { theme } from '@dia/editor-ui';

interface ObserverListProps {
    observers: string[];
}

export function ObserverList({ observers }: ObserverListProps) {
    return (
        <div>
            <div
                style={{
                    fontSize: 10,
                    textTransform: 'uppercase',
                    letterSpacing: '0.07em',
                    color: theme.textMuted,
                    padding: '5px 4px 3px',
                    marginTop: 4,
                    borderTop: `1px solid ${theme.borderMuted}`,
                }}
            >
                Observers
            </div>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: 4, padding: '4px 4px 6px' }}>
                {observers.length === 0 ? (
                    <span
                        style={{
                            fontSize: 10,
                            fontFamily: 'monospace',
                            padding: '1px 8px',
                            borderRadius: 10,
                            background: 'none',
                            border: `1px solid ${theme.border}`,
                            color: '#555',
                        }}
                    >
                        none
                    </span>
                ) : (
                    observers.map((name) => (
                        <span
                            key={name}
                            style={{
                                fontSize: 10,
                                fontFamily: 'monospace',
                                padding: '1px 8px',
                                borderRadius: 10,
                                background: '#0e3a3a',
                                border: '1px solid #1a5c5c',
                                color: '#4ec9b0',
                            }}
                        >
                            {name}
                        </span>
                    ))
                )}
            </div>
        </div>
    );
}
