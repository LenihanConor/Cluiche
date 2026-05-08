# Feature Spec: Validation Navigation

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Validation Navigation** | (this document) |

## Problem Statement

The ValidationPanel shows errors and warnings, but clicking on them does nothing. Users must manually find the offending node in the Tree/Flow/Lifecycle view. For errors that reference a JSON path (e.g., `processing_units[0].modules[2]`), the editor should navigate directly to the relevant node.

## Acceptance Criteria

- [ ] Clicking a validation error/warning in the ValidationPanel selects the corresponding node
- [ ] Selected node is scrolled/panned into view in the active view
- [ ] Validation error rows show a clickable style (pointer cursor, hover highlight)
- [ ] If the error references a node that doesn't exist (e.g., deleted between validation and click), show a brief "Node not found" indication
- [ ] JSON path in validation errors is resolved to a node ID: `processing_units[0]` → PU node, `processing_units[0].phases[1]` → Phase node, `processing_units[0].modules[2]` → Module node
- [ ] If error has no resolvable path (e.g., top-level schema error), clicking still dismisses but doesn't navigate

## Design

### Path Resolution

```typescript
function resolvePathToNodeId(manifest: ManifestData, path: string): string | null {
    // Parse JSON path segments: "processing_units[0].phases[1].modules[2]"
    const puMatch = path.match(/processing_units\[(\d+)\]/);
    if (!puMatch) return null;
    
    const puIdx = parseInt(puMatch[1]);
    const pu = manifest.processing_units[puIdx];
    if (!pu) return null;
    
    const phaseMatch = path.match(/phases\[(\d+)\]/);
    if (!phaseMatch) return pu.instance_id;  // PU-level error
    
    const phaseIdx = parseInt(phaseMatch[1]);
    const phase = pu.phases[phaseIdx];
    if (!phase) return pu.instance_id;
    
    const moduleMatch = path.match(/modules\[(\d+)\]/);
    if (!moduleMatch) return `${pu.instance_id}_${phase.instance_id}`;  // Phase-level error
    
    const modIdx = parseInt(moduleMatch[1]);
    const mod = pu.modules[modIdx];
    if (!mod) return `${pu.instance_id}_${phase.instance_id}`;
    
    // Find which phase this module belongs to for the full node ID
    const modPhase = mod.phases?.[0] ?? phase.instance_id;
    return `${pu.instance_id}_${modPhase}_${mod.instance_id}`;
}
```

### ValidationPanel Click Handler

```typescript
const handleErrorClick = (error: ValidationError) => {
    if (!error.path || !manifest) return;
    
    const nodeId = resolvePathToNodeId(manifest as ManifestData, error.path);
    if (!nodeId) return;
    
    setSelectedNode(nodeId);
    // View-specific navigation happens reactively when selectedNode changes
};
```

### View-Specific Scroll/Pan

Each view already reacts to `selectedNode` changes. Extend:

- **Tree View:** react-arborist supports `tree.scrollTo(nodeId)` — call on selection change
- **Flow View:** reactflow supports `fitView({ nodes: [{ id: nodeId }] })` — call on selection change
- **Lifecycle View:** scroll the row into view via `element.scrollIntoView()`

### Styling

```typescript
// ValidationPanel error row
<div
    style={{ cursor: error.path ? 'pointer' : 'default' }}
    onClick={() => handleErrorClick(error)}
    onMouseEnter={e => error.path && (e.currentTarget.style.background = '#2a2d3a')}
    onMouseLeave={e => (e.currentTarget.style.background = '')}
>
```

## Implementation Files

- `Dia/DiaApplicationEditor/UI/src/ValidationPanel.tsx` - Click handler, hover styles, path resolution
- `Dia/DiaApplicationEditor/UI/src/TreeView.tsx` - scrollTo on selectedNode change
- `Dia/DiaApplicationEditor/UI/src/FlowView.tsx` - fitView on selectedNode change
- `Dia/DiaApplicationEditor/UI/src/LifecycleView.tsx` - scrollIntoView on selectedNode change

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-005 | Display validation errors inline at source location | ✅ Extends inline display with navigation — click to jump to source |
| DiaApplicationEditor | DAED-001 | Reuse ApplicationManifestValidator | ✅ Consumes existing validation output; adds navigation on top |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Path format | Is the JSON path format stable? | Yes — it's produced by ApplicationManifestValidator and follows standard JSON pointer patterns. If the validator changes path format, this resolver needs updating. |
| 2 | Module resolution | modules[] index vs phase_ids? | The path references the array index in the manifest JSON. Modules are under PU, not phase. The node ID includes a phase for UI routing, so we pick the first phase the module belongs to. |
| 3 | Multiple errors same node | What if 5 errors point to the same node? | Fine — each click selects the same node. No special handling needed. |

## Status

`Approved`
