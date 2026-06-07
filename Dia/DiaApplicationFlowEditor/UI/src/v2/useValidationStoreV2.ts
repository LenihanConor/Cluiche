import { create } from 'zustand';
import { bridgeRequest } from './bridge';

export type ValidationTargetKind = '' | 'pu' | 'module' | 'stream';

export interface ValidationIssueV2 {
    ruleId: number;
    severity: 'error' | 'warning';
    message: string;
    targetKind: ValidationTargetKind;
    targetPuId: string;
    targetModuleId: string;
    targetStreamId: string;
    suggestedActionLabel: string;
    suggestedCommand: Record<string, unknown> | null;
}

export interface ValidationResultV2 {
    errorCount: number;
    warningCount: number;
    issues: ValidationIssueV2[];
}

interface ValidationStoreV2State {
    result: ValidationResultV2 | null;
    isExpanded: boolean;

    setResult: (result: ValidationResultV2) => void;
    toggleExpanded: () => void;
    runValidation: () => Promise<void>;
}

export function normalizeValidationResult(raw: any): ValidationResultV2 {
    const issues: ValidationIssueV2[] = (raw?.issues ?? []).map((i: any) => ({
        ruleId: i.ruleId,
        severity: i.severity,
        message: i.message,
        targetKind: (i.targetKind ?? '') as ValidationTargetKind,
        targetPuId: i.targetPuId ?? '',
        targetModuleId: i.targetModuleId ?? '',
        targetStreamId: i.targetStreamId ?? '',
        suggestedActionLabel: i.suggestedActionLabel ?? '',
        suggestedCommand: i.suggestedCommand ?? null,
    }));
    return { errorCount: raw?.errorCount ?? 0, warningCount: raw?.warningCount ?? 0, issues };
}

export const useValidationStoreV2 = create<ValidationStoreV2State>((set) => ({
    result: null,
    isExpanded: false,

    setResult: (result) => set({ result }),
    toggleExpanded: () => set(s => ({ isExpanded: !s.isExpanded })),

    runValidation: async () => {
        const res = await bridgeRequest('validation.run') as any;
        if (res?.ok) set({ result: normalizeValidationResult(res) });
    },
}));
