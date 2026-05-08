import React from 'react';
import { useManifestStore } from './ManifestStore';
import { ModuleInspector } from './ModuleInspector';
import { PUInspector } from './PUInspector';
import { PhaseInspector } from './PhaseInspector';

export const InspectorPanel: React.FC = () => {
    const selectedNode = useManifestStore(s => s.selectedNode);

    if (!selectedNode) {
        return (
            <div style={{ padding: 12, color: '#555', fontSize: 12 }}>
                Select a node to inspect
            </div>
        );
    }

    const segments = selectedNode.split('_').length;

    if (segments === 1) return <PUInspector puId={selectedNode} />;
    if (segments === 2) return <PhaseInspector nodeId={selectedNode} />;
    return <ModuleInspector />;
};
