import React, { useState, useEffect, useRef } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { bridgeRequest } from './bridge';

export interface PUInspectorProps {
    puId: string | null;
}

interface ModuleType {
    id: string;
    displayName?: string;
}

interface TypesGetResult {
    ok?: boolean;
    moduleTypes?: ModuleType[];
}

interface SectionProps {
    title: string;
    testId: string;
    defaultOpen?: boolean;
    children: React.ReactNode;
}

const Section: React.FC<SectionProps> = ({ title, testId, defaultOpen = true, children }) => {
    const [open, setOpen] = useState(defaultOpen);

    return (
        <div data-testid={testId} style={{ marginBottom: 8 }}>
            <div
                onClick={() => setOpen(o => !o)}
                style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: 6,
                    cursor: 'pointer',
                    padding: '4px 8px',
                    background: '#2d2d2d',
                    borderRadius: 4,
                    userSelect: 'none',
                    fontSize: 13,
                    color: '#ccc',
                    fontWeight: 600,
                }}
            >
                <span>{open ? '▼' : '▶'}</span>
                <span>{title}</span>
            </div>
            {open && (
                <div style={{ padding: '8px 8px 0 8px' }}>
                    {children}
                </div>
            )}
        </div>
    );
};

interface AddModuleAffordanceProps {
    puId: string;
    existingModuleIds: string[];
}

const AddModuleAffordance: React.FC<AddModuleAffordanceProps> = ({ puId, existingModuleIds }) => {
    const [open, setOpen] = useState(false);
    const [instanceId, setInstanceId] = useState('');
    const [typeId, setTypeId] = useState('');
    const [moduleTypes, setModuleTypes] = useState<ModuleType[]>([]);
    const inputRef = useRef<HTMLInputElement>(null);

    useEffect(() => {
        if (!open) return;
        let cancelled = false;
        bridgeRequest('types.get').then((res) => {
            if (cancelled) return;
            const r = res as TypesGetResult;
            setModuleTypes(r?.moduleTypes ?? []);
        });
        return () => { cancelled = true; };
    }, [open]);

    const trimmed = instanceId.trim();
    const isDuplicate = trimmed.length > 0 && existingModuleIds.includes(trimmed);
    const canSubmit = trimmed.length > 0 && typeId.length > 0 && !isDuplicate;

    const reset = () => {
        setInstanceId('');
        setTypeId('');
        setOpen(false);
    };

    const handleAdd = () => {
        if (!canSubmit) return;
        bridgeRequest('manifest.applyCommand', {
            commandType: 'AddModule',
            puId,
            instanceId: trimmed,
            typeId,
        });
        reset();
    };

    const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement | HTMLSelectElement>) => {
        if (e.key === 'Enter') handleAdd();
        else if (e.key === 'Escape') reset();
    };

    if (!open) {
        return (
            <button
                data-testid="add-module-btn"
                onClick={() => setOpen(true)}
                style={{
                    background: 'none', border: '1px solid #444', borderRadius: 3,
                    color: '#888', cursor: 'pointer', padding: '4px 10px', fontSize: 12,
                    marginTop: 4, width: '100%', textAlign: 'left',
                }}
            >
                + Add Module
            </button>
        );
    }

    return (
        <div
            data-testid="add-module-form"
            style={{
                background: '#252526', border: '1px solid #444', borderRadius: 4,
                padding: 8, marginTop: 4, display: 'flex', flexDirection: 'column', gap: 6,
            }}
        >
            <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                <label style={{ color: '#888', minWidth: 70, fontSize: 11 }}>instanceId</label>
                <input
                    ref={inputRef}
                    autoFocus
                    data-testid="add-module-instance-input"
                    type="text"
                    placeholder="e.g. AudioModule"
                    value={instanceId}
                    onChange={(e) => setInstanceId(e.target.value)}
                    onKeyDown={handleKeyDown}
                    style={{
                        flex: 1, background: '#2d2d2d', borderRadius: 3, color: '#eee',
                        padding: '2px 6px', fontSize: 12,
                        border: isDuplicate ? '1px solid #e87878' : '1px solid #444',
                    }}
                />
            </div>
            {isDuplicate && (
                <div data-testid="add-module-error" style={{ color: '#e87878', fontSize: 11 }}>
                    Already used in this PU.
                </div>
            )}
            <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                <label style={{ color: '#888', minWidth: 70, fontSize: 11 }}>type</label>
                <select
                    data-testid="add-module-type-select"
                    value={typeId}
                    onChange={(e) => setTypeId(e.target.value)}
                    onKeyDown={handleKeyDown}
                    style={{
                        flex: 1, background: '#2d2d2d', border: '1px solid #444',
                        borderRadius: 3, color: '#eee', padding: '2px 6px', fontSize: 12,
                    }}
                >
                    <option value="" disabled>pick a type…</option>
                    {moduleTypes.map(t => (
                        <option key={t.id} value={t.id}>
                            {t.displayName ? `${t.id} — ${t.displayName}` : t.id}
                        </option>
                    ))}
                </select>
            </div>
            <div style={{ display: 'flex', gap: 6, justifyContent: 'flex-end', marginTop: 4 }}>
                <button
                    data-testid="add-module-cancel"
                    onClick={reset}
                    style={{ background: 'none', border: '1px solid #444', borderRadius: 3, color: '#888', cursor: 'pointer', padding: '3px 12px', fontSize: 12 }}
                >
                    Cancel
                </button>
                <button
                    data-testid="add-module-confirm"
                    onClick={handleAdd}
                    disabled={!canSubmit}
                    style={{
                        background: canSubmit ? '#2e7d4a' : '#444',
                        border: 'none', borderRadius: 3,
                        color: canSubmit ? '#fff' : '#666',
                        cursor: canSubmit ? 'pointer' : 'not-allowed',
                        padding: '3px 12px', fontSize: 12,
                    }}
                >
                    Add
                </button>
            </div>
        </div>
    );
};

export const PUInspector: React.FC<PUInspectorProps> = ({ puId }) => {
    const manifest = useManifestStoreV2(s => s.manifest);

    if (!puId) {
        return (
            <div
                data-testid="pu-inspector"
                style={{
                    background: '#1e1e1e',
                    color: '#888',
                    padding: 16,
                    fontSize: 13,
                }}
            >
                No PU selected
            </div>
        );
    }

    const pu = manifest?.processingUnits.find(p => p.instanceId === puId) ?? null;

    if (!pu) {
        return (
            <div
                data-testid="pu-inspector"
                style={{ background: '#1e1e1e', color: '#888', padding: 16, fontSize: 13 }}
            >
                PU not found: {puId}
            </div>
        );
    }

    const handleFreqChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        const value = parseFloat(e.target.value);
        if (isNaN(value)) return;
        bridgeRequest('manifest.applyCommand', {
            commandType: 'SetPUFrequency',
            instanceId: puId,
            frequencyHz: value,
        });
    };

    const handleThreadChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'SetPUThread',
            instanceId: puId,
            dedicatedThread: e.target.checked,
        });
    };

    const handleRemoveModule = (moduleInstanceId: string) => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'RemoveModule',
            puId,
            instanceId: moduleInstanceId,
        });
    };

    return (
        <div
            data-testid="pu-inspector"
            style={{
                background: '#1e1e1e',
                color: '#ccc',
                padding: 8,
                fontSize: 13,
                minWidth: 220,
            }}
        >
            {/* Properties — always open (no collapse) */}
            <div style={{ marginBottom: 8 }}>
                <div style={{
                    padding: '4px 8px',
                    background: '#2d2d2d',
                    borderRadius: 4,
                    fontSize: 13,
                    color: '#ccc',
                    fontWeight: 600,
                    marginBottom: 6,
                }}>
                    Properties
                </div>
                <div style={{ padding: '0 8px', display: 'flex', flexDirection: 'column', gap: 6 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                        <span style={{ color: '#888', minWidth: 80 }}>instanceId</span>
                        <span style={{ color: '#eee' }}>{pu.instanceId}</span>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                        <label style={{ color: '#888', minWidth: 80 }} htmlFor="freq-input">
                            frequencyHz
                        </label>
                        <input
                            id="freq-input"
                            data-testid="freq-input"
                            type="number"
                            value={pu.frequencyHz}
                            onBlur={handleFreqChange}
                            onChange={handleFreqChange}
                            style={{
                                background: '#2d2d2d',
                                border: '1px solid #444',
                                borderRadius: 3,
                                color: '#eee',
                                padding: '2px 6px',
                                width: 80,
                                fontSize: 13,
                            }}
                        />
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                        <label style={{ color: '#888', minWidth: 80 }} htmlFor="dedicated-thread">
                            dedicatedThread
                        </label>
                        <input
                            id="dedicated-thread"
                            data-testid="dedicated-thread-checkbox"
                            type="checkbox"
                            checked={pu.dedicatedThread}
                            onChange={handleThreadChange}
                        />
                    </div>
                </div>
            </div>

            {/* Modules — expandable, default open */}
            <Section title="Modules" testId="modules-section" defaultOpen={true}>
                {pu.modules.length === 0 ? (
                    <span style={{ color: '#666', fontSize: 12 }}>No modules</span>
                ) : (
                    pu.modules.map(mod => (
                        <div
                            key={mod.instanceId}
                            data-testid="module-card"
                            data-module-id={mod.instanceId}
                            style={{
                                background: '#2d2d2d',
                                borderRadius: 4,
                                padding: '4px 8px',
                                marginBottom: 4,
                                fontSize: 12,
                                color: '#ccc',
                                cursor: 'default',
                                display: 'flex',
                                alignItems: 'center',
                                justifyContent: 'space-between',
                                gap: 6,
                            }}
                        >
                            <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                                {mod.instanceId}
                            </span>
                            <button
                                data-testid="remove-module-btn"
                                data-module-id={mod.instanceId}
                                onClick={() => handleRemoveModule(mod.instanceId)}
                                title={`Remove ${mod.instanceId}`}
                                style={{ background: 'none', border: 'none', color: '#888', cursor: 'pointer', fontSize: 14, padding: '0 4px', lineHeight: 1 }}
                            >
                                ×
                            </button>
                        </div>
                    ))
                )}
                <AddModuleAffordance puId={puId} existingModuleIds={pu.modules.map(m => m.instanceId)} />
            </Section>

            {/* Dependency Order — expandable, default collapsed */}
            <Section title="Dependency Order" testId="dep-order-section" defaultOpen={false}>
                {pu.modules.length === 0 ? (
                    <span style={{ color: '#666', fontSize: 12 }}>No modules</span>
                ) : (
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 3 }}>
                        {pu.modules.map((mod, idx) => (
                            <div
                                key={mod.instanceId}
                                style={{ fontSize: 12, color: '#aaa', padding: '2px 4px' }}
                            >
                                {idx + 1}. {mod.instanceId}
                            </div>
                        ))}
                    </div>
                )}
            </Section>
        </div>
    );
};
