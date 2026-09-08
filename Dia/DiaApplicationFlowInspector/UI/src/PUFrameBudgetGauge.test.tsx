import { render, screen } from '@testing-library/react';
import { PUFrameBudgetGauge } from './PUFrameBudgetGauge';
import type { PUTimingState } from './useInspectorStore';

function makeTiming(overrides: Partial<PUTimingState> = {}): PUTimingState {
    return {
        puId: 'SimPU',
        lastTickMs: 8,
        targetPeriodMs: 16.667,
        ...overrides,
    };
}

describe('PUFrameBudgetGauge', () => {
    it('renders PU id', () => {
        render(<PUFrameBudgetGauge timing={makeTiming()} />);
        expect(screen.getByTestId('pu-id').textContent).toBe('SimPU');
    });

    it('shows Hz label from targetPeriodMs', () => {
        render(<PUFrameBudgetGauge timing={makeTiming({ targetPeriodMs: 10 })} />);
        expect(screen.getByTestId('pu-hz-label').textContent).toBe('100Hz');
    });

    it('budget bar width matches percent (capped at 100%)', () => {
        // 8ms / 16ms = 50%
        render(<PUFrameBudgetGauge timing={makeTiming({ lastTickMs: 8, targetPeriodMs: 16 })} />);
        const bar = screen.getByTestId('budget-bar');
        expect(bar.style.width).toBe('50%');
    });

    it('bar is green when < 60% budget', () => {
        render(<PUFrameBudgetGauge timing={makeTiming({ lastTickMs: 4, targetPeriodMs: 16 })} />);
        const bar = screen.getByTestId('budget-bar');
        expect(bar.style.background).toBe('rgb(76, 175, 80)');
    });

    it('bar is amber when 60-84% budget', () => {
        render(<PUFrameBudgetGauge timing={makeTiming({ lastTickMs: 11, targetPeriodMs: 16 })} />);
        const bar = screen.getByTestId('budget-bar');
        expect(bar.style.background).toBe('rgb(255, 152, 0)');
    });

    it('bar is red when >= 85% budget', () => {
        render(<PUFrameBudgetGauge timing={makeTiming({ lastTickMs: 15, targetPeriodMs: 16 })} />);
        const bar = screen.getByTestId('budget-bar');
        expect(bar.style.background).toBe('rgb(244, 67, 54)');
    });

    it('shows OVER BUDGET label when >= 100%', () => {
        render(<PUFrameBudgetGauge timing={makeTiming({ lastTickMs: 20, targetPeriodMs: 16 })} />);
        expect(screen.getByTestId('over-budget-label')).toBeInTheDocument();
        expect(screen.getByTestId('over-budget-label').textContent).toBe('OVER BUDGET');
    });

    it('does not show OVER BUDGET label when < 100%', () => {
        render(<PUFrameBudgetGauge timing={makeTiming({ lastTickMs: 8, targetPeriodMs: 16 })} />);
        expect(screen.queryByTestId('over-budget-label')).toBeNull();
    });

    it('percent label shows budget used', () => {
        render(<PUFrameBudgetGauge timing={makeTiming({ lastTickMs: 8, targetPeriodMs: 16 })} />);
        expect(screen.getByTestId('budget-percent-label').textContent).toBe('50% budget used');
    });
});
