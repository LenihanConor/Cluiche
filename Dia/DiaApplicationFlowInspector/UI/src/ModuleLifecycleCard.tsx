import React from 'react';
import type { ModuleState, ModuleLifecycleState } from './useInspectorStore';

interface Props {
    module: ModuleState;
}

const STATE_COLORS: Record<ModuleLifecycleState, string> = {
    Running: '#4caf50',
    Loading: '#ff9800',
    Stopped: '#888',
    Failed: '#f44336',
};

const STATE_BG: Record<ModuleLifecycleState, string> = {
    Running: '#1a3a1a',
    Loading: '#3a2a00',
    Stopped: '#2a2a2a',
    Failed: '#3a0000',
};

function formatDuration(ms: number): string {
    if (ms < 1000) return `${ms}ms`;
    if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
    return `${Math.floor(ms / 60000)}m${Math.floor((ms % 60000) / 1000)}s`;
}

export const ModuleLifecycleCard: React.FC<Props> = ({ module }) => {
    const color = STATE_COLORS[module.lifecycleState];
    const bg = STATE_BG[module.lifecycleState];
    const fillPercent = module.timeoutMs
        ? Math.min(100, Math.round((module.timeInStateMs / module.timeoutMs) * 100))
        : null;
    const isOverTimeout = fillPercent !== null && fillPercent >= 100;

    const barColor = fillPercent === null ? null
        : fillPercent < 60 ? '#4caf50'
        : fillPercent < 85 ? '#ff9800'
        : '#f44336';

    return (
        <div
            data-testid="module-lifecycle-card"
            data-state={module.lifecycleState}
            style={{
                background: bg,
                border: `1px solid ${color}44`,
                borderRadius: 4,
                padding: '6px 10px',
                fontSize: 12,
                marginBottom: 4,
            }}
        >
            <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                <span
                    data-testid="module-state-badge"
                    style={{
                        background: color,
                        color: '#fff',
                        borderRadius: 3,
                        padding: '1px 6px',
                        fontSize: 10,
                        fontWeight: 600,
                    }}
                >
                    {module.lifecycleState}
                </span>
                <span data-testid="module-name" style={{ fontWeight: 600, color: '#ccc' }}>
                    {module.moduleId}
                </span>
                <span data-testid="module-pu-badge" style={{ color: '#666', fontSize: 10, marginLeft: 'auto' }}>
                    {module.puId}
                </span>
            </div>

            <div style={{ color: '#888', marginTop: 3 }}>
                {formatDuration(module.timeInStateMs)} in state
            </div>

            {fillPercent !== null && (
                <div data-testid="timeout-bar-container" style={{ marginTop: 4 }}>
                    <div style={{ background: '#333', borderRadius: 2, height: 4, overflow: 'hidden' }}>
                        <div
                            data-testid="timeout-bar"
                            style={{
                                width: `${fillPercent}%`,
                                height: '100%',
                                background: barColor!,
                                transition: 'width 0.3s',
                            }}
                        />
                    </div>
                    {isOverTimeout && (
                        <div data-testid="timeout-exceeded" style={{ color: '#f44336', fontSize: 10, marginTop: 2 }}>
                            Timeout exceeded
                        </div>
                    )}
                </div>
            )}

            {module.blockedByDep && (
                <div data-testid="blocked-by-dep" style={{ color: '#ff9800', marginTop: 3, fontSize: 11 }}>
                    Blocked by: {module.blockedByDep}
                </div>
            )}

            {module.errorMessage && (
                <div data-testid="error-message" style={{ color: '#f44336', marginTop: 3, fontSize: 11 }}>
                    {module.errorMessage}
                </div>
            )}
        </div>
    );
};
