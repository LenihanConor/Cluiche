import { describe, it, expect, beforeEach, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

vi.mock('./bridge', () => ({
    bridgeRequest: vi.fn(),
    bridgeEvent: vi.fn(),
}));

import { ValidationBarV2 } from './ValidationBarV2';
import { useValidationStoreV2, type ValidationIssueV2 } from './useValidationStoreV2';
import { useSelectionStoreV2 } from './useSelectionStoreV2';
import { bridgeRequest } from './bridge';

const mockBridge = bridgeRequest as unknown as ReturnType<typeof vi.fn>;

function makeIssue(overrides: Partial<ValidationIssueV2> = {}): ValidationIssueV2 {
    return {
        ruleId: 0,
        severity: 'error',
        message: 'msg',
        targetKind: '',
        targetPuId: '',
        targetModuleId: '',
        targetStreamId: '',
        suggestedActionLabel: '',
        suggestedCommand: null,
        ...overrides,
    };
}

beforeEach(() => {
    useValidationStoreV2.setState({ result: null, isExpanded: false });
    useSelectionStoreV2.setState({ puId: null, streamId: null });
    mockBridge.mockClear();
});

describe('ValidationBarV2', () => {
    it('shows no-manifest message when result is null', () => {
        render(<ValidationBarV2 />);
        expect(screen.getByText('No manifest loaded')).toBeTruthy();
    });

    it('shows error count when errors exist', () => {
        useValidationStoreV2.setState({
            result: { errorCount: 2, warningCount: 0, issues: [] },
        });
        render(<ValidationBarV2 />);
        expect(screen.getByTestId('error-count').textContent).toContain('2');
    });

    it('shows warning count when warnings exist', () => {
        useValidationStoreV2.setState({
            result: { errorCount: 0, warningCount: 3, issues: [] },
        });
        render(<ValidationBarV2 />);
        expect(screen.getByTestId('warning-count').textContent).toContain('3');
    });

    it('clicking bar with issues toggles expanded panel', () => {
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [makeIssue({ message: 'Cycle detected' })] },
            isExpanded: false,
        });
        const { getByTestId } = render(<ValidationBarV2 />);
        fireEvent.click(getByTestId('validation-bar'));
        expect(screen.getByTestId('validation-issues')).toBeTruthy();
    });

    it('expanded panel shows issue messages', () => {
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [makeIssue({ message: 'Dep cycle in MainPU' })] },
            isExpanded: true,
        });
        render(<ValidationBarV2 />);
        expect(screen.getByText(/Dep cycle in MainPU/)).toBeTruthy();
    });

    it('clicking PU-target issue switches to graph tab and selects PU', () => {
        const setActiveTab = vi.fn();
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [makeIssue({ targetKind: 'pu', targetPuId: 'MainPU' })] },
            isExpanded: true,
        });
        render(<ValidationBarV2 setActiveTab={setActiveTab} />);
        fireEvent.click(screen.getByTestId('issue-0'));
        expect(setActiveTab).toHaveBeenCalledWith('graph');
        expect(useSelectionStoreV2.getState().puId).toBe('MainPU');
    });

    it('clicking module-target issue switches to graph tab and selects parent PU', () => {
        const setActiveTab = vi.fn();
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [makeIssue({ targetKind: 'module', targetPuId: 'MainPU', targetModuleId: 'RenderModule' })] },
            isExpanded: true,
        });
        render(<ValidationBarV2 setActiveTab={setActiveTab} />);
        fireEvent.click(screen.getByTestId('issue-0'));
        expect(setActiveTab).toHaveBeenCalledWith('graph');
        expect(useSelectionStoreV2.getState().puId).toBe('MainPU');
    });

    it('clicking stream-target issue switches to streams tab and selects stream', () => {
        const setActiveTab = vi.fn();
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [makeIssue({ targetKind: 'stream', targetStreamId: 'BadStream' })] },
            isExpanded: true,
        });
        render(<ValidationBarV2 setActiveTab={setActiveTab} />);
        fireEvent.click(screen.getByTestId('issue-0'));
        expect(setActiveTab).toHaveBeenCalledWith('streams');
        expect(useSelectionStoreV2.getState().streamId).toBe('BadStream');
    });

    it('non-navigable issue (targetKind="") is clickless', () => {
        const setActiveTab = vi.fn();
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [makeIssue()] },
            isExpanded: true,
        });
        render(<ValidationBarV2 setActiveTab={setActiveTab} />);
        fireEvent.click(screen.getByTestId('issue-0'));
        expect(setActiveTab).not.toHaveBeenCalled();
    });

    it('Fix button dispatches manifest.applyCommand with verbatim suggestedCommand', () => {
        const cmd = { commandType: 'RemoveModuleRead', puId: 'MainPU', instanceId: 'RenderModule', streamId: 'BadStream' };
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [makeIssue({
                targetKind: 'module', targetPuId: 'MainPU', targetModuleId: 'RenderModule', targetStreamId: 'BadStream',
                suggestedActionLabel: 'Remove read',
                suggestedCommand: cmd,
            })] },
            isExpanded: true,
        });
        render(<ValidationBarV2 />);
        fireEvent.click(screen.getByTestId('fix-0'));
        expect(mockBridge).toHaveBeenCalledWith('manifest.applyCommand', cmd);
    });

    it('Fix click does not also navigate (stopPropagation)', () => {
        const setActiveTab = vi.fn();
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [makeIssue({
                targetKind: 'module', targetPuId: 'MainPU', targetModuleId: 'RenderModule',
                suggestedActionLabel: 'Remove read',
                suggestedCommand: { commandType: 'RemoveModuleRead' },
            })] },
            isExpanded: true,
        });
        render(<ValidationBarV2 setActiveTab={setActiveTab} />);
        fireEvent.click(screen.getByTestId('fix-0'));
        expect(setActiveTab).not.toHaveBeenCalled();
        expect(useSelectionStoreV2.getState().puId).toBeNull();
    });

    it('issue with no suggestedCommand renders no Fix button', () => {
        useValidationStoreV2.setState({
            result: { errorCount: 1, warningCount: 0, issues: [makeIssue({ targetKind: 'pu', targetPuId: 'MainPU' })] },
            isExpanded: true,
        });
        render(<ValidationBarV2 />);
        expect(screen.queryByTestId('fix-0')).toBeNull();
    });
});
