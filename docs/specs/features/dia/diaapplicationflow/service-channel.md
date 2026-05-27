# Feature Spec: Service Channel

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Research
@docs/research/service_channel_topology/full_topology_mock.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Service Channel | (this file) |

## Problem Statement

Modules in different Processing Units share stable lifecycle handles (canvas, texture handler, render context fence) via raw static pointers and `GetStatic*()` accessors, making cross-PU dependencies invisible in config and unverifiable by the framework. Additionally, the existing stream model (per-concern FrameStream/EventStream) has no growth ceiling — it needs consolidation to composite-per-PU-pair to stay manageable at scale.

## Design Decisions (from research)

1. **ServiceStream timing: collected-then-committed.** All providers register handles during DoStart. Framework validates all declared ServiceStreams have handles, then commits. Consumers only go live after commit. No ordering reasoning needed.
2. **EventStream timing: frame-batched.** Events collected during producer tick, flushed at frame boundary. Consumer gets exactly "last frame's batch." Enables deterministic replay and frame coherence with FrameStream data.
3. **FrameStream model: composite per PU-pair.** One struct carries all per-frame data for that direction. New features extend the composite, never add streams. Shape B (DebugDrawList) folds into existing SimToRenderFrame. Shape C per-frame data (StageHUD, AutomationStatus) becomes a new MainToRenderFrame composite.
4. **No debug/non-debug split on FrameStream.** Strip at the producer in Release, not the transport.
5. **Unified suffix: Stream.** ServiceStream, FrameStream, EventStream. Prefix tells frequency.
6. **Role names:** `provides`/`consumes` for ServiceStream; `reads`/`writes` for FrameStream/EventStream.
7. **Scope inferred from manifest location:** Global manifest = app-lifetime. Stage manifest = stage-lifetime, resets on stage unload.

## Stream Model — Frequency Spectrum

| Primitive | Frequency | Topology | Timing | Commit Model |
|-----------|-----------|----------|--------|-------------|
| **ServiceStream** | Once (scope lifetime) | 1 per handle | Collected-then-committed at scope start | All providers must register → framework commits → consumers go live |
| **FrameStream** | Every tick | 1 per PU-pair direction | Latest-wins per frame | Write at end of producer tick, read at start of consumer tick |
| **EventStream** | Batched discrete | 1 per PU-pair direction | Flushed at producer frame boundary | Consumer gets exactly previous frame's batch |

## Coupling Inventory

Full audit of static cross-PU coupling in the codebase as of 2026-05-25. Every coupling is assigned a shape and a resolution path.

### Shape Definitions

| Shape | Description | Resolution |
|-------|-------------|------------|
| **A — Stable handle** | Set once at DoStart, never changes, stable for module lifetime | `ServiceStream<T>` (this feature) |
| **B — Per-frame data** | Data flows between PUs every frame | Fold into composite FrameStream for that PU-pair (this feature) |
| **C — Session state / events** | Slow-changing flags or discrete lifecycle transitions | Fold into composite FrameStream/EventStream for that PU-pair (this feature) |

### Inventory

| Coupling | Owner Module | Owner PU | Reader PU(s) | Data | Shape | Resolution |
|----------|-------------|----------|--------------|------|-------|------------|
| `KernelModule::sCanvas` | KernelModule | MainPU | RenderPU | `ICanvas*` | A | `ServiceStream<ICanvas>` |
| `KernelModule::sTextureHandler` | KernelModule | MainPU | RenderPU, SimPU | `ITextureHandler*` | A | `ServiceStream<ITextureHandler>` |
| `KernelModule::sRenderContextReleased` | RenderPU writes / MainPU reads | RenderPU | MainPU | `atomic<bool>` | A (deferred) | Framework lifecycle hook (follow-on feature) |
| `VisualDebuggerModule::sLayerManager` | VisualDebuggerModule | SimPU | RenderPU | `DebugLayerManager*` | B | Fold into `SimToRenderFrame` composite (DebugFrameData already carries this) |
| `TestResultsRegistry` | Session singleton | MainPU writes | RenderPU reads | Stage lifecycle + frame count | C | `MainToRenderFrame` composite + `MainToRenderEvents` EventStream |
| `AutomationModule::sInstance` | AutomationModule | MainPU | RenderPU | Heartbeat flag | C | `MainToRenderFrame` composite |
| `AssetServiceModule::sInstance` | AssetServiceModule | MainPU | SimPU | Stage load state | C | `MainToSim` EventStream (StageLoadEvent kind) |

### Same-PU (not a cross-PU issue)

| Coupling | Module | PU | Notes |
|----------|--------|----|-------|
| `JobSystemModule::sInstance` | JobSystemModule | MainPU | MainPU-only; no action needed |

### Shape A note — RenderContextFence (deferred)

`sRenderContextReleased` is directionally reversed from the other Shape A cases (RenderPU signals MainPU during shutdown). It is a shutdown synchronization primitive, not a service handle. Deferred to a follow-on feature as a framework-level lifecycle hook.

## Consolidated Stream Topology (target state)

| Stream ID | Kind | Payload Type | From | To | Notes |
|-----------|------|-------------|------|-----|-------|
| MainToSim | EventStream | MainToSimEvent | MainPU | SimPU | Input events + stage load commands |
| SimToMain | EventStream | SimToMainEvent | SimPU | MainPU | UI commands |
| MainToRenderEvents | EventStream | MainToRenderEvent | MainPU | RenderPU | Stage lifecycle events |
| SimToRender | FrameStream | SimToRenderFrame | SimPU | RenderPU | FrameData + debug draw (multi_writer) |
| MainToSimFrame | FrameStream | MainToSimFrame | MainPU | SimPU | UIDataBuffer |
| MainToRender | FrameStream | MainToRenderFrame | MainPU | RenderPU | StageHUD + AutomationStatus |
| Canvas | ServiceStream | ICanvas | MainPU | (any) | Global stable handle |
| TextureHandler | ServiceStream | ITextureHandler | MainPU | (any) | Global stable handle |

**Total: 8 streams** (consolidates existing 4 fine-grained + resolves all Shape A/B/C couplings)

## Acceptance Criteria

1. **AC1 — ServiceStream primitive.** A `ServiceStream<T>` exists with `ServiceStreamWriter<T>` (provides) and `ServiceStreamReader<T>` (consumes). Uses collected-then-committed model.
2. **AC2 — Commit gate.** After all modules' DoStart completes within a scope (app start or stage load), the framework validates that every declared ServiceStream has a registered handle. If any missing → Start() fails. Consumers cannot call Get() before commit.
3. **AC3 — Manifest-declared.** `ServiceStream` channels are declared in `.diaapp` manifest with `kind: "ServiceStream"`. No cross-PU service handle is wired without a corresponding manifest declaration.
4. **AC4 — Unified channels array.** Module manifest entries use a unified `channels` array with a `role` field (`reads`, `writes`, `provides`, `consumes`). The legacy `reads`/`writes` arrays are replaced. All existing CluicheTest manifests are migrated.
5. **AC5 — Validator coverage.** The manifest validator fails loud at `Application::Start()` for: missing provider, orphan consumer, payload-type mismatch, provider PU starts after consumer PU, multiple providers for same ServiceStream, orphan provider (no consumers).
6. **AC6 — Shape A migration (Canvas + TextureHandler).** `KernelModule::sCanvas` and `KernelModule::sTextureHandler` are migrated off raw statics onto `ServiceStream`. All consumers use `ServiceStreamReader<T>`.
7. **AC7 — Statics deleted.** `KernelModule::sCanvas`, `KernelModule::sTextureHandler`, and their `GetStatic*()` public accessors are removed.
8. **AC8 — EventStream frame-batching.** EventStream gains a flush mechanism. Producers' events are batched per-tick. Consumers receive exactly "previous frame's batch" via Consume(). Existing EventStream behavior updated.
9. **AC9 — FrameStream consolidation.** Existing fine-grained streams consolidated into composite-per-PU-pair. Shape B (DebugDrawList) folded into SimToRenderFrame. Shape C frame data (StageHUD, AutomationStatus) into new MainToRenderFrame. Shape C events (StageLifecycle, StageLoad) into PU-pair EventStreams.
10. **AC10 — Manifest migration.** All `.diaapp` manifests updated to the consolidated 8-stream topology with unified `channels` arrays.
11. **AC11 — Config visible.** All streams and channel bindings visible in manifests.
12. **AC12 — Scope inference.** ServiceStream scope (global vs stage) inferred from manifest location. Stage-scoped services reset on stage unload.
13. **AC13 — Editor updated.** DiaApplicationFlowEditor renders the unified `channels` array with role-based display. ServiceStream kind visible. ModuleInspector, StreamsTab, GraphView, and ModuleCommands updated.

## Public API

### ServiceStream (new)

```cpp
namespace Dia::ApplicationFlow {

    // Writer — held by the providing module.
    // Call Register(handle) in DoStart.
    template<typename T>
    class ServiceStreamWriter {
    public:
        void Connect(Application& app);
        void Register(T& handle);       // called in DoStart; handle stored but not yet live
        bool IsRegistered() const;
    };

    // Reader — held by consuming modules.
    // Call Get() any time after framework commit.
    template<typename T>
    class ServiceStreamReader {
    public:
        void Connect(Application& app);
        T& Get() const;                 // assert if not yet committed
        bool IsAvailable() const;       // true after commit
    };

}
```

### EventStream (updated — frame-batched)

```cpp
namespace Dia::ApplicationFlow {

    // Existing EventStreamWriter gains flush semantics.
    // Framework calls Flush() at end of producer PU tick.
    // Send() accumulates events within the current tick.
    template<typename T>
    class EventStreamWriter {
    public:
        // ... existing interface unchanged from caller's perspective
        SendResult Send(const T& event);
        // Framework-internal: called at end of producer tick
        // void Flush();  // not public — framework manages this
    };

    // Consumer sees exactly "last frame's batch"
    template<typename T>
    class EventStreamReader {
        // ... existing Consume() now returns only events up to last flush
        void Consume(DynamicArrayC<Event<T>, N>& outEvents);
        bool HasPending() const;
    };

}
```

### Composite FrameStream Payloads (updated)

```cpp
// SimToRenderFrame — existing FrameData already is composite.
// Shape B (DebugDrawList) already carried via DebugFrameData inheritance.
// No structural change needed — just verify VisualDebuggerModule writes
// through SimToRender FrameStream instead of sLayerManager static.

// MainToRenderFrame — NEW composite for MainPU → RenderPU
struct MainToRenderFrame {
    StageHUDState       stageHUD;
    AutomationStatus    automationStatus;
    // Future: editor overlay state, profiler HUD, etc.
};

// MainToSimEvent — consolidated event envelope
struct MainToSimEvent {
    enum class Kind : uint8_t { kInput, kStageLoad };
    Kind kind;
    // Payload accessed via typed accessor methods
};

// SimToMainEvent — consolidated event envelope  
struct SimToMainEvent {
    enum class Kind : uint8_t { kUICommand };
    Kind kind;
};

// MainToRenderEvent — NEW
struct MainToRenderEvent {
    enum class Kind : uint8_t { kStageLifecycle };
    Kind kind;
};
```

## Manifest Schema

**Stream declaration:**
```json
{
  "id": "Canvas",
  "kind": "ServiceStream",
  "payload_type": "ICanvas",
  "from": "MainPU"
}
```

ServiceStream has no `to` field — subscriber PUs inferred from `consumes` entries. The `from` field identifies the providing PU for validator ordering checks.

**Module entries — unified `channels` array:**
```json
{
  "instance_id": "KernelModule",
  "channels": [
    { "id": "MainToSim",       "role": "writes" },
    { "id": "Canvas",          "role": "provides" },
    { "id": "TextureHandler",  "role": "provides" }
  ]
}

{
  "instance_id": "RenderModule",
  "channels": [
    { "id": "SimToRender",     "role": "reads" },
    { "id": "Canvas",          "role": "consumes" },
    { "id": "TextureHandler",  "role": "consumes" }
  ]
}
```

Valid roles: `reads`, `writes` (FrameStream/EventStream), `provides`, `consumes` (ServiceStream).

## Internal Architecture

### ServiceStream Commit Lifecycle

```
Scope start (App start or Stage load):
  1. All modules DoStart()
     - Providers call ServiceStreamWriter::Register(handle)
     - Handles stored but NOT yet accessible
  2. Framework COMMIT gate
     - Validates: every declared ServiceStream (for this scope) has a registered handle
     - If any missing → Start() fails (fail-fast)
     - If all present → flip committed flag atomically
  3. Consumers go live
     - ServiceStreamReader::Get() now returns the handle
     - Before commit: Get() asserts

Scope end (Stage unload):
  - Stage-scoped ServiceStreams reset (committed flag cleared)
  - Global ServiceStreams unaffected
```

### EventStream Frame-Batch Lifecycle

```
Producer tick N:
  1. Modules call Send() throughout the tick — events accumulate in buffer
  2. End of tick: framework calls Flush() — marks batch boundary (tick N sealed)

Consumer tick M:
  1. Consume() returns events up to last flush only
  2. = exactly "what happened during producer's last frame"
  3. Deterministic: replay flush boundaries → same behavior
```

### Manifest Validation (extended)

```
Application::Start()
  1. Parse manifest — unified channels array
  2. Validate streams (existing checks + new ServiceStream checks):
     — Every ServiceStream has exactly one provider (role=provides)
     — Every consumer's stream ID matches a declared ServiceStream
     — payload_type registered in StreamTypeRegistry
     — Provider PU position in config array ≤ all consumer PU positions (or same PU)
     — No orphan provider (ServiceStream with no consumers)
     — No orphan consumer (subscribes to non-existent ServiceStream)
  3. Create stream stores from manifest
  4. OnConnectStreams on each module — Writer/Reader/ServiceStreamWriter/Reader Connect()
  5. PUs start in config order; provider DoStart calls Register()
  6. After all DoStart completes in scope: COMMIT gate for ServiceStreams
  7. If any validation or commit fails → Start() returns false
```

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/Streams/ServiceStreamWriter.h` | New |
| `Dia/DiaApplicationFlow/Streams/ServiceStreamReader.h` | New |
| `Dia/DiaApplicationFlow/Streams/ServiceStreamStore.h` | New |
| `Dia/DiaApplicationFlow/Streams/EventStreamStore.h` | Add flush/batch boundary support |
| `Dia/DiaApplicationFlow/Streams/EventStreamReader.h` | Consume() returns up to last flush |
| `Dia/DiaApplicationFlow/Manifest/ApplicationManifestV3.h` | Add `ServiceStream` to `StreamKind`; replace `reads`/`writes` with `channels` array |
| `Dia/DiaApplicationFlow/Manifest/JsonApplicationManifestSerializer.{h,cpp}` | Parse `channels` array; parse `kind: "ServiceStream"` |
| `Dia/DiaApplicationFlow/Manifest/ManifestValidatorV2.{h,cpp}` | New ServiceStream error codes; adapt existing checks to channels array |
| `Dia/DiaApplicationFlow/Application.{h,cpp}` | Create `ServiceStreamStore` instances; commit gate after DoStart; flush EventStreams at tick boundary |
| `Dia/DiaApplicationFlow/ProcessingUnit.{h,cpp}` | Call EventStream flush at end of PU tick |
| `Dia/DiaApplicationFlow/Streams/IStreamStore.h` | Add `kService` to StreamKind enum |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj{,.filters}` | Add new files |
| `Cluiche/CluicheGameBaseline/Modules/KernelModule.{h,cpp}` | Remove statics; add ServiceStreamWriter; call Register() |
| `Cluiche/CluicheGameBaseline/Modules/RenderModule.{h,cpp}` | Use ServiceStreamReader |
| `Cluiche/CluicheGameBaseline/Modules/AssetServiceModule.cpp` | Use ServiceStreamReader |
| `Cluiche/CluicheGameBaseline/Modules/DummyLevelModule.cpp` | Use ServiceStreamReader |
| `Cluiche/CluicheGameBaseline/Types/MainToRenderFrame.h` | New — composite payload |
| `Cluiche/CluicheGameBaseline/Types/MainToSimEvent.h` | New — consolidated event envelope |
| `Cluiche/CluicheGameBaseline/Types/SimToMainEvent.h` | New — consolidated event envelope |
| `Cluiche/CluicheGameBaseline/Types/MainToRenderEvent.h` | New — consolidated event envelope |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Rewrite to 8-stream consolidated topology + channels |
| `Cluiche/Assets/Stages/*/Misc/ApplicationFlow/*.diaapp` | Convert to channels array |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestServiceChannel.cpp` | New tests |
| `Dia/DiaApplicationEditor/UI/src/v2/types.ts` | channels + ServiceStream kind |
| `Dia/DiaApplicationEditor/UI/src/v2/ModuleInspector.tsx` | Render channels by role |
| `Dia/DiaApplicationEditor/UI/src/v2/StreamsTab.tsx` | ServiceStream display |
| `Dia/DiaApplicationEditor/UI/src/v2/GraphView.tsx` | ServiceStream edge style |
| `Dia/DiaApplicationEditor/V2/Commands/ModuleCommands.h/cpp` | AddModuleChannel/RemoveModuleChannel |
| `Dia/DiaApplicationFlow/dia.application.architecture.module.md` | Update stream docs |

## Dependencies

- **stream-topology-manifest** (Done) — `StreamTypeRegistry`, `DIA_STREAM_TYPE`, manifest-authoritative creation
- **stream-policy-envelope** (Done) — `IStreamStore` base, `StreamKind`, `Event<T>` envelope
- **DiaCore/CRC/StringCRC** — payload type IDs

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — stream IDs and payload types are `StringCRC`; `ServiceStreamStore` keyed by `StringCRC` |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — feature operates inside existing Module lifecycle hooks; no new lifecycle introduced |
| PD-003 | Platform | Component-based entities | N/A — framework-internal |
| PD-004 | Platform | No STL containers in public APIs | Compliant — `ServiceStreamWriter/Reader` hold no STL members; store uses `DynamicArrayC` |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant — vcxproj manually maintained |
| PD-007 | Platform | C++20 required | Compliant |
| PD-008 | Platform | Directory.Build.props owns build settings | Compliant — no per-project overrides |
| PD-009 | Platform | Generated output in `Cluiche/out/` | N/A |
| PD-010 | Platform | `.diagame` is project root | Compliant |
| AD-001 | Dia App | Module YAML frontmatter | Compliant — module docs updated |
| AD-002 | Dia App | No STL in public APIs | Compliant (reinforces PD-004) |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — `Dia::ApplicationFlow::` |
| AD-004 | Dia App | PU/Phase/Module for app structure | Compliant |
| AD-005 | Dia App | Component-based entities | N/A |
| SD-001 | DiaAppFlow | Config is sole source of truth | **Strengthens** — all cross-PU handles must be manifest-declared |
| SD-002 | DiaAppFlow | Stages replace phases | Compliant |
| SD-003 | DiaAppFlow | ModuleRef<T> is sole module access pattern | Compliant — ServiceStreamReader is a handle to data, not a module reference |
| SD-004 | DiaAppFlow | TransitionTo is app-wide | N/A |
| SD-005 | DiaAppFlow | Transitions execute next frame | N/A |
| SD-006 | DiaAppFlow | Streams are framework-owned, declared in config | **Strengthens** — ServiceStream extends stream framework under same ownership model |
| SD-007 | DiaAppFlow | MessageBus removed, EventStream replaces | Compliant |
| SD-008 | DiaAppFlow | One-liner registration macro | Compliant — `DIA_STREAM_TYPE(T)` covers ServiceStream payload types |
| SD-009 | DiaAppFlow | Module failure: assert debug, rollback release, shutdown on boot | Compliant — commit failure routes through existing failure paths |
| SD-010 | DiaAppFlow | PU startup order = array order in config | **Strengthens** — validator enforces provider PU precedes consumer PUs |
| SD-011 | DiaAppFlow | Shutdown is framework-level | Compliant |
| SD-012 | DiaAppFlow | Hot reload = re-enter current stage | Compliant — store resets on stage unload; provider re-registers on next DoStart |
| SD-013 | DiaAppFlow | DoStart returns StartResult | Compliant — Register() called from DoStart |
| SD-014 | DiaAppFlow | Full manifest validation at load, fail-fast | **Reinforced** — adds ServiceStream validator checks + commit gate |
| SD-015 | DiaAppFlow | No shared modules across PUs | Compliant |
| SD-016 | DiaAppFlow | IApplicationInspectable exposes runtime state | Compliant — GetStreamInfo extended |
| SD-017 | DiaAppFlow | Clean break, no v1 backward compat | Compliant |
| SD-018 | DiaAppFlow | Reserved `$`-prefix for framework-owned streams | Compliant |
| SD-019 | DiaAppFlow | Manifest schema v3 | Compliant — `channels` replaces `reads`/`writes`; orthogonal to stage-object v3 changes |
| SD-020 | DiaAppFlow | Transition guards are module-registered veto hooks | N/A |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | ServiceStream | Is `sRenderContextReleased` a ServiceStream or a lifecycle hook? | Lifecycle hook — shutdown sync, not a service | **Deferred.** Shutdown primitive, not a handle. Follow-on feature. |
| 2 | Validator | Should orphan provider (no consumers) be an error? | Yes — fail-fast, mirrors orphan-writer | **Yes — error.** |
| 3 | Ordering | Can a subscriber's Get() fail if framework guarantees commit? | No — framework guarantees via commit gate | **Correct.** Get() is guaranteed after commit. IsAvailable() exists but defensive check unnecessary. |
| 4 | Lifetime | Does store reset on stage unload for stage-scoped services? | Yes — consistent with module lifecycle | **Yes.** Scope inferred from manifest location. |
| 5 | Multiple providers | Reject multiple providers for same ServiceStream? | Yes — one provider per ServiceStream | **Yes — validator error.** |
| 6 | Roles | `provides`/`consumes` vs `publishes`/`subscribes`? | `provides`/`consumes` — DI language, consistent with handle semantics | **`provides`/`consumes`** — reads clearer for "I give you a handle" / "I use a handle." |
| 7 | EventStream batching | Does frame-batching break existing EventStream consumers? | No — consumer already drains at tick boundaries; making it explicit just prevents mid-tick partial consumption | **No behavioral break.** Current consumers already drain per-tick. Batching makes the guarantee explicit and enables deterministic replay. |
| 8 | FrameStream consolidation | Does folding DebugDrawList into SimToRenderFrame increase its size when debug is off? | Minimal — empty arrays cost ~pointer+size (16 bytes). Strip at producer. | **Acceptable.** DebugFrameData already exists in the composite. No new allocation when empty. |
| 9 | Composite events | Tagged union vs type-erased envelope for consolidated EventStreams? | Tagged union with Kind enum — type-safe, no dynamic alloc, known variant set | **Tagged union.** Variant set is known at compile time. Accessor methods provide typed access. |
| 10 | Growth ceiling | At 5 PUs, what's the max stream count? | ~18 practical, ~30 hard ceiling | **Confirmed by topology mock.** Hybrid model caps at ~30 for 5 PUs. Fine-grained would be 60-80+. |

## Open Questions

None.

## Status

`Done`

**Plan:** [service-channel.plan.md](service-channel.plan.md)
