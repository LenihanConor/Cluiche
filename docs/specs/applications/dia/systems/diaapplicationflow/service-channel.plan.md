# Implementation Plan: Service Channel

**Spec:** @docs/specs/applications/dia/systems/diaapplicationflow/service-channel.md

## Session Notes — Spec Decisions Summary

**Stream model — frequency spectrum:**
- ServiceStream: once (scope lifetime), collected-then-committed at scope start. `provides`/`consumes` roles.
- FrameStream: every tick, composite per PU-pair direction. `reads`/`writes` roles.
- EventStream: batched discrete per PU-pair direction, flushed at producer frame boundary. `reads`/`writes` roles.

**Topology (8 consolidated streams):** MainToSim (Event), SimToMain (Event), MainToRenderEvents (Event), SimToRender (Frame, multi_writer), MainToSimFrame (Frame), MainToRender (Frame), Canvas (Service), TextureHandler (Service).

**Key constraints:**
- Unified `channels` array with `role` field replaces legacy `reads`/`writes`.
- ServiceStream commit gate: all providers register in DoStart → framework validates → commit → consumers live.
- EventStream flush: framework calls flush at end of producer PU tick; consumer Consume() returns up to last flush only.
- Composite FrameStreams: new features extend the struct, never add streams.
- Scope inference: global manifest = app-lifetime; stage manifest = stage-lifetime (resets on unload).
- Shape B (DebugDrawList) folds into existing FrameData/DebugFrameData. Shape C becomes MainToRenderFrame + MainToRenderEvents.
- `sRenderContextReleased` is OUT OF SCOPE — deferred to lifecycle hook feature.

## Implementation Patterns

### ServiceStreamStore<T>

Holds a single `T*` (non-owning). Published flag as `std::atomic<bool>` — NOT set by Register(); set by framework Commit(). Commit is called after all DoStart in the scope completes.

```cpp
template<typename T>
class ServiceStreamStore : public IStreamStore {
    Dia::Core::StringCRC mId;
    Dia::Core::StringCRC mPayloadType;
    std::atomic<bool>    mCommitted{false};
    T*                   mHandle = nullptr;
public:
    void Register(T& handle);  // stores pointer, does NOT set committed
    void Commit();             // framework-only: sets committed flag
    T& Get() const;            // asserts committed
    bool IsCommitted() const;
    void Reset();              // clears handle + committed flag (stage unload)
};
```

### EventStream Frame-Batching

Add a batch boundary marker to EventStreamStore:
- `std::atomic<uint64_t> mFlushSequence{0}` — incremented by Flush()
- Each event stamped with the flush sequence it belongs to
- Consume() only returns events with sequence ≤ last observed flush
- Framework calls Flush() on all EventStreamStores owned by a PU at end of its tick

### Composite Payloads

```cpp
// NEW: MainToRenderFrame
struct MainToRenderFrame {
    StageHUDState    stageHUD;
    AutomationStatus automationStatus;
};

// NEW: Consolidated event envelopes
struct MainToSimEvent {
    enum class Kind : uint8_t { kInput, kStageLoad };
    Kind kind;
    union { InputEvent input; StageLoadEvent stageLoad; };
    // Typed accessors for safety
};
```

### Channels Array Migration

`ModuleDeclaration` replaces:
```cpp
DynamicArrayC<StringCRC, 4> reads;
DynamicArrayC<StringCRC, 4> writes;
```
with:
```cpp
struct ChannelBinding {
    StringCRC id;
    StringCRC role;  // "reads", "writes", "provides", "consumes"
};
DynamicArrayC<ChannelBinding, 8> channels;
```

### Validator Error Codes (new)

- `kServiceStreamMissingProvider` — ServiceStream declared but no module has role=provides
- `kServiceStreamOrphanConsumer` — module consumes a stream that is not kind=ServiceStream
- `kServiceStreamTypeMismatch` — provider and consumer payload types disagree
- `kServiceStreamProviderAfterConsumer` — provider PU appears after consumer PU in config order
- `kServiceStreamMultipleProviders` — more than one module declares role=provides for same ServiceStream
- `kServiceStreamOrphanProvider` — ServiceStream has provider but no consumers

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `StreamKind::kService` to enum in `IStreamStore.h` | Compile | Done | haiku | One-line addition |
| 2 | Implement `ServiceStreamStore.h` — collected-then-committed model (Register, Commit, Get, IsCommitted, Reset) | Unit test: register/commit/get round-trip, assert on get-before-commit, assert on double-register, reset clears state | Done | sonnet | New file in Streams/ |
| 3 | Implement `ServiceStreamWriter.h` — Connect(), Register(handle) | Tested via store tests | Done | sonnet | Follows EventStreamWriter pattern |
| 4 | Implement `ServiceStreamReader.h` — Connect(), Get(), IsAvailable() | Tested via store tests | Done | sonnet | |
| 5 | EventStream frame-batching — add Flush()/ConsumeUpToFlush()/GetFlushSequence() to `EventStreamStore`; parallel batchStamp array per reader | 7/7 tests GREEN `dia run googletest --filter="EventStreamBatch*"` | Done | sonnet | `mCurrentBatchId`+`mFlushSequence` atomics; batchStamps[] parallel array |
| 6 | Framework flush integration — PU tick end calls Flush() on all EventStreamStores it owns | Integration: mock PU tick produces events, consumer sees batch only after next tick | Done | sonnet | SetPostTickFn() wired in Application::Start() |
| 7 | Add `ChannelBinding` struct to `ApplicationManifestV3.h`; replace `reads`/`writes` on `ModuleDeclaration` with `channels` array | Compile | Done | sonnet | Breaking change — all downstream must update |
| 8 | Update `ApplicationManifestLoaderV2.cpp` — parse `"channels"` array with roles; legacy `reads`/`writes` fallback with warning | Unit test: parse channels JSON; parse legacy with warning | Done | sonnet | |
| 9 | Update `ManifestValidatorV2` — adapt existing checks to `channels`; add 6 new ServiceStream error codes; fix provider-after-consumer two-pass ordering | 7/7 validator tests GREEN; full suite 5302/5304 (2 pre-existing failures) | Done | opus | Most complex task |
| 10 | Update `Application` — add `CommitReadyServiceStreams()` commit gate after main PU tick; add `IsCommitted`/`IsRegistered`/`Commit`/`Reset` virtual override on `IStreamStore`; ServiceStreamStore overrides them | 2/2 commit-gate tests GREEN; full suite clean | Done | opus | Core orchestration change |
| 11 | Add ServiceStream files to `DiaApplicationFlow.vcxproj` and `.vcxproj.filters` | Compile full solution | Done | haiku | |
| 12 | Create composite payload types — `MainToRenderFrame.h`, `MainToRenderEvent.h` + DIA_STREAM_TYPE registrations + vcxproj entries | Compile | Done | sonnet | New files in CluicheGameBaseline/Types/ |
| 13 | Write `TestServiceChannel.cpp` — all validator error paths + commit lifecycle + reset on unload | 14/14 tests GREEN `dia run googletest --filter="ServiceChannel*:ServiceStreamStore*:ServiceStreamValidator*"` | Done | sonnet | TDD: 21/21 total across tasks 13+14 |
| 14 | Write `TestEventStreamBatching.cpp` — flush semantics, cross-tick isolation, deterministic replay | 7/7 tests GREEN `dia run googletest --filter="EventStreamBatch*"` | Done | sonnet | TDD: RED first |
| 15 | Migrate `KernelModule` — remove sCanvas, sTextureHandler, GetStatic*(); add ServiceStreamWriter<ICanvas>, ServiceStreamWriter<ITextureHandler>; call Register() in DoStart | `dia run cluichetest` boots | Done | sonnet | |
| 16 | Migrate `RenderModule` — ServiceStreamReader<ICanvas>, ServiceStreamReader<ITextureHandler> | `dia run cluichetest` | Done | sonnet | |
| 17 | Migrate `AssetServiceModule` — ServiceStreamReader<ITextureHandler> | `dia run cluichetest` | Done | haiku | Single call-site |
| 18 | Migrate `DummyLevelModule` — ServiceStreamReader<ITextureHandler> | `dia run cluichetest` | Done | haiku | Single call-site |
| 19 | Consolidate EventStreams — merge InputToSim+StageLoad→MainToSimEvent; SimToUI→SimToMainEvent; add MainToRenderEvent; update all producers/consumers | `dia run cluichetest` + `dia run googletest` | Done | sonnet | Multiple modules touched |
| 20 | Consolidate FrameStreams — verify DebugDrawList already in FrameData (Shape B); create MainToRender FrameStream with MainToRenderFrame; migrate Shape C producers/consumers | `dia run cluichetest` | Done | sonnet | MainStateProducerModule added; TestStageHUDModule reads via stream |
| 21 | Migrate all `.diaapp` manifests — 8-stream consolidated topology + channels arrays | `dia run cluichetest` + `dia run googletest` | Done | sonnet | cluiche_main, dummy_stage, rigidbody2d_stage, editor.diaapp all migrated |
| 22 | Delete legacy `reads`/`writes` fallback from loader | `dia run googletest` | Done | haiku | Removed else-branch from ApplicationManifestLoaderV2 |
| 23 | Update `types.ts` — channels + ServiceStream kind + role types | TypeScript compiles | Done | sonnet | StreamKind, ChannelRole, ChannelBinding; ModuleV2.channels replaces reads/writes |
| 24 | Update `ModuleInspector.tsx` — render channels grouped by role | Visual | Done | sonnet | Unified Channels section; add-channel form with role <select> |
| 25 | Update `StreamsTab.tsx` — ServiceStream kind display | Visual | Done | haiku | Kind badge colours; ServiceStream hides To PU / Capacity / MaxReaders |
| 26 | Update `GraphView.tsx` — ServiceStream edges gold/dashed | Visual | Done | sonnet | Purple dashed stub from provider with arrowhead-service marker |
| 27 | Update `ModuleCommands.h/cpp` — AddModuleChannel/RemoveModuleChannel with role parameter | Editor e2e | Done | sonnet | AddModuleChannelCommand / RemoveModuleChannelCommand replace 4 Read/Write cmds |
| 28 | Update `dia.application.architecture.module.md` — document 3-stream model, roles, commit lifecycle | N/A (docs) | Done | haiku | All 3 stream primitives + unified channels[] + ServiceStreamStore/Writer/Reader in public API |
| 29 | Final verification — `dia run googletest` + `dia run cluichetest` + `dia pipeline --target cluicheeditor` | Quoted output | Done | haiku | 5310/5311 (1 pre-existing VisualDebugger failure); vitest 154/154; pipeline ✓; CluicheTest boots |

## Dependency Graph

```
Phase 1 — Framework primitives
[1] ──→ [2,3,4] (ServiceStream store + writer + reader)
[5,6] (EventStream batching — independent of ServiceStream)

Phase 2 — Tests RED
[13] (ServiceChannel tests)
[14] (EventStreamBatch tests)

Phase 3 — Manifest schema
[7] ──→ [8] ──→ [9] (channels + loader + validator)

Phase 4 — Application orchestration
[9,10] depend on [2,3,4,5,6,7,8]
Tests go GREEN after [9,10]

Phase 5 — Composite payloads
[12] (new types — independent)

Phase 6 — Migration
[15,16,17,18] (ServiceStream migration — after [10])
[19,20] (EventStream/FrameStream consolidation — after [5,6,12])
[21] (manifest migration — after [15-20])
[22] (delete legacy — after [21])

Phase 7 — Editor
[23] ──→ [24,25,26,27] (types first, then components)

Phase 8 — Docs + verification
[28,29]
```

## Build Order (serial phases)

**Phase 1 — Framework Primitives (tasks 1–6, 11)**
New ServiceStream files + EventStream batching. No existing code breakage.

**Phase 2 — Tests RED (tasks 13–14)**
Write all unit tests. They fail (RED).

**Phase 3 — Manifest Schema + Loader (tasks 7–8)**
Breaking change to ModuleDeclaration. All code touching reads/writes must compile after this.

**Phase 4 — Validator + Application Orchestration (tasks 9–10)**
ServiceStream commit gate + validator. Tests go GREEN.

**Phase 5 — Composite Payloads (task 12)**
New payload types. Can run in parallel with Phase 4.

**Phase 6 — Migration (tasks 15–22)**
Module-by-module migration: ServiceStream first (15-18), then EventStream/FrameStream consolidation (19-20), then manifest rewrite (21), then legacy cleanup (22).

**Phase 7 — Editor (tasks 23–27)**
TypeScript types first, then UI components, then backend commands.

**Phase 8 — Docs + Final Verification (tasks 28–29)**
Update module docs, run full verification.
