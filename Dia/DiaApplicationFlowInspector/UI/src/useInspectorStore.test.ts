import { describe, it, expect, beforeEach } from 'vitest';
import { useInspectorStore } from './useInspectorStore';

beforeEach(() => {
    useInspectorStore.getState().clearAll();
});

describe('useInspectorStore — timeline', () => {
    it('starts empty', () => {
        expect(useInspectorStore.getState().timeline).toEqual([]);
    });

    it('pushStageEntry adds entry', () => {
        useInspectorStore.getState().pushStageEntry({ stageName: 'Boot', enteredAtMs: 100 });
        expect(useInspectorStore.getState().timeline).toHaveLength(1);
        expect(useInspectorStore.getState().timeline[0].stageName).toBe('Boot');
    });

    it('caps at 1024 entries, drops oldest', () => {
        for (let i = 0; i < 1025; i++) {
            useInspectorStore.getState().pushStageEntry({ stageName: `S${i}`, enteredAtMs: i });
        }
        const t = useInspectorStore.getState().timeline;
        expect(t).toHaveLength(1024);
        expect(t[0].stageName).toBe('S1');
        expect(t[1023].stageName).toBe('S1024');
    });
});

describe('useInspectorStore — modules', () => {
    it('updateModuleState adds a module', () => {
        useInspectorStore.getState().updateModuleState({
            moduleId: 'mod1', puId: 'pu1', lifecycleState: 'Running',
            timeInStateMs: 100, timeoutMs: null, blockedByDep: null, errorMessage: null,
        });
        expect(useInspectorStore.getState().modules['mod1']).toBeDefined();
        expect(useInspectorStore.getState().modules['mod1'].lifecycleState).toBe('Running');
    });

    it('updateModuleState updates existing module', () => {
        useInspectorStore.getState().updateModuleState({
            moduleId: 'mod1', puId: 'pu1', lifecycleState: 'Running',
            timeInStateMs: 0, timeoutMs: null, blockedByDep: null, errorMessage: null,
        });
        useInspectorStore.getState().updateModuleState({
            moduleId: 'mod1', puId: 'pu1', lifecycleState: 'Failed',
            timeInStateMs: 500, timeoutMs: null, blockedByDep: null, errorMessage: 'crash',
        });
        expect(useInspectorStore.getState().modules['mod1'].lifecycleState).toBe('Failed');
        expect(useInspectorStore.getState().modules['mod1'].errorMessage).toBe('crash');
    });

    it('removeModule removes it', () => {
        useInspectorStore.getState().updateModuleState({
            moduleId: 'mod1', puId: 'pu1', lifecycleState: 'Stopped',
            timeInStateMs: 0, timeoutMs: null, blockedByDep: null, errorMessage: null,
        });
        useInspectorStore.getState().removeModule('mod1');
        expect(useInspectorStore.getState().modules['mod1']).toBeUndefined();
    });
});

describe('useInspectorStore — event log', () => {
    it('pushEventLogEntry adds entry', () => {
        useInspectorStore.getState().pushEventLogEntry('info', 'hello', 1000);
        expect(useInspectorStore.getState().eventLog).toHaveLength(1);
        expect(useInspectorStore.getState().eventLog[0].message).toBe('hello');
        expect(useInspectorStore.getState().eventLog[0].severity).toBe('info');
    });

    it('caps at 1024 entries, drops oldest', () => {
        for (let i = 0; i < 1025; i++) {
            useInspectorStore.getState().pushEventLogEntry('info', `msg${i}`, i);
        }
        const log = useInspectorStore.getState().eventLog;
        expect(log).toHaveLength(1024);
        expect(log[0].message).toBe('msg1');
        expect(log[1023].message).toBe('msg1024');
    });

    it('setSeverityFilter stores filter', () => {
        useInspectorStore.getState().setSeverityFilter('warn');
        expect(useInspectorStore.getState().severityFilter).toBe('warn');
    });
});

describe('useInspectorStore — clearAll', () => {
    it('resets all state', () => {
        useInspectorStore.getState().pushStageEntry({ stageName: 'Boot', enteredAtMs: 0 });
        useInspectorStore.getState().pushEventLogEntry('error', 'bad', 0);
        useInspectorStore.getState().clearAll();
        const s = useInspectorStore.getState();
        expect(s.timeline).toEqual([]);
        expect(s.eventLog).toEqual([]);
        expect(s.modules).toEqual({});
        expect(s.streams).toEqual({});
    });
});
