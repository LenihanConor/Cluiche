// DiaValueSchema Feature 7 task 10: per-prefix schema cache for the Entity Inspector.
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { SchemaCache } from './schemaCache';

const E = (key: string) => ({ for: 'foo-component', key, kind: 'component_field', description: 'd' });

describe('SchemaCache', () => {
    beforeEach(() => { vi.useFakeTimers(); });
    afterEach(() => { vi.useRealTimers(); });

    it('requests a prefix once however often ensure is called', () => {
        const request = vi.fn();
        const c = new SchemaCache(request);
        c.ensure('foo-component/');
        c.ensure('foo-component/');
        expect(request).toHaveBeenCalledTimes(1);
        expect(request).toHaveBeenCalledWith('foo-component/');
        c.receive('foo-component/', true, [E('speed')]);
        c.ensure('foo-component/');
        expect(request).toHaveBeenCalledTimes(1);
        expect(c.get('foo-component/')?.get('speed')?.description).toBe('d');
    });

    it('keeps one request in flight and sends the next prefix after the reply', () => {
        const request = vi.fn();
        const c = new SchemaCache(request);
        c.ensure('a/');
        c.ensure('b/');
        expect(request).toHaveBeenCalledTimes(1);
        c.receive('a/', true, []);
        expect(request).toHaveBeenCalledTimes(2);
        expect(request).toHaveBeenLastCalledWith('b/');
    });

    it('a failed reply stores an empty result and does not retry', () => {
        const request = vi.fn();
        const c = new SchemaCache(request);
        c.ensure('a/');
        c.receive('a/', false, []);
        c.ensure('a/');
        expect(request).toHaveBeenCalledTimes(1);
        expect(c.get('a/')?.size).toBe(0);
    });

    it('a timeout stores an empty result, advances the queue, and does not throw', () => {
        const request = vi.fn();
        const c = new SchemaCache(request, 1000);
        c.ensure('a/');
        c.ensure('b/');
        expect(() => vi.advanceTimersByTime(1000)).not.toThrow();
        expect(c.get('a/')?.size).toBe(0);
        expect(request).toHaveBeenLastCalledWith('b/');
    });

    it('a throwing request function is swallowed', () => {
        const c = new SchemaCache(() => { throw new Error('no bridge'); });
        expect(() => c.ensure('a/')).not.toThrow();
        expect(c.get('a/')?.size).toBe(0);
    });

    it('reset clears the cache so prefixes are fetched again', () => {
        const request = vi.fn();
        const c = new SchemaCache(request);
        c.ensure('a/');
        c.receive('a/', true, [E('x')]);
        c.reset();
        expect(c.get('a/')).toBeUndefined();
        c.ensure('a/');
        expect(request).toHaveBeenCalledTimes(2);
    });

    it('notifies subscribers when a reply arrives', () => {
        const c = new SchemaCache(vi.fn());
        const fn = vi.fn();
        c.subscribe(fn);
        c.ensure('a/');
        c.receive('a/', true, []);
        expect(fn).toHaveBeenCalled();
    });

    it('ignores a reply for a prefix that is not in flight', () => {
        const c = new SchemaCache(vi.fn());
        c.receive('zzz/', true, [E('x')]);
        expect(c.get('zzz/')).toBeUndefined();
    });
});
