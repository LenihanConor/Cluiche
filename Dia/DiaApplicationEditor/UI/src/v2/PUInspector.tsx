import React, { useState } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { bridgeRequest } from './bridge';

export interface PUInspectorProps {
    puId: string | null;
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
                            }}
                        >
                            {mod.instanceId}
                        </div>
                    ))
                )}
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
