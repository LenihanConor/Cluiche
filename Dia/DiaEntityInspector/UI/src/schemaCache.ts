// Per-prefix cache of `valueschema.get` results for the Entity Inspector
// (DiaValueSchema Feature 7 task 10). The request leaves through the plugin bridge
// (entity_inspector.schema_get) and the reply comes back as entity_inspector.schema_data,
// which App feeds into receive(). One request is in flight at a time (the editor-side
// game connection matches replies by command name); a failure or timeout caches an empty
// result so rows render as before and nothing is re-requested until reset().
import { useEffect, useState } from 'react';
import type { ValueInfoLike } from '@dia/editor-ui';

export type SchemaEntry = Partial<ValueInfoLike> & { key: string; for?: string };
export type SchemaMap = Map<string, SchemaEntry>;

export class SchemaCache {
    private loaded = new Map<string, SchemaMap>();
    private queue: string[] = [];
    private inFlight: string | null = null;
    private timer: ReturnType<typeof setTimeout> | null = null;
    private listeners = new Set<() => void>();
    private version = 0;

    constructor(private request: (prefix: string) => void, private timeoutMs = 5000) {}

    get(prefix: string): SchemaMap | undefined {
        return this.loaded.get(prefix);
    }

    getVersion(): number {
        return this.version;
    }

    ensure(prefix: string): void {
        if (this.loaded.has(prefix) || this.inFlight === prefix || this.queue.includes(prefix)) return;
        this.queue.push(prefix);
        this.pump();
    }

    receive(prefix: string, success: boolean, entries: SchemaEntry[]): void {
        if (this.inFlight !== prefix) return;
        this.settle(prefix, success ? entries : []);
    }

    reset(): void {
        this.clearTimer();
        this.loaded.clear();
        this.queue = [];
        this.inFlight = null;
        this.notify();
    }

    subscribe(fn: () => void): () => void {
        this.listeners.add(fn);
        return () => { this.listeners.delete(fn); };
    }

    private pump(): void {
        if (this.inFlight !== null || this.queue.length === 0) return;
        const prefix = this.queue.shift()!;
        this.inFlight = prefix;
        try {
            this.request(prefix);
        } catch {
            this.settle(prefix, []);
            return;
        }
        this.timer = setTimeout(() => {
            if (this.inFlight === prefix) this.settle(prefix, []);
        }, this.timeoutMs);
    }

    private settle(prefix: string, entries: SchemaEntry[]): void {
        this.clearTimer();
        const map: SchemaMap = new Map();
        for (const e of entries) {
            if (e && typeof e.key === 'string') map.set(e.key, e);
        }
        this.loaded.set(prefix, map);
        this.inFlight = null;
        this.notify();
        this.pump();
    }

    private clearTimer(): void {
        if (this.timer !== null) { clearTimeout(this.timer); this.timer = null; }
    }

    private notify(): void {
        this.version++;
        this.listeners.forEach((fn) => fn());
    }
}

/** Re-render the caller whenever the cache changes. */
export function useSchemaCacheVersion(cache: SchemaCache | undefined): number {
    const [version, setVersion] = useState(cache ? cache.getVersion() : 0);
    useEffect(() => {
        if (!cache) return;
        setVersion(cache.getVersion());
        return cache.subscribe(() => setVersion(cache.getVersion()));
    }, [cache]);
    return version;
}
