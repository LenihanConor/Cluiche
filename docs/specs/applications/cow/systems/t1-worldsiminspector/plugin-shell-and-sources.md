# Feature Spec: T-1 Plugin Shell and Sources

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md

**Gameplay Domains:** debug

## Summary

The foundation every other T-1 feature builds on. It delivers the `CoWEditor` static library, the `CoWWorldSimInspectorPlugin` (a `LiveConnectionPluginBase`), game-scoped registration through `cow_editor.diaapp`, the live subscription to DiaCellHistory topics, the offline `dia.cell_history` file open, the single `Source` model that serves both, the shared time brush and the schema-driven labels and tooltips. No chart or table is drawn here; the views arrive in Features 2 to 5.

## Problem

T-1's views all need the same things: a plugin that exists only while `cow.diagame` is loaded, one data model that hides whether data came from a running game or a report file (CH-004), a time range every view shares, and labels that come from DiaValueSchema. Building these once, against a fixture, lets the views be built and tested before any simulation or DiaCellHistory exists.

## Goals

1. **Game-scoped plugin.** The plugin is created only while `cow.diagame` is loaded and closed when another game loads (Game-Scoped Editor Plugins).
2. **One `Source` model.** Live and file sources expose the same interface: schema, groups, aggregates, events, `cellQuery()`.
3. **Live source.** Subscribes to `cells.history.aggregates`, `cells.history.events` and `cells.view.state`; queries `valueschema.get` on connect.
4. **File source.** Opens a `dia.cell_history` file through a native dialog; C++ reads and validates it and returns chunks; the embedded schema makes the file self-explaining.
5. **Shared time brush.** One start/end sample range in the store, set from a toolbar control, snapped to sample labels.
6. **Honest connection state.** The pill shows connected, disconnected or stale; data survives a disconnect read-only.
7. **Schema-driven text.** A `ValueLabel` and `ValueTooltip` wrapper reads labels, units and descriptions from the source's schema.
8. **Testable before the simulation exists.** A checked-in fixture file and a stub live publisher exercise every path.

## Non-Goals

- Any chart, table, histogram or mini-map (Features 2, 3, 5).
- Event or breakdown navigation, and gating on world match (Feature 6). This feature only exposes the live/file kind and `world` id that Feature 6 needs.
- Saved views and CSV export (Feature 2).
- Source B / compare (Feature 5). The store holds source slots A and B, but only A is populated here.
- Recent-files lists and drag-and-drop file open.
- Writing or editing history files.

## Architecture

### Placement

CoW editor code (AD-002): `Cluiche/CoW/Editor/WorldSimInspector/`, built into the `CoWEditor` static library.

- `CluicheEditor.vcxproj` gains a reference to `CoWEditor`. Shared build file, so it needs AD-001 sign-off at build time.
- The plugin is listed in `Cluiche/Assets/CoW/cow_editor.diaapp` and named from `cow.diagame` `editor.manifests`.

```
CoWWorldSimInspectorPlugin : Dia::Editor::LiveConnectionPluginBase   (C++)
  ├─ OnConnected      subscribe cells.history.aggregates, .events, cells.view.state; query valueschema.get
  ├─ OnDisconnected   mark live source stale (data kept)
  ├─ openHistoryFile  bridge request: native dialog → read → validate → return chunks
  └─ UI bridge        pushes topic updates and file chunks to the React UI

UI (React 18 + zustand + @dia/editor-ui, Vite)    .../WorldSimInspector/UI/
  ├─ model/    Source, LiveSource, FileSource, TimeBrush (pure TS, no React)
  ├─ store/    sources A/B, brush, connection state, selection mirror
  └─ views/    Toolbar, ConnectionPill, SourcePicker, BrushControl, ValueLabel, ValueTooltip
```

Views call only `model/` and the store, never the bridge. The UI is separate from the analysis code.

### Source model

```ts
interface Source {
  kind: 'live' | 'file';
  id: string;                       // file path or "live"
  worldId: string | null;
  schema: ValueSchema;              // labels, units, ramps, event kinds
  groups: GroupInfo[];              // regions, biomes, ...
  samples: SampleInfo[];            // index + label (e.g. "Y3 W12")
  aggregates(layer, group, stat): Float32Array;
  events(): HistoryEvent[];
  cellQuery(layer, sample): Promise<Float32Array>;   // only tracked layers
  status: 'connected' | 'stale' | 'static';
}
```

`LiveSource` appends to its arrays as topics arrive. `FileSource` is fully loaded once the chunks arrive. `cellQuery()` on a layer that is not tracked rejects with `NotTracked`, so views can offer `cells.history.track`.

### File open (`openHistoryFile`)

- The C++ handler opens a native file dialog filtered to `*.json`/`*.cellhistory.json`.
- It validates `format == "dia.cell_history"` and a supported `version`; anything else is rejected with a logged error and a message to the UI.
- It requires an embedded `schema` block. A file with none is rejected: exports must be self-explaining (DiaCellHistory amendment).
- Large files are returned in chunks, so the UI thread never receives one multi-MB message.
- Per-cell data is present only if the export was made with `--cells`.

### Time brush

State `{ startSample, endSample }` in the store, defaulting to the whole run. The toolbar control is a draggable range snapped to sample labels. It is clamped to the populated sample range, and it follows new live samples only while the brush end is at the latest sample ("follow live").

### Connection states

| State | Pill | Data | Notes |
|---|---|---|---|
| Connected | green | live, growing | |
| Disconnected | grey | kept, read-only, marked stale | navigation disabled (Feature 6); reconnect resumes subscription and refills gaps |
| File | blue | static | no connection needed |

### Logging and observation

Channel `[EDITOR] T1`. Logged: source opened/closed, file validation failures, schema query failure, disconnect/reconnect. UI timings in dev builds use `performance.mark`.

## Acceptance Criteria

1. The `CoWEditor` library builds, and `CluicheEditor` references it (after AD-001 sign-off).
2. With `cow.diagame` loaded the plugin exists; loading `cluichetest.diagame` closes it, and it is not creatable there.
3. Opening a valid `dia.cell_history` fixture through the dialog produces a `FileSource` with its schema, groups, samples, aggregates and events.
4. A file with a wrong `format`, an unsupported `version`, or no embedded schema is rejected with a logged `[EDITOR] T1` error and the current source is left unchanged.
5. A file larger than one chunk loads completely and identically to the same data in a single chunk.
6. A stub publisher driving the three topics produces a `LiveSource` whose arrays grow as updates arrive.
7. On disconnect the live source keeps its data and shows `stale`; on reconnect the subscription resumes.
8. The brush defaults to the full run, snaps to sample labels, clamps to the populated range, and follows new live samples only while at the latest sample.
9. `cellQuery()` on an untracked layer rejects with `NotTracked`; on a tracked layer it returns the values for that sample.
10. `ValueLabel` and `ValueTooltip` show the label, unit and description from the active source's schema, and fall back to the raw id with a logged warning when a schema entry is missing.
11. Source A and Source B slots exist in the store; B is empty and nothing in this feature populates it.

## Test Strategy

- **Vitest (model/):** `FileSource` parsing from the fixture; chunk reassembly; `LiveSource` append; `NotTracked`; brush clamp, snap and follow-live; missing-schema fallback.
- **React Testing Library:** toolbar, pill states, source picker, `ValueLabel`/`ValueTooltip` from a fixture schema.
- **GoogleTests (C++):** `openHistoryFile` chunks a large file; rejects bad format, bad version and missing schema; game-scope registration creates and closes the plugin.
- **E2E:** load `cow.diagame` with the stub publisher, see live samples arrive; load `cluichetest.diagame`, plugin is gone.
- **Fixture:** a hand-written `dia.cell_history` (a few layers, 2 groupings, about 100 samples, a handful of typed events, with and without cells) is checked in beside the UI tests.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-001 | Ask before Dia / CluicheEditor / shared build edits | The `CluicheEditor.vcxproj` reference is made only after sign-off; no DiaEditor change is planned |
| CoW AD-002 | Four-tier placement | All code under `Cluiche/CoW/Editor/`; nothing CoW-specific goes into Dia |
| AED-002 | Plugins are listed in a `.diaapp` `"editor"` section | `cow_editor.diaapp` lists the plugin |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` |
| Game-Scoped Plugins | Fully unavailable out of scope | AC 2: never created unless CoW is loaded |
| CH-004 | Live and file data share one format | One `Source` interface, two implementations |

No conflicts.

## Open Design Questions

1. **Chunk size and message shape.** Pick the chunk unit (per layer, per sample range, or fixed bytes) when the C++ handler is written, based on how the DiaDebugServer bridge handles large payloads.
2. **Gap refill after reconnect.** Whether reconnect asks the game for the missed samples (needs a DiaCellHistory range query) or accepts a gap marked in the series. Depends on DiaCellHistory Feature 2.
3. **Follow-live and the brush.** Whether "follow live" should be an explicit toggle instead of inferred from the brush end.

## Status

`Approved` (2026-10-20). Plan not yet created. Build needs AD-001 sign-off for the `CluicheEditor.vcxproj` reference.
