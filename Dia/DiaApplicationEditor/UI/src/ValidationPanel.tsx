import React from 'react';
import { useManifestStore } from './ManifestStore';

export interface ValidationError {
    message: string;
    context: string;
    severity: 'error' | 'warning';
    code: string;
}

export interface ValidationResult {
    is_valid: boolean;
    errors: ValidationError[];
}

export const ValidationPanel: React.FC = () => {
    const validationResult = useManifestStore(s => s.validationResult);
    const setSelectedNode  = useManifestStore(s => s.setSelectedNode);

    if (!validationResult) return null;

    const { errors } = validationResult;

    if (errors.length === 0) {
        return (
            <div style={styles.panel}>
                <span style={styles.success}>No validation errors</span>
            </div>
        );
    }

    const errorCount   = errors.filter(e => e.severity === 'error').length;
    const warningCount = errors.filter(e => e.severity === 'warning').length;

    const handleClick = (error: ValidationError) => {
        if (error.context) setSelectedNode(error.context);
    };

    return (
        <div style={styles.panel}>
            <div style={styles.header}>
                <span style={styles.title}>Validation Issues</span>
                <span style={styles.counts}>
                    {errorCount > 0 && <span style={styles.errorBadge}>{errorCount} error{errorCount !== 1 ? 's' : ''}</span>}
                    {warningCount > 0 && <span style={styles.warnBadge}>{warningCount} warning{warningCount !== 1 ? 's' : ''}</span>}
                </span>
            </div>
            <div style={styles.list}>
                {errors.map((e, i) => (
                    <div
                        key={i}
                        style={{ ...styles.item, borderLeft: `3px solid ${e.severity === 'error' ? '#f44336' : '#ff9800'}` }}
                        onClick={() => handleClick(e)}
                    >
                        <span style={styles.icon}>{e.severity === 'error' ? '✕' : '!'}</span>
                        <div style={styles.content}>
                            <div style={styles.message}>{e.message}</div>
                            {e.context && <div style={styles.location}>{e.context}</div>}
                        </div>
                    </div>
                ))}
            </div>
        </div>
    );
};

const styles: Record<string, React.CSSProperties> = {
    panel:      { background: '#1e1e1e', color: '#ccc', fontSize: 12, display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' },
    header:     { display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '4px 8px', borderBottom: '1px solid #333' },
    title:      { fontWeight: 600, color: '#fff' },
    counts:     { display: 'flex', gap: 6 },
    errorBadge: { background: '#f44336', color: '#fff', borderRadius: 3, padding: '1px 5px' },
    warnBadge:  { background: '#ff9800', color: '#fff', borderRadius: 3, padding: '1px 5px' },
    success:    { padding: 8, color: '#4caf50' },
    list:       { overflowY: 'auto', flex: 1 },
    item:       { display: 'flex', alignItems: 'flex-start', gap: 8, padding: '5px 8px', cursor: 'pointer', borderBottom: '1px solid #2a2a2a' },
    icon:       { width: 14, textAlign: 'center', flexShrink: 0, marginTop: 1 },
    content:    { flex: 1, minWidth: 0 },
    message:    { color: '#e0e0e0', wordBreak: 'break-word' },
    location:   { color: '#777', fontSize: 11, marginTop: 2 },
};
