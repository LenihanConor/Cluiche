import React from 'react';
import { useValidationStoreV2 } from './useValidationStoreV2';

export const ValidationBarV2: React.FC = () => {
    const result = useValidationStoreV2(s => s.result);
    const isExpanded = useValidationStoreV2(s => s.isExpanded);
    const toggleExpanded = useValidationStoreV2(s => s.toggleExpanded);

    const errorCount = result?.errorCount ?? 0;
    const warnCount = result?.warningCount ?? 0;
    const hasIssues = errorCount > 0 || warnCount > 0;

    const barColor = errorCount > 0 ? '#c0392b' : warnCount > 0 ? '#f0a030' : '#007acc';

    return (
        <div style={{ display: 'flex', flexDirection: 'column' }}>
            <div
                data-testid="validation-bar"
                onClick={hasIssues ? toggleExpanded : undefined}
                style={{
                    height: 28,
                    background: barColor,
                    display: 'flex',
                    alignItems: 'center',
                    padding: '0 8px',
                    fontSize: 11,
                    cursor: hasIssues ? 'pointer' : 'default',
                    userSelect: 'none',
                    gap: 8,
                }}
            >
                {!result && <span>No manifest loaded</span>}
                {result && errorCount === 0 && warnCount === 0 && <span>No issues</span>}
                {result && errorCount > 0 && (
                    <span data-testid="error-count">{errorCount} error{errorCount !== 1 ? 's' : ''}</span>
                )}
                {result && warnCount > 0 && (
                    <span data-testid="warning-count">{warnCount} warning{warnCount !== 1 ? 's' : ''}</span>
                )}
                {hasIssues && <span style={{ marginLeft: 'auto' }}>{isExpanded ? '▲' : '▼'}</span>}
            </div>

            {isExpanded && result && (
                <div data-testid="validation-issues" style={{ background: '#1e1e1e', borderTop: '1px solid #444', maxHeight: 200, overflow: 'auto' }}>
                    {result.issues.map((issue, i) => (
                        <div
                            key={i}
                            data-testid={`issue-${i}`}
                            style={{
                                padding: '4px 8px',
                                borderBottom: '1px solid #333',
                                fontSize: 11,
                                color: issue.severity === 'error' ? '#f48771' : '#cca700',
                                cursor: 'pointer',
                            }}
                        >
                            [{issue.severity.toUpperCase()}] {issue.message}
                        </div>
                    ))}
                </div>
            )}
        </div>
    );
};
