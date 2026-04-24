import React, { useState, useCallback, useMemo, useRef, useEffect } from 'react';
import { Tree } from 'react-arborist';
import type { NodeApi, NodeRendererProps } from 'react-arborist';
import { TreeNodeRenderer } from './TreeNodeRenderer';
import { useManifestStore } from './ManifestStore';

export interface ManifestTreeNode {
    id: string;
    name: string;
    nodeType: 'processing_unit' | 'phase' | 'module';
    typeName: string;
    moduleCount?: number;
    configKeys?: string[];
    children?: ManifestTreeNode[];
}

interface ManifestData {
    processing_units: Array<{
        instance_id: string;
        type: string;
        phases: Array<{
            instance_id: string;
            type: string;
            modules?: Array<{ instance_id: string; type: string; config?: Record<string, unknown> }>;
        }>;
        modules?: Array<{ instance_id: string; type: string; config?: Record<string, unknown> }>;
    }>;
}

function buildTree(manifest: ManifestData): ManifestTreeNode[] {
    return manifest.processing_units.map(pu => ({
        id: pu.instance_id,
        name: pu.instance_id,
        nodeType: 'processing_unit' as const,
        typeName: pu.type,
        children: pu.phases.map(phase => {
            const phaseModules = (pu.modules ?? []).filter(m =>
                Array.isArray((m as unknown as { phases?: string[] }).phases) &&
                (m as unknown as { phases?: string[] }).phases!.includes(phase.instance_id)
            );
            return {
                id: `${pu.instance_id}_${phase.instance_id}`,
                name: phase.instance_id,
                nodeType: 'phase' as const,
                typeName: phase.type,
                moduleCount: phaseModules.length,
                children: phaseModules.map(mod => ({
                    id: `${pu.instance_id}_${phase.instance_id}_${mod.instance_id}`,
                    name: mod.instance_id,
                    nodeType: 'module' as const,
                    typeName: mod.type,
                    configKeys: mod.config ? Object.keys(mod.config) : [],
                })),
            };
        }),
    }));
}

function filterTree(nodes: ManifestTreeNode[], term: string): ManifestTreeNode[] {
    const lower = term.toLowerCase();
    return nodes.flatMap(node => {
        const matches = node.name.toLowerCase().includes(lower) || node.typeName.toLowerCase().includes(lower);
        const filteredChildren = node.children ? filterTree(node.children, term) : [];
        if (matches || filteredChildren.length > 0) {
            return [{ ...node, children: filteredChildren.length > 0 ? filteredChildren : node.children }];
        }
        return [];
    });
}

function sendToPlugin(type: string, data: object): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

export const TreeView: React.FC = () => {
    const [searchTerm, setSearchTerm] = useState('');
    const [contextMenu, setContextMenu] = useState<{ x: number; y: number; node: ManifestTreeNode } | null>(null);
    const containerRef = useRef<HTMLDivElement>(null);
    const [containerHeight, setContainerHeight] = useState(400);

    useEffect(() => {
        if (!containerRef.current) return;
        const ro = new ResizeObserver(entries => {
            setContainerHeight(entries[0].contentRect.height);
        });
        ro.observe(containerRef.current);
        return () => ro.disconnect();
    }, []);
    const selectedNode = useManifestStore(s => s.selectedNode);
    const setSelectedNode = useManifestStore(s => s.setSelectedNode);
    const manifest = useManifestStore(s => s.manifest);

    const treeData = useMemo(() => {
        if (!manifest) return [];
        return buildTree(manifest as ManifestData);
    }, [manifest]);

    const displayData = useMemo(() => {
        if (!searchTerm) return treeData;
        return filterTree(treeData, searchTerm);
    }, [treeData, searchTerm]);

    const handleSelect = useCallback((nodes: NodeApi<ManifestTreeNode>[]) => {
        if (nodes.length === 0) return;
        const node = nodes[0].data;
        setSelectedNode(node.id);
        sendToPlugin('node_selected', { node_id: node.id });
    }, [setSelectedNode]);

    const handleMove = useCallback(({ dragIds, parentId, index }: { dragIds: string[]; parentId: string | null; index: number }) => {
        if (dragIds.length === 0) return;
        sendToPlugin('reorder_node', { node_id: dragIds[0], new_parent: parentId, new_index: index });
    }, []);

    const handleContextMenu = useCallback((e: React.MouseEvent, node: ManifestTreeNode) => {
        e.preventDefault();
        setContextMenu({ x: e.clientX, y: e.clientY, node });
    }, []);

    const closeMenu = useCallback(() => setContextMenu(null), []);

    const handleAddPhase = useCallback(() => {
        if (!contextMenu) return;
        sendToPlugin('add_node', { parent_id: contextMenu.node.id, node_type: 'phase' });
        closeMenu();
    }, [contextMenu, closeMenu]);

    const handleAddModule = useCallback(() => {
        if (!contextMenu) return;
        sendToPlugin('add_node', { parent_id: contextMenu.node.id, node_type: 'module' });
        closeMenu();
    }, [contextMenu, closeMenu]);

    const handleRemove = useCallback(() => {
        if (!contextMenu) return;
        sendToPlugin('remove_node', { node_id: contextMenu.node.id });
        closeMenu();
    }, [contextMenu, closeMenu]);

    if (!manifest) {
        return null;
    }

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }} onClick={contextMenu ? closeMenu : undefined}>
            <div style={{ padding: '4px 8px', borderBottom: '1px solid #333', flexShrink: 0 }}>
                <input
                    type="text"
                    placeholder="Search..."
                    value={searchTerm}
                    onChange={e => setSearchTerm(e.target.value)}
                    style={{
                        width: '100%', background: '#2a2a2a', border: '1px solid #444',
                        color: '#ccc', padding: '3px 6px', fontSize: 12, borderRadius: 3,
                    }}
                />
            </div>
            <div style={{ flex: 1, overflow: 'hidden' }} ref={containerRef}>
                <Tree<ManifestTreeNode>
                    data={displayData}
                    width="100%"
                    height={containerHeight}
                    indent={20}
                    rowHeight={28}
                    openByDefault={true}
                    selection={selectedNode ?? undefined}
                    onSelect={handleSelect}
                    onMove={handleMove}
                >
                    {(props: NodeRendererProps<ManifestTreeNode>) => (
                        <TreeNodeRenderer
                            {...props}
                            selectedNodeId={selectedNode}
                            onContextMenu={handleContextMenu}
                        />
                    )}
                </Tree>
            </div>

            {contextMenu && (
                <div
                    style={{
                        position: 'fixed', top: contextMenu.y, left: contextMenu.x,
                        background: '#252526', border: '1px solid #444', borderRadius: 3,
                        padding: '4px 0', zIndex: 1000, minWidth: 160,
                    }}
                    onClick={e => e.stopPropagation()}
                >
                    {contextMenu.node.nodeType === 'processing_unit' && (
                        <button style={menuItemStyle} onClick={handleAddPhase}>Add Phase</button>
                    )}
                    {contextMenu.node.nodeType === 'phase' && (
                        <button style={menuItemStyle} onClick={handleAddModule}>Add Module</button>
                    )}
                    <button style={menuItemStyle} onClick={handleRemove}>Remove</button>
                </div>
            )}
        </div>
    );
};

const menuItemStyle: React.CSSProperties = {
    display: 'block', width: '100%', background: 'none', border: 'none',
    color: '#ccc', padding: '6px 16px', textAlign: 'left', cursor: 'pointer',
    fontSize: 12,
};
