# Plan: Save and Load (DiaDeferredStep Feature 3)

**Spec:** @docs/specs/applications/dia/systems/diadeferredstep/save-load.md
**Status:** Not Started
**System plan:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.plan.md (row 3)

Starts once Feature 1 **Task 8** is Done (it needs `DrainAll` and its "does not apply" guarantee).
Runs against the manual launcher, so it does **not** need Feature 2 and can run in parallel with it.

Task 1 edits `DeferredStepScheduler.h/.cpp`, which Feature 1's later tasks also touch — if Feature 1
is still in flight, serialise against it rather than dispatching both at once.

## Implementation Patterns

### Scheduler additions (Task 1)
- `PendingStepInfo`, `GetPendingCount`, `GetPendingAt`, `DrainAndDiscardAll`, `RestorePending` are
  added to the **existing** `DeferredStepScheduler` — a second class would split the channel table.
- `GetPendingAt` enumerates in **kick order** (the `kickSequence` field Feature 1 already stamps), so
  apply order survives a round trip. It returns only channels in `kComplete`.
- `RestorePending` requires the channel registered and `kIdle`; it sets `state = kComplete`,
  `hasToken = false`, `applyKey`, and `keyFloor = kickKey`. `hasToken` already exists from Feature 1
  Task 3 — this is why it was added early: `Tick` must never call `IsComplete`/`Wait`/`Release` on a
  restored channel.
- `DrainAndDiscardAll` is `DrainAll` followed by discard-and-reset per channel; share the wait loop
  rather than duplicating it. Both must leave the table in a consistent state if a `Wait` throws.

### Participant (Tasks 2–5)
- `DeferredStepSaveParticipant` inherits `Dia::SaveGame::ISaveable` **and** `Dia::Core::Observer`
  (`DiaCore/Architecture/Observer.h`). It attaches to two subjects — `OnSaveStarted` and
  `OnLoadStarted` — and dispatches on the `int` message cast back to `SaveGame::SaveEvent`. The other
  three `SaveEvent` codes fall through silently (AC 4).
- Registered under `kParticipantId = StringCRC("DeferredStep")`, one of `SaveRegistry::kMaxParticipants`
  (32). Declared `static const` in the header, defined in the `.cpp` (runtime CRC, same rule as
  Feature 1).
- `Attach(registry, manager)` / `Detach()` with a `bool mAttached` latch; the destructor calls
  `Detach` if still attached (AC 25). Double attach / detach-without-attach assert in Debug, no-op in
  Release (AC 26).
- **The drain happens in the notification, not in `Serialize`.** `Serialize` is `const` and must stay
  side-effect-free (AC 9) — that is the whole reason `OnSaveStarted` is used.

### Record shape (Task 3)
- `SaveContext` has **no array-element writer** and **no `uint64_t` overload** (checked 2026-11-13).
  So: `pending_count` as `int32_t`, then `BeginObject("channel_0") … EndObject()` per pending step in
  kick order, and every key written as `int64_t`.
- `channel_id` is `static_cast<int64_t>(channelId.GetValue())`.
- `IDeferredStep::SerializePending` is called **inside** that channel's object, so two channels cannot
  collide on a key (AC 6).
- Reading back uses `LoadContext::BeginObject("channel_<i>")` for `i` in `[0, pending_count)`.
  `LoadContext` *can* read arrays, but the writer cannot write them — do not use the array path on
  one side only.
- Key names are `StringCRC`; build the `channel_<i>` names with a small fixed buffer helper, not
  `std::string`.

### Validate (Task 4)
- `Validate` is called on every participant **after** migration and **before** any `Deserialize`, so a
  rejected load leaves live state untouched (that is DiaSaveGame's all-or-nothing contract). It must
  therefore read only — no `RestorePending`, no `DeserializePending` (AC 13), and the test asserts
  the scheduler is byte-identical afterwards.
- Two failure cases: an unregistered `channel_id` (AC 10) and `version > kFormatVersion` (AC 11).
  Both log an Error naming the cause.

### Round-trip test (Task 6) — the headline
- `Testing/PendingStateStep.h`: an `IDeferredStep` with an inspectable working copy *and* committed
  state, both serializable, plus counters for `Run` and `Apply` and a record of the key each `Apply`
  landed on.
- The test runs `ScriptedKeyDriver` over keys 1…20, kicking at 10 with k = 7:
  1. **Reference run** — no save. Capture the committed state at key 20, the `Apply` order and the
     applied keys.
  2. **Round-trip run** — `SaveManager::Save` at key 12, build a *fresh* scheduler + step + launcher,
     `Load`, continue to key 20.
  3. Compare byte-wise, plus `Run` call count == 1 across the whole cycle (AC 21).
- Use `Dia/DiaSaveGame/Testing/SaveTestHelpers.h` for slot setup and temp directories.

### Verification cadence
- Per task: narrow `--filter` on the suite under work (e.g. `DeferredStepSave*`) with `--shards 2`; the feature-wide `DeferredStepSave*:DeferredStepRoundTrip*` runs in task 8 only. Sanitizer: on request only (user, 2026-11-17). The AC 26 Release no-op check moved to the system close-out, Task 6 (2026-11-17, speed-ups).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Add the DiaSaveGame reference to `DiaDeferredStep.vcxproj`, add `DiaSaveGame` to the module doc's `dependent_modules`, `dia check deps`. Confirm in code that `SaveManager::Save` notifies `OnSaveStarted` before the `Serialize` loop and that `Load` notifies `OnLoadStarted` before `Validate` — quote both line numbers in notes | Compiles with `<DiaSaveGame/SaveManager.h>` included | Done | haiku | The whole feature rests on that ordering; verify it rather than trusting the spec; SaveManager.cpp: OnSaveStarted L54 before Serialize loop L70; OnLoadStarted L138 before Validate L267 and Deserialize L279 (both in Load). dia check deps clean. |
| 1 | RED → GREEN: scheduler additions — `PendingStepInfo`, `GetPendingCount`, `GetPendingAt` (kick order, `kComplete` only), `DrainAndDiscardAll`, `RestorePending` (+ `hasToken` honoured in `Tick`) | Pending enumeration after a `DrainAll` with 3 channels in kick order; `RestorePending` rejects an unknown or non-idle channel; a restored channel never reaches the launcher (asserted from `ManualStepLauncher`'s call log) | Done | sonnet | Serialise against Feature 1 if it is still in flight — same file; 10 tests DeferredStepSavePending; RED=compile fail, GREEN pass; regression 309 pass 0 fail (DeferredStep/Cap/Serial/JobStepLauncher). Tick/GetApplyKey/DrainEntry handle token-less restored channels. |
| 2 | RED → GREEN: `DeferredStepSaveParticipant` skeleton — `Attach`/`Detach`, observer dispatch on both subjects, `GetVersion`, lifetime | AC 4, 8, 24–26: only the two events act; double attach; detach then save does nothing; destructor detaches | Done | sonnet | No `Serialize` body yet; Participant skeleton; 11 Lifetime tests GREEN (Debug). RED not captured (impl written before tests) - process deviation. Spec says OnObservableUpdate(int); real Observer API is ObserverNotification(subject,int), implemented that. Release no-op branch of AC26 not compiled (system close-out task 6). |
| 3 | RED → GREEN: drain-on-save + `Serialize` | AC 1–3, 5–7, 9: a probe participant registered after `DeferredStep` sees the step already finished; no `Apply` during the drain; `pending_count: 0` path; field names and kick order with 3 channels; step keys nested per channel; idle channel omitted; `Serialize` mutates nothing | Done | sonnet | Numbered `channel_<i>` objects, `int64_t` keys; Serialize + 11 DeferredStepSaveWrite tests; RED quoted (behavioural), GREEN 33/33 across shards. StringCRC accessor is Value(). |
| 4 | RED → GREEN: `Validate` | AC 10–13: unregistered channel → false + Error; future version → false + Error; clean cases → true; scheduler unchanged after a failure | Not Started | sonnet | Read-only. Assert no state change, not just the return value |
| 5 | RED → GREEN: load path — abandon on `OnLoadStarted`, `Deserialize` + `RestorePending`, apply at the stored key with no stall, key floor, load twice | AC 14–19 | Not Started | sonnet | A restored step must record **no** stall (AC 16) — easy to get wrong if `Tick` treats "no token" as incomplete |
| 6 | RED → GREEN: the round-trip equivalence test, with `Testing/PendingStateStep.h` | AC 20–23: byte-identical committed state, `Apply` order and keys; `Run` called once; save after apply == no save; save while incomplete still round-trips | Not Started | sonnet | The headline AC. If it fails, the bug is real — do not relax the comparison |
| 7 | Exhaustive test pass: gap audit of every AC, both `Validate` branches, every `RestorePending` rejection; edge cases — save with 8 pending channels, a channel registered in a different order on load, a step whose `SerializePending` writes nothing, `pending_count` disagreeing with the objects present, a save taken between `DrainAll` and `Tick`; size the worst realistic pending blob and record it against system ODQ 1 | Gap list + the size figure in notes | Not Started | sonnet | No mutation check. The size figure feeds system ODQ 1 |
| 8 | Feature close-out: `dia run googletest --filter="DeferredStepSave*:DeferredStepRoundTrip*" --shards 2`; (sanitizer removed 2026-11-17: on request only); `dia check deps`; observation scan in notes; update the system spec features table | All ACs, quoted | Not Started | sonnet | Note in the plan that the CoW "E1 calls `DrainAll`" follow-up is now unnecessary |
