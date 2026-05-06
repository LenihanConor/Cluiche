# Feature Spec: PU/Phase Inspector

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **PU/Phase Inspector** | (this document) |

## Problem Statement

The existing ModuleInspector panel only shows module config. When a user selects a ProcessingUnit or Phase node (in Tree View, Flow View, or Lifecycle View), there is no way to edit PU-level properties (`frequency_hz`, `dedicated_thread`, `instance_id`, `type_id`) or Phase-level properties (`instance_id`, `type_id`, `initial_phase` designation, phase config). These are fundamental manifest fields that currently require raw JSON editing.

## Acceptance Criteria

- [ ] Selecting a PU node in any view shows PU properties in the right inspector panel (280px)
- [ ] PU Inspector shows editable fields: `type_id` (dropdown from type discovery), `instance_id` (text), `frequency_hz` (number), `dedicated_thread` (checkbox), `root` (checkbox), config (JSON/form)
- [ ] Selecting a Phase node in any view shows Phase properties in the right inspector panel
- [ ] Phase Inspector shows editable fields: `type_id` (dropdown), `instance_id` (text), config (JSON/form), "Is Initial Phase" (checkbox — sets this phase as the PU's `initial_phase`)
- [ ] "Is Initial Phase" checkbox is exclusive — checking it unchecks the previous initial phase
- [ ] `initial_phase` dropdown also available on the PU inspector (selects from existing phases in that PU)
- [ ] Changes are debounced 500ms before sending to C++ (consistent with module config editor)
- [ ] Changes mark the manifest dirty
- [ ] Inspector panel title changes contextually: "Module", "Phase", or "Processing Unit"
- [ ] Inspector panel shows nothing when no node is selected

## Design

### Selection Routing

The existing `selectedNode` in ManifestStore encodes type via ID format:
- PU: `"MainProcessingUnit"` (single segment)
- Phase: `"MainProcessingUnit_InitPhase"` (two segments separated by `_`)
- Module: `"MainProcessingUnit_InitPhase_RenderModule"` (three segments)

App.tsx already uses segment count to decide whether to show ModuleInspector. Extend this:

```typescript
const nodeSegments = selectedNode?.split('_').length ?? 0;
// 1 segment = PU, 2 segments = Phase, 3+ segments = Module
```

### Inspector Components

```typescript
// InspectorPanel.tsx — router component
const InspectorPanel: React.FC = () => {
    const selectedNode = useManifestStore(s => s.selectedNode);
    const segments = selectedNode?.split('_').length ?? 0;
    
    if (segments === 1) return <PUInspector puId={selectedNode!} />;
    if (segments === 2) return <PhaseInspector nodeId={selectedNode!} />;
    if (segments >= 3) return <ModuleInspector />;
    return null;
};

// PUInspector.tsx
interface PUInspectorProps { puId: string; }
// Fields: type_id (dropdown), instance_id (text), frequency_hz (number),
//         dedicated_thread (checkbox), root (checkbox),
//         initial_phase (dropdown of phase instance_ids in this PU),
//         config (FormView/JSON toggle)

// PhaseInspector.tsx
interface PhaseInspectorProps { nodeId: string; }
// Fields: type_id (dropdown), instance_id (text),
//         isInitialPhase (checkbox — exclusive within parent PU),
//         config (FormView/JSON toggle)
```

### C++ Bridge Events

```typescript
// On PU property change (debounced 500ms):
sendToPlugin('pu_property_changed', {
    pu_id: 'MainProcessingUnit',
    property: 'frequency_hz',
    value: 60.0
});

// On Phase property change (debounced 500ms):
sendToPlugin('phase_property_changed', {
    pu_id: 'MainProcessingUnit',
    phase_id: 'InitPhase',
    property: 'type_id',
    value: 'UpdatePhase'
});

// On initial_phase change (immediate, not debounced):
sendToPlugin('initial_phase_changed', {
    pu_id: 'MainProcessingUnit',
    phase_id: 'UpdatePhase'
});
```

### Reuse of Existing Patterns

- FormView.tsx already handles bool/number/string field rendering — reuse for PU/Phase properties
- jsonLinter.ts and CodeMirror toggle already exist in ModuleInspector — reuse for config sections
- 500ms debounce pattern from useValidation — reuse

## Implementation Files

- `Dia/DiaApplicationEditor/UI/src/InspectorPanel.tsx` - Router component (replaces direct ModuleInspector usage in App.tsx)
- `Dia/DiaApplicationEditor/UI/src/PUInspector.tsx` - ProcessingUnit property editor
- `Dia/DiaApplicationEditor/UI/src/PhaseInspector.tsx` - Phase property editor
- `Dia/DiaApplicationEditor/UI/src/ModuleInspector.tsx` - Existing (unchanged)
- `Dia/DiaApplicationEditor/UI/src/App.tsx` - Replace ModuleInspector with InspectorPanel
- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Handle pu_property_changed, phase_property_changed, initial_phase_changed events

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-004 | Query ApplicationTypeRegistry for available types | ✅ type_id fields use dropdown from type discovery |
| DiaApplicationEditor | DAED-006 | Runtime config changes debounced 500ms | ✅ All property changes debounced 500ms |
| DiaApplicationEditor | DAED-008 | Manual save workflow | ✅ Changes mark dirty; user saves explicitly |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | ID format | What if instance_id contains underscores? | Valid concern. The node ID format uses `_` as separator which could collide. Mitigate: use the manifest structure to resolve ambiguity (look up PU first, then phase within it). The split heuristic is a shortcut; real resolution walks the tree. |
| 2 | Rename | Can user change instance_id? | Yes. This triggers a rename across all references (transitions, module phase_ids, initial_phase). C++ handler must cascade the rename. |
| 3 | Validation | What happens on invalid frequency_hz (0 or negative)? | Debounced change still sent; real-time validation catches it and shows inline error. Don't block the edit. |
| 4 | Initial phase | What if user unchecks "Is Initial Phase" without setting another? | Disallow: checkbox is exclusive-set only. You can set a different phase as initial, but you can't have no initial phase. UI enforces this by making the checkbox non-uncheckable (you change it by selecting a different phase as initial). |

## Status

`Approved`
