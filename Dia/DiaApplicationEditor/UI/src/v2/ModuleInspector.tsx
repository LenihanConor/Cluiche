import React, { useState, useRef } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { bridgeRequest } from './bridge';
import { TrafficLightDot } from './TrafficLightDot';

export interface ModuleInspectorProps {
    moduleId: string | null;
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
                    display: 'flex', alignItems: 'center', gap: 6, cursor: 'pointer',
                    padding: '4px 8px', background: '#2d2d2d', borderRadius: 4,
                    userSelect: 'none', fontSize: 13, color: '#ccc', fontWeight: 600,
                }}
            >
                <span>{open ? '▼' : '▶'}</span>
                <span>{title}</span>
            </div>
            {open && <div style={{ padding: '8px 8px 0 8px' }}>{children}</div>}
        </div>
    );
};

const chipStyle: React.CSSProperties = {
    display: 'inline-flex', alignItems: 'center', gap: 4,
    background: '#3a3a3a', borderRadius: 12, padding: '2px 8px',
    fontSize: 11, color: '#ccc', margin: '2px',
};

const removeBtn: React.CSSProperties = {
    background: 'none', border: 'none', color: '#888', cursor: 'pointer',
    fontSize: 12, padding: '0 2px', lineHeight: 1,
};

export const ModuleInspector: React.FC<ModuleInspectorProps> = ({ moduleId, puId }) => {
    const manifest = useManifestStoreV2(s => s.manifest);
    const [addingDep, setAddingDep] = useState(false);
    const [addingRead, setAddingRead] = useState(false);
    const [addingWrite, setAddingWrite] = useState(false);
    const depInputRef = useRef<HTMLInputElement>(null);
    const readInputRef = useRef<HTMLInputElement>(null);
    const writeInputRef = useRef<HTMLInputElement>(null);

    if (!moduleId || !puId) {
        return (
            <div data-testid="module-inspector" style={{ background: '#1e1e1e', color: '#888', padding: 16, fontSize: 13 }}>
                No module selected
            </div>
        );
    }

    let module = null;
    for (const pu of manifest?.processingUnits ?? []) {
        const found = pu.modules.find(m => m.instanceId === moduleId);
        if (found) { module = found; break; }
    }

    if (!module) {
        return (
            <div data-testid="module-inspector" style={{ background: '#1e1e1e', color: '#888', padding: 16, fontSize: 13 }}>
                Module not found: {moduleId}
            </div>
        );
    }

    const stages = manifest?.stages ?? [];
    const mod = module;

    const handleStartTimeout = (e: React.ChangeEvent<HTMLInputElement>) => {
        const value = parseInt(e.target.value, 10);
        if (isNaN(value)) return;
        bridgeRequest('manifest.applyCommand', {
            commandType: 'SetModuleStartTimeout',
            instanceId: moduleId,
            puId,
            startTimeoutMs: value,
        });
    };

    const handleStopTimeout = (e: React.ChangeEvent<HTMLInputElement>) => {
        const value = parseInt(e.target.value, 10);
        if (isNaN(value)) return;
        bridgeRequest('manifest.applyCommand', {
            commandType: 'SetModuleStopTimeout',
            instanceId: moduleId,
            puId,
            stopTimeoutMs: value,
        });
    };

    const handleStageDotClick = (stageName: string) => {
        const hasStage = mod.stages.includes(stageName);
        const newStages = hasStage
            ? mod.stages.filter(s => s !== stageName)
            : [...mod.stages, stageName];
        bridgeRequest('manifest.applyCommand', {
            commandType: 'SetModuleStages',
            instanceId: moduleId,
            puId,
            stages: newStages,
        });
    };

    const handleAddDep = () => {
        const val = depInputRef.current?.value.trim();
        if (!val) { setAddingDep(false); return; }
        bridgeRequest('manifest.applyCommand', {
            commandType: 'AddModuleDep',
            instanceId: moduleId,
            puId,
            dependency: val,
        });
        setAddingDep(false);
    };

    const handleRemoveDep = (dep: string) => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'RemoveModuleDep',
            instanceId: moduleId,
            puId,
            dependency: dep,
        });
    };

    const handleAddRead = () => {
        const val = readInputRef.current?.value.trim();
        if (!val) { setAddingRead(false); return; }
        bridgeRequest('manifest.applyCommand', {
            commandType: 'AddModuleRead',
            instanceId: moduleId,
            puId,
            streamId: val,
        });
        setAddingRead(false);
    };

    const handleRemoveRead = (streamId: string) => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'RemoveModuleRead',
            instanceId: moduleId,
            puId,
            streamId,
        });
    };

    const handleAddWrite = () => {
        const val = writeInputRef.current?.value.trim();
        if (!val) { setAddingWrite(false); return; }
        bridgeRequest('manifest.applyCommand', {
            commandType: 'AddModuleWrite',
            instanceId: moduleId,
            puId,
            streamId: val,
        });
        setAddingWrite(false);
    };

    const handleRemoveWrite = (streamId: string) => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'RemoveModuleWrite',
            instanceId: moduleId,
            puId,
            streamId,
        });
    };

    return (
        <div data-testid="module-inspector" style={{ background: '#1e1e1e', color: '#ccc', padding: 8, fontSize: 13, minWidth: 220 }}>
            {/* Properties */}
            <div style={{ marginBottom: 8 }}>
                <div style={{ padding: '4px 8px', background: '#2d2d2d', borderRadius: 4, fontSize: 13, color: '#ccc', fontWeight: 600, marginBottom: 6 }}>
                    Properties
                </div>
                <div style={{ padding: '0 8px', display: 'flex', flexDirection: 'column', gap: 6 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                        <span style={{ color: '#888', minWidth: 80 }}>instanceId</span>
                        <span style={{ color: '#eee' }}>{mod.instanceId}</span>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                        <span style={{ color: '#888', minWidth: 80 }}>typeId</span>
                        <span style={{ color: '#eee' }}>{mod.typeId}</span>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                        <label style={{ color: '#888', minWidth: 80 }} htmlFor="start-timeout">startTimeout</label>
                        <input
                            id="start-timeout"
                            data-testid="start-timeout-input"
                            type="number"
                            defaultValue={mod.startTimeoutMs}
                            onChange={handleStartTimeout}
                            style={{ background: '#2d2d2d', border: '1px solid #444', borderRadius: 3, color: '#eee', padding: '2px 6px', width: 80, fontSize: 13 }}
                        />
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                        <label style={{ color: '#888', minWidth: 80 }} htmlFor="stop-timeout">stopTimeout</label>
                        <input
                            id="stop-timeout"
                            data-testid="stop-timeout-input"
                            type="number"
                            defaultValue={mod.stopTimeoutMs}
                            onChange={handleStopTimeout}
                            style={{ background: '#2d2d2d', border: '1px solid #444', borderRadius: 3, color: '#eee', padding: '2px 6px', width: 80, fontSize: 13 }}
                        />
                    </div>
                </div>
            </div>

            {/* Stages */}
            <div style={{ marginBottom: 8 }}>
                <div style={{ padding: '4px 8px', background: '#2d2d2d', borderRadius: 4, fontSize: 13, color: '#ccc', fontWeight: 600, marginBottom: 6 }}>
                    Stages
                </div>
                <div style={{ padding: '0 8px', display: 'flex', flexWrap: 'wrap', gap: 6 }}>
                    {stages.map(stage => {
                        const active = mod.stages.includes(stage.name);
                        return (
                            <div
                                key={stage.name}
                                data-testid="stage-dot"
                                data-stage-name={stage.name}
                                onClick={() => handleStageDotClick(stage.name)}
                                style={{ display: 'flex', alignItems: 'center', gap: 4, cursor: 'pointer', userSelect: 'none' }}
                                title={stage.name}
                            >
                                <TrafficLightDot state={active ? 'green' : 'grey'} size={10} />
                                <span style={{ fontSize: 11, color: active ? '#3cb370' : '#666' }}>{stage.name}</span>
                            </div>
                        );
                    })}
                    {stages.length === 0 && <span style={{ color: '#666', fontSize: 11 }}>No stages defined</span>}
                </div>
            </div>

            {/* Dependencies */}
            <Section title="Dependencies" testId="deps-section" defaultOpen={true}>
                <div style={{ display: 'flex', flexWrap: 'wrap' }}>
                    {mod.dependencies.map(dep => (
                        <span key={dep} style={chipStyle}>
                            {dep}
                            <button style={removeBtn} onClick={() => handleRemoveDep(dep)} title={`Remove ${dep}`}>×</button>
                        </span>
                    ))}
                </div>
                {addingDep ? (
                    <div style={{ display: 'flex', gap: 4, marginTop: 4 }}>
                        <input
                            ref={depInputRef}
                            autoFocus
                            placeholder="dependency id"
                            style={{ background: '#2d2d2d', border: '1px solid #444', borderRadius: 3, color: '#eee', padding: '2px 6px', fontSize: 12, flex: 1 }}
                            onKeyDown={e => { if (e.key === 'Enter') handleAddDep(); if (e.key === 'Escape') setAddingDep(false); }}
                        />
                        <button onClick={handleAddDep} style={{ background: '#444', border: 'none', borderRadius: 3, color: '#eee', cursor: 'pointer', padding: '2px 8px', fontSize: 12 }}>Add</button>
                        <button onClick={() => setAddingDep(false)} style={{ background: 'none', border: 'none', color: '#888', cursor: 'pointer', fontSize: 12 }}>Cancel</button>
                    </div>
                ) : (
                    <button
                        data-testid="add-dep-btn"
                        onClick={() => setAddingDep(true)}
                        style={{ background: 'none', border: '1px solid #444', borderRadius: 3, color: '#888', cursor: 'pointer', padding: '2px 8px', fontSize: 12, marginTop: 4 }}
                    >
                        +
                    </button>
                )}
            </Section>

            {/* Reads/Writes */}
            <Section title="Streams" testId="streams-section" defaultOpen={true}>
                <div style={{ marginBottom: 8 }}>
                    <div style={{ fontSize: 11, color: '#888', marginBottom: 4 }}>Reads</div>
                    <div style={{ display: 'flex', flexWrap: 'wrap' }}>
                        {mod.reads.map(r => (
                            <span key={r} style={chipStyle}>
                                {r}
                                <button style={removeBtn} onClick={() => handleRemoveRead(r)} title={`Remove ${r}`}>×</button>
                            </span>
                        ))}
                    </div>
                    {addingRead ? (
                        <div style={{ display: 'flex', gap: 4, marginTop: 4 }}>
                            <input
                                ref={readInputRef}
                                autoFocus
                                placeholder="stream id"
                                style={{ background: '#2d2d2d', border: '1px solid #444', borderRadius: 3, color: '#eee', padding: '2px 6px', fontSize: 12, flex: 1 }}
                                onKeyDown={e => { if (e.key === 'Enter') handleAddRead(); if (e.key === 'Escape') setAddingRead(false); }}
                            />
                            <button onClick={handleAddRead} style={{ background: '#444', border: 'none', borderRadius: 3, color: '#eee', cursor: 'pointer', padding: '2px 8px', fontSize: 12 }}>Add</button>
                        </div>
                    ) : (
                        <button onClick={() => setAddingRead(true)} style={{ background: 'none', border: '1px solid #444', borderRadius: 3, color: '#888', cursor: 'pointer', padding: '2px 8px', fontSize: 12, marginTop: 4 }}>+</button>
                    )}
                </div>
                <div>
                    <div style={{ fontSize: 11, color: '#888', marginBottom: 4 }}>Writes</div>
                    <div style={{ display: 'flex', flexWrap: 'wrap' }}>
                        {mod.writes.map(w => (
                            <span key={w} style={chipStyle}>
                                {w}
                                <button style={removeBtn} onClick={() => handleRemoveWrite(w)} title={`Remove ${w}`}>×</button>
                            </span>
                        ))}
                    </div>
                    {addingWrite ? (
                        <div style={{ display: 'flex', gap: 4, marginTop: 4 }}>
                            <input
                                ref={writeInputRef}
                                autoFocus
                                placeholder="stream id"
                                style={{ background: '#2d2d2d', border: '1px solid #444', borderRadius: 3, color: '#eee', padding: '2px 6px', fontSize: 12, flex: 1 }}
                                onKeyDown={e => { if (e.key === 'Enter') handleAddWrite(); if (e.key === 'Escape') setAddingWrite(false); }}
                            />
                            <button onClick={handleAddWrite} style={{ background: '#444', border: 'none', borderRadius: 3, color: '#eee', cursor: 'pointer', padding: '2px 8px', fontSize: 12 }}>Add</button>
                        </div>
                    ) : (
                        <button onClick={() => setAddingWrite(true)} style={{ background: 'none', border: '1px solid #444', borderRadius: 3, color: '#888', cursor: 'pointer', padding: '2px 8px', fontSize: 12, marginTop: 4 }}>+</button>
                    )}
                </div>
            </Section>

            {/* Provenance */}
            <div style={{ padding: '6px 8px', fontSize: 11, color: '#666', borderTop: '1px solid #333', marginTop: 4 }}>
                Parent PU: <span style={{ color: '#888' }}>{puId}</span>
            </div>
        </div>
    );
};
