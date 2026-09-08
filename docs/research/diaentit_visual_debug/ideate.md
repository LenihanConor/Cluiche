# Research: Ideate — diaentitytemplate Visual Debugger & Editor Options

**Input:** docs/research/diaentit_visual_debug/explore.md

## Candidates

### Candidate 1: In-game ImGui Entity Inspector
**Home module/system:** DiaVisualDebugger (DiaVisualDebuggerConsole sibling)
**Size:** M (1–3 weeks)
**Description:** An in-process ImGui window guarded by `#ifdef DIA_DEBUG` that renders alongside `DiaVisualDebuggerConsole`. Accepts a pointer to `IEntityInspectable` and renders: entity list, component-fields panel for selected entity driven by `ComponentTypeDesc` reflection, Tier (b) live field edit via `WriteField()` on commit. Selection state shared with `DebugLayerManager::SetSelectedEntityId()`. No WebSocket, no editor process.
**Primary value:** First live view into Domain entity state with zero infrastructure — fastest path to any diaentitytemplate debugging. Dropped before evaluation as redundant once Candidate 2 ships.

### Candidate 2: CluicheEditor Entity Inspector Panel
**Home module/system:** New `DiaEntityEditor` plugin (`Dia/DiaEntityEditor/`) implementing `IEditorPlugin`
**Size:** M (1–3 weeks)
**Description:** A React/CEF editor panel inside CluicheEditor, subscribing to a new `entity.inspect` WebSocket topic pushed by DiaDebugServer. Renders an entity table, component accordion for the selected entity, hierarchy breadcrumb. Field values update on topic push. Tier (b) field edit routes through `GameConnectionManager::SendUpdate()` with undo via `CommandHistory`.
**Primary value:** Persistent, persistent canonical long-term debugging surface integrated with the full editor.

### Candidate 3: Entity Viewport Picking
**Home module/system:** DiaVisualDebugger (DebugLayerManager)
**Size:** S (≤1 week)
**Description:** Wires the existing picking seam. Implements `PickAt(screenX, screenY)` on `DebugLayerManager` — iterates `DebugFrameData`, finds nearest tagged primitive within threshold, calls `SetSelectedEntityId(entityId)`. Implements `debug.pick x y` DiaAPI command body (previously a no-op stub). No DiaInput dependency — caller supplies coordinates.
**Primary value:** Click-to-select in the game viewport. Pure seam completion — highest leverage per engineering day.

### Candidate 4: Entity Viewport Overlay Draw Class
**Home module/system:** DiaVisualDebugger (new draw class `"entity.labels"` layer)
**Size:** S (≤1 week)
**Description:** New `EntityLabelsDrawer` implementing `IVisualDebugger`. Reads entity list via `IEntityInspectable`, emits `DebugPrimitiveText2D` over each live entity that has a registered position provider. Label: debug name + component short tags. Selected entity highlighted with `kGoal` (cyan). Caller registers `StringCRC → Vec2 fn` position providers and `StringCRC → short tag` mappings.
**Primary value:** World-space entity labels in the game viewport.

### Candidate 5: Query Browser Tab
**Home module/system:** DiaEntityEditor — secondary tab
**Size:** S (≤1 week add-on)
**Description:** Secondary tab showing all active query descriptors: component-type signature, result set count, entity membership on expand. Click entity row to select. Requires additive extension to `IEntityInspectable`: `GetQueryCount()`, `GetQuerySignature(index)`, `GetQueryEntityCount(index)`.
**Primary value:** Verify that queries return the right entities — most common diaentitytemplate debugging task after "does my entity exist."

### Candidate 6: Mailbox Traffic Monitor
**Home module/system:** DiaEntityEditor — tab/panel
**Size:** M (1–3 weeks)
**Description:** Dedicated view showing diaentitytemplate mailbox activity. C++ side adds a ring buffer to `Domain::EndOfFrame()` recording dispatched messages (sender, address kind, message type CRC, frame number). Editor panel renders message table, aggregate histogram, per-entity filter, pause/snapshot mode.
**Primary value:** Makes the mailbox — diaentitytemplate's most complex and least visible subsystem — observable.

### Candidate 7: Blueprint JSON Editor Panel (Static)
**Home module/system:** New `DiaEntityEntityTemplateEditor` plugin — static editing, no live game required
**Size:** L (1–2 months)
**Description:** CluicheEditor panel for authoring `.blueprint` JSON files. Loads a blueprint, parses its entity graph, renders as an editable tree with component accordions and per-field type-aware widgets generated from `ComponentTypeDesc` reflection. Validates against registered component schemas in real time. Undo/redo via `CommandHistory`. Requires `export-component-schema` DiaCLI command as prerequisite.
**Primary value:** Blueprint authoring without run-reload cycles.

### Candidate 8: Entity Watch List
**Home module/system:** DiaEntityEditor — persistent panel
**Size:** M (1–3 weeks)
**Description:** Persistent "watch list" tracking user-configured `(entity stable-id, component type, field name)` triples regardless of current selection. Entity referenced by debug name (stable across reconnects). Serialized to `watchlist.json` session sidecar. On reconnect, re-resolves entity handles from the Domain by debug name lookup.
**Primary value:** "Keep an eye on field X of entity Y while stress-testing scenario Z" — the debugging pattern that doesn't fit the selection model.

## Coverage Map

**Surface type:** In-game (1, 4) · Editor panel (2, 5, 6, 7, 8) · Pure plumbing bridge (3)
**Interaction tier:** Read-only (3, 4, 5, 6) · Read + live field edit (1, 2, 8) · Static file edit (7)
**Data scope:** Per-entity fields (1, 2, 8) · Entity population + picking (3, 4) · Query aggregate (5) · Communication (6) · Authoring/static (7)
**Scope range:** S (3, 4, 5) · M (1, 2, 6, 8) · L (7)
**Infrastructure required:** Zero (1, 3, 4) · WebSocket + editor (2, 5, 6, 8) · Full static pipeline (7)
