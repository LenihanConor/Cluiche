vi.mock('./bridge', () => ({ sendPerfCommand: vi.fn() }));

import { sendPerfCommand } from './bridge';
import { usePauseStore } from './usePauseStore';

const mockSend = vi.mocked(sendPerfCommand);
const ok = (paused: boolean) => ({ success: true, result: { kind: 'module', id: 'A', paused } });

beforeEach(() => {
    mockSend.mockReset();
    usePauseStore.getState().reset();
});

describe('usePauseStore', () => {
    it('toggle on a running module sends perf.pause with the module name and records paused', async () => {
        mockSend.mockResolvedValue(ok(true) as never);
        await usePauseStore.getState().toggle('Main', 'A');
        expect(mockSend).toHaveBeenCalledWith('perf.pause', { kind: 'module', id: 'A' });
        expect(usePauseStore.getState().paused['Main/A']).toBe(true);
    });

    it('toggle on a paused module sends perf.resume and reflects paused:false', async () => {
        mockSend.mockResolvedValueOnce(ok(true) as never);
        await usePauseStore.getState().toggle('Main', 'A');
        mockSend.mockResolvedValueOnce(ok(false) as never);
        await usePauseStore.getState().toggle('Main', 'A');
        expect(mockSend).toHaveBeenLastCalledWith('perf.resume', { kind: 'module', id: 'A' });
        expect(usePauseStore.getState().paused['Main/A']).toBe(false);
    });

    it('resume reply with paused:true (another client holds it) keeps it paused', async () => {
        usePauseStore.setState({ paused: { 'Main/A': true } });
        mockSend.mockResolvedValue(ok(true) as never);
        await usePauseStore.getState().toggle('Main', 'A');
        expect(usePauseStore.getState().paused['Main/A']).toBe(true);
    });

    it('a game error is stored per row and state unchanged', async () => {
        mockSend.mockResolvedValue({ success: true, result: { error: 'cannot pause KernelModule' } } as never);
        await usePauseStore.getState().toggle('Main', 'A');
        const s = usePauseStore.getState();
        expect(s.errors['Main/A']).toBe('cannot pause KernelModule');
        expect(s.paused['Main/A']).toBeFalsy();
        mockSend.mockResolvedValue(ok(true) as never);
        await usePauseStore.getState().toggle('Main', 'A');
        expect(usePauseStore.getState().errors['Main/A']).toBeUndefined();
    });

    it('an unknown-command reply disables the switches with a reason', async () => {
        mockSend.mockResolvedValue({ success: false, result: { success: false, message: 'Unknown command' } } as never);
        await usePauseStore.getState().toggle('Main', 'A');
        expect(usePauseStore.getState().unsupported).toMatch(/Debug/);
    });

    it('once unsupported, toggle sends nothing', async () => {
        usePauseStore.setState({ unsupported: 'x' });
        await usePauseStore.getState().toggle('Main', 'A');
        expect(mockSend).not.toHaveBeenCalled();
    });

    it('resumeAll sends perf.resume_all and clears paused flags', async () => {
        usePauseStore.setState({ paused: { 'Main/A': true, 'Sim/X': true } });
        mockSend.mockResolvedValue({ success: true, result: {} } as never);
        await usePauseStore.getState().resumeAll();
        expect(mockSend).toHaveBeenCalledWith('perf.resume_all', {});
        expect(Object.values(usePauseStore.getState().paused).some(Boolean)).toBe(false);
    });

    it('reset (disconnect) clears everything including unsupported', () => {
        usePauseStore.setState({ paused: { 'Main/A': true }, unsupported: 'x', errors: { a: 'b' } });
        usePauseStore.getState().reset();
        const s = usePauseStore.getState();
        expect(s.paused).toEqual({});
        expect(s.unsupported).toBeNull();
        expect(s.errors).toEqual({});
    });
});
