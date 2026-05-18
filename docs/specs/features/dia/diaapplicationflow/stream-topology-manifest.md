# Feature Spec: Stream Topology — Manifest-Authoritative

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Research
@docs/research/event_stream_diapplic/summary.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Stream Topology — Manifest-Authoritative | (this file) |

## Problem Statement

The current event-stream layer treats the `.diaapp` `streams[]` array and `module.reads`/`writes` arrays as descriptive metadata, while the runtime actually creates stream stores lazily on first `Connect()` from C++ handles. That divergence has three consequences:

1. **Hard caps** (`kMaxStreams=16`, `kMaxReaders=8`) are compile-time constants, hit silently when exceeded.
2. **No type-tag check** — `static_cast<EventStreamStore<T>*>(istore)` trusts the first registrant's `T`, so a second handle with a different `T` is silent UB.
3. **Topology is invisible to the manifest** — orphan readers, mismatched writers, undeclared streams all fail at runtime (or worse, silently).

This feature makes the manifest the single source of truth for stream topology: streams are created at `Application::Start()` from `streams[]` declarations; module handles are bound by manifest `reads`/`writes` rather than by code-side IDs; manifests carry the payload type as a `StringCRC` so type aliasing is caught at startup; and per-stream caps come from manifest data, not magic constants.

## Acceptance Criteria

1. **AC1 — Manifest-authoritative creation.** `Application::Start()` creates every `IStreamStore` from `manifest.streams[]`. `FindOrRegisterStreamStoreAtStartup` is removed; modules cannot create streams. A code handle whose `streamId` does not appear in `manifest.streams[]` causes `Start()` to return `false`.
2. **AC2 — `reads`/`writes` are binding.** When a module's `EventStreamWriter<T>` / `EventStreamReader<T>` (or Frame variants) calls `Connect(app)`, the framework verifies the stream ID appears in that module's `manifest.module.reads` (for readers) or `writes` (for writers). Mismatch → `Start()` fails.
3. **AC3 — Type tag enforced.** Each `IStreamStore` records the `StringCRC` payload type at construction (from `manifest.streams[].payload_type`). A handle's `T` is mapped to a `StringCRC` via the type registry; `Connect()` asserts `store.payloadType == registry.GetTypeId<T>()`. Mismatch → `Start()` fails with diagnostic.
4. **AC4 — Per-stream caps from manifest.** `EventStreamStore::kMaxReaders` becomes a per-store value. The store sizes its reader-slot array from a manifest-declared count or from the count of modules that declare the stream in `reads`. `kMaxStreams` is raised to a generous compile-time bound (64) and counted, not silently truncated.
5. **AC5 — `DIA_STREAM_TYPE` macro.** A static-init macro `DIA_STREAM_TYPE(T)` registers the `StringCRC` for an event/frame payload type, symmetric to `DIA_MODULE`. `EventStreamWriter<T>`/`EventStreamReader<T>` look up `T`'s `StringCRC` from this registry at `Connect()`.
6. **AC6 — Schema split.** Manifest stream entries use `kind` (`"FrameStream"` or `"EventStream"`) and `payload_type` (string mapped to `StringCRC`). The legacy single-`type` field is accepted with a deprecation warning during the transition window (which ends when CluicheTest's manifest is migrated, in this feature's plan).
7. **AC7 — Validator coverage.** `ManifestValidatorV2` returns distinct error codes for: orphan reader (no writer for stream), orphan writer (no reader), unknown stream in `reads`/`writes`, payload-type registration missing, capacity overflow, duplicate stream ID, fromPU/toPU mismatch with module placement.
8. **AC8 — CluicheTest migrated.** `cluiche_main.diaapp` updated with `payload_type` for all four streams; type registrations added for `InputEvent`, `UICommand`, `FrameData`, `UIDataBuffer`. CluicheTest runs end-to-end on the new schema.
9. **AC9 — v1 MessageBus deleted.** `MessageBus.h`/`.cpp` removed from DiaApplicationFlow; vcxproj+filters updated; no compile errors.
10. **AC10 — Dead v1 phase manifests removed.** `cluiche_sim.diaapp`, `cluiche_render.diaapp`, their `assets.catalogue.json` entries, and the `stage.global` `contains` references to them are deleted. Build still produces a working CluicheTest.
11. **AC11 — Tests cover every validator error path.** GoogleTests in `Cluiche/Tests/GoogleTests/ApplicationFlow/` covers each error code from AC7 with a manifest fragment that triggers it. Existing `TestStreams.cpp` round-trip tests continue to pass.

## Public API

```cpp
namespace Dia::ApplicationFlow {

    // Type registration — symmetric to DIA_MODULE.
    // Static-init registers StringCRC("InputEvent") -> typeid(InputEvent).
    #define DIA_STREAM_TYPE(T) \
        static Dia::ApplicationFlow::StreamTypeRegistration<T> \
            s_streamtype_##T{StringCRC(#T)}

    // Type registry — populated by DIA_STREAM_TYPE static init.
    class StreamTypeRegistry {
    public:
        template<typename T>
        static StringCRC GetTypeId();   // asserts if T not registered

        static bool IsRegistered(const StringCRC& typeId);
    };

    // IStreamStore — extended with payload type
    class IStreamStore {
    public:
        virtual const StringCRC& GetId() const = 0;
        virtual StreamKind GetKind() const = 0;
        virtual const StringCRC& GetPayloadType() const = 0;   // NEW
        virtual unsigned int GetMaxReaders() const = 0;        // NEW (per-store cap)
    };

    // Application — narrowed
    class Application {
    public:
        // FindStreamStore stays. FindOrRegisterStreamStoreAtStartup is REMOVED.
        IStreamStore* FindStreamStore(const StringCRC& id) const;

        // ... rest unchanged
    };

    // Handles — Connect signature unchanged from caller perspective; internally:
    //   1. Looks up store by streamId in app
    //   2. Verifies store.GetPayloadType() == StreamTypeRegistry::GetTypeId<T>()
    //   3. Verifies streamId is in owner module's manifest reads/writes
    //   4. For readers: registers a reader slot (must succeed; cap is from manifest)
    //   5. Asserts on any failure; sets internal state to "failed-connect"
    //      which causes Application::Start() to return false.
}
```

## Manifest Schema

**v2.1 stream entry (split fields):**
```json
{
  "id": "InputToSim",
  "kind": "EventStream",
  "payload_type": "InputEvent",
  "from": "MainPU",
  "to": "SimPU",
  "capacity": 256,        // optional; default 256 for events, N/A for frames
  "max_readers": 1,       // optional; if absent, derived from count of modules reading this stream
  "multi_writer": false   // optional; default false
}
```

**v2.0 entry (deprecated, accepted with warning during transition):**
```json
{ "id": "InputToSim", "type": "EventStream", "from": "MainPU", "to": "SimPU" }
```

The transition ends when CluicheTest's manifest is migrated (a task in this feature's plan). After that, v2.0 is rejected.

**Module reads/writes (already present, now binding):**
```json
{
  "instance_id": "KernelModule",
  "reads": [],
  "writes": ["InputToSim"]
}
```

## Internal Architecture

```
Application::Start()
  1. Validate manifest (ManifestValidatorV2)
       — every stream has resolvable payload_type in StreamTypeRegistry
       — every reads/writes entry references a declared stream
       — no orphan readers/writers
       — fromPU/toPU match module placement
  2. Build PUs and modules
  3. Create stream stores from manifest.streams[]
       — store knows its id, kind, payloadType, maxReaders, capacity
  4. Call OnConnectStreams on each module
       — handles look up store by id
       — verify type tag
       — verify streamId in owner's reads/writes
       — register reader slot (event streams)
  5. If any handle fails to connect, Start() returns false
```

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/Manifest/ApplicationManifestV2.h` | Add `payload_type`, optional `capacity`, optional `max_readers` to `StreamDeclaration`; rename `type` to `kind` |
| `Dia/DiaApplicationFlow/Manifest/JsonApplicationManifestSerializer.{h,cpp}` | Parse new fields; accept legacy `type` with warning |
| `Dia/DiaApplicationFlow/Manifest/ManifestValidatorV2.{h,cpp}` | New error codes (AC7), type-registry checks |
| `Dia/DiaApplicationFlow/Streams/IStreamStore.h` | Add `GetPayloadType()`, `GetMaxReaders()` |
| `Dia/DiaApplicationFlow/Streams/EventStreamStore.h` | Per-store `kMaxReaders` (was `static constexpr`); store payload type |
| `Dia/DiaApplicationFlow/Streams/FrameStreamStore.h` | Store payload type |
| `Dia/DiaApplicationFlow/Streams/EventStreamWriter.h` | `Connect()` does type-tag + reads/writes check |
| `Dia/DiaApplicationFlow/Streams/EventStreamReader.h` | Same |
| `Dia/DiaApplicationFlow/Streams/StreamWriter.h` | Same |
| `Dia/DiaApplicationFlow/Streams/StreamReader.h` | Same |
| `Dia/DiaApplicationFlow/Streams/StreamTypeRegistry.{h,cpp}` | New |
| `Dia/DiaApplicationFlow/RegistrationMacrosV2.h` | Add `DIA_STREAM_TYPE` |
| `Dia/DiaApplicationFlow/Application.{h,cpp}` | Remove `FindOrRegisterStreamStoreAtStartup`; create stores from manifest in `Start()` |
| `Dia/DiaApplicationFlow/MessageBus.h`/`.cpp` | **Delete** (AC9) |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj{,.filters}` | Add new files; remove deleted ones |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Migrate to v2.1 schema (AC8) |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_sim.diaapp` | **Delete** (AC10 — dead v1 fragment) |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_render.diaapp` | **Delete** (AC10) |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Remove `manifest.cluiche_sim`, `manifest.cluiche_render` entries; remove from `stage.global` contains refs (AC10) |
| `Cluiche/CluicheGameBaseline/Types/InputEvent.h` | Add `DIA_STREAM_TYPE(InputEvent)` registration |
| `Cluiche/CluicheGameBaseline/Types/UICommand.h` | Add `DIA_STREAM_TYPE(UICommand)` |
| Other event types used in CluicheTest (FrameData, UIDataBuffer) | Add registrations |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestStreams.cpp` | Update for new APIs |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestStreamTopology.cpp` | New — validator error path coverage (AC11) |
| `Dia/DiaApplicationFlow/dia.application.architecture.module.md` | Remove MessageBus from responsibilities (AC9); update streams section |

## Dependencies

- **Streams** feature (this system) — superseded by this feature once approved
- **Config Format v2** (this system) — schema evolves to v2.1
- **Validation** (this system) — new error codes
- **DiaCore/CRC/StringCRC** — payload type IDs
- **DiaSerializer** — manifest parsing

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — payload types are `StringCRC`; stream IDs are `StringCRC`; type registry is `StringCRC`-keyed |
| PD-002 | Platform | ProcessingUnit/Phase/Module (PU/Stage/Module per superseding system spec) | Compliant — feature operates inside the existing Module lifecycle (`OnConnectStreams`); no new lifecycle introduced |
| PD-003 | Platform | Component-based entities | N/A — feature is framework-internal |
| PD-004 | Platform | No STL containers in public APIs | Compliant — `DynamicArrayC` for handle lists; type registry uses `HashTableC` |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant — vcxproj manually maintained |
| PD-007 | Platform | C++20 required | Compliant |
| PD-008 | Platform | Directory.Build.props owns build settings | Compliant — no per-project overrides |
| PD-009 | Platform | Generated output in Cluiche/out/ | N/A |
| PD-010 | Platform | .diagame is project root, .diastage stage metadata, typed imports | Compliant — feature works on `.diaapp` consumed via `.diagame` import; no change to entry-point conventions |
| AD-001 | Dia App | Module YAML frontmatter | Compliant — `dia.application.architecture.module.md` updated |
| AD-002 | Dia App | No STL in public APIs | Compliant (reinforces PD-004) |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — all code in `Dia::ApplicationFlow::` |
| AD-004 | Dia App | PU/Phase/Module for app structure | Compliant — feature respects existing structure |
| AD-005 | Dia App | Component-based entities | N/A |
| SD-001 | DiaAppFlow | Config is sole source of truth for structural wiring | **Strengthens** — manifest becomes binding for streams, not just descriptive |
| SD-002 | DiaAppFlow | Stages replace phases | Compliant — no phase logic introduced |
| SD-003 | DiaAppFlow | ModuleRef<T> is sole module access pattern | Compliant — feature does not change module access |
| SD-004 | DiaAppFlow | TransitionTo is app-wide | N/A — feature is startup-only |
| SD-005 | DiaAppFlow | Transitions execute next frame | N/A |
| SD-006 | DiaAppFlow | Streams are framework-owned, declared in config | **Strengthens** — closes the "declared in config but created lazily in code" gap |
| SD-007 | DiaAppFlow | MessageBus removed, EventStream replaces | **Completes** — physical deletion of the v1 files (AC9) |
| SD-008 | DiaAppFlow | One-liner registration macro | Compliant — `DIA_STREAM_TYPE` follows the same pattern |
| SD-009 | DiaAppFlow | Module failure: assert debug, rollback release, shutdown on boot | Compliant — connect failures route through existing module-failure paths |
| SD-010 | DiaAppFlow | PU startup order = array order in config | N/A |
| SD-011 | DiaAppFlow | Shutdown is framework-level | N/A |
| SD-012 | DiaAppFlow | Hot reload = re-enter current stage | N/A |
| SD-013 | DiaAppFlow | DoStart returns StartResult | N/A |
| SD-014 | DiaAppFlow | Full manifest validation at load, fail-fast | **Compliant and reinforced** — adds new validator error paths (AC7), all hard-fail at `Start()` (AC1–AC4) |
| SD-015 | DiaAppFlow | No shared modules across PUs | Compliant |
| SD-016 | DiaAppFlow | IApplicationInspectable for runtime state | Compliant — `GetStreamInfo` continues to surface stream metadata |
| SD-017 | DiaAppFlow | Clean break, no v1 backward compat | **Compliant** — schema v2.0 deprecation window is *internal* to this feature's migration; ends when CluicheTest is migrated within the same plan. No long-lived shim. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Schema | The legacy `type` field is accepted with a deprecation warning during the transition. How is "transition window" defined? | The window opens when this feature's plan starts and closes within the same plan once the CluicheTest manifest migration task is complete. After that, the legacy parser path is deleted. There is no inter-PR shim period — it's a sequence within one feature's implementation. |
| 2 | Schema | Why split `type` into `kind` + `payload_type` rather than embed (`EventStream<InputEvent>`)? | Splitting keeps each field a clean `StringCRC`, which (a) lets the validator look up the payload type in the registry without parsing, (b) makes the editor's stream view trivially renderable as two columns, (c) matches PD-001's "StringCRC for IDs" intent. Parsing `EventStream<InputEvent>` is a minor convenience for hand-edits at the cost of every consumer. |
| 3 | Type registry | Where do `DIA_STREAM_TYPE(T)` registrations live for engine-only types vs game-defined types? | Engine types in their owning module's header (e.g. DiaInput's `Event.h` registers `Event`). Game types in the game's `Types/` headers (e.g. `Cluiche::UICommand`). The macro is included via `RegistrationMacrosV2.h`. |
| 4 | Type registry | What happens if two `DIA_STREAM_TYPE` calls register the same `StringCRC` to different C++ types (collision or duplicate)? | Static-init asserts on duplicate registration. In release the second registration is dropped and a `DIA_LOG_WARNING` is emitted. CI builds run with asserts enabled so collisions are caught. |
| 5 | Caps | If `max_readers` is omitted from a stream entry, the count is "derived from manifest reader count". What if `module.reads` is wrong (extra/missing entry)? | The validator also checks reads/writes for orphans and unknowns (AC7). If the manifest is internally consistent, derived count is right. If inconsistent, the validator fires the appropriate orphan/unknown error before reader count is computed. |
| 6 | Capacity | Is `capacity` (event-stream ring depth) tunable per-stream in production builds, or only via manifest? | Manifest only. There is no runtime tuning API. This matches SD-001 (config is sole source of truth). |
| 7 | Frame streams | Frame streams have one writer and zero-cost lock-free reads — does `max_readers` apply to them? | No. `max_readers` is event-stream only (frame streams expose latest-only with no per-reader buffer). The manifest field is ignored for `kind: "FrameStream"` and the validator warns if specified. |
| 8 | Multi-writer | The current `multi_writer` flag is preserved. Does manifest-authoritative creation also enforce that the *only* writers are those declared in `module.writes`? | Yes. Even if `multi_writer: true`, only modules whose `writes` array lists the stream may obtain a writer handle. The flag relaxes the "one writer" constraint, not the "declared writers only" constraint. |
| 9 | Type registry storage | Should `StreamTypeRegistry` be a singleton, a static within DiaApplicationFlow, or per-Application? | Process-static. Type identity is a build-time fact, not a runtime one. Avoids passing a registry through `Connect`, and there is no plausible use case for two Applications with different type taxonomies in one process. |
| 10 | Migration | The CluicheTest migration touches `cluiche_main.diaapp`. Are the bin-deployed copies under `Cluiche/bin/.../Data/Manifests/` regenerated by the asset pipeline? | Yes. The asset pipeline (`dia pipeline --target cluichetest`) copies the source manifest to the bin tree; the source is the only edit target. Deployed copies regenerate next build. |
| 11 | Dead v1 fragments | Why delete `cluiche_sim.diaapp` and `cluiche_render.diaapp` in this feature? | They are unreachable code: (a) no `.diagame` imports them, (b) their v1 phase schema is not compatible with the v2 loader, (c) their PUs (Sim, Render) are already declared in `cluiche_main.diaapp`. Removing them prevents future confusion when the validator starts rejecting v1. The work is small enough to bundle here rather than carry as a backlog item that could rot. |
| 12 | Backwards-compat policy | SD-017 says "clean break, no v1 backward compat." This feature has a deprecation window for the schema. Is that consistent? | Yes. SD-017 forbids long-lived shims for v1 *systems*. The schema deprecation window is a transient sequencing detail inside one plan: write parser → migrate manifest → delete legacy parser. The "clean break" property holds at PR boundaries and at all merge points. |
| 13 | Asset catalogue | Removing `manifest.cluiche_sim` / `manifest.cluiche_render` from `assets.catalogue.json` may be referenced by the asset pipeline's validator. Is this safe? | The catalogue's `stage.global.contains` references will be edited in the same change. The pipeline validator checks that every `contains` target exists; consistent removal keeps it green. The plan includes a `dia pipeline --target cluichetest` verification task. |
| 14 | Test coverage | AC11 requires "every validator error path" to have a test. Does this include the deprecation warning path? | Yes — the warning path is tested via a v2.0 fixture that asserts the warning is logged and the manifest still loads. The deletion-of-deprecation-path itself is covered by removing that fixture in the migration step. |
| 15 | Type tag failure mode | What happens if `Connect()` finds a type-tag mismatch? Does it return without setting the handle, or assert? | Both: `DIA_ASSERT` in debug; in release the handle is left disconnected (`IsConnected()` returns false) and a connect-failure flag is recorded so `Application::Start()` returns false. The application never proceeds in a half-wired state. |

## Open Questions

None — all interview answers locked, all AI review questions answered.

## Status

`Done` — 2026-05-18. All F1 tasks complete; manifest-gating, `kind`/`payloadType` fields, legacy `type` field removed, reserved `$`-prefix validation, kMaxStreams raised to 64. Build and ApplicationFlow* tests passing.
