# Research: Explore — DiaEntity Visual Debugger & Editor Options

**Session date:** 2026-05-24
**Folder:** docs/research/diaentit_visual_debug/

## Problem Space Overview

DiaEntity provides the gameplay-level entity model for the Cluiche platform: generational handles, component pools, mailbox routing, hierarchy, query caches, and reflection metadata via `ComponentTypeDesc`. This data is generated continuously at runtime but is presently invisible to the developer outside of code-level inspection and log output. A visual debugger and editor surface would make entity state observable and — at higher interaction tiers — editable without recompiling or restarting the game.

The need is two-sided. During authoring a developer wants to browse the Domain's entity population, inspect component field values, understand hierarchy relationships, and verify that a blueprint loaded correctly. During debugging a developer wants to observe live state changes frame-by-frame, correlate component values with rendered primitives in the viewport (via the existing `entityId` field on every `DebugPrimitive`), and pick entities by clicking on them in the 2D view. Neither workflow is currently served by any built tool in Cluiche.

The problem space sits at an intersection of three already-built systems — DiaEntity's `IEntityInspectable`, DiaVisualDebugger's entity-picking seam, and DiaEditor's plugin + WebSocket data pipeline — which means the infrastructure to serve this feature exists in fragments and needs to be connected rather than invented. The primary design challenge is deciding what the surface should look like, which interaction tier (read-only, live field edit, or deferred structural edit) to expose first, and how to manage the data volume that a large Domain with many entities and components generates.

## What Data Exists to Show

**Domain state (from IEntityInspectable)**
- Total live entity count
- Peak entity count since Domain creation
- Number of registered component types (ComponentRegistry)
- Domain name / identity (StringCRC)
- Per-entity: index, generation, alive/dead flag
- Per-entity: bitmask or list of which component types are attached

**Component fields (via reflection — ComponentTypeDesc)**
- Field name (string)
- Field offset and size in the component struct
- Field type tag (int, float, bool, vec2, vec3, string, etc.)
- Field default value
- Live field value read via `IEntityInspectable::ReadField()`
- Tier (b): writable via `IEntityInspectable::WriteField()`

**Mailbox**
- Queued message count per entity
- Message type IDs (StringCRC) in flight
- History of dispatched messages for a selected entity (if a ring buffer is kept)

**Hierarchy**
- Parent entity handle per entity
- Children list per entity
- Depth in hierarchy tree
- Subtree size

**Queries**
- Registered query descriptors (component type masks)
- Result set size per query
- Whether a query result is cached or stale
- Selected entity's membership in each registered query

## Existing Approaches (industry patterns)

- **Unity Inspector** — property panel bound to a selected GameObject; reflects fields from MonoBehaviour components; live edit while Play mode is active; runs inside the Unity Editor process (not over a socket)
- **Unreal Details Panel** — multi-selection capable property panel over UObject hierarchy; handles per-type customizations via IDetailCustomization; supports transactional undo/redo
- **flecs explorer** — browser-based web UI that connects over HTTP to a running flecs world; shows entity tree, component values as JSON, query builder, and relationship graph; entirely read-only by default
- **Dear ImGui entity tree (roll-your-own)** — common pattern in indie/mid-size engines: ImGui window rendered in-game showing a collapsible tree of entities; zero infrastructure, low latency, no editor process needed
- **EnTT + custom editor integration** — EnTT provides `entt::meta` for runtime reflection; teams build ImGui property grids driven by meta type iteration; field edit via meta set(); same process, no network layer
- **Godot Scene Tree dock** — tree view of nodes (analogous to entities); Inspector dock below shows properties; separates structure navigation from field editing
- **Entitas visual debugger (Unity plugin)** — dedicated window showing pools, group (query) membership counts, component breakdown per pool; primarily aggregate statistics, not per-entity field editing
- **Bevy Inspector egui** — Rust/Bevy ecosystem; reflect-driven egui panel; automatic UI generated from `#[derive(Reflect)]`; runs in-process in debug builds; field edit via Commands queue to avoid race conditions
- **Lumberyard/O3DE Entity Inspector** — property panel driven by serialization context; component add/remove at edit time only; runtime inspection read-only with separate "debug component" concept
- **Frida-style live patch** — game mod tools using DLL injection + memory write to patch component fields; not applicable here but represents the far end of the "structural edit" tier axis

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Surface type | In-game ImGui overlay / CluicheEditor React panel / standalone browser page / hybrid | ImGui has lowest latency and zero infrastructure; editor panel has persistence, layout, and undo |
| Data scope | All entities in Domain / filtered by query / single selected entity / aggregate statistics only | Full population may be thousands of entities; scope needs a filter model to be usable |
| Update frequency | Every frame (streaming) / on-demand poll / on-selection-change only / diff-only push | Full streaming is high bandwidth; diff-only push over WebSocket is practical for a selected entity |
| Entity selection | Viewport click (entityId from DebugPrimitive) / tree-view click / filter-by-component / API command `debug.pick` | The picking seam already exists in DebugLayerManager; needs implementation wired to IEntityInspectable |
| Interaction tier | Tier (a) read-only / Tier (b) live field edit via WriteField() / Tier (c) structural edit (deferred) | SD-ENT-016 defers Tier (c); v1 should deliver (a) + (b) |
| In-game vs editor | In-game debug build only / editor-connected live session / editor static (non-running game) / both | Static editing requires blueprint round-trip; live editing requires game running |
| Protocol | DiaDebugServer WebSocket topic push / direct ImGui in-process / DiaAPI command channel / all three | Topic push fits editor panel; ImGui fits in-game overlay |
| Pick seam integration | Wire `debug.pick` command to IEntityInspectable selection / add mouse-ray hit-test over DebugPrimitive entityId tags / both | Current `debug.pick` is a no-op stub; DebugPrimitive carries entityId already |
| Hierarchy display | Flat list + component filter / tree view reflecting parent-child / both with toggle | Tree view is more intuitive but expensive to build in React |
| Field type rendering | Generic string fallback / per-type widgets (slider for float, checkbox for bool) / custom renderers per component | Generic fallback ships fastest; per-type widgets dramatically improve usability |

## Known Tradeoffs

- **Latency vs bandwidth**: streaming full entity state every frame gives the most responsive display but will saturate the WebSocket channel in a Domain with thousands of entities
- **In-game ImGui vs editor panel**: ImGui overlays are trivial to add and have zero latency but disappear in Release builds and clutter the game view
- **Tier (b) field edit safety**: writing component fields live while the simulation is running creates race conditions if the write is not synchronized with the update loop
- **Reflection coverage vs macro discipline**: `ComponentTypeDesc` is generated only for components declared with `DIA_COMPONENT` + `FIELD` macros; components that skip the macro are invisible
- **Entity ID stability**: generational handles make entity IDs stable within a session but IDs change across runs
- **Query result display cost**: materializing query result sets for display may touch many component pools; show as counts by default with on-demand expansion
- **Structural edit deferral creates a gap**: Tier (c) is deferred; controls need to be visually present but disabled so users understand the limitation
- **Data volume for hierarchy**: a deep or wide hierarchy is expensive to serialize; lazy expansion required

## Known Pitfalls (C++ / game engine context)

- **Dangling entity handles**: if an entity is destroyed between the editor requesting its data and the response being sent, the handle is invalid; protocol must handle "entity no longer exists" as a normal response code
- **Thread-safety of WriteField during simulation**: the update loop runs on a separate thread; writing a field from the editor's response handler without synchronization causes data races
- **StringCRC display problem**: component type IDs and field names are stored as `StringCRC`; the debugger needs the original string — ComponentTypeDesc stores it but protocol serialization must preserve it
- **Macro-generated metadata completeness**: `FIELD` macros only capture explicitly listed fields; the panel should show only declared fields
- **Component pool pointer stability**: component data pointers can move if pools are resized; the debugger must re-query via `IEntityInspectable` on every refresh cycle

## Cluiche-Specific Opportunities

### Existing Seams Already Built

- **IEntityInspectable on Domain** — Tier (a) read and Tier (b) field write interface already specced (editor-inspection feature)
- **EntityId picking seam in DebugLayerManager** — `SetSelectedEntityId(uint32_t)` method already reserved; `DebugPrimitive` variants already carry an `entityId` field (0 = untagged)
- **`debug.pick` DiaAPI no-op stub** — registered command stub (SD-DBG-008) exists; needs an implementation body
- **DiaDebugServer WebSocket topic push** — existing server-side broadcast mechanism; adding an `entity.inspect` topic requires a new topic handler, not a new transport
- **GameConnectionManager subscribe model in DiaEditor** — editor-side subscription to topics is already wired
- **WebUIBridge `NotifyUIDataChanged(topic, data)`** — C++ → JS notification path already exists
- **ComponentRegistry** — runtime registry of all `DIA_COMPONENT` types with their `ComponentTypeDesc` metadata

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaEntity / Domain | Primary data source: entity population, component pools, hierarchy, query cache, mailbox |
| DiaEntity / IEntityInspectable | The inspection API that the debugger will call for read and field-write operations |
| DiaEntity / ComponentRegistry | Provides the full list of registered component types and their `ComponentTypeDesc` for UI generation |
| DiaVisualDebugger / DebugLayerManager | Holds the picking seam (`SetSelectedEntityId`); owns DebugPrimitive stream tagged with `entityId` |
| DiaEditor / IEditorPlugin | Plugin interface that the entity inspector panel will implement |
| DiaEditor / GameConnectionManager | Manages the WebSocket connection; the plugin subscribes to the `entity.inspect` topic through this |
| DiaEditor / WebUIBridge | Pushes serialized entity data to the React UI layer via `NotifyUIDataChanged` |
| DiaDebugServer | Server running in the game process; needs a new topic handler for entity inspection data |
| DiaDebugProtocol | Shared header defining WebSocket message schemas; needs an `entity.inspect` message type added |
| DiaMaths | Vec2/Vec3 types that appear as component field types; the panel needs type-aware widgets for these |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001: StringCRC for all identifiers | Component type IDs and entity IDs in the protocol must carry both the CRC value and the original string name for human-readable display |
| PD-004: No STL containers in public APIs | `IEntityInspectable` and any new DiaDebugProtocol message structs must use DiaCore containers or fixed-size arrays |
| PD-007: C++20 | Reflection metadata helpers and protocol serialization can use C++20 features |
| SD-DBG-002: Debug draw guarded by `#ifdef DIA_DEBUG` | The entity visual overlay must be inside `DIA_DEBUG` guards; the editor panel itself does not need the guard |
| SD-ENT-016: Tier (c) structural edit deferred | Add/remove component UI must be present but disabled in v1 |
| SD-DBG-007 / SD-DBG-008: Picking seam reserved | Implement against the reserved `SetSelectedEntityId` seam and `debug.pick` DiaAPI command stub |

## Open Questions for Ideation

- Should the entity inspector be a single CluicheEditor plugin panel, an in-game ImGui overlay, or both?
- What is the right WebSocket topic granularity: one `entity.inspect` topic or multiple independent topics?
- How should the panel handle entity destruction while it is the selected entity?
- Is the primary navigation entry point the viewport (click to pick) or the entity tree panel?
- For Tier (b) live field editing, should writes be applied immediately on value change or only on explicit commit?
- Should the panel expose query membership as a secondary tab or inline per entity as a tag list?
- How should hierarchy depth be visualized — indented tree rows, separate graph, breadcrumb, or combination?
- Should component field widgets be generic or type-aware? Where do range hints come from?
- Is there value in a "watch list" — a persistent set of (entity, component, field) triples?
- How should the mailbox data be surfaced — count badge, dedicated tab, historical ring buffer, or all three?
- Should the entity tree support multi-selection for batch field comparison or batch field edit?
- Should the in-game ImGui overlay auto-annotate entity positions in screen space?
- What is the right session persistence model for the editor panel?
- How does the entity inspector interact with the planned Stages tab?
