# Feature Spec: Stream Inspector

## Parent System
@docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007 |
| Application | @docs/specs/applications/dia/dia.md | AD-003 |
| System | @docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md | ED-002, ED-007, ED-008, ED-010 |
| System (upstream) | @docs/specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md | SD-006, SD-018 |

## Purpose

Provide the Streams tab and stream detail inspector. The Streams tab is the third tab (ED-002) showing all streams in a detailed list view with type, from/to PU, readers, writers, and (in live mode) throughput. Clicking a stream row shows full detail in the sidebar inspector. Also supports add/remove/edit of stream declarations.

## Acceptance Criteria

1. **Streams tab** — Third tab in the layout (ED-002). Shows all streams as a list/table.
2. **Stream list columns** — ID, type (FrameStream/EventStream + payload type), from PU, to PU, reader count, writer count.
3. **Stream detail sidebar** — Clicking a stream row populates a sidebar inspector with: full properties, readers list (module instance_ids), writers list (module instance_ids), capacity, maxReaders.
4. **Add stream** — "Add Stream" button opens dialog: ID, type (dropdown), payload type, from PU, to PU, capacity, maxReaders.
5. **Edit stream** — All stream properties editable in the detail sidebar (via commands/Undo/Redo).
6. **Delete stream** — Delete button on detail sidebar. Compound command: removes stream + removes from all modules' reads/writes arrays.
7. **`$`-prefix streams read-only** — Framework-reserved `$`-prefix streams are displayed but cannot be edited or deleted (SD-018).
8. **Navigation from Graph** — Stream labels clicked on Graph View navigate here with that stream selected (ED-010).
9. **Traffic-light dot** — Each stream row has a `.tl` dot (ED-008). Grey offline; green in live mode when data is flowing.
10. **Readers/writers clickable** — Module instance_ids in readers/writers lists are clickable → navigate to Module Inspector.

## Design

### React Component Structure

```
StreamsTab (tab content)
├── StreamToolbar
│   └── AddStreamButton → AddStreamDialog (modal)
├── StreamTable
│   └── StreamRow[] (clickable)
│       ├── TrafficLightDot
│       ├── IDCell (stream id)
│       ├── TypeCell (FrameStream/EventStream<PayloadType>)
│       ├── FromCell (PU name)
│       ├── ToCell (PU name)
│       ├── ReadersCell (count)
│       └── WritersCell (count)
└── (sidebar) StreamDetailInspector (on row selection)
    ├── StreamHeader (id, .tl dot, $-prefix badge if reserved)
    ├── PropertiesSection
    │   ├── TypeField (editable dropdown, disabled for $-prefix)
    │   ├── PayloadTypeField (text input)
    │   ├── FromPUField (dropdown of PU instance_ids)
    │   ├── ToPUField (dropdown of PU instance_ids)
    │   ├── CapacityField (number)
    │   └── MaxReadersField (number)
    ├── ReadersSection
    │   └── ModuleRef[] (clickable → Module Inspector)
    ├── WritersSection
    │   └── ModuleRef[] (clickable → Module Inspector)
    ├── LiveThroughputSection (only in live mode)
    │   └── messages/sec, bytes/sec (from runtime data)
    └── DeleteButton (disabled for $-prefix)
```

### Commands

| Action | Command | Notes |
|--------|---------|-------|
| Add stream | `AddStreamCommand` | Validates unique ID, valid from/to PUs |
| Delete stream | `RemoveStreamCommand` | Compound: removes from all module reads/writes |
| Set stream type | `SetStreamTypeCommand` | |
| Set stream payload type | `SetStreamPayloadTypeCommand` | |
| Set stream from PU | `SetStreamFromPUCommand` | |
| Set stream to PU | `SetStreamToPUCommand` | |
| Set stream capacity | `SetStreamCapacityCommand` | Must be > 0 |
| Set stream maxReaders | `SetStreamMaxReadersCommand` | Must be > 0 |

### `$`-prefix Stream Protection

On render, check if stream ID starts with `$`:
- If yes: all fields disabled, delete button disabled, "Reserved" badge shown
- User cannot create streams with `$` prefix (Add dialog validates this)

### Navigation

When Graph View dispatches `navigateToStream(streamId)`:
1. Switch to Streams tab
2. Scroll to and highlight the matching stream row
3. Populate the detail sidebar

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | React `StreamsTab` with table layout | Manual: renders stream list | Todo | |
| 2 | `StreamDetailInspector` sidebar | Manual: shows full properties on selection | Todo | |
| 3 | Add Stream dialog with validation | Manual: creates stream, blocks $-prefix | Todo | SD-018 |
| 4 | Edit stream properties (all command classes) | Unit test: execute/undo for each | Todo | |
| 5 | Delete stream compound command | Unit test: removes stream + references | Todo | |
| 6 | $-prefix read-only protection | Manual: reserved streams not editable | Todo | SD-018 |
| 7 | Navigation from Graph View (ED-010) | Manual: click graph label → stream selected | Todo | |
| 8 | Readers/writers click → Module Inspector | Manual: click navigates | Todo | |
| 9 | Live throughput display (render slot) | Manual: shows data when live | Todo | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Stream IDs stored as StringCRC, displayed as strings. |
| PD-007 | C++20 required | Backend command code uses C++20. |
| AD-003 | Namespace Dia::\<Module\>:: | C++ in `Dia::ApplicationFlow::Editor::`. |
| ED-002 | Three-tab layout | Streams is the third tab. |
| ED-007 | React + CEF frontend | Implemented in React. |
| ED-008 | Traffic-light dot primitive | Each stream row has a `.tl` dot. |
| ED-010 | Stream labels navigate to Streams tab | Graph edge label click lands here with stream selected. |
| SD-006 | Streams in config, framework-owned | Editor visualizes and edits stream declarations from config. |
| SD-018 | Reserved `$`-prefix streams | Shown read-only; cannot create/edit/delete `$`-prefix streams. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Type | Should stream type be a free-text field or constrained dropdown (FrameStream/EventStream)? | Constrained dropdown for the container type (FrameStream/EventStream). Payload type is free-text since it's application-defined. |
| 2 | Capacity | What's a reasonable default capacity for new streams? | 1 for FrameStream (latest-only), 64 for EventStream (ring buffer). Shown as default in Add dialog. |
| 3 | Validation | Should from/to PU being the same be blocked? | Yes — a stream from a PU to itself is invalid (streams are inter-PU communication). Blocked in Add dialog and edit field. |
| 4 | Sort | Should the stream table be sortable by column? | Yes — click column header to sort. Default: config declaration order. |

## Status

`Approved` — 2026-05-19
