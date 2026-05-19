# Audit — DiaApplicationFlow Event Stream

**Date:** 2026-05-17
**Subject:** `Dia::ApplicationFlow::EventStreamStore<T>` and the `EventStreamWriter<T>` / `EventStreamReader<T>` handles in `Dia/DiaApplicationFlow/Streams/`.
**Scope:** Audit only — no design, no spec. Each section answers one of the user's questions, gives the verdict (`OK / Watch / Fix`), and points to evidence.

## At a glance

| # | Question | Verdict |
|---|----------|---------|
| 1 | Is it good enough? | **OK** for current uses; **Watch** for the safety holes |
| 2 | Is it configurable? | **Watch** — capacity is the only knob |
| 3 | Should I have debug streams? | **Fix** — debug today bypasses the stream layer |
| 4 | Is it a memory hog? | **OK** at current scale; per-reader fan-out doesn't scale linearly with reader count |
| 5 | Can we do rewind? | **Fix** — not possible by design today |
| 6 | Should we save to disk to save memory? | **OK to skip** — wrong tool for memory; relevant only for replay |
| 7 | Should it be its own thing? | **Watch** — currently coupled to `Application`; extraction is feasible |

## 1. Is it good enough? — Verdict: OK for current usage, Watch for safety holes

**Today it carries:**
- `InputToSim` — `Dia::Input::Event` (KernelModule → InputStreamModule) — `Cluiche/CluicheGameBaseline/Modules/KernelModule.h:44`, `InputStreamModule.h:30`.
- `SimToUI` — `Cluiche::UICommand` (DummyLevelModule → UIModule) — `Cluiche/Stages/DummyStage/DummyLevelModule.h:29`, `UIModule.h:47`.

Two streams, four handles, all single-writer, mostly single-reader. Round-trip integration is exercised by `TestStreams.cpp:358` (`EventStreamWriterReaderRoundTrip`). The mechanism does what the current callers need.

**What's good:**
- Typed end-to-end — no `void*` payload, no `std::function`. Compares favourably to the superseded v1 `MessageBus` (`Dia/DiaApplicationFlow/MessageBus.h:36`) which used `void*` + `std::deque`/`std::vector`/`std::unordered_map` — non-compliant with PD-004.
- Per-reader fan-out isolates a slow reader from blocking another.
- Connect-at-startup contract (`Module::OnConnectStreams`) is enforced by `Application::FindOrRegisterStreamStoreAtStartup` via `mConnectingStreams` guard (`Application.h:177`, `Application.cpp:806–822`).
- Drop-oldest with a `DIA_LOG_WARNING` makes overflow visible (`EventStreamStore.h:141`).

**Holes worth flagging:**
- **Type aliasing is unchecked.** `Connect()` does `static_cast<EventStreamStore<T>*>(istore)` (`EventStreamWriter.h:70`, `EventStreamReader.h:85`) without validating that the existing store's `T` matches. If two modules declare the same StringCRC stream ID with different `T`, second-comer silently writes/reads through a mistyped store — UB. `IStreamStore::GetKind()` distinguishes `kFrame` vs `kEvent` but not the underlying type.
- **StringCRC collisions are silent.** Two distinct stream names with hash-equal IDs alias the same store.
- **`kMaxReaders = 8` and `kMaxStreams = 16` fail soft.** Exceeding `kMaxReaders` returns `-1` and `IsConnected()` quietly returns false (`EventStreamStore.h:122`, `EventStreamReader.h:77`). `kMaxStreams` asserts in debug but returns null in release (`Application.cpp:820`).
- **Single-writer convention is not enforced.** The `Send` mutex makes multi-writer safe in practice, but there's no contract preventing it; the architecture doc says "framework-owned" stores but doesn't pin writer count.
- **Lock held during full fan-out.** `Send` takes `mMutex` and iterates every reader (`EventStreamStore.h:127`); a high-frequency stream with many readers serializes producers behind reader-buffer churn.

## 2. Is it configurable? — Verdict: Watch

**What you can configure today:**
- Per-stream `capacity` (constructor arg, default `256`) — `EventStreamStore.h:23,27`.

**What you can't configure:**
- **Overflow policy** — drop-oldest is hard-coded (`EventStreamStore.h:136–142`). No drop-newest, no block-writer, no fail-loud option.
- **Reader buffering** — per-reader fixed-capacity ring is hard-coded. No "latest-N only" or "single shared cursor" mode.
- **Envelope** — events are bare `T`. No timestamp, sender ID, or sequence number embedded by the framework. Compare to `FrameStreamStore<T>::Write` which carries `TimeAbsolute` (`FrameStreamStore.h:27`).
- **Topology declaration** — streams are declared in C++ (`EventStreamWriter<T> mX{this, "Id"}`) and connected in `OnConnectStreams`. The `.diaapp` manifest does not list streams, so debug tools, validation, and dataflow visualisation must reconstruct the graph from runtime introspection (`Application::GetStreamInfo` exists for this).
- **Per-stream tuning** — capacity is set at the writer/reader handle's `Connect`, not in data. No way for a deployment to override capacity for a noisy stream without a code change.

The capacity-only configurability is acceptable for the two streams Cluiche has today, but every additional stream is C++ work, not data work.

## 3. Should I have debug streams? — Verdict: Fix

**Where debug data flows today (not via streams):**
- `DiaDebugServer/SubscriptionManager` (`Dia/DiaDebugServer/SubscriptionManager.h:20`) — completely separate pub/sub keyed by `dataType` StringCRC, owned by `DebugServer`, indexed by WebSocket `connectionId`. Used for stage transitions (`DebugServer.cpp:476`) and other debug topics (`:501`).
- `DebugServer.cpp` polls/snapshot-pulls Application state through `IApplicationInspectable` — see all the introspection methods on `Application.h:69–77`.
- Module lifecycle (start/active/stop/failed transitions, stage transitions, rollback retries) is **not emitted** as events — it's only readable via `GetActiveModules`, `IsTransitioning`, `GetTransitionInfo`. That means anyone wanting to react to "module X failed" must poll.

**What's missing for debug:**
- A well-known event stream that emits lifecycle events (`ModuleStateChanged`, `StageTransitionStarted/Committed`, `RollbackAttempted`, `ShutdownRequested`).
- A way for `DiaDebugServer` to **tap** an existing typed stream (e.g. mirror `InputToSim` to a WebSocket subscriber) without the writer module knowing.
- A type-erased view of any `EventStreamStore<T>` for the debug server — today the debug server cannot iterate stores generically because reading requires the typed `Consume<N>` API.

This is the biggest gap of the seven questions: debug-time observability is a parallel mechanism, not a tap on the production stream layer.

## 4. Is it a memory hog? — Verdict: OK at current scale

**Formula:** per stream, the heap cost is `sizeof(T) × capacity × N_readers` (only registered readers allocate; `mReaders[i].buffer = new T[mCapacity]` runs on `RegisterReader`, `EventStreamStore.h:110`).

**Concrete numbers for Cluiche today** (default `capacity = 256`):

| Stream | `T` | `sizeof(T)` (approx) | Readers | Bytes |
|--------|-----|----------------------|---------|-------|
| InputToSim | `Dia::Input::Event` (tagged union, ~16 B) | ~16 B | 1 | ~4 KB |
| SimToUI | `Cluiche::UICommand` (enum + StringCRC[68 B] + float) | ~76 B | 1 | ~19 KB |

**Total today: ~23 KB across the event-stream subsystem.** Not a hog.

**Watch items:**
- `UICommand` includes a full `StringCRC` (4 B value + 64 B `mString[]` = 68 B, `StringCRC.h:35`). The `mString` is paid in every event of every stream that carries a `StringCRC` field, even though only the 4-byte CRC is used at runtime. A 256-deep ring of a struct with two `StringCRC` fields = 256 × ~140 B ≈ 35 KB per reader.
- Per-reader fan-out scales with `N_readers`. If a stream sprouts 4 readers, memory is 4× (per-reader rings, no shared storage).
- `kMaxReaders = 8` puts a hard ceiling on fan-out before silent failure.
- Capacity is per-stream — there's no global bound, so adding many streams sums freely.

The architecture is fine for the current population. It would become noticeable if you (a) carry large value-types per event, (b) fan out to many readers, or (c) declare many streams.

## 5. Can we do rewind? — Verdict: Fix (not possible today, by design)

**Today's behaviour:** `Consume` advances `tail` and decrements `count` (`EventStreamStore.h:163–166`). Once an event is consumed, it is gone from that reader's buffer. Other readers' copies are independent but follow the same drain pattern. There is **no journal, no checkpoint, no replay path.**

**What rewind would require:**
- An append-only log in addition to (or replacing) per-reader rings, with reader cursors instead of mutating the underlying buffer.
- A way to rebuild reader state (game state) deterministically from the log — out of scope of the bus itself; this is a recording/replay subsystem.
- Either a fixed-size circular journal (last-N events recoverable) or persistence (see Q6).

The current design is consume-and-forget by intent, optimized for live latency over historical inspection. Adding rewind is not a tweak — it's a different storage model.

## 6. Should we save to disk to save memory? — Verdict: OK to skip

**Disk does not solve a memory problem here.** The live ring needs to be in memory to serve reads at ProcessingUnit tick rate. Spilling to disk would either:
- Add a write-back path that costs more than the memory it saves (current footprint is ~23 KB), or
- Move *cold* events to disk to bound retention — but today there is no retention beyond consumption, so there's nothing cold to spill.

**Disk only becomes interesting if rewind/replay is in scope.** In that case the disk journal is for *replay capability*, not memory savings. Same conclusion as Q5: this is a recording subsystem feature, not a tweak to the bus.

If recording is wanted, the natural design is a tap module that subscribes to all streams via a type-erased iteration API (Q3) and serializes via DiaSerializer (already a DiaApplicationFlow dependency, see module doc `dia.application.architecture.module.md` line 104).

## 7. Should it be its own thing? — Verdict: Watch

**Coupling today:**
- `EventStreamStore<T>` itself depends only on DiaCore and DiaLogger — clean.
- `EventStreamWriter<T>` / `EventStreamReader<T>` depend on `Application::FindOrRegisterStreamStoreAtStartup` (`EventStreamWriter.h:67`, `EventStreamReader.h:82`). The Application is the registry.
- The handles take `Module*` for ownership tracking (currently unused — `mOwner` is stored but not dereferenced).
- The architecture doc lists streams under DiaApplicationFlow's responsibilities: "FrameStreamStore / EventStreamStore — framework-owned inter-PU data channels" (`dia.application.architecture.module.md:35`).

**Why it lives in DiaApplicationFlow:**
- Connect-at-startup contract is tied to the `Module` lifecycle and `OnConnectStreams` callback.
- The store registry is a fixed-capacity array on `Application` (`Application.h:157`).
- Stream introspection is part of `IApplicationInspectable::GetStreamInfo`.

**What extraction would look like (if pursued):**
- A new `DiaStreams` module owning `IStreamStore`, `EventStreamStore<T>`, `FrameStreamStore<T>`, the typed handles, and an abstract registry interface (`IStreamRegistry`).
- DiaApplicationFlow keeps the `Module::OnConnectStreams` lifecycle hook and provides an `IStreamRegistry` implementation backed by the existing fixed-array storage.
- Other consumers (DiaDebugServer, a hypothetical recorder) consume `IStreamRegistry` rather than `Application`.

**Why it might not be worth it:**
- Two streams in production. Two stores. One consumer (`Application`). Extraction has real cost (new vcxproj, dependency rewiring) for marginal current benefit.
- The startup-only registration window is enforced by Application's `mConnectingStreams` flag — that lifecycle coupling is load-bearing for thread safety, not incidental.

**Recommendation:** keep extraction on the table only if (a) Q3 lands and the debug server needs a registry it can iterate without an `Application&`, or (b) a non-DiaApplicationFlow consumer of streams shows up.

## What might be worth specifying

In rough priority order, only the items that aren't `OK`:

1. **Debug streams as a first-class concept (Q3, Q5)** — lifecycle event stream(s) (`ModuleStateChanged`, stage transitions) emitted by `Application` itself, plus a generic "stream tap" path for `DiaDebugServer`. Largest current gap; unlocks observability and is a prerequisite for replay.
2. **Type-tag check on stream lookup (Q1)** — record `typeid(T)` or a registered type ID alongside each `IStreamStore` so the second-comer's `static_cast` is validated. Small fix, removes a UB risk.
3. **Per-stream overflow policy + envelope metadata (Q2)** — let a stream opt into block / drop-newest / fail-loud, and carry timestamp + sender + sequence in the framework rather than in every payload `T`. Modest scope; opens debug ergonomics.
4. **Replay/rewind subsystem (Q5, Q6)** — separate recorder module that taps streams and serializes via DiaSerializer; not a change to the bus itself. Only worth it if recording is a real feature requirement, not for memory.
5. **Extraction to DiaStreams (Q7)** — only if Q3 or a new non-AppFlow consumer materializes.

Q4 (memory) and Q6 (disk-for-memory) need no action on the current trajectory.
