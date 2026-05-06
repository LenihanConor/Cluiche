# Feature Spec: Import Management

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Import Management** | (this document) |

## Problem Statement

Manifests can import other manifests (e.g., `cluiche_main.diaapp` imports `cluiche_sim.diaapp` and `cluiche_render.diaapp`). Currently the editor shows only the local manifest content with no visibility into imports. Users cannot see what PUs/phases/modules are coming from imports, cannot add or remove import statements, and cannot navigate to imported files. For a multi-file manifest architecture, this makes the editor incomplete.

## Acceptance Criteria

- [ ] When a manifest with `"imports"` is opened, the editor resolves and displays imported content alongside local content
- [ ] Imported nodes are visually distinct — badge/overlay indicating source file (e.g., "[sim]" tag, dimmed border, or italic label)
- [ ] Imported nodes are editable inline — changes propagate to the source file
- [ ] Saving after editing imported content saves the modified source file (not the importing file, unless it also changed)
- [ ] If both local and imported files are dirty, Save saves all dirty files (with .bak backup for each)
- [ ] Imports panel in toolbar area shows list of imported files with:
  - File name and resolution status (found / not found)
  - Click to open the import in a new editor instance (or focus it)
  - Remove button (removes the import statement from the manifest)
- [ ] "Add Import" button — file picker to select a .diaapp file to add to the imports array
- [ ] Circular import detection — if adding an import would create a cycle, show error and block
- [ ] Tree View: imported PUs appear at the top level with a badge showing origin file
- [ ] Flow View: imported phase nodes have a distinct border style (dashed) and origin label
- [ ] Lifecycle View: imported modules have an italic label and origin file indicator
- [ ] If an imported file cannot be found on disk, show a warning badge on that import and exclude its content (don't block the whole editor)

## Design

### Manifest Resolution

On `OpenManifest`, the C++ side already uses `ApplicationManifestLoader` which handles imports. Extend the data pushed to the UI to include provenance:

```cpp
// ManifestSerializer extension: include _source field per PU
void ManifestSerializer::SerializeWithProvenance(
    const ApplicationManifest& merged,
    const DynamicArrayC<ImportSource, 8>& sources,
    Json::Value& outJson);

struct ImportSource {
    StringCRC puId;
    const char* sourceFile;  // relative path of the .diaapp that defines this PU
};
```

### UI Data Model Extension

```typescript
// types.ts extension
interface ProcessingUnitData {
    // existing fields...
    _source?: string;  // e.g., "cluiche_sim.diaapp" — absent for local PUs
}

interface ImportEntry {
    path: string;        // relative path as written in the manifest
    resolved: boolean;   // whether the file was found on disk
    dirty: boolean;      // whether this imported file has unsaved changes
}
```

### ManifestStore Extension

```typescript
interface ManifestState {
    // existing...
    imports: ImportEntry[];
    setImports: (imports: ImportEntry[]) => void;
    addImport: (path: string) => void;
    removeImport: (path: string) => void;
}
```

### Visual Distinction

**Tree View:** Append `[filename]` badge in grey after the PU name:
```
▶ SimProcessingUnit [cluiche_sim.diaapp]
```

**Flow View:** Dashed border on group node + small label in top-right corner:
```typescript
style: {
    ...existingGroupStyle,
    border: isImported ? '1px dashed #666' : '1px dashed #444',
}
```

**Lifecycle View:** Italic module name + origin in dim text.

### Editing Imported Content

When a user edits an imported node (module config, phase property, etc.), the bridge event includes the source file:

```typescript
sendToPlugin('module_config_changed', {
    pu_id: 'SimProcessingUnit',
    module_id: 'Sim::TimeServerModule',
    config: { hz: 60.0 },
    _source: 'cluiche_sim.diaapp',  // tells C++ which file to mark dirty
});
```

C++ tracks dirty state per file. Save writes all dirty files.

### Import Management UI

Small collapsible panel below the toolbar (or dropdown from an "Imports" button):

```typescript
const ImportsPanel: React.FC = () => {
    const imports = useManifestStore(s => s.imports);
    return (
        <div>
            {imports.map(imp => (
                <div key={imp.path}>
                    <span>{imp.path}</span>
                    {!imp.resolved && <span className="warning">⚠ Not found</span>}
                    {imp.dirty && <span className="dirty">*</span>}
                    <button onClick={() => openImport(imp.path)}>Open</button>
                    <button onClick={() => removeImport(imp.path)}>×</button>
                </div>
            ))}
            <button onClick={handleAddImport}>+ Add Import</button>
        </div>
    );
};
```

### Circular Import Detection

Done on the C++ side when handling `add_import`:
```cpp
bool DiaApplicationEditor::WouldCreateCycle(const char* targetPath) {
    // Walk the import graph from targetPath; if it reaches the current file, cycle exists
}
```

## Implementation Files

- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Import resolution, provenance tracking, multi-file dirty/save, cycle detection
- `Dia/DiaApplicationEditor/ManifestSerializer.cpp` - `SerializeWithProvenance`
- `Dia/DiaApplicationEditor/UI/src/types.ts` - `_source` field, `ImportEntry`
- `Dia/DiaApplicationEditor/UI/src/ManifestStore.ts` - imports state, add/remove actions
- `Dia/DiaApplicationEditor/UI/src/ImportsPanel.tsx` - Import list UI
- `Dia/DiaApplicationEditor/UI/src/Toolbar.tsx` - "Imports" button/dropdown
- `Dia/DiaApplicationEditor/UI/src/TreeView.tsx` - Badge rendering for imported PUs
- `Dia/DiaApplicationEditor/UI/src/FlowView.tsx` - Dashed border for imported groups
- `Dia/DiaApplicationEditor/UI/src/LifecycleView.tsx` - Italic + origin for imported modules

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-008 | Manual save workflow | ✅ Dirty tracking per-file; user triggers save; .bak backup for each |
| DiaApplicationEditor | DAED-009 | Detect file conflicts | ✅ File watcher extended to watch imported files too |
| DiaApplicationEditor | DAED-001 | Reuse ApplicationManifestValidator | ✅ Validation runs on merged manifest (post-import resolution) |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Editing | Why allow inline editing of imports? | User specified. Alternative (read-only + "open in new tab") adds friction. Inline edit with save-to-source is more productive. |
| 2 | Save | What if source file has a conflict during save? | Same conflict detection as local file — ConflictBanner appears per-file. User resolves each independently. |
| 3 | Provenance | How to distinguish a PU that exists in both local and imported? | Import merging is handled by ApplicationManifestLoader — it either errors on duplicate PU IDs or merges. The editor shows the loader's result and marks provenance per the source file that defined it. |
| 4 | Performance | Loading many imports? | Manifests are small. Even 10 imports of 100-line files is trivial. No lazy loading needed. |
| 5 | File watching | Do we watch imported files? | Yes — extend FileWatcher to monitor all resolved import paths. Conflict banner identifies which file changed. |

## Status

`Approved`
