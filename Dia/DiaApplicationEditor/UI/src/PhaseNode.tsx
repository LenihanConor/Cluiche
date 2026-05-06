import React from 'react';
import { Handle, Position } from 'reactflow';
import type { NodeProps } from 'reactflow';
import type { PhaseData, ModuleData } from './types';

export interface PhaseNodeData {
    phase: PhaseData;
    modules: ModuleData[];
    puId: string;
    isSelected: boolean;
    isCurrent: boolean;
}

export const PHASE_COLORS: { label: string; color: string; description: string }[] = [
    { label: 'Init / Boot', color: '#2e7d32', description: 'Startup phases — resource loading, subsystem initialization, one-time setup' },
    { label: 'Update / Running', color: '#1565c0', description: 'Main loop phases — per-frame logic, simulation, input processing' },
    { label: 'Shutdown', color: '#b71c1c', description: 'Teardown phases — resource release, cleanup, save state' },
    { label: 'Other', color: '#37474f', description: 'Phases that don\'t match a known lifecycle category' },
];

export function phaseColor(type: string): string {
    if (type.includes('Init') || type.includes('Boot'))       return '#2e7d32';
    if (type.includes('Update') || type.includes('Running'))  return '#1565c0';
    if (type.includes('Shutdown'))                            return '#b71c1c';
    return '#37474f';
}

export const PhaseNode: React.FC<NodeProps<PhaseNodeData>> = ({ data }) => {
    const { phase, modules, isSelected, isCurrent } = data;
    const bg = phaseColor(phase.type);

    const borderColor = isCurrent ? '#ff6d00' : isSelected ? '#ffd600' : '#555';
    const borderWidth = (isCurrent || isSelected) ? 2 : 1;
    const shadow = isCurrent ? '0 0 10px rgba(255, 109, 0, 0.7)' : 'none';

    return (
        <div style={{
            padding: '8px 12px',
            borderRadius: 6,
            border: `${borderWidth}px solid ${borderColor}`,
            background: bg,
            color: '#fff',
            minWidth: 160,
            boxShadow: shadow,
            fontSize: 12,
        }}>
            <Handle type="target" position={Position.Top} style={{ background: '#888' }} />

            <div style={{ fontWeight: 600, fontSize: 13, marginBottom: 2 }}>{phase.instance_id}</div>
            <div style={{ opacity: 0.8, fontSize: 11 }}>{phase.type}</div>

            {modules.length > 0 && (
                <div style={{ marginTop: 4, fontSize: 10, opacity: 0.7 }}>
                    {modules.length} module{modules.length !== 1 ? 's' : ''}
                </div>
            )}

            <Handle type="source" position={Position.Bottom} style={{ background: '#888' }} />
        </div>
    );
};
