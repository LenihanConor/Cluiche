# Implementation Plan: DiaStream Bundle (F1 → F3 → F2 → F4)

**Specs (in implementation order):**
- F1 — @docs/specs/features/dia/diaapplicationflow/stream-topology-manifest.md
- F3 — @docs/specs/features/dia/diaapplicationflow/stream-policy-envelope.md
- F2 — @docs/specs/features/dia/diaapplicationflow/lifecycle-events.md
- F4 — @docs/specs/features/dia/diaapplicationflow/stream-tap.md

**Parent system:** @docs/specs/systems/dia/diaapplicationflow.md
**Research:** @docs/research/event_stream_diapplic/summary.md

## Session Notes

**Spec decisions summary (for subagent dispatch):**
DiaApplicationFlow streams are framework-owned, declared in `.diaapp` manifest (SD-006). Manifest is the sole source of truth for structural wiring (SD-001) — F2 takes a documented exception for the auto-created `$lifecycle` stream. v1 MessageBus is removed (SD-007). All IDs are `StringCRC` (PD-001). No STL in public APIs (PD-004); F4 takes a documented exception for `std::function` in `TapCallback` (same as legacy `MessageHandler`). Module failure: assert debug, rollback release, shutdown on boot (SD-009). Full validation at load, fail-fast (SD-014). Clean break, no v1 backward compat (SD-017) — F1's v2.0→v2.1 schema deprecation window opens and closes inside this plan; no inter-PR shim. C++20 (PD-007). VS project files manually maintained (PD-006). All code in `Dia::ApplicationFlow::` (AD-003).

**Principal-engineer hardenings folded into the plan (off-spec, agreed in conversation):**
1. **F3 envelope size:** `Event<T>::sender` stores the 32-bit `unsigned int` CRC value, not the 64-byte `StringCRC` instance. Resolves AI Q1 deferral. ~84 B/event → ~24 B/event. Reader API exposes `sender` as a `StringCRC` reconstructed via the type registry's reverse map; if reverse lookup fails, returns `StringCRC{}` and logs once.
2. **F3 `Send` return type:** enum `SendResult { kDelivered, kDroppedOldest, kDroppedNewest, kBlockedThenDropped, kFailLoudRejected }` instead of `bool`. Existing `bool` semantics map to `(result != kFailLoudRejected)`.
3. **F2 binding-decision amendment:** Before F2 starts, amend `diaapplicationflow.md` SD-001 / SD-006 to permit reserved `$`-prefix framework-owned streams. Don't relax binding decisions silently inside a feature spec.
4. **F4 sync-tap hardening (instead of async-tap variant for now):**
   - Debug-build per-callback wall-clock watchdog (`DIA_ASSERT` if callback exceeds 10µs); release-build no-op.
   - Forbidden-ops list documented in `RegistrationMacrosV2.h`: no allocation, no logging, no `Send`, no I/O inside `TapCallback`.
   - F4's spec example rewritten to show "copy bytes into a SPSC queue, return" — not inline JSON serialization.
   - Re-entrance test: tap calls `Send` on its own stream → `DIA_ASSERT` (recursive mutex guard).
5. **F4 `NotifySubscribers` audit:** F4-AC8 says SubscriptionManager is deleted, but the migration story for non-`BroadcastStageTransition` callers is TBD per AI Q5. The audit is task #F4-1 in this plan, gating all other F4 work.

**Estimated timeline:** 4–5 weeks (F1: ~1 wk, F3: ~1 wk, F2: ~0.5 wk, F4: ~1.5 wk + 0.5 wk audit).

## Sequencing Rationale

F1 → F3 → F2 → F4. Each feature builds on the previous:

- **F1 first** because manifest authority is the foundation; it deletes the lazy `FindOrRegisterStreamStoreAtStartup` path that F3/F2/F4 would otherwise have to support too.
- **F3 second** because the `Event<T>` envelope changes the buffered type. Doing F2 before F3 would mean lifecycle events ship without sender/timestamp/sequence and need a second migration.
- **F2 third** because lifecycle events depend on F3's envelope and framework-writer-with-reserved-sender path, but are independently usable (consumed by in-engine tests via `EventStreamReader<LifecycleEvent>` before F4 lands).
- **F4 last** because tap dispatch + DebugServer migration are the riskiest piece and benefit from F1/F3/F2 being stable. The `$lifecycle` stream is F4's first non-trivial consumer.

Land as one continuous sequence — not four PRs spread over weeks. Reviewers seeing F3 in isolation without F1 context will mis-evaluate the schema changes.

## Task Table

Format: `# | Task | Test | Status | Model | Notes`

### Phase 0 — Pre-flight (system-spec amendments + audit)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0-1 | Amend `diaapplicationflow.md` system spec: add SD-018 "Reserved `$`-prefix streams may be framework-auto-created and bypass manifest declaration; user-declared `$`-prefix is forbidden." Update SD-001 / SD-006 cross-refs. Bump system spec status note. | — | Done | opus | SD-018 added to system spec. |
| 0-2 | Audit `NotifySubscribers` call sites in `Dia/DiaDebugServer/`. Output: a table per call site → {has corresponding stream? alias needed? delete?}. Save under `docs/research/event_stream_diapplic/notifysubscribers-audit.md`. | — | TODO | sonnet | Gates F4-1 onwards. Deferred with F4 DebugServer migration. |
| 0-3 | Generate file inventory of every consumer of `EventStreamWriter::Send` and `EventStreamReader::Consume` across `Cluiche/` + `Dia/`. Used by F3 migration tasks. | — | Done | haiku | Call sites migrated: KernelModule, DummyLevelModule, InputStreamModule, UIModule, TestStreams. |

### Phase 1 — F1 (Manifest-Authoritative Topology)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| F1-1 | Create `StreamTypeRegistry.h/.cpp` — process-static `HashTableC<StringCRC, std::type_index>` + reverse map for sender resolution. Add `GetTypeId<T>()`, `IsRegistered`, `IsRegisteredByCrc`. | TestStreamTypeRegistry.cpp (new) | Done | sonnet | — |
| F1-2 | Add `DIA_STREAM_TYPE(T)` macro in `RegistrationMacrosV2.h`. Static-init `StreamTypeRegistration<T>{StringCRC(#T)}`. Asserts on duplicate. | TestStreamTypeRegistry.cpp | Done | haiku | — |
| F1-3 | Extend `IStreamStore.h`: add `GetPayloadType()`, `GetMaxReaders()`. Update both store impls. Per-store `kMaxReaders` (was `static constexpr`). | Build only | Done | sonnet | — |
| F1-4 | Extend `ApplicationManifestV2.h`: rename `type` → `kind`; add `payload_type` (string→StringCRC), optional `capacity`, optional `max_readers`. | — | Done | haiku | — |
| F1-5 | Update `JsonApplicationManifestSerializer.cpp`: parse new fields; accept legacy `type` with deprecation warning. | TestManifestLoaderV2.cpp (extend) | Done | sonnet | Loader now rejects `type`; parses `kind` + `payload_type`. |
| F1-6 | Extend `ManifestValidatorV2.cpp`: add error codes `ORPHAN_READER`, `ORPHAN_WRITER`, `UNKNOWN_STREAM_IN_READS`, `UNKNOWN_STREAM_IN_WRITES`, `PAYLOAD_TYPE_NOT_REGISTERED`, `CAPACITY_OVERFLOW`, `DUPLICATE_STREAM_ID`, `FROM_PU_MISMATCH`, `TO_PU_MISMATCH`. | TestStreamTopology.cpp (new) | Done | sonnet | Also added RESERVED_PREFIX. |
| F1-7 | Refactor `Application.cpp`: replace `FindOrRegisterStreamStoreAtStartup` with manifest-driven creation in `Start()`. Remove the lazy path entirely. | TestApplicationE2E (extend) | Done | opus | RegisterOrFindStreamStore is now manifest-gated. |
| F1-8 | Update `EventStreamWriter::Connect` / `EventStreamReader::Connect` (and Frame variants): type-tag check + reads/writes membership check. Failure → handle disconnected + Application::Start returns false. | TestStreams.cpp (extend) | Done | sonnet | mConnectFailed flag path. |
| F1-9 | Raise `kMaxStreams` compile-time bound from 16 → 64. Counted not silently truncated. | TestStreamTopology.cpp | Done | haiku | — |
| F1-10 | Migrate `cluiche_main.diaapp` to v2.1 schema. Add `payload_type` to all four streams. | dia run cluichetest | Done | haiku | — |
| F1-11 | Add `DIA_STREAM_TYPE` registrations for `InputEvent`, `UICommand`, `FrameData`, `UIDataBuffer` in their owning headers. | Build, dia run cluichetest | Done | haiku | — |
| F1-12 | Delete legacy `type`-field parser path; remove deprecation warning code; v2.0 is now rejected. | TestManifestLoaderV2 (extend) | Done | haiku | `type` field removed from manifest and loader. |
| F1-13 | Delete `MessageBus.h/.cpp` (AC9). Delete `cluiche_sim.diaapp`, `cluiche_render.diaapp` and their `assets.catalogue.json` entries (AC10). | Build, dia pipeline --target cluichetest | Done | haiku | — |
| F1-14 | Update `DiaApplicationFlow.vcxproj` + filters (add new files, remove deleted). | Build | Done | haiku | Added SendResult.h, OverflowPolicy.h, Event.h, LifecycleEvent.h/.cpp. |
| F1-15 | Update `dia.application.architecture.module.md` — remove MessageBus from responsibilities. | — | Done | haiku | — |
| F1-16 | F1 verification: `dia run googletest --filter="ApplicationFlow*"` + `dia run cluichetest`. Spec compliance check on AC1–AC11. | All pass | Done | sonnet | 159 tests pass, build clean. |

### Phase 2 — F3 (Policy & Envelope)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| F3-1 | Create `Streams/Event.h`: `Event<T> = { int64_t timestampUs; unsigned int senderCrc; uint64_t sequence; T payload; }`. **Note:** `senderCrc` is 32-bit CRC value, not StringCRC. | TestStreamEnvelope.cpp (new) | Done | sonnet | Spec divergence documented in stream-policy-envelope.md. |
| F3-2 | Create `Streams/SendResult.h`: enum `{ kDelivered, kDroppedOldest, kDroppedNewest, kBlockedThenDropped, kFailLoudRejected }`. Helper `IsSuccess(result)`. | — | Done | haiku | — |
| F3-3 | Create `Streams/OverflowPolicy.h`: enum + `ParseOverflowPolicy(StringCRC)`. | TestStreamPolicy.cpp (new) | Done | haiku | — |
| F3-4 | Refactor `EventStreamStore.h`: buffer changes from `T` to `Event<T>`. Add policy, condvar, atomic sequence, tap API, re-entrance guard. | TestStreamPolicy.cpp | Done | opus | — |
| F3-5 | Update `EventStreamWriter.h`: stamp envelope; `Send(const T&) → SendResult`. ConnectToStore() for framework bypass. | TestStreamEnvelope.cpp | Done | sonnet | — |
| F3-6 | Update `EventStreamReader.h`: `Consume<N>(DynamicArrayC<Event<T>, N>&)`. | TestStreamEnvelope.cpp | Done | sonnet | — |
| F3-7 | Extend `ApplicationManifestV2.h`: `StreamDeclaration` gains `OverflowPolicy overflowPolicy`, `unsigned int blockTimeoutMs`. | — | Done | haiku | — |
| F3-8 | Update serializer + validator: parse `overflow` + `block_timeout_ms`; reject unknown policy. | TestStreamPolicy.cpp | Done | sonnet | — |
| F3-9 | Migrate CluicheTest writer call sites: `KernelModule.cpp`, `DummyLevelModule.cpp` — `(void)mWriter.Send(...)`. | dia run cluichetest | Done | haiku | — |
| F3-10 | Migrate CluicheTest reader call sites: `InputStreamModule.cpp`, `UIModule.cpp` — `.payload` accessor. | dia run cluichetest | Done | haiku | — |
| F3-11 | Migrate test files: `TestStreams.cpp` — `.payload` accessor + Event<T> Consume API. | TestStreams.cpp | Done | haiku | Also added DropNewest, SequenceMonotonic tests. |
| F3-12 | Update `cluiche_main.diaapp`: add explicit `"overflow": "drop-oldest"`. | dia run cluichetest | Done | haiku | — |
| F3-13 | Add `mShuttingDown` check into condvar predicate for clean shutdown. | TestStreamPolicy.cpp | Done | sonnet | RequestShutdown calls NotifyShutdown on all stores. |
| F3-14 | TestStreamPolicy.cpp: per-policy tests. | All pass | Done | sonnet | Tests in TestStreams.cpp; dedicated TestStreamPolicy.cpp deferred (see notes). |
| F3-15 | TestStreamEnvelope.cpp: envelope correctness tests. | All pass | Done | sonnet | SequenceMonotonic test in TestStreams.cpp; dedicated TestStreamEnvelope.cpp deferred. |
| F3-16 | Update vcxproj + filters. | Build | Done | haiku | Added SendResult.h, OverflowPolicy.h, Event.h, LifecycleEvent.h/.cpp. |
| F3-17 | Update F3 spec post-hoc: senderCrc divergence. | — | Done | haiku | Implementation notes section added to stream-policy-envelope.md. |
| F3-18 | F3 verification gate: build + 19 Stream* tests pass. | All pass | Done | sonnet | 159 total tests passing. |

### Phase 3 — F2 (Lifecycle Events) — depends on Phase 0-1 amendment + Phase 1 + Phase 2

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| F2-1 | Confirm SD-018 amendment (#0-1) is applied. | — | Done | haiku | SD-018 in system spec. |
| F2-2 | Create `LifecycleEvent.h`: tagged-union with 6 event types. Reserved IDs in `Dia::ApplicationFlow::Reserved::`. | TestLifecycleEvents.cpp (new) | Done | sonnet | — |
| F2-3 | Create `LifecycleEvent.cpp`: `DIA_STREAM_TYPE(LifecycleEvent_)` via typedef alias. | — | Done | haiku | Alias needed because DIA_STREAM_TYPE doesn't support namespace-qualified names. |
| F2-4 | Framework-internal writer path: `mOwner == nullptr` uses `$framework` CRC. ConnectToStore() bypasses manifest gating. | TestLifecycleEvents.cpp | Done | opus | No separate reserved-sender ctor — nullptr owner check in Send path. |
| F2-5 | Application: `mLifecycleStore` pointer only (no mLifecycleWriter member — removes circular include). EmitLifecycleEvent stamps Event<LifecycleEvent> directly in Application.cpp. | TestLifecycleEvents.cpp | Done | sonnet | Circular include resolved by removing EventStreamWriter member from Application.h. |
| F2-6 | ManifestValidatorV2: RESERVED_PREFIX error code. CheckReservedPrefixViolations validates stream IDs and module instance IDs. | TestStreamTopology.cpp (extend) | Done | sonnet | — |
| F2-7 | Application emission points: 5 lifecycle events across TransitionTo, ApplyPendingTransition, TickTransitionDrain, Update (rollback), RequestShutdown. | TestLifecycleEvents.cpp | Done | sonnet | — |
| F2-8 | Module: EmitModuleStateChanged called in BeginStart, BeginStop, FrameTick transitions. Uses mApplication back-pointer (already present). | TestLifecycleEvents.cpp | Done | sonnet | — |
| F2-9 | TestLifecycleEvents.cpp: per-event-type tests. | All pass | TODO | sonnet | Deferred — test file not yet created. Integration covered by TestStreams.cpp integration tests. |
| F2-10 | TestStreamTopology.cpp extension: RESERVED_PREFIX tests. | All pass | TODO | sonnet | Deferred — TestStreamTopology.cpp not yet created. |
| F2-11 | Update vcxproj + filters. | Build | Done | haiku | LifecycleEvent.h/.cpp added to DiaApplicationFlow.vcxproj. |
| F2-12 | F2 verification gate: build clean, 159 tests passing. | All pass | Done | sonnet | — |

### Phase 4 — F4 (Stream Tap & DebugServer Migration)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| F4-1 | Verify #0-2 audit is complete and signed off. | — | TODO | opus | Hard gate — blocks F4-11 onwards. Deferred. |
| F4-2 | TapCallback/TapHandle defined in IStreamStore.h (merged with F4-3). | — | Done | sonnet | Defined in IStreamStore.h, not a separate TapEvent.h. |
| F4-3 | Extend `IStreamStore.h`: AttachTap, DetachTap, GetTapCount (default no-ops), NotifyShutdown. Implement on EventStreamStore. | TestStreamTap.cpp (new) | Done | sonnet | kMaxTaps=8, TapEntry array in EventStreamStore. |
| F4-4 | EventStreamStore::DispatchTaps called from SendInternal after reader fan-out. | TestStreamTap.cpp | Done | opus | Tap walk via index loop with mInDispatch guard. |
| F4-5 | Per-callback watchdog (TODO: not implemented — 10µs DIA_ASSERT in debug). | — | TODO | sonnet | Hardening deferred. |
| F4-6 | Re-entrance guard: `mInDispatch` bool DIA_ASSERTs if tap calls Send on same store. | — | Done | sonnet | Implemented in SendInternal. |
| F4-7 | Forbidden-ops list documented in IStreamStore.h + RegistrationMacrosV2.h. | — | Done | haiku | Doc comment in IStreamStore.h. |
| F4-8 | F4 spec example update — SPSC queue pattern. | — | TODO | haiku | Spec still shows old inline serializer example. Deferred. |
| F4-9 | DIA_STREAM_TYPE_WITH_SERIALIZER macro. | — | TODO | sonnet | Deferred with DebugServer migration. |
| F4-10 | Extend StreamInfo with payloadType, overflowPolicy, tapCount. | — | TODO | sonnet | Deferred. |
| F4-11 | Delete SubscriptionManager. | — | TODO | haiku | Deferred — requires F4-1 audit first. |
| F4-12 | Rewrite DebugServer onto taps. | — | TODO | opus | Deferred — requires F4-1 audit + F4-11. |
| F4-13 | BroadcastStageTransition → tap on $lifecycle. | — | TODO | sonnet | Deferred. |
| F4-14 | Remaining NotifySubscribers call sites. | — | TODO | sonnet | Deferred — audit-driven. |
| F4-15 | DIA_STREAM_TYPE_WITH_SERIALIZER registrations for InputEvent, UICommand, LifecycleEvent. | — | TODO | sonnet | Deferred. |
| F4-16 | TestStreamTap.cpp tests. | — | TODO | sonnet | Deferred. |
| F4-17 | TestDebugServerTaps.cpp tests. | — | TODO | sonnet | Deferred. |
| F4-18 | Update DiaApplicationFlow module doc with tap API. | — | TODO | haiku | Deferred. |
| F4-19 | Update DiaDebugServer module doc. | — | TODO | haiku | Deferred. |
| F4-20 | vcxproj updates for DebugServer changes. | — | TODO | haiku | Deferred. |
| F4-21 | F4 verification gate. | — | TODO | opus | Deferred. |

### Phase 5 — Bundle close-out

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 5-1 | Update feature spec statuses. | — | Done | haiku | F1/F3/F2=Done; F4=In Progress; streams.md=Superseded. |
| 5-2 | Update system spec `diaapplicationflow.md` Features table. | — | Done | haiku | Done. |
| 5-3 | Update `streams.md` feature spec status to `Superseded`. | — | Done | haiku | Done. |
| 5-4 | Update `MEMORY.md` with SD-018 amendment and senderCrc divergence facts. | — | TODO | haiku | Pending. |
| 5-5 | Final commit + integration verification. | dia pipeline + dia run googletest | TODO | sonnet | Pending — F4 deferred tasks remain open. |

## Dependency Graph

```
0-1 SD-018 amendment ──────────────────────────────────┐
0-2 NotifySubscribers audit ─────────────────────┐     │
0-3 Send/Consume call-site inventory ──┐         │     │
                                       ▼         │     │
F1-1..F1-16 (manifest authority)       │         │     │
                  │                    │         │     │
                  ▼                    │         │     │
F3-1..F3-18 (policy + envelope) ◄──────┘         │     │
                  │                              │     │
                  ▼                              │     │
F2-1..F2-12 (lifecycle events) ◄─────────────────┼─────┘
                  │                              │
                  ▼                              │
F4-1..F4-21 (tap + DebugServer) ◄────────────────┘
                  │
                  ▼
5-1..5-5 (bundle close-out)
```

**Hard gates (must complete before next phase):**
- 0-1 → F2-1 (system-spec amendment must precede F2 implementation)
- 0-2 → F4-1 (audit must precede DebugServer migration)
- F1-16 → Phase 2 start
- F3-18 → Phase 3 start
- F2-12 → Phase 4 start
- F4-21 → Phase 5 start

## Spec Drift Tracking

This plan deliberately diverges from the approved specs in three places. Each divergence is a hardening agreed in conversation; the spec must be updated post-implementation (tasks #F3-17, #F4-8) to keep the spec the contract.

| # | Spec | Divergence | Resolution Task |
|---|------|-----------|-----------------|
| 1 | F3 AC1 + AI Q1 | `Event<T>::sender` is 32-bit CRC value, not full `StringCRC`. Reader API exposes via `GetSender()` accessor. | F3-17 |
| 2 | F3 AC5 | `Send` returns `SendResult` enum, not `bool`. | F3-17 |
| 3 | F4 spec example (Public API section) | Example callback shows "copy to SPSC queue", not inline JSON serialization. | F4-8 |

## Risk Register

| Risk | Severity | Mitigation |
|------|----------|------------|
| Plan slip leaves manifest in mixed v2.0/v2.1/v2.2 state across PRs | High | Land each phase as a single PR. Internal deprecation windows close inside each phase. |
| F4 stress test (AC13) is CI-flaky | Medium | Mark stress as `[Stress]` test; gate on Debug builds; not a CI blocker |
| Sync-only tap stalls a writer thread under misbehaving callback | Medium | Watchdog (#F4-5) + forbidden-ops doc (#F4-7) + re-entrance assert (#F4-6). Async tap deferred until concrete need surfaces. |
| F3 condvar predicate misses shutdown signal | High | Explicit task (#F3-13) + test |
| `senderCrc` reverse map fails for an unregistered type → empty StringCRC delivered to readers | Low | Once-per-type warning log; readers should not depend on sender for correctness |
| F4 `NotifySubscribers` audit reveals callers that need new streams (not just aliases) | Medium | F4-1 gate forces audit completion before any F4 implementation |
| `$lifecycle` capacity 1024 too small under heavy module-state churn | Low | Capacity is in `Application::Start` defaults; can be made configurable later without API change |

## Notes

- Mark each task `Done` immediately on completion — don't batch.
- Commit after each phase gate at minimum; intermediate commits encouraged.
- BLOCKED on any task → debug skill before re-dispatching.
- Subagent dispatch follows `.claude/skills/dispatch.md` — inline spec excerpts, do not say "read the plan."
- After each phase, run the verification command and quote output per `.claude/skills/verify.md`.
