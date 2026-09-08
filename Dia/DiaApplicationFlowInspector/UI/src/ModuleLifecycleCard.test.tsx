import { render, screen } from '@testing-library/react';
import { ModuleLifecycleCard } from './ModuleLifecycleCard';
import type { ModuleState } from './useInspectorStore';

function makeModule(overrides: Partial<ModuleState> = {}): ModuleState {
    return {
        moduleId: 'RenderModule',
        puId: 'SimPU',
        lifecycleState: 'Running',
        timeInStateMs: 1500,
        timeoutMs: null,
        blockedByDep: null,
        errorMessage: null,
        ...overrides,
    };
}

describe('ModuleLifecycleCard', () => {
    it('renders module name and PU badge', () => {
        render(<ModuleLifecycleCard module={makeModule()} />);
        expect(screen.getByTestId('module-name').textContent).toBe('RenderModule');
        expect(screen.getByTestId('module-pu-badge').textContent).toBe('SimPU');
    });

    it('renders Running state badge', () => {
        render(<ModuleLifecycleCard module={makeModule({ lifecycleState: 'Running' })} />);
        expect(screen.getByTestId('module-state-badge').textContent).toBe('Running');
        expect(screen.getByTestId('module-lifecycle-card').getAttribute('data-state')).toBe('Running');
    });

    it('renders Loading state badge', () => {
        render(<ModuleLifecycleCard module={makeModule({ lifecycleState: 'Loading' })} />);
        expect(screen.getByTestId('module-state-badge').textContent).toBe('Loading');
    });

    it('renders Stopped state badge', () => {
        render(<ModuleLifecycleCard module={makeModule({ lifecycleState: 'Stopped' })} />);
        expect(screen.getByTestId('module-state-badge').textContent).toBe('Stopped');
    });

    it('renders Failed state badge', () => {
        render(<ModuleLifecycleCard module={makeModule({ lifecycleState: 'Failed' })} />);
        expect(screen.getByTestId('module-state-badge').textContent).toBe('Failed');
    });

    it('shows timeout bar with correct fill percent', () => {
        render(<ModuleLifecycleCard module={makeModule({ timeInStateMs: 500, timeoutMs: 1000 })} />);
        const bar = screen.getByTestId('timeout-bar');
        expect(bar.style.width).toBe('50%');
    });

    it('shows timeout-exceeded when fill is 100%', () => {
        render(<ModuleLifecycleCard module={makeModule({ timeInStateMs: 2000, timeoutMs: 1000 })} />);
        expect(screen.getByTestId('timeout-exceeded')).toBeInTheDocument();
    });

    it('does not show timeout bar when timeoutMs is null', () => {
        render(<ModuleLifecycleCard module={makeModule({ timeoutMs: null })} />);
        expect(screen.queryByTestId('timeout-bar-container')).toBeNull();
    });

    it('shows blocked-by-dep text when present', () => {
        render(<ModuleLifecycleCard module={makeModule({ blockedByDep: 'PhysicsModule' })} />);
        expect(screen.getByTestId('blocked-by-dep').textContent).toContain('PhysicsModule');
    });

    it('shows error message when Failed with errorMessage', () => {
        render(<ModuleLifecycleCard module={makeModule({ lifecycleState: 'Failed', errorMessage: 'null ptr' })} />);
        expect(screen.getByTestId('error-message').textContent).toBe('null ptr');
    });
});
