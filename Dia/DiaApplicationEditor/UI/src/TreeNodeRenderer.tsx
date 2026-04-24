import React from 'react';
import type { NodeRendererProps } from 'react-arborist';
import type { ManifestTreeNode } from './TreeView';
import { useManifestStore } from './ManifestStore';

interface TreeNodeRendererProps extends NodeRendererProps<ManifestTreeNode> {
    selectedNodeId: string | null;
    onContextMenu: (e: React.MouseEvent, node: ManifestTreeNode) => void;
}

const NODE_ICONS: Record<ManifestTreeNode['nodeType'], string> = {
    processing_unit: '\u{1F4E6}', // 📦
    phase: '\u{1F504}',           // 🔄
    module: '⚙️',       // ⚙️
};

export const TreeNodeRenderer: React.FC<TreeNodeRendererProps> = ({
    node,
    style,
    dragHandle,
    selectedNodeId,
    onContextMenu,
}) => {
    const { nodeType, typeName, moduleCount, configKeys } = node.data;
    const validationResult = useManifestStore(s => s.validationResult);

    const hasError = validationResult?.errors.some(
        e => e.context && e.context.includes(node.data.id)
    ) ?? false;

    const isSelected = node.data.id === selectedNodeId;

    const borderColor = hasError ? '#e05555' : isSelected ? '#007acc' : 'transparent';

    const hasChildren = (node.data.children?.length ?? 0) > 0;

    return (
        <div
            ref={dragHandle}
            style={{
                ...style,
                display: 'flex',
                alignItems: 'center',
                gap: 4,
                paddingRight: 6,
                borderLeft: `2px solid ${borderColor}`,
                background: isSelected ? '#094771' : node.isHovered ? '#2a2d2e' : 'transparent',
                cursor: 'pointer',
                userSelect: 'none',
                fontSize: 12,
                color: '#ccc',
            }}
            onClick={() => node.select()}
            onContextMenu={e => onContextMenu(e, node.data)}
        >
            <span
                style={{ width: 14, flexShrink: 0, textAlign: 'center', fontSize: 9, color: '#666', cursor: hasChildren ? 'pointer' : 'default' }}
                onClick={e => { if (hasChildren) { e.stopPropagation(); node.toggle(); } }}
            >
                {hasChildren ? (node.isOpen ? '▼' : '▶') : ''}
            </span>
            <span style={{ marginRight: 2 }}>{NODE_ICONS[nodeType]}</span>

            <span style={{ flexShrink: 0 }}>{node.data.name}</span>
            <span style={{ color: '#666', fontSize: 11, flexShrink: 0 }}>({typeName})</span>

            {nodeType === 'phase' && moduleCount !== undefined && moduleCount > 0 && (
                <span style={{
                    background: '#444', borderRadius: 8, padding: '0 5px',
                    fontSize: 10, color: '#aaa', marginLeft: 4,
                }}>
                    {moduleCount}
                </span>
            )}

            {nodeType === 'module' && configKeys && configKeys.length > 0 && (
                <span style={{ color: '#555', fontSize: 10, marginLeft: 4, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                    {configKeys.join(', ')}
                </span>
            )}

            {hasError && (
                <span style={{ marginLeft: 'auto', color: '#e05555', fontSize: 11 }}>⚠</span>
            )}
        </div>
    );
};
