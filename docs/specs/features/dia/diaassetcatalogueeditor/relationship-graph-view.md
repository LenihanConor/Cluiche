# Feature Spec: Relationship Graph View

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetCatalogueEditor | @docs/specs/systems/dia/diaassetcatalogueeditor.md |
| Feature | Relationship Graph View | (this document) |

## Summary

Visual directed graph of asset relationships rendered in CEF using VisJS. Selecting an asset shows its forward and reverse dependency edges as a node-link diagram. Nodes are colored by asset type and clickable to navigate to the corresponding record in the Record Editor. The graph is read-only — all relationship mutations are performed through the Relationship Editor (Feature 4).

## Problem

Understanding asset dependencies from a flat record list is difficult. Developers need to see at a glance which assets depend on a selected asset (reverse refs) and which assets it depends on (forward refs). A visual graph makes circular dependencies, orphaned assets, and deep dependency chains immediately visible. Without this, developers must manually trace relationships record by record.

## Acceptance Criteria

1. Selecting an asset in the Asset List populates the Relationship Graph panel with that asset's direct forward and reverse edges
2. Nodes represent assets, colored by type (e.g., texture=blue, entity=green, config=orange, stage=purple); legend shown
3. Edges represent relationship type (contains, uses) with labeled arrows indicating direction
4. Clicking a node in the graph navigates to that record in the Record Editor panel
5. Default view shows direct edges only (depth 1) — not transitive closure
6. Each node has an "expand" action that fetches and displays that node's own forward/reverse edges (one level at a time)
7. Graph uses VisJS (or equivalent) library rendered inside CEF
8. Graph is read-only — no drag-to-connect, no delete-edge interaction; mutations go through Relationship Editor
9. Expanding a node that has already been expanded is a no-op (no duplicate fetches)
10. Root node (selected asset) visually distinguished (e.g., thicker border, larger size)
11. Graph panel clears and reloads when a different asset is selected in the Asset List
12. Empty state message shown when selected asset has no relationships

## API Design

### DiaAPI Commands Used

| Command | Usage |
|---------|-------|
| `asset_catalogue.get_forward_refs { id }` | Fetch forward relationship edges for selected asset or expanded node |
| `asset_catalogue.get_reverse_refs { id }` | Fetch reverse relationship edges for selected asset or expanded node |

### C++ Types Involved

```cpp
namespace Dia::AssetCatalogue::Editor
{
    // Command handler registered in DiaAssetCatalogueEditor::OnLoad
    // Delegates to RelationshipIndex via AssetRegistry

    // Response JSON shape for get_forward_refs / get_reverse_refs:
    // {
    //   "edges": [
    //     { "fromId": "texture.hero_diffuse", "type": "uses", "toId": "texture.hero_normal" },
    //     ...
    //   ]
    // }
}
```

No new C++ types are introduced. The graph is entirely a CEF-side visualization that calls existing DiaAPI commands. The plugin's command handlers marshal `RelationshipIndex` query results into JSON.

### CEF Panel Description

**Relationship Graph Panel** — a dockable panel containing:
- A VisJS `Network` instance rendering nodes and edges
- A type-color legend bar at the top
- A "depth" indicator showing how many expansions have been performed
- Click handler on nodes that dispatches navigation to Record Editor
- Expand button (or double-click) on nodes to load the next level of dependencies

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement get_forward_refs command handler | Register `asset_catalogue.get_forward_refs` in DiaAPI; query `RelationshipIndex::GetForwardRefs`, serialize edges to JSON response |
| 2 | Implement get_reverse_refs command handler | Register `asset_catalogue.get_reverse_refs` in DiaAPI; query `RelationshipIndex::GetReverseRefs`, serialize edges to JSON response |
| 3 | Create VisJS graph panel HTML/JS | Build the CEF panel with VisJS Network, type-color map, legend bar, empty state message |
| 4 | Wire asset selection to graph | On Asset List selection change, call get_forward_refs + get_reverse_refs for selected asset, populate VisJS dataset |
| 5 | Implement node expand action | Double-click or expand button on a node calls get_forward_refs + get_reverse_refs for that node; merge new nodes/edges into the existing VisJS dataset |
| 6 | Implement click-to-navigate | Click handler on VisJS node dispatches a message to open the clicked asset's record in the Record Editor panel |
| 7 | Add type-color mapping and root node styling | Define color palette per asset type; apply thicker border and larger size to the root node |
| 8 | Unit tests for command handlers | Test get_forward_refs / get_reverse_refs JSON serialization with known RelationshipIndex state |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetCatalogue | `RelationshipIndex::GetForwardRefs`, `RelationshipIndex::GetReverseRefs` for querying edges |
| DiaAssetCatalogueEditor (Feature 2) | Asset List selection event to trigger graph population |
| DiaAssetCatalogueEditor (Feature 4) | Relationship Editor performs actual mutations; graph is read-only consumer |
| DiaEditor | `IEditorPlugin` panel hosting, CEF rendering via DiaUICEF |
| DiaUICEF | CEF browser instance for rendering VisJS |
| VisJS | Graph visualization library loaded in CEF |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetCatalogueEditor/Commands/GetForwardRefsHandler.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/GetForwardRefsHandler.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/GetReverseRefsHandler.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/GetReverseRefsHandler.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/relationship-graph/index.html` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/relationship-graph/graph.js` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/relationship-graph/graph.css` | Create |
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditor.cpp` | Modify — register graph panel and command handlers |
| `Cluiche/UnitTests/DiaAssetCatalogueEditor/RelationshipGraphTests.cpp` | Create |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | **Compliant** — asset IDs and relationship type IDs are StringCRC in C++; serialized as string for JSON/JS bridge |
| PD-004 / AD-002 | No STL in public APIs | **Compliant** — command handlers use `const char*` and DiaCore containers; JSON bridge uses jsoncpp `Json::Value` |
| PD-005 | x64 Windows only | **Compliant** — no cross-platform considerations |
| PD-007 | C++20 required | **Compliant** — all new C++ files compiled under `/std:c++20` |
| PD-008 | Directory.Build.props owns build settings | **Compliant** — no build setting overrides in DiaAssetCatalogueEditor.vcxproj |
| PD-009 | Generated output under Cluiche/out/ | **Compliant** — no generated output from this feature (graph is ephemeral in-memory UI) |
| AD-001 | Module system with YAML frontmatter | **Compliant** — DiaAssetCatalogueEditor module doc already exists at system level |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant** — all C++ code in `Dia::AssetCatalogue::Editor::` |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant** — graph nodes display and key on `type.name` composite IDs |
| SD-ACE-001 | Editor owns catalogue manifest authoring | **N/A** — graph is read-only visualization, no manifest mutation |
| SD-ACE-004 | All mutations are IEditorCommand | **N/A** — graph is read-only; no mutations |
| SD-ACE-005 | Output to Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/ | **Compliant** — no file output from this feature |
| SD-ACE-007 | Rules engine logic in DiaAssetCatalogue, editor UI only | **Compliant** — graph queries RelationshipIndex (DiaAssetCatalogue), no logic in editor |
| SED-009 | Undo/redo via IEditorCommand + CommandHistory | **N/A** — read-only feature, no undoable actions |
| SED-015 | DiaEditor is pure C++ library, no DiaApplication dependency | **Compliant** — no DiaApplication types used |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | **Compliant** — no file output |
| SED-021 | Per-plugin session context via .context.json | **Compliant** — selected asset for graph can be persisted in session context |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Performance | What happens with a highly connected asset (100+ edges)? | VisJS handles hundreds of nodes well. For extremely large graphs, the one-level-at-a-time expand approach limits visible nodes. A future iteration could add a max-visible-nodes cap with a "too many connections" warning. |
| 2 | Expand | Can expansion create cycles in the visible graph? | Yes — asset A uses B, B uses A. VisJS handles cycles; duplicate nodes are prevented by keying on asset ID. Expand on an already-expanded node is a no-op. |
| 3 | Navigation | What happens if the clicked node's asset has been deleted since the graph was populated? | The click-to-navigate message includes the asset ID. If the Record Editor cannot find it, it shows a "record not found" inline error. The graph is not auto-refreshed on registry changes (explicit re-select to refresh). |
| 4 | Library | Why VisJS instead of react-flow or D3? | VisJS is already an external dependency in the project (External/VisJS) and provides a purpose-built directed graph widget. react-flow is used by DiaApplicationEditor for flow diagrams, but VisJS is better suited for dependency graphs with expand/collapse. |
| 5 | Concurrency | Can the user expand multiple nodes simultaneously? | Each expand triggers two async DiaAPI calls (get_forward_refs + get_reverse_refs). Results are merged into the VisJS dataset on callback. Multiple concurrent expansions are safe because VisJS dataset add is idempotent by node ID. |
| 6 | Empty state | What does the panel show when no asset is selected? | A centered message: "Select an asset to view its relationships." No VisJS instance is created until the first selection. |
| 7 | Refresh | Does the graph auto-update when relationships change? | No. The graph reflects the state at the time of selection or last expand. To see updated relationships, the user re-selects the asset. This avoids continuous polling and keeps the feature read-only. |

## Visual Reference

[mockups/diaassetcatalogueeditor.html](mockups/diaassetcatalogueeditor.html) — Click **"Relationship Graph"** tab. Shows SVG mockup of directed graph with type-colored nodes, labeled edges (uses, contains), and click-to-navigate. Real implementation will use interactive VisJS. Use as the visual acceptance gate after implementation.

## Status

`Approved`
