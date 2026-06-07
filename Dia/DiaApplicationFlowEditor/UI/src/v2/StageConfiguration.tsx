import React, { useState, useRef } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { bridgeRequest } from './bridge';

export const StageConfiguration: React.FC = () => {
    const manifest = useManifestStoreV2(s => s.manifest);
    const [addingStage, setAddingStage] = useState(false);
    const [editingStage, setEditingStage] = useState<string | null>(null);
    const [addingTransitionFor, setAddingTransitionFor] = useState<string | null>(null);
    const addInputRef = useRef<HTMLInputElement>(null);
    const editInputRef = useRef<HTMLInputElement>(null);
    const transitionInputRef = useRef<HTMLInputElement>(null);

    const stages = manifest?.stages ?? [];
    const initialStage = manifest?.initialStage ?? '';

    const handleAddStage = () => {
        const name = addInputRef.current?.value.trim();
        if (!name) { setAddingStage(false); return; }
        bridgeRequest('manifest.applyCommand', {
            commandType: 'AddStage',
            name,
            manifestPath: '',
        });
        setAddingStage(false);
    };

    const handleRemoveStage = (name: string) => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'RemoveStage',
            name,
        });
    };

    const handleSetInitial = (name: string) => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'SetInitialStage',
            name,
        });
    };

    const handleToggleAuto = (name: string, currentAutoAdvance: boolean) => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'SetStageTrigger',
            name,
            isAuto: !currentAutoAdvance,
        });
    };

    const handleRenameConfirm = (oldName: string) => {
        const newName = editInputRef.current?.value.trim();
        if (!newName || newName === oldName) { setEditingStage(null); return; }
        bridgeRequest('manifest.applyCommand', {
            commandType: 'RenameStage',
            oldName,
            newName,
        });
        setEditingStage(null);
    };

    const handleAddTransition = (stageName: string) => {
        const target = transitionInputRef.current?.value.trim();
        if (!target) { setAddingTransitionFor(null); return; }
        bridgeRequest('manifest.applyCommand', {
            commandType: 'AddStageTransition',
            name: stageName,
            target,
        });
        setAddingTransitionFor(null);
    };

    const handleRemoveTransition = (stageName: string, target: string) => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'RemoveStageTransition',
            name: stageName,
            target,
        });
    };

    return (
        <div
            data-testid="stage-config"
            style={{
                background: '#1e1e1e',
                color: '#ccc',
                padding: 8,
                fontSize: 13,
                minWidth: 220,
            }}
        >
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 8 }}>
                <span style={{ fontWeight: 600, fontSize: 14 }}>Stages</span>
                <button
                    data-testid="add-stage-btn"
                    onClick={() => setAddingStage(true)}
                    style={{
                        background: 'none',
                        border: '1px solid #444',
                        borderRadius: 3,
                        color: '#ccc',
                        cursor: 'pointer',
                        padding: '2px 10px',
                        fontSize: 14,
                        lineHeight: 1.4,
                    }}
                    title="Add stage"
                >
                    +
                </button>
            </div>

            {addingStage && (
                <div style={{ display: 'flex', gap: 4, marginBottom: 8 }}>
                    <input
                        ref={addInputRef}
                        autoFocus
                        placeholder="stage name"
                        style={{ background: '#2d2d2d', border: '1px solid #444', borderRadius: 3, color: '#eee', padding: '3px 8px', fontSize: 12, flex: 1 }}
                        onKeyDown={e => { if (e.key === 'Enter') handleAddStage(); if (e.key === 'Escape') setAddingStage(false); }}
                    />
                    <button onClick={handleAddStage} style={{ background: '#444', border: 'none', borderRadius: 3, color: '#eee', cursor: 'pointer', padding: '3px 10px', fontSize: 12 }}>Add</button>
                    <button onClick={() => setAddingStage(false)} style={{ background: 'none', border: 'none', color: '#888', cursor: 'pointer', fontSize: 12 }}>Cancel</button>
                </div>
            )}

            <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                {stages.map(stage => {
                    const isInitial = stage.name === initialStage;
                    const isAuto = stage.autoAdvance;
                    const transitions = stage.transitions ?? [];

                    return (
                        <div
                            key={stage.name}
                            data-testid="stage-row"
                            data-stage-name={stage.name}
                            style={{
                                background: '#2d2d2d',
                                borderRadius: 4,
                                padding: '4px 8px',
                            }}
                        >
                            {/* Name row */}
                            <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                                {editingStage === stage.name ? (
                                    <input
                                        ref={editInputRef}
                                        autoFocus
                                        defaultValue={stage.name}
                                        style={{ background: '#3a3a3a', border: '1px solid #555', borderRadius: 3, color: '#eee', padding: '1px 6px', fontSize: 12, flex: 1 }}
                                        onBlur={() => handleRenameConfirm(stage.name)}
                                        onKeyDown={e => { if (e.key === 'Enter') handleRenameConfirm(stage.name); if (e.key === 'Escape') setEditingStage(null); }}
                                    />
                                ) : (
                                    <span
                                        style={{ flex: 1, fontSize: 12, color: '#eee', cursor: 'text' }}
                                        onDoubleClick={() => setEditingStage(stage.name)}
                                    >
                                        {stage.name}
                                    </span>
                                )}

                                {/* Initial indicator */}
                                <div
                                    data-testid="initial-indicator"
                                    data-is-initial={isInitial}
                                    onClick={() => !isInitial && handleSetInitial(stage.name)}
                                    title={isInitial ? 'Initial stage' : 'Set as initial stage'}
                                    style={{
                                        width: 14, height: 14,
                                        borderRadius: '50%',
                                        background: isInitial ? '#3cb370' : '#444',
                                        border: `2px solid ${isInitial ? '#3cb370' : '#555'}`,
                                        cursor: isInitial ? 'default' : 'pointer',
                                        flexShrink: 0,
                                    }}
                                />

                                {/* Auto-advance indicator */}
                                <div
                                    data-testid="auto-indicator"
                                    data-is-auto={isAuto}
                                    onClick={() => handleToggleAuto(stage.name, isAuto)}
                                    title={isAuto ? 'Auto-advance (click to disable)' : 'Not auto-advance (click to enable)'}
                                    style={{
                                        fontSize: 11,
                                        color: isAuto ? '#f0a030' : '#555',
                                        cursor: 'pointer',
                                        userSelect: 'none',
                                        fontWeight: 600,
                                        minWidth: 24,
                                        textAlign: 'center',
                                    }}
                                >
                                    {isAuto ? 'A' : 'a'}
                                </div>

                                {/* Remove button */}
                                <button
                                    data-testid="remove-stage-btn"
                                    onClick={() => handleRemoveStage(stage.name)}
                                    style={{
                                        background: 'none', border: 'none', color: '#888',
                                        cursor: 'pointer', fontSize: 14, padding: '0 2px', lineHeight: 1,
                                    }}
                                    title={`Remove ${stage.name}`}
                                >
                                    ×
                                </button>
                            </div>

                            {/* Transitions row */}
                            <div
                                data-testid="transitions-row"
                                style={{ display: 'flex', flexWrap: 'wrap', gap: 4, marginTop: 4, alignItems: 'center' }}
                            >
                                {transitions.map(target => (
                                    <span
                                        key={target}
                                        data-testid="transition-chip"
                                        data-transition-target={target}
                                        style={{
                                            display: 'inline-flex', alignItems: 'center', gap: 3,
                                            background: '#3a3a3a', borderRadius: 10,
                                            padding: '1px 8px', fontSize: 11, color: '#aaa',
                                        }}
                                    >
                                        {target}
                                        <span
                                            data-testid="remove-transition-btn"
                                            onClick={() => handleRemoveTransition(stage.name, target)}
                                            style={{ cursor: 'pointer', color: '#666', fontSize: 12, lineHeight: 1 }}
                                            title={`Remove transition to ${target}`}
                                        >
                                            ×
                                        </span>
                                    </span>
                                ))}
                                {addingTransitionFor === stage.name ? (
                                    <>
                                        <input
                                            ref={transitionInputRef}
                                            autoFocus
                                            placeholder="target stage"
                                            style={{ background: '#3a3a3a', border: '1px solid #555', borderRadius: 3, color: '#eee', padding: '1px 6px', fontSize: 11, width: 90 }}
                                            onKeyDown={e => {
                                                if (e.key === 'Enter') handleAddTransition(stage.name);
                                                if (e.key === 'Escape') setAddingTransitionFor(null);
                                            }}
                                        />
                                        <button
                                            onClick={() => handleAddTransition(stage.name)}
                                            style={{ background: '#444', border: 'none', borderRadius: 3, color: '#eee', cursor: 'pointer', padding: '1px 6px', fontSize: 11 }}
                                        >
                                            Add
                                        </button>
                                        <button
                                            onClick={() => setAddingTransitionFor(null)}
                                            style={{ background: 'none', border: 'none', color: '#888', cursor: 'pointer', fontSize: 11 }}
                                        >
                                            Cancel
                                        </button>
                                    </>
                                ) : (
                                    <button
                                        data-testid="add-transition-btn"
                                        onClick={() => setAddingTransitionFor(stage.name)}
                                        style={{
                                            background: 'none', border: '1px solid #444', borderRadius: 10,
                                            color: '#666', cursor: 'pointer', padding: '1px 8px', fontSize: 11,
                                        }}
                                        title={`Add transition from ${stage.name}`}
                                    >
                                        + transition
                                    </button>
                                )}
                            </div>
                        </div>
                    );
                })}

                {stages.length === 0 && !addingStage && (
                    <div style={{ color: '#555', fontSize: 12, padding: '4px 8px' }}>No stages defined</div>
                )}
            </div>
        </div>
    );
};
