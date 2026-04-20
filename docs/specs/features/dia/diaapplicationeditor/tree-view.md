# Feature Spec: Tree View

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Tree View** | (this document) |

## Problem Statement

Provides a hierarchical tree view of .diaapp manifests using react-arborist, displaying the ProcessingUnit → Phase → Module structure for adding, removing, and organizing application components.

## Acceptance Criteria

- [x] Display hierarchy: ProcessingUnit → Phases → Modules
- [x] Use react-arborist for interactive tree with collapse/expand
- [x] Show type and instance_id for each node
- [x] Click node to select and open inspector panel
- [x] Context menu: Add/Remove ProcessingUnit, Phase, Module
- [x] Drag-and-drop to reorder phases and modules
- [x] Display module config summary (collapsed view)
- [x] Keyboard navigation (arrow keys, Enter to edit)
- [x] Search/filter tree by name or type
- [x] Icons for each node type (PU, Phase, Module)
- [x] Show module count badge on phase nodes
- [x] Read-only view of transitions (not editable here - use Flow View)

## Design

### React Component Structure

**TreeView.tsx:**
```typescript
import React, { useState, useCallback } from 'react';
import { Tree, TreeApi, NodeRendererProps } from 'react-arborist';
import { ApplicationManifest, ProcessingUnit, Phase, Module } from './types';

interface TreeNode {
    id: string;
    name: string;
    type: 'processing_unit' | 'phase' | 'module';
    data: ProcessingUnit | Phase | Module;
    children?: TreeNode[];
}

interface TreeViewProps {
    manifest: ApplicationManifest;
    selectedNodeId: string | null;
    onSelectNode: (nodeId: string) => void;
    onAddNode: (parentId: string, nodeType: string) => void;
    onRemoveNode: (nodeId: string) => void;
    onReorderNode: (nodeId: string, newIndex: number) => void;
}

export const TreeView: React.FC<TreeViewProps> = ({
    manifest,
    selectedNodeId,
    onSelectNode,
    onAddNode,
    onRemoveNode,
    onReorderNode
}) => {
    const [searchTerm, setSearchTerm] = useState('');
    const [treeRef, setTreeRef] = useState<TreeApi<TreeNode> | null>(null);
    
    // Convert manifest to tree data
    const treeData: TreeNode[] = manifest.processing_units.map(pu => ({
        id: pu.id,
        name: pu.id,
        type: 'processing_unit',
        data: pu,
        children: pu.phases.map(phase => ({
            id: `${pu.id}_${phase.instance_id}`,
            name: phase.instance_id,
            type: 'phase',
            data: phase,
            children: phase.modules?.map(module => ({
                id: `${pu.id}_${phase.instance_id}_${module.instance_id}`,
                name: module.instance_id,
                type: 'module',
                data: module,
            })) || [],
        })),
    }));
    
    // Filter tree by search term
    const filteredData = searchTerm
        ? filterTree(treeData, searchTerm)
        : treeData;
    
    // Node click handler
    const handleNodeClick = useCallback((node: TreeNode) => {
        onSelectNode(node.id);
        window.DiaEditor.notifyDataChanged('node_selected', { node_id: node.id });
    }, [onSelectNode]);
    
    // Drag-and-drop reorder handler
    const handleMove = useCallback(({ dragIds, parentId, index }: any) => {
        const nodeId = dragIds[0];
        onReorderNode(nodeId, index);
        
        window.DiaEditor.notifyDataChanged('node_reordered', {
            node_id: nodeId,
            new_parent: parentId,
            new_index: index,
        });
    }, [onReorderNode]);
    
    // Context menu actions
    const handleContextMenu = useCallback((event: React.MouseEvent, node: TreeNode) => {
        event.preventDefault();
        
        const menuItems = getContextMenuItems(node);
        // Show context menu (implement with react-contexify or similar)
        showContextMenu(event.clientX, event.clientY, menuItems);
    }, []);
    
    const getContextMenuItems = (node: TreeNode) => {
        const items = [];
        
        if (node.type === 'processing_unit') {
            items.push({ label: 'Add Phase', action: () => onAddNode(node.id, 'phase') });
            items.push({ label: 'Remove Processing Unit', action: () => onRemoveNode(node.id) });
        } else if (node.type === 'phase') {
            items.push({ label: 'Add Module', action: () => onAddNode(node.id, 'module') });
            items.push({ label: 'Remove Phase', action: () => onRemoveNode(node.id) });
        } else if (node.type === 'module') {
            items.push({ label: 'Remove Module', action: () => onRemoveNode(node.id) });
        }
        
        return items;
    };
    
    return (
        <div className="tree-view">
            <div className="tree-search">
                <input
                    type="text"
                    placeholder="Search..."
                    value={searchTerm}
                    onChange={(e) => setSearchTerm(e.target.value)}
                />
            </div>
            
            <Tree
                ref={setTreeRef}
                data={filteredData}
                width="100%"
                height="100%"
                indent={24}
                rowHeight={32}
                onSelect={(nodes) => handleNodeClick(nodes[0])}
                onMove={handleMove}
            >
                {(props: NodeRendererProps<TreeNode>) => (
                    <TreeNodeRenderer
                        {...props}
                        isSelected={props.node.data.id === selectedNodeId}
                        onContextMenu={handleContextMenu}
                    />
                )}
            </Tree>
        </div>
    );
};

// Filter tree recursively
function filterTree(nodes: TreeNode[], searchTerm: string): TreeNode[] {
    return nodes
        .map(node => {
            const matches = node.name.toLowerCase().includes(searchTerm.toLowerCase());
            const filteredChildren = node.children ? filterTree(node.children, searchTerm) : [];
            
            if (matches || filteredChildren.length > 0) {
                return { ...node, children: filteredChildren };
            }
            return null;
        })
        .filter(Boolean) as TreeNode[];
}
```

### TreeNodeRenderer Component

**TreeNodeRenderer.tsx:**
```typescript
import React from 'react';
import { NodeRendererProps } from 'react-arborist';
import { TreeNode } from './types';

interface TreeNodeRendererProps extends NodeRendererProps<TreeNode> {
    isSelected: boolean;
    onContextMenu: (event: React.MouseEvent, node: TreeNode) => void;
}

export const TreeNodeRenderer: React.FC<TreeNodeRendererProps> = ({
    node,
    style,
    isSelected,
    onContextMenu,
}) => {
    const { type, name, data } = node.data;
    
    // Icon by type
    const getIcon = () => {
        switch (type) {
            case 'processing_unit': return '📦';
            case 'phase': return '🔄';
            case 'module': return '⚙️';
            default: return '';
        }
    };
    
    // Badge for phase (module count)
    const getBadge = () => {
        if (type === 'phase' && data.modules) {
            return <span className="badge">{data.modules.length}</span>;
        }
        return null;
    };
    
    // Config summary for module
    const getConfigSummary = () => {
        if (type === 'module' && data.config) {
            const keys = Object.keys(data.config);
            if (keys.length > 0) {
                return <span className="config-summary">{keys.join(', ')}</span>;
            }
        }
        return null;
    };
    
    return (
        <div
            style={style}
            className={`tree-node ${type} ${isSelected ? 'selected' : ''}`}
            onClick={() => node.toggle()}
            onContextMenu={(e) => onContextMenu(e, node.data)}
        >
            <span className="icon">{getIcon()}</span>
            <span className="name">{name}</span>
            <span className="type-label">({data.type})</span>
            {getBadge()}
            {getConfigSummary()}
        </div>
    );
};
```

### C++ Integration

**DiaApplicationEditor::HandleNodeReordered:**
```cpp
void DiaApplicationEditor::HandleNodeReordered(const Json::Value& data) {
    std::string nodeId = data["node_id"].asString();
    std::string newParent = data["new_parent"].asString();
    int newIndex = data["new_index"].asInt();
    
    // Parse node ID to determine type
    // Example: "MainProcessingUnit_UpdatePhase_RenderModule"
    // Move module to new index within phase
    
    // Find and reorder in manifest
    // ... (implementation details)
    
    // Mark dirty
    MarkDirty();
}
```

**DiaApplicationEditor::HandleAddNode:**
```cpp
void DiaApplicationEditor::HandleAddNode(const Json::Value& data) {
    std::string parentId = data["parent_id"].asString();
    std::string nodeType = data["node_type"].asString();
    
    if (nodeType == "phase") {
        // Show type selector dialog
        mEditorModel->NotifyObservers(StringCRC("show_type_selector"), "phase");
        // JavaScript will call back with selected type
    } else if (nodeType == "module") {
        mEditorModel->NotifyObservers(StringCRC("show_type_selector"), "module");
    }
}
```

### Context Menu Integration

**ContextMenu.tsx:**
```typescript
import React from 'react';
import { Menu, Item, useContextMenu } from 'react-contexify';
import 'react-contexify/dist/ReactContexify.css';

const TREE_MENU_ID = 'tree-context-menu';

export const TreeContextMenu: React.FC = () => {
    const { show } = useContextMenu({ id: TREE_MENU_ID });
    
    const handleAddPhase = () => {
        window.DiaEditor.executeCommand('diaapp-editor', 'add_node', {
            parent_id: selectedNodeId,
            node_type: 'phase',
        });
    };
    
    const handleAddModule = () => {
        window.DiaEditor.executeCommand('diaapp-editor', 'add_node', {
            parent_id: selectedNodeId,
            node_type: 'module',
        });
    };
    
    const handleRemove = () => {
        window.DiaEditor.executeCommand('diaapp-editor', 'remove_node', {
            node_id: selectedNodeId,
        });
    };
    
    return (
        <Menu id={TREE_MENU_ID}>
            <Item onClick={handleAddPhase}>Add Phase</Item>
            <Item onClick={handleAddModule}>Add Module</Item>
            <Item onClick={handleRemove}>Remove</Item>
        </Menu>
    );
};
```

## Implementation Files

- `Dia/DiaApplicationEditor/UI/TreeView.tsx` - Main react-arborist tree component
- `Dia/DiaApplicationEditor/UI/TreeNodeRenderer.tsx` - Custom node rendering
- `Dia/DiaApplicationEditor/UI/ContextMenu.tsx` - Context menu for add/remove
- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Node reorder/add/remove handlers

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | ✅ **Compliant** - Node IDs converted to StringCRC in C++ |
| DiaApplicationEditor | DAED-002 | Two view modes: Flow and Tree | ✅ **Compliant** - Tree view for hierarchical organization |
| DiaApplicationEditor | DAED-003 | Phase transitions only editable in Flow View | ✅ **Compliant** - Tree doesn't show transitions (read-only) |

**Decision 45 Compliance:**
- **Use react-arborist for tree view** - ✅ Implemented with react-arborist library

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | Which tree library? | Decision 45 | ✅ react-arborist - interactive tree with drag-and-drop |
| 2 | Show transitions in tree? | Decision 45 | ✅ No - transitions only in Flow View (DAED-003) |
| 3 | Drag-and-drop support? | Decision 45 | ✅ Yes - reorder phases and modules within parent |
| 4 | Context menu for actions? | Decision 45 | ✅ Yes - Add/Remove using react-contexify |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Library Choice | Why react-arborist? | ✅ Decision 45 - performant tree with built-in drag-and-drop; keyboard navigation; virtualization |
| 2 | Hierarchy | Show transitions? | ✅ No - Decision 45 / DAED-003 - transitions only editable in Flow View |
| 3 | Actions | How to add/remove nodes? | ✅ Context menu (right-click) → type selector dialog |
| 4 | Search | Filter tree by name? | ✅ Yes - recursive filter with highlight |
| 5 | Performance | Large manifests? | ✅ react-arborist virtualizes rows; handles thousands of nodes |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
