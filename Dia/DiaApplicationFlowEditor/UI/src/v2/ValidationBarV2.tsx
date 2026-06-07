import React from 'react';
import { useValidationStoreV2, type ValidationIssueV2 } from './useValidationStoreV2';
import { useSelectionStoreV2 } from './useSelectionStoreV2';
import { bridgeRequest } from './bridge';

type Tab = 'graph' | 'presence' | 'streams';

interface ValidationBarV2Props {
    setActiveTab?: (tab: Tab) => void;
}

const RULE_ID_LABELS = [
    'DEPENDENCY_CYCLE',
    'ORPHAN_MODULE',
    'UNKNOWN_STREAM_IN_READS',
    'UNKNOWN_STREAM_IN_WRITES',
    'ORPHAN_READER_STREAM',
    'ORPHAN_WRITER_STREAM',
    'PAYLOAD_TYPE_MISSING',
    'STAGE_REF_INVALID',
    'DUPLICATE_INSTANCE_ID',
    'INITIAL_STAGE_INVALID',
    'STREAM_PU_INVALID',
    'STREAM_SELF_LOOP',
];

const ruleLabel = (id: number): string => RULE_ID_LABELS[id] ?? `RULE_${id}`;

export const ValidationBarV2: React.FC<ValidationBarV2Props> = ({ setActiveTab }) => {
    const result = useValidationStoreV2(s => s.result);
    const isExpanded = useValidationStoreV2(s => s.isExpanded);
    const toggleExpanded = useValidationStoreV2(s => s.toggleExpanded);
    const setPU = useSelectionStoreV2(s => s.setPU);
    const setStream = useSelectionStoreV2(s => s.setStream);

    const errorCount = result?.errorCount ?? 0;
    const warnCount = result?.warningCount ?? 0;
    const hasIssues = errorCount > 0 || warnCount > 0;

    const barColor = errorCount > 0 ? '#c0392b' : warnCount > 0 ? '#f0a030' : '#007acc';

    const onIssueClick = (issue: ValidationIssueV2) => {
        if (issue.targetKind === 'pu' || issue.targetKind === 'module') {
            setActiveTab?.('graph');
            if (issue.targetPuId) setPU(issue.targetPuId);
        } else if (issue.targetKind === 'stream') {
            setActiveTab?.('streams');
            if (issue.targetStreamId) setStream(issue.targetStreamId);
        }
    };

    const onFixClick = (issue: ValidationIssueV2, e: React.MouseEvent) => {
        e.stopPropagation();
        if (issue.suggestedCommand) {
            bridgeRequest('manifest.applyCommand', issue.suggestedCommand);
        }
    };

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
                    {result.issues.map((issue, i) => {
                        const navigable = issue.targetKind !== '';
                        const dotColor = issue.severity === 'error' ? '#c0392b' : '#f0a030';
                        return (
                            <div
                                key={i}
                                data-testid={`issue-${i}`}
                                data-target-kind={issue.targetKind}
                                data-rule-id={issue.ruleId}
                                onClick={navigable ? () => onIssueClick(issue) : undefined}
                                title={issue.message}
                                style={{
                                    display: 'flex',
                                    alignItems: 'center',
                                    gap: 8,
                                    padding: '4px 8px',
                                    borderBottom: '1px solid #333',
                                    fontSize: 11,
                                    color: '#ccc',
                                    cursor: navigable ? 'pointer' : 'default',
                                }}
                                onMouseEnter={navigable ? (e) => { (e.currentTarget as HTMLDivElement).style.background = '#2d2d2d'; } : undefined}
                                onMouseLeave={navigable ? (e) => { (e.currentTarget as HTMLDivElement).style.background = 'transparent'; } : undefined}
                            >
                                <span style={{ color: dotColor, fontSize: 10 }}>●</span>
                                <span style={{ fontFamily: 'monospace', color: '#888', flexShrink: 0 }}>
                                    {ruleLabel(issue.ruleId)}
                                </span>
                                <span style={{
                                    flex: 1,
                                    minWidth: 0,
                                    overflow: 'hidden',
                                    textOverflow: 'ellipsis',
                                    whiteSpace: 'nowrap',
                                }}>
                                    {issue.message}
                                </span>
                                {navigable && <span style={{ color: '#666', flexShrink: 0 }}>›</span>}
                                {issue.suggestedCommand && issue.suggestedActionLabel && (
                                    <button
                                        data-testid={`fix-${i}`}
                                        onClick={(e) => onFixClick(issue, e)}
                                        style={{
                                            background: 'transparent',
                                            border: '1px solid #555',
                                            borderRadius: 3,
                                            color: '#aaa',
                                            padding: '2px 8px',
                                            fontSize: 10,
                                            cursor: 'pointer',
                                            flexShrink: 0,
                                        }}
                                        onMouseEnter={(e) => { (e.currentTarget as HTMLButtonElement).style.color = '#fff'; (e.currentTarget as HTMLButtonElement).style.borderColor = '#888'; }}
                                        onMouseLeave={(e) => { (e.currentTarget as HTMLButtonElement).style.color = '#aaa'; (e.currentTarget as HTMLButtonElement).style.borderColor = '#555'; }}
                                    >
                                        {issue.suggestedActionLabel}
                                    </button>
                                )}
                            </div>
                        );
                    })}
                </div>
            )}
        </div>
    );
};
