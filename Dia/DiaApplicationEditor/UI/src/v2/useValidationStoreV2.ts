import { create } from 'zustand';
import { bridgeRequest } from './bridge';

export interface ValidationIssueV2 {
    ruleId: number;
    severity: 'error' | 'warning';
    message: string;
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

export const useValidationStoreV2 = create<ValidationStoreV2State>((set) => ({
    result: null,
    isExpanded: false,

    setResult: (result) => set({ result }),
    toggleExpanded: () => set(s => ({ isExpanded: !s.isExpanded })),

    runValidation: async () => {
        const res = await bridgeRequest('validation.run') as any;
        if (res?.ok) {
            set({ result: { errorCount: res.errorCount ?? 0, warningCount: res.warningCount ?? 0, issues: res.issues ?? [] } });
        }
    },
}));
