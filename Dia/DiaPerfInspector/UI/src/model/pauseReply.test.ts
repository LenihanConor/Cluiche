import { interpretPauseReply } from './pauseReply';

describe('interpretPauseReply', () => {
    it('reads paused true/false', () => {
        expect(interpretPauseReply({ success: true, result: { kind: 'module', id: 'A', paused: true } })).toEqual({ kind: 'ok', paused: true });
        expect(interpretPauseReply({ success: true, result: { paused: false } })).toEqual({ kind: 'ok', paused: false });
    });
    it('reads a data-wrapped reply', () => {
        expect(interpretPauseReply({ success: true, result: { success: true, data: { paused: true } } })).toEqual({ kind: 'ok', paused: true });
    });
    it('treats an empty success (resume_all) as ok without a paused flag', () => {
        expect(interpretPauseReply({ success: true, result: {} })).toEqual({ kind: 'ok', paused: null });
    });
    it('reports game errors', () => {
        expect(interpretPauseReply({ success: true, result: { error: 'refused' } })).toEqual({ kind: 'error', message: 'refused' });
    });
    it('detects unknown-command replies (Release builds)', () => {
        expect(interpretPauseReply({ success: false, result: { success: false, message: 'Unknown command' } }).kind).toBe('unsupported');
        expect(interpretPauseReply({ success: false, result: { success: false, error: 'command not found' } }).kind).toBe('unsupported');
    });
    it('does not throw on garbage', () => {
        expect(interpretPauseReply(null).kind).toBe('error');
        expect(interpretPauseReply({ success: false, result: { error: 'query timeout: x' } }).kind).toBe('error');
    });
});
