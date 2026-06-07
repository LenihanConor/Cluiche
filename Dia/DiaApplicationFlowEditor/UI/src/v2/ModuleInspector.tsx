import React, { useState, useRef } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { bridgeRequest } from './bridge';
import { TrafficLightDot } from './TrafficLightDot';
import type { ChannelRole } from './types';

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
    const [addingChannel, setAddingChannel] = useState(false);
    const [newChannelRole, setNewChannelRole] = useState<ChannelRole>('reads');
    const depInputRef = useRef<HTMLInputElement>(null);
    const channelInputRef = useRef<HTMLInputElement>(null);

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

    const handleAddChannel = () => {
        const val = channelInputRef.current?.value.trim();
        if (!val) { setAddingChannel(false); return; }
        bridgeRequest('manifest.applyCommand', {
            commandType: 'AddModuleChannel',
            instanceId: moduleId,
            puId,
            streamId: val,
            role: newChannelRole,
        });
        setAddingChannel(false);
    };

    const handleRemoveChannel = (streamId: string, role: ChannelRole) => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'RemoveModuleChannel',
            instanceId: moduleId,
            puId,
            streamId,
            role,
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

            {/* Channels */}
            <Section title="Channels" testId="streams-section" defaultOpen={true}>
                {(['reads', 'writes', 'provides', 'consumes'] as ChannelRole[]).map(role => {
                    const bindings = mod.channels.filter(c => c.role === role);
                    const roleColor: Record<ChannelRole, string> = {
                        reads: '#5b9bd5', writes: '#e8a838', provides: '#c8a0e0', consumes: '#c8a0e0',
                    };
                    if (bindings.length === 0) return null;
                    return (
                        <div key={role} style={{ marginBottom: 6 }}>
                            <div style={{ fontSize: 11, color: '#888', marginBottom: 3, textTransform: 'capitalize' }}>{role}</div>
                            <div style={{ display: 'flex', flexWrap: 'wrap' }}>
                                {bindings.map(ch => (
                                    <span key={ch.id + ':' + ch.role} style={{ ...chipStyle, borderLeft: `3px solid ${roleColor[role]}` }}>
                                        {ch.id}
                                        <button style={removeBtn} onClick={() => handleRemoveChannel(ch.id, ch.role as ChannelRole)} title={`Remove ${ch.id}`}>×</button>
                                    </span>
                                ))}
                            </div>
                        </div>
                    );
                })}
                {addingChannel ? (
                    <div style={{ display: 'flex', gap: 4, marginTop: 4 }}>
                        <select
                            data-testid="channel-role-select"
                            value={newChannelRole}
                            onChange={e => setNewChannelRole(e.target.value as ChannelRole)}
                            style={{ background: '#2d2d2d', border: '1px solid #444', borderRadius: 3, color: '#eee', padding: '2px 4px', fontSize: 12 }}
                        >
                            <option value="reads">reads</option>
                            <option value="writes">writes</option>
                            <option value="provides">provides</option>
                            <option value="consumes">consumes</option>
                        </select>
                        <input
                            ref={channelInputRef}
                            autoFocus
                            placeholder="stream id"
                            style={{ background: '#2d2d2d', border: '1px solid #444', borderRadius: 3, color: '#eee', padding: '2px 6px', fontSize: 12, flex: 1 }}
                            onKeyDown={e => { if (e.key === 'Enter') handleAddChannel(); if (e.key === 'Escape') setAddingChannel(false); }}
                        />
                        <button onClick={handleAddChannel} style={{ background: '#444', border: 'none', borderRadius: 3, color: '#eee', cursor: 'pointer', padding: '2px 8px', fontSize: 12 }}>Add</button>
                    </div>
                ) : (
                    <button data-testid="add-channel-btn" onClick={() => setAddingChannel(true)} style={{ background: 'none', border: '1px solid #444', borderRadius: 3, color: '#888', cursor: 'pointer', padding: '2px 8px', fontSize: 12, marginTop: 4 }}>+</button>
                )}
            </Section>

            {/* Provenance */}
            <div style={{ padding: '6px 8px', fontSize: 11, color: '#666', borderTop: '1px solid #333', marginTop: 4 }}>
                Parent PU: <span style={{ color: '#888' }}>{puId}</span>
            </div>
        </div>
    );
};
