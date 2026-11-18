# Plan: DeferredStepModule and Observability (DiaDeferredStep Feature 4)

**Spec:** @docs/specs/applications/dia/systems/diadeferredstep/module-and-observability.md
**Status:** Not Started
**System plan:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.plan.md (row 4)

Starts once Features 1, 2 and 3 are Done — this feature owns and wires all three. Tasks 1–6 are
sequential (same module files); Tasks 7–8 (the stage) can start as soon as Task 4 is Done.

## Implementation Patterns

### Module (Tasks 1–2)
- Mirror `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.h` for the service-stream side and
  `Dia/DiaSimTime/DiaSimTimeModule.h` for the injection side — between them they show every pattern
  this module needs.
- `class DeferredStepModule : public Dia::ApplicationFlow::SimModule`, with
  `static const Dia::Core::StringCRC kTypeId;` (defined in the `.cpp`),
  `static constexpr PUAffinity kAllowedPUs = PUAffinity::kSim;` and `kDescription`. Registered with
  the v2 registration macro from `RegistrationMacrosV2.h`.
- `ServiceStreamReader<Dia::Threading::JobSystem> mJobSystemReader{this, "JobSystemService"};`
  connected in `OnConnectStreams`. The provider is `JobSystemModule` on MainPU — already in both
  `.diaapp` files, today consumed only by CoW's placeholder module.
- Owned members in **declaration order**, because members destruct in reverse and the launcher must
  outlive nothing that uses it: metrics/health first, then `CapGate`, `JobStepLauncher`,
  `DeferredStepScheduler`, `DeferredStepSaveParticipant`. They are constructed in `DoStart` (the
  job system is not available at construction), so hold them in `Dia::Core::UniquePtr` the way
  `DiaSimTimeModule` holds `mSaveState`.
- `SetSaveServices(SaveRegistry&, SaveManager&)` and `SetKeySource(IStepKeySource&)` follow
  `DiaSimTimeModule::SetSaveRegistry`'s precedent: injected by whoever assembles the Application,
  **before** `Application::Start()` drives `DoStart`. Both are optional; missing either is a logged
  Info/Warning, not a failure.
- `GetScheduler()` returns a reference for sibling modules reached via `ModuleRef<DeferredStepModule>`
  — the sole inter-module access pattern in v2 (`ModuleRefV2.h`). No stream, no singleton.
- `DoUpdate(const SimTimeContext&)`: if a key source is set, one `Tick(source->GetCurrentKey())`.
  The missing-source Warning is latched (`bool mWarnedNoKeySource`) so it fires once, not every frame
  (AC 13).
- `DoStop`: `DrainAndDiscardAll`, detach the participant, unregister metrics and health, assert no
  channel remains registered. Consumer modules unregister in their own `DoStop`, which manifest
  ordering makes run first (stop is reverse order).

### Config (Task 3)
- `OnConfigure(const char* configJson)` — the hook on `Module` (Module.h:82). Parse with the DiaCore
  Json wrapper, not raw jsoncpp.
- `worker_cap` accepts the string `"auto"` or an integer. Every rejected shape (`0`, negative,
  non-integer string, wrong JSON type) logs **one** Warning and falls back to `"auto"`; the actual
  resolution to `max(1, workers/4)` happens in `CapGate`'s constructor, so this layer only decides
  "auto or N".
- `ranges_per_worker` defaults to 4, clamped to ≥ 1 with one Warning.
- Absent `config` is valid and silent above Info (AC 11).

### Metrics (Task 4)
- `DeferredStepMetrics.h/.cpp` owns the nine registrations and one `Update(const SchedulerStats&, const CapStats&)`
  called from `DoUpdate` after `Tick`. Names exactly as the spec's table.
- `MetricRegistry::Instance()` and `HealthRegistry::Instance()` are pre-existing engine singletons,
  not new ones — this does not breach feedback_no_singletons, and the module adds no static of its
  own beyond `kTypeId`.
- Counters are **set from the stats**, not incremented locally, so nothing is counted twice (AC 20).
  `MetricRegistry` counters are monotonic, so store the last-published value and add the delta.
- Register with unit/description info strings that are string literals (the registry stores the
  pointer, it does not copy — see the note in `MetricRegistry.h`).
- Unregister on `DoStop`; a second start must not warn about duplicate names (AC 22).

### Health (Task 5)
- `DeferredStepHealth.h/.cpp`: subclass `Dia::Observation::Health::HealthReporterBase` so `Report()`
  is already atomic and allocation-free. `Report()` reads one `std::atomic<uint64_t> mLastStallNs`
  and compares against `steady_clock::now()`; Degraded inside the 60 s window, OK outside.
- **Never Failing** (user decision 2026-11-13) — so a stall cannot flip an E2E run's
  `overall_status`. Make that an explicit comment next to the status mapping, not just a spec line.
- `Report()` must be safe from `SessionManager::Tick` on Main and from the crash handler, so it
  takes no lock and allocates nothing (AC 25).

### Stage (Tasks 7–8)
- `dia scaffold stage DeferredStep --modules DeferredStepModule` first, then fill in the module. The
  scaffold owns the manifest and vcxproj touch points; do not hand-edit what it generates.
- The stage module is **both** the `IStepKeySource` and the channel consumer: it advances a key each
  sim step, registers one channel in `DoStart` via `ModuleRef<DeferredStepModule>`, kicks on a
  cadence, and unregisters in `DoStop`.
- Manifest order in `cluiche_main.diaapp`: the stage module is listed **before**
  `DeferredStepModule`. Because the stage is both the key source and the channel consumer, that one
  ordering satisfies both of the spec's rules at once — update runs the stage's key advance before
  the module's `Tick`, and stop (reverse order) runs the stage first, so it unregisters its channel
  while the scheduler is still alive. Record the reasoning as a manifest comment; it is the one
  non-obvious wiring detail, and nothing enforces it (F4 ODQ 1).
- The synthetic step spins over a cell array through the executor and commits a **checksum** that the
  test reads. Verification is cell/stat dumps, never screenshots (feedback_cell_data_over_pixels).
- Spin duration and k come from stage config, so k = 7 and k = 1 are the same binary (AC 30, 31).

### Verification cadence
- Per task: `dia run googletest --filter="DeferredStepModule*" --shards 2` (narrow further to the suite under work; feature-wide filter in task 11 only). Sanitizer: on request only (user, 2026-11-17) (2026-11-17, speed-ups).
- `dia run cluichetest --stage DeferredStep` for Tasks 8–9. Kill leftover `CluicheTest.exe` first;
  the summary line is unreliable, so read the session log (feedback_dia_run_gotchas).
- The full suite, Release and the system-wide E2E are the **system** plan's Task 6.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Add DiaApplicationFlow, DiaStreams and DiaObservation references to `DiaDeferredStep.vcxproj`; complete the module doc's `dependent_modules`; document **both** manifest ordering rules in the module doc; `dia check deps` | Compiles with `<DiaApplicationFlow/SimModule.h>` included | Done | haiku | Mechanical, TDD-exempt; vcxproj refs + module doc ordering rules; dia check deps PASS. NB: plan's stage-ordering rationale (stage before module => stage stops first) is wrong under reverse-order stop; resolve before task 7 |
| 1 | `IStepKeySource.h`. RED → GREEN: `DeferredStepModule` skeleton — `kTypeId`, `kAllowedPUs`, `kDescription`, type registration, `OnConnectStreams` with the `JobSystemService` reader | AC 1–2: `dia validate manifest` accepts the type; the reader connects; PU affinity rejected on MainPU | Done | sonnet | No scheduler construction yet; RED (missing header) -> GREEN 5 tests pass. 'dia validate manifest' part of AC1 deferred to task 7 (no manifest entry yet). Task 7 must add ProjectReference + /WHOLEARCHIVE:DiaDeferredStep.lib (Debug+Release) to CluicheTest.vcxproj. mJobSystemReader is protected for test probe. |
| 2 | RED → GREEN: `DoStart` / `DoStop` — construct gate, launcher, scheduler, participant; the no-job-system path; drain and assert on stop; no submit after stop | AC 3–7: start with and without `JobSystemService` (one Warning, inline launcher); stop drains; double start/stop; no new static introduced | Done | sonnet | Declaration order of members matters — see Implementation Patterns; RED (GetScheduler missing) -> GREEN 11 tests. Stop-drain test unregisters before DoStop (assert covers leftover channels; death test added). Save attach/metrics/health left as SEAM comments. |
| 3 | RED → GREEN: `OnConfigure` | AC 8–11: `"auto"`, explicit 3, over-cap clamp + Warning, `0`/negative/garbage → one Warning + auto, `ranges_per_worker` default and clamp, absent `config` silent | Done | sonnet | Table-driven test over the config shapes; RED->GREEN: ConfigTable 17 cases + 11 others. Clamp Warning comes from CapGate ctor (single). |
| 4 | RED → GREEN: key source and tick | AC 12–16: one `Tick` per sim step with the source's key; no source → no `Tick` and exactly one Warning over 100 updates; unchanged key applies once; catch-up produces several `Tick`s in one frame; `GetScheduler()` usable from a sibling via `ModuleRef` | Not Started | sonnet | **Unblocks the stage tasks.** AC 15 needs a test PU driving several sim steps in one update |
| 5 | RED → GREEN: save services injection | AC 17–18: injected → participant attached and a save contains the section; not injected → starts, logs one Info, save has no section | Not Started | sonnet | Follows `DiaSimTimeModule::SetSaveRegistry` |
| 6 | RED → GREEN: `DeferredStepMetrics` (nine metrics) and `DeferredStepHealth` (Degraded-only, 60 s window) | AC 19–25: all nine present after start and absent after stop; refreshed each `Tick`; no double counting; stall moves `stalls` + `stall_ms`; re-register cleanly; OK → Degraded on stall → OK after the window; `Report()` from a second thread; never Failing | Not Started | sonnet | Use `Dia/DiaObservation/Testing/` fixtures. Drive the clock, do not sleep 60 s — inject or offset the timestamp |
| 7 | Trace and profile zones (AC 26) plus the start/stop/fallback logs (AC 34); `dia scaffold stage DeferredStep --modules DeferredStepModule` and the stage module: key source, one channel, synthetic spin step with a committed checksum, config for spin ms and k | AC 26, 34: zones present on `Capture`/`Apply`/stall/`DrainAll`/`DoUpdate`; logs fire once each; nothing per frame. Stage compiles and runs | In Progress | sonnet | Scaffold first, then fill in. Do not hand-edit generated touch points; USER SIGN-OFF 2026-11-18 (option A): stage = ClockModule (IStepKeySource, listed BEFORE DeferredStepModule) + ConsumerModule (registers channel, kicks, verifies, listed AFTER). Supersedes AC 28 'both' wording and Implementation Patterns stage-ordering paragraph. |
| 8 | Stage verification: k = 7 (applies on kick + 7, zero stalls, SimPU inside budget), k = 1 (stalls, Degraded health, still correct and still on the expected key), `worker_cap` = 1 (completes, cap utilisation 1.0). Plus the profiling check | AC 27, 29–32: committed-state dump + metrics snapshot per run, quoted. `Run` scopes on worker thread ids, `Capture`/`Apply` on Sim | Not Started | sonnet | Cell/stat dumps, not screenshots. Read the session log, not the summary line |
| 9 | `Dia/DiaCLI/tests/e2e/test_deferred_step_stage.py` — headless run, assert no error-level log records, `applies` as expected, `stalls == 0` at k = 7 | AC 33 | Not Started | sonnet | E2E does not build — build first. Kill leftover `CluicheTest.exe` |
| 10 | Exhaustive test pass: gap audit of every AC, every config fallback branch, every lifecycle path, every metric; edge cases — key source removed after start, key source returning a decreasing key, `DoStop` with a step in flight, start/stop/start with channels registered between, `worker_cap` above the worker count on a 1-worker machine | Gap list in notes; new tests green | Not Started | sonnet | No mutation check |
| 11 | Feature close-out: `dia run googletest --filter="DeferredStepModule*" --shards 2`; `dia validate manifest`; `dia check deps`; `dia docs registry`; observation scan across the five pillars in notes; update the system spec features table | All ACs, quoted | Not Started | sonnet | Full suite / Release / cross-feature E2E are the system plan's Task 6 |
