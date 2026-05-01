import { useEffect } from 'react';
import { useManifestStore } from './ManifestStore';
import type { ValidationResult } from './ValidationPanel';

function debounce<T extends (...args: unknown[]) => void>(fn: T, ms: number): T {
    let timer: ReturnType<typeof setTimeout>;
    return ((...args: unknown[]) => {
        clearTimeout(timer);
        timer = setTimeout(() => fn(...args), ms);
    }) as T;
}

function sendToPlugin(type: string, data: object = {}): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

type BridgeMessage = {
    __dia?: boolean;
    topic?: string;
    data?: unknown;
};

export function useValidation(manifestVersion: number): void {
    const setValidationResult = useManifestStore(s => s.setValidationResult);
    const setManifest         = useManifestStore(s => s.setManifest);
    const setDirty            = useManifestStore(s => s.setDirty);

    useEffect(() => {
        const trigger = debounce(() => sendToPlugin('validate'), 500);
        trigger();
    }, [manifestVersion]);

    useEffect(() => {
        const handler = (event: MessageEvent<BridgeMessage>) => {
            if (!event.data?.__dia) return;
            const { topic, data } = event.data;

            if (topic === 'validation_complete') {
                setValidationResult(data as ValidationResult);
            } else if (topic === 'manifest_loaded') {
                const d = data as { manifest?: object; is_dirty?: boolean } | undefined;
                if (d?.manifest) setManifest(d.manifest);
                setDirty(d?.is_dirty ?? false);
            } else if (topic === 'manifest_updated') {
                const d = data as { manifest?: object; is_dirty?: boolean } | undefined;
                if (d?.manifest) setManifest(d.manifest);
                setDirty(d?.is_dirty ?? true);
            } else if (topic === 'manifest_saved') {
                setDirty(false);
            } else if (topic === 'manifest_closed') {
                setManifest(null);
                setDirty(false);
            }
        };
        window.addEventListener('message', handler);
        return () => window.removeEventListener('message', handler);
    }, [setValidationResult, setManifest, setDirty]);
}
