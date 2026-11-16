# Feature Spec: Save and Load

**Parent:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.md (Feature 3)
**Status:** Approved (2026-11-13)
**Plan:** @docs/specs/applications/dia/systems/diadeferredstep/save-load.plan.md
**Depends on:** Feature 1 (scheduler, `IDeferredStep::SerializePending`). Independent of Feature 2 —
tests use the manual launcher.

## Summary

DFS-005 made concrete. `DeferredStepSaveParticipant` observes `SaveManager::OnSaveStarted`, drains
every in-flight step before anything is serialized, and writes each completed-but-unapplied result
with its apply key. On load it abandons whatever the old world had in flight, restores the pending
results, and puts their channels back in `kComplete` so the very next `Tick` at or past each apply
key applies them — at the same key the uninterrupted run would have. Save-then-load is therefore
invisible to the simulation: the step is not re-run, and it does not land early.

The consumer writes no save code. It implements `SerializePending` / `DeserializePending` on its own
working copy and nothing else.

## Goals

- A save taken mid-step produces a load whose outcome is byte-identical to never having saved.
- No save can ever capture a half-finished step, with no discipline required from the game code.
- A load never applies a result belonging to the world that was just discarded.
- A save referencing a channel the game no longer registers fails the load cleanly, before any
  participant has mutated live state.

## Design

### The drain hook already exists

`SaveManager::Save` notifies `OnSaveStarted` (SaveManager.cpp:54) synchronously, on the calling
thread, before it walks the participant list and calls `Serialize` (SaveManager.cpp:70). That is the
pre-save hook the system spec's Open Design Question 1 worried was missing, so no DiaSaveGame API
change is needed:

```
SaveManager::Save(slot)
  OnSaveStarted ──► DeferredStepSaveParticipant::OnObservableUpdate
                      scheduler.DrainAll()          // waits; does NOT apply
  for each participant: Serialize(ctx)
      "DeferredStep" ──► per channel: channelId, kickKey, applyKey, version
                                      step->SerializePending(ctx)

SaveManager::Load(slot)
  OnLoadStarted ──► scheduler.DrainAndDiscardAll()  // the old world's steps are abandoned
  for each participant: Validate(ctx)               // all-or-nothing
  for each participant: Deserialize(ctx)
      "DeferredStep" ──► step->DeserializePending(ctx)
                         scheduler.RestorePending(channelId, kickKey, applyKey)
```

`DrainAll` deliberately does not apply (Feature 1 AC 26) — applying at drain time would change the
result versus the uninterrupted run, which is the whole point of storing the pending result instead.

### Record shape

`SaveContext` has no array-element writer (`BeginArray` exists but elements are only reachable through
`CurrentNode()`), and no `uint64_t` overload. So channels are written as numbered objects and keys as
`int64_t`:

```json
"DeferredStep": {
  "version": 1,
  "pending_count": 1,
  "channel_0": {
    "channel_id": 2743819233,
    "kick_key":   412,
    "apply_key":  419,
    "step_version": 3,
    "...": "whatever the step's SerializePending wrote"
  }
}
```

`channel_id` is the `StringCRC` value as an `int64_t`. `step_version` is the step's own
`GetPendingVersion()`, so a consumer can migrate its working copy independently of this participant's
format version.

### Scheduler additions

This feature extends `DeferredStepScheduler` (same module) with the state a save needs:

```cpp
    struct PendingStepInfo
    {
        Dia::Core::StringCRC channelId;
        StepKey              kickKey  = 0;
        StepKey              applyKey = 0;
        IDeferredStep*       step     = nullptr;
    };

    // Completed-but-unapplied steps, in kick order. Valid after DrainAll.
    uint32_t GetPendingCount() const;
    bool     GetPendingAt(uint32_t index, PendingStepInfo& out) const;

    // Waits for every in-flight step and discards it without applying. Load, and shutdown.
    void DrainAndDiscardAll();

    // Puts a registered idle channel into kComplete with no launch token, so the next
    // Tick at or past applyKey applies it. The channel's monotonic key floor becomes kickKey.
    bool RestorePending(Dia::Core::StringCRC channelId, StepKey kickKey, StepKey applyKey);
```

A restored channel has no `LaunchToken`, so `Tick` must treat "complete, no token" as complete
without consulting the launcher — tracked by a `hasToken` flag on the channel entry.

## Public API

### `Dia/DiaDeferredStep/DeferredStepSaveParticipant.h`

```cpp
namespace Dia::DeferredStep {

    class DeferredStepSaveParticipant final : public Dia::SaveGame::ISaveable,
                                              public Dia::Core::Observer
    {
    public:
        static const uint32_t kFormatVersion = 1;
        static const Dia::Core::StringCRC kParticipantId;    // "DeferredStep"

        explicit DeferredStepSaveParticipant(DeferredStepScheduler& scheduler);
        ~DeferredStepSaveParticipant() override;

        // Registers with the SaveRegistry and attaches to OnSaveStarted / OnLoadStarted.
        void Attach(Dia::SaveGame::SaveRegistry& registry, Dia::SaveGame::SaveManager& manager);
        void Detach();

        // ISaveable
        void     Serialize  (Dia::SaveGame::SaveContext& ctx) const override;
        void     Deserialize(Dia::SaveGame::LoadContext& ctx) override;
        uint32_t GetVersion () const override;
        bool     Validate   (Dia::SaveGame::LoadContext& ctx) const override;

        // Observer — SaveEvent::SaveStarted drains, SaveEvent::LoadStarted abandons.
        void OnObservableUpdate(int message) override;
    };
}
```

The exact `Observer` base and callback signature follow `DiaCore/Architecture/Observer.h`; the
`SaveEvent` int codes are already defined in `SaveManager.h`.

## Acceptance Criteria

### Drain on save

1. `SaveManager::Save` with one step in flight calls `DrainAll` before any participant's `Serialize`.
   A step whose `Run` is still executing when `Save` is called has returned before the first
   `Serialize` call.
2. The drain does **not** call `Apply` on any step. A save taken at key 12 for a step applying at 17
   leaves the simulation state unchanged by the save.
3. `Save` with nothing in flight performs no drain and writes `pending_count: 0`.
4. The participant only reacts to `SaveEvent::SaveStarted` and `SaveEvent::LoadStarted`; the other
   three `SaveEvent` codes are ignored.

### Writing

5. `Serialize` writes `version`, `pending_count`, and one `channel_<i>` object per pending step in
   **kick order**, each with `channel_id`, `kick_key`, `apply_key` and `step_version`.
6. For each pending channel, `IDeferredStep::SerializePending` is called exactly once, inside that
   channel's object, so the step's keys do not collide across channels.
7. A step that is registered but idle writes nothing — only completed-but-unapplied results are
   stored.
8. `GetVersion()` returns `kFormatVersion` (1).
9. `Serialize` is `const` and mutates no scheduler state (the drain already happened in the
   notification, not here).

### Validate

10. `Validate` returns false, aborting the load with `LoadResultCode::DeserializeError`, when a saved
    `channel_id` is not currently registered. It logs an Error naming the channel.
11. `Validate` returns false when the stored `version` is greater than `kFormatVersion`.
12. `Validate` returns true for a save with `pending_count: 0`, and for a save whose channels are all
    registered.
13. `Validate` mutates nothing — no `RestorePending`, no `DeserializePending`. A failing load leaves
    the live scheduler exactly as it was.

### Load

14. `SaveManager::Load` calls `DrainAndDiscardAll` on `OnLoadStarted`, before any `Validate` or
    `Deserialize`: an in-flight step from the pre-load world is waited for and discarded without
    `Apply`.
15. `Deserialize` calls `DeserializePending` on the matching registered step, then `RestorePending`,
    for each stored channel in the order written.
16. A restored channel reports `kComplete` with the stored `apply_key`. `Tick` at a key below that
    does not apply it; the first `Tick` at or past it applies it exactly once, with **no stall
    recorded** — the result was already complete.
17. A restored channel's key floor is the stored `kick_key`, so a `Kick` with a lower key is rejected
    as non-monotonic after a load.
18. `Deserialize` on a save with `pending_count: 0` leaves every channel idle.
19. Loading twice in a row is clean: the second load's `DrainAndDiscardAll` clears the first load's
    restored-but-unapplied channels, and nothing from the first load applies afterwards.

### Round trip equivalence — the headline test

20. A scripted 20-key run that kicks at key 10 with k = 7 and applies at 17 produces the same applied
    result whether or not a save/load cycle happens at key 12. The comparison is byte-wise over the
    consumer's committed state, and over the recorded `Apply` call order and keys.
21. `Run` is called exactly once across the whole save/load cycle. The step is never re-run on load
    (DFS-005).
22. A save/load at a key *after* the apply key behaves identically to no save at all — nothing is
    pending, so nothing is stored or restored.
23. A save taken while a step is in flight but *not yet complete* still round-trips: the drain
    completes it, so it is stored as pending and applied at its original key on load.

### Registration and lifetime

24. `Attach` registers the participant under `kParticipantId` ("DeferredStep") — one `SaveRegistry`
    slot of the 32 available — and attaches to both `OnSaveStarted` and `OnLoadStarted`.
25. `Detach` unregisters and detaches; a `Save` afterwards writes no `DeferredStep` section and does
    not drain. The destructor calls `Detach` if it was not called.
26. `Attach` called twice, or `Detach` without `Attach`, asserts in Debug and is a no-op in Release.

### Observation

27. Logs, channel `DeferredStep`: Info on a save drain with the number of steps drained and the
    pending count; Info on a load restore with the pending count; Warning when a load discards an
    in-flight step (AC 14); Error on either `Validate` failure (AC 10, 11). `DIA_TRACE_ZONE` around
    the save drain and around restore.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaDeferredStep/DeferredStepSaveParticipant.h/.cpp` | New |
| `Dia/DiaDeferredStep/DeferredStepScheduler.h/.cpp` | Add `PendingStepInfo`, `GetPendingCount`, `GetPendingAt`, `DrainAndDiscardAll`, `RestorePending`; `hasToken` on the channel entry so a restored channel never calls into the launcher |
| `Dia/DiaDeferredStep/DiaDeferredStep.vcxproj(.filters)` | New files (`dia docs vcxproj-add`); add the DiaSaveGame reference |
| `Dia/DiaDeferredStep/dia.diadeferredstep.architecture.module.md` | Add `DiaSaveGame` to `dependent_modules` (`dia check deps`) |
| `Dia/DiaDeferredStep/Testing/PendingStateStep.h` | New — an `IDeferredStep` whose working copy and committed state are both inspectable and serializable, for AC 20 |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestDeferredStepSave.cpp` | New |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestDeferredStepRoundTrip.cpp` | New — AC 20–23 |

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| DFS-005 | Saves drain, then store pending results with their apply keys; steps are not re-run on load | The whole feature. AC 1–3 drain, AC 5–6 store, AC 16 applies at the stored key, AC 21 proves no re-run |
| DFS-003 | The apply key never slips | A restored step keeps its original `apply_key` (AC 16) and its key floor (AC 17) |
| DFS-002 | Monotonic keys | AC 17 |
| PD-001 | `StringCRC` for ids | Participant id and channel ids; `channel_id` is stored as the CRC value |
| PD-004 / AD-002 | No STL in public APIs | `PendingStepInfo` is a plain struct; the save format is jsoncpp behind `SaveContext`, as every other participant |
| AD-001 | Sign-off for engine API changes | No DiaSaveGame change is needed — `OnSaveStarted` / `OnLoadStarted` already exist. The scheduler additions are inside this system's own module |
| no-singletons | No statics | The participant is constructed with its scheduler and handed the registry and manager by the module (Feature 4) |

## Test Plan

TDD — new public API and new behaviour.

- **GoogleTests** (`--filter=DeferredStepSave*`, `--shards 2`), using the manual launcher plus a real
  `SaveManager` / `SaveRegistry` and `Dia/DiaSaveGame/Testing/SaveTestHelpers.h`:
  - drain ordering: a probe participant registered after `DeferredStep` asserts the step had finished
    before its own `Serialize` ran
  - no apply on drain; nothing pending; non-save events ignored
  - written shape: keys, kick order with 3 channels, step keys nested per channel, idle channel omitted
  - validate: unregistered channel, future version, clean cases, no mutation on failure
  - load: discard-on-load-start, restore state and key, no stall on the restoring `Tick`, key floor,
    empty save, load twice
  - round trip: AC 20 byte comparison; `Run` call count; save after apply; save while incomplete
  - lifetime: attach/detach, double attach, destructor
- **Sanitizer:** `dia check sanitizer --filter=DeferredStepSave*` — the observer attach/detach and
  the restored-channel-without-token path are the use-after-free risks.
- **Gap audit** as the final task: every `SaveEvent` code, both `Validate` failure branches, and
  every `RestorePending` rejection reason covered.

## Open Design Questions

1. **How large can a pending result get before JSON text hurts?** `SaveContext::kBufferSize` is
   100 MB and A4's weather grid is ~100 KB at Large, so there is plenty of headroom — but the result
   is written as JSON text, and A4 already rejected a binary blob (project memory, 2026-10-15). If a
   consumer's working copy turns out to be megabytes, the options are a binary side-file (as
   DiaCellHistory snapshots do) or applying at drain time and accepting the divergence. Revisit when
   A1 Ecology's working copy is sized.
2. **Should the participant own a migration?** Today a stored `step_version` the consumer does not
   recognise is the consumer's problem inside `DeserializePending`. Routing it through
   `SaveRegistry::RegisterMigration` instead would give consumers the standard migration path, but
   migrations are keyed by participant id, and the participant is shared by every channel. Left to
   the consumer for v1.
3. **`SaveContext` cannot write array elements and has no `uint64_t`** — hence numbered
   `channel_<i>` objects and `int64_t` keys. `LoadContext` *can* read arrays
   (`BeginArray` + `SetArrayIndex`), so the asymmetry is a small gap in DiaSaveGame rather than
   something this feature should work around twice. Worth a backlog row to add
   `SaveContext::BeginArrayElement()` and `uint64_t` overloads; until then the numbered-object form
   is deliberate, not accidental.

## Resolved Decisions

1. **One save participant, drain driven automatically** (user decision 2026-11-13). Dia observes
   `OnSaveStarted` itself, so consumers write no save code and a forgotten `DrainAll` cannot silently
   save a half-finished step. The system spec's CoW follow-up "amend E1's save to call `DrainAll`"
   is therefore dropped.
2. **One `SaveRegistry` slot for all channels.** Channel results nest inside the `DeferredStep`
   section rather than each consumer registering its own `ISaveable`, keeping the 32-slot registry
   free and keeping kick order (which matters for apply order) in one place.
3. **`DrainAll` stores; `DrainAndDiscardAll` abandons.** Save needs the results kept; load and
   shutdown need them thrown away. Two explicit methods rather than one with a flag.
