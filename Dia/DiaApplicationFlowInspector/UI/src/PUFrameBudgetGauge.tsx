import React from 'react';
import type { PUTimingState } from './useInspectorStore';

interface Props {
    timing: PUTimingState;
}

function barColor(percent: number): string {
    if (percent < 60) return '#4caf50';
    if (percent < 85) return '#ff9800';
    return '#f44336';
}

export const PUFrameBudgetGauge: React.FC<Props> = ({ timing }) => {
    const budgetPercent = timing.targetPeriodMs > 0
        ? Math.round((timing.lastTickMs / timing.targetPeriodMs) * 100)
        : 0;
    const isOverBudget = budgetPercent >= 100;
    const hz = timing.targetPeriodMs > 0 ? Math.round(1000 / timing.targetPeriodMs) : 0;
    const color = barColor(budgetPercent);
    const displayPercent = Math.min(100, budgetPercent);

    return (
        <div
            data-testid="pu-frame-budget-gauge"
            data-pu-id={timing.puId}
            style={{
                padding: '6px 8px',
                borderBottom: '1px solid #333',
                fontSize: 12,
            }}
        >
            <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                <span data-testid="pu-id" style={{ color: '#ccc', fontWeight: 600, minWidth: 80 }}>
                    {timing.puId}
                </span>
                <span data-testid="pu-timing-label" style={{ color: '#888' }}>
                    {timing.lastTickMs.toFixed(1)}ms / {timing.targetPeriodMs.toFixed(1)}ms
                </span>
                <span data-testid="pu-hz-label" style={{ color: '#666', marginLeft: 'auto' }}>
                    {hz}Hz
                </span>
                {isOverBudget && (
                    <span
                        data-testid="over-budget-label"
                        style={{ color: '#f44336', fontWeight: 700, fontSize: 11 }}
                    >
                        OVER BUDGET
                    </span>
                )}
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginTop: 4 }}>
                <div style={{ flex: 1, background: '#333', borderRadius: 2, height: 6, overflow: 'hidden' }}>
                    <div
                        data-testid="budget-bar"
                        style={{
                            width: `${displayPercent}%`,
                            height: '100%',
                            background: color,
                            transition: 'width 0.3s',
                        }}
                    />
                </div>
                <span data-testid="budget-percent-label" style={{ color, fontSize: 11, minWidth: 40, textAlign: 'right' }}>
                    {budgetPercent}% budget used
                </span>
            </div>
        </div>
    );
};
