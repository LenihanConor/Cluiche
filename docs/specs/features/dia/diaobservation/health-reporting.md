# Feature Spec: DiaHealth Reporting

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaobservation.md | **health-reporting** |

**Status:** `Draft`

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #2 (foundation) — `SessionManager::Stop` and `EmergencyDump` poll `HealthRegistry`; `SessionManager::Tick` drives periodic health polling. Implementation is serial.

---

## Problem Statement

After Features #1–#5, a session can log, trace, and measure — but has no structured way to answer "did this run succeed?" beyond `exit_code`. The future `DiaE2E` framework needs a one-file pass/fail predicate and per-module status without parsing log text. This feature delivers `IHealthReporter`, `HealthRegistry`, periodic health transition capture to `log.jsonl`, a `health.json` final rollup, and the `DIA_OBSERVATION_ASSERT` / `DIA_OBSERVATION_FAIL` macros that E2E scenarios use to report failure without crashing the process.

---

## Solution Overview

`HealthRegistry` is a singleton (mirroring `MetricRegistry`). Modules that want to report health implement `IHealthReporter` and call `HealthRegistry::Instance().Register(this)` in `DoStart`, `Unregister(this)` in `DoStop`.

`SessionManager::Tick` polls `HealthRegistry` every 500ms, compares each reporter's current `Health` against a cached last-known status, and emits a `record_type: "health"` log record to `log.jsonl` on any transition (OK→Degraded, Degraded→Failing, or any reverse). The `session.json` `modules` array (empty in Feature #2) is populated at `Stop` time from the final poll. `health.json` is written as a standalone file on both clean stop and crash (`EmergencyDump`).

`DIA_OBSERVATION_ASSERT(reporter, cond, reason, msg)` and `DIA_OBSERVATION_FAIL(reporter, reason, msg)` write an `level: "error"` log record AND call `reporter->SetFailing(reason)` — a new method on a base class `HealthReporterBase` that modules can subclass instead of implementing `IHealthReporter` from scratch.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `HealthRegistry::Instance().Register(reporter)` succeeds; reporter appears in subsequent polls | Unit test |
| AC2 | `HealthRegistry::Instance().Unregister(reporter)` removes reporter; it no longer appears in polls | Unit test |
| AC3 | `SessionManager::Tick` polls `HealthRegistry` every 500ms; a status transition from OK→Degraded emits a `record_type: "health"` log record in `log.jsonl` | Unit test: mock tick calls summing >500ms, flip reporter to Degraded, assert record emitted |
| AC4 | A status transition from Degraded→Failing also emits a health record | Unit test |
| AC5 | No health record is emitted when status is unchanged between polls | Unit test: tick 1000ms with stable OK status, assert no health records written |
| AC6 | `health.json` is written on `SessionManager::Stop`; contains `schema_version`, `overall_status`, and a `modules` array with one entry per registered reporter | Unit test: start + stop session with one reporter, parse file, assert fields |
| AC7 | `health.json` `overall_status` is the worst status across all reporters: `"failing"` if any reporter is Failing; `"degraded"` if any is Degraded and none Failing; `"ok"` otherwise | Unit test: three reporters at different statuses, assert rollup |
| AC8 | `health.json` is also written by `EmergencyDump` on crash | Unit test: crash subprocess, assert `health.json` exists |
| AC9 | `session.json` `modules` array is populated at `Stop` with the final status of all registered reporters | Unit test: start + stop, parse `session.json`, assert `modules` not empty |
| AC10 | `DIA_OBSERVATION_ASSERT(reporter, false, reason, "msg")` writes a `level: "error"` log record to `log.jsonl` with `channel: "scenario"` | Unit test: assert, parse log, confirm record |
| AC11 | `DIA_OBSERVATION_ASSERT(reporter, false, reason, "msg")` flips `reporter` health to `kFailing` with the supplied `reason` | Unit test: assert reporter status after macro |
| AC12 | `DIA_OBSERVATION_ASSERT(reporter, true, reason, "msg")` is a no-op (condition is true — no log, no health flip) | Unit test |
| AC13 | `DIA_OBSERVATION_FAIL(reporter, reason, "msg")` unconditionally writes error log + flips health to `kFailing` | Unit test |
| AC14 | Neither macro crashes the process; control returns to caller | Unit test: call macro, assert execution continues |
| AC15 | `HealthReporterBase::SetFailing(reason)` and `SetDegraded(reason)` / `SetOK()` are thread-safe | Unit test: concurrent SetFailing + Report calls from two threads, assert no crash |
| AC16 | `IHealthReporter::Report()` is const and must not throw or assert (documented constraint) | Code review; unit test: `Report()` called from crash handler context (simulated), assert no exception propagates |
| AC17 | Health transition records in `log.jsonl` contain: `schema_version`, `ts_unix_nano`, `session_id`, `level: "info"`, `channel: "health"`, `record_type: "health"`, `reporter`, `old_status`, `new_status`, `reason` | Unit test: trigger transition, parse record, assert all fields |
| AC18 | `dia pipeline --target cluichetest` green in Debug + Release | Build + run verification |

---

## Public API

### `IHealthReporter` interface (lives in `Dia/DiaObservation/Health/`)

```cpp
namespace Dia::Observation::Health {

enum class HealthStatus : uint8_t { kOK, kDegraded, kFailing };

struct Health {
    HealthStatus         status;
    unsigned int         errors;
    unsigned int         warnings;
    Dia::Core::StringCRC reason;   // kInvalidCRC if status == kOK
};

class IHealthReporter {
public:
    virtual ~IHealthReporter() = default;
    virtual Dia::Core::StringCRC GetReporterName() const = 0;
    virtual Health               Report() const = 0;  // must be thread-safe, must not throw
};

} // namespace Dia::Observation::Health
```

### `HealthReporterBase` (lives in `Dia/DiaObservation/Health/`)

```cpp
namespace Dia::Observation::Health {

// Convenience base — modules subclass this instead of IHealthReporter directly.
// Stores status atomically; Report() reads it without locking.
class HealthReporterBase : public IHealthReporter {
public:
    Health Report() const override;   // reads mStatus / mErrors / mWarnings atomically

    // Called by DIA_OBSERVATION_ASSERT / DIA_OBSERVATION_FAIL macros
    void SetFailing (const Dia::Core::StringCRC& reason);
    void SetDegraded(const Dia::Core::StringCRC& reason);
    void SetOK();

    void IncrementErrors()  ;
    void IncrementWarnings();

protected:
    std::atomic<uint8_t>  mStatus   = static_cast<uint8_t>(HealthStatus::kOK);
    std::atomic<uint32_t> mErrors   = 0;
    std::atomic<uint32_t> mWarnings = 0;
    Dia::Core::StringCRC  mReason;      // last set reason; not atomic (written under status transition)
};

} // namespace Dia::Observation::Health
```

### `HealthRegistry` singleton (lives in `Dia/DiaObservation/Health/`)

```cpp
namespace Dia::Observation::Health {

class HealthRegistry {
public:
    static HealthRegistry& Instance();

    void Register  (IHealthReporter* reporter);
    void Unregister(IHealthReporter* reporter);

    // Poll all reporters; returns list of transitions since last poll.
    // Called by SessionManager::Tick and SessionManager::Stop/EmergencyDump.
    struct Transition {
        Dia::Core::StringCRC reporterName;
        HealthStatus         oldStatus;
        HealthStatus         newStatus;
        Dia::Core::StringCRC reason;
    };

    // Snapshot all current statuses (used at Stop / EmergencyDump for final rollup)
    void Snapshot(Health* out, unsigned int maxCount, unsigned int& outCount) const;

    // Poll for transitions since last call; fills transitions array
    void PollTransitions(Transition* out, unsigned int maxCount, unsigned int& outCount);
};

} // namespace Dia::Observation::Health
```

### `DIA_OBSERVATION_ASSERT` / `DIA_OBSERVATION_FAIL` macros

```cpp
// Writes level:"error" log record + flips reporter to kFailing. Does NOT crash.
#define DIA_OBSERVATION_ASSERT(reporter, cond, reason_crc, msg)         \
    do {                                                                  \
        if (!(cond)) {                                                    \
            DIA_LOG_ERROR("scenario", "%s", msg);                        \
            (reporter)->SetFailing(reason_crc);                          \
        }                                                                 \
    } while (0)

#define DIA_OBSERVATION_FAIL(reporter, reason_crc, msg)                  \
    do {                                                                  \
        DIA_LOG_ERROR("scenario", "%s", msg);                            \
        (reporter)->SetFailing(reason_crc);                              \
    } while (0)
```

---

## Wire Format

### Health transition record in `log.jsonl`

```json
{
  "schema_version": "1.0",
  "record_type":    "health",
  "ts_unix_nano":   1747484430000000000,
  "session_id":     "20260517-143022-a3f2c1b9",
  "level":          "info",
  "severity_number": 9,
  "channel":        "health",
  "reporter":       "RenderModule",
  "old_status":     "ok",
  "new_status":     "degraded",
  "reason":         "gpu_timeout"
}
```

Status strings: `"ok"` | `"degraded"` | `"failing"`.

### `health.json` (standalone final rollup)

```json
{
  "schema_version":  "1.0",
  "session_id":      "20260517-143022-a3f2c1b9",
  "overall_status":  "ok",
  "modules": [
    { "name": "RenderModule",  "status": "ok",       "errors": 0, "warnings": 0, "reason": "" },
    { "name": "AssetModule",   "status": "degraded",  "errors": 0, "warnings": 2, "reason": "slow_load" }
  ]
}
```

`overall_status` rollup: `"failing"` if any module Failing; `"degraded"` if any Degraded and none Failing; `"ok"` otherwise.

---

## `SessionManager::Tick` health polling (amends Feature #2)

```cpp
// Added to SessionManager — accumulate time, poll every 500ms
void SessionManager::Tick(float deltaTime) {
    // existing metrics snapshot logic (Feature #5) ...

    mHealthPollAccumMs += deltaTime * 1000.0f;
    if (mHealthPollAccumMs >= kHealthPollIntervalMs) {
        mHealthPollAccumMs = 0.0f;
        PollAndEmitHealthTransitions();
    }
}
```

`PollAndEmitHealthTransitions()` calls `HealthRegistry::Instance().PollTransitions(...)` and writes a `record_type: "health"` entry to `log.jsonl` via `Logger` for each transition.

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaObservation/Health/IHealthReporter.h` | New |
| `Dia/DiaObservation/Health/HealthReporterBase.h/.cpp` | New |
| `Dia/DiaObservation/Health/HealthRegistry.h/.cpp` | New |
| `Dia/DiaObservation/Health/DiaHealth.h` | New — macro definitions |
| `Dia/DiaObservation/Session/SessionManager.h/.cpp` | Add health poll accumulator to `Tick`; `Stop` + `EmergencyDump` call `HealthRegistry::Snapshot` + write `health.json`; populate `session.json` `modules` array |
| `Dia/DiaObservation/DiaObservation.vcxproj` | Add new files |
| `Dia/DiaObservation/DiaObservation.vcxproj.filters` | Add new files |
| `Dia/DiaObservation/Testing/HealthFixture.h` | New test utility |

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | Reporter name, `reason`, health channel — all `StringCRC`. |
| PD-002 | ProcessingUnit/Phase/Module architecture | `HealthRegistry` singleton callable from any thread/module. `HealthReporterBase` is a plain base class — no `IModule` coupling. |
| PD-003 | Component-based entities | Orthogonal. |
| PD-004 | No STL containers in public APIs | `HealthRegistry::Snapshot` and `PollTransitions` use caller-supplied fixed arrays + count. `HealthReporterBase` uses `std::atomic` internals (permitted). |
| PD-005 | x64 only | `std::atomic<uint8_t>`, `std::atomic<uint32_t>` lock-free on x64. |
| PD-006 | VS project files source of truth | `DiaObservation.vcxproj` updated manually. |
| PD-007 | C++20 required | No new C++20 features; existing baseline applies. |
| PD-008 | Directory.Build.props owns build settings | No overrides added. |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | `health.json` written to session directory. |
| PD-010 | `.diagame` is project root | No config additions in this feature; health polling interval is a compile-time constant in v1. |
| AD-001 | Module system with YAML frontmatter | `dia.dia.observation.architecture.module.md` updated to declare `Health/` subsystem. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | All new code in `Dia::Observation::Health::`. |
| SD-O01 | One module for all four pillars | `Health/` is a subdirectory of `Dia/DiaObservation/`. |
| SD-O05 | `schema_version: "1.0"` on every record | Health transition records in `log.jsonl` carry `"schema_version":"1.0"`; `health.json` carries it at top level. |
| SD-O06 | OTel wire-format, no SDK | Health is not an OTel primitive; field names are Dia-native. No SDK dependency. |
| SD-O10 | Health polled at exit AND on crash | `SessionManager::Stop` and `EmergencyDump` both call `HealthRegistry::Instance().Snapshot(...)`. |
| SD-O11 | Health duplicated across three sinks | Transition records → `log.jsonl`; final rollup → `health.json`; summary → `session.json` `modules` array. |
| SD-O12 | `DIA_OBSERVATION_ASSERT`/`FAIL` log + flip health; do NOT crash | Macros call `DIA_LOG_ERROR` + `reporter->SetFailing(reason)`. No `DIA_ASSERT`. |
| SD-O16 | Session ID on every record | Health transition records carry `session_id`. `health.json` carries `session_id` at top level. |
| SD-O20 | Test utilities in `Dia/DiaObservation/Testing/` | `HealthFixture.h` placed there. |
| SD-O21 | DiaCore is only required dependency | `HealthRegistry` and `IHealthReporter` depend only on `DiaCore`. |

---

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | `HealthReporterBase::mReason` is not atomic — it is written under a status transition which itself writes `mStatus` atomically. Is there a race? | `mStatus` is atomic; `mReason` is written immediately before the status flip. Readers (`Report()`) read `mStatus` first; if the status they observe is the new one, `mReason` may not yet be visible (store reordering). Mitigation: use `std::atomic_thread_fence(std::memory_order_seq_cst)` between `mReason` write and `mStatus` store in `SetFailing`/`SetDegraded`. Documented in implementation notes. |
| OQ2 | Health poll interval 500ms — should this be configurable in v1? | No. Compile-time constant `kHealthPollIntervalMs = 500`. Same rationale as metrics snapshot interval — format does not foreclose a future `observation.health.poll_interval_ms` config key. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | `IHealthReporter::Report()` is called from `SessionManager::Tick` (main PU thread) and from `EmergencyDump` (crash handler thread). Must reporters be thread-safe? | Yes — `Report()` must be thread-safe and must not throw or assert (system spec AI-Q9, AI-Q10). `HealthReporterBase::Report()` reads only atomics, satisfying this. Custom `IHealthReporter` implementations are responsible for their own thread safety — documented constraint. |
| 2 | Threading | `HealthRegistry::Register`/`Unregister` vs `PollTransitions`/`Snapshot` — concurrent access? | `HealthRegistry` internal reporter list is protected by a `std::mutex`. `PollTransitions` and `Snapshot` lock briefly to copy the list, then release before calling `Report()` on each reporter. Prevents deadlock if a reporter's `Report()` tries to access the registry. |
| 3 | Crash | `EmergencyDump` must not call `DIA_LOG_*` (re-entrant). How does it emit the health data into `health.json` without logging? | `EmergencyDump` writes `health.json` directly via `Json::Value` → `fwrite`, same as `session.json` crash path (Feature #2 AI-Q13). It does NOT write a health transition record to `log.jsonl` on crash — the final state is captured in `health.json` and `crashes/<n>.json`. |
| 4 | Macros | `DIA_OBSERVATION_ASSERT` calls `DIA_LOG_ERROR("scenario", ...)`. Does "scenario" need to be a registered channel? | No — channels are `StringCRC` values; unregistered channels pass through (level filtering only checks if channel has an override, otherwise uses global level). "scenario" is a new well-known channel, added to the channel registry documentation alongside this feature. |
| 5 | Transitions | What if a reporter flips OK→Failing without going through Degraded? | Valid transition — each poll compares current status to last-known regardless of intermediate states. A direct OK→Failing transition emits one record with `old_status: "ok"`, `new_status: "failing"`. |
| 6 | Transitions | What if a reporter's status improves (Failing→OK)? Is a recovery transition emitted? | Yes — any status change triggers a record, including recoveries. `old_status: "failing"`, `new_status: "ok"`. Useful for E2E scenarios that verify a module recovers after a transient error. |
| 7 | session.json | Feature #2 left `modules` as an empty array. This feature populates it at `Stop` time. Is this a breaking schema change? | No — adding entries to an empty array is additive. `schema_version: "1.0"` is preserved; the field was always defined as an array in the schema. Consumers that read `modules: []` will now read actual entries — correct behaviour. |
| 8 | `HealthReporterBase` | A module that subclasses `HealthReporterBase` but never calls `SetFailing`/`SetDegraded` — what does `Report()` return? | `Health{ kOK, errors:0, warnings:0, reason:kInvalidCRC }`. The default-constructed state is healthy. |
| 9 | E2E | `DIA_OBSERVATION_ASSERT` flips health to Failing. Does a failing health status cause the session `overall_status` in `session.json` to be `"failing"`? | Yes — `session.json` `modules` array is written at `Stop` from the final `HealthRegistry::Snapshot()`. If any reporter is `kFailing` at that point, `overall_status` is `"failing"`. This is the primary pass/fail predicate for the future `DiaE2E` orchestrator. |
| 10 | Lifecycle | `DIA_OBSERVATION_ASSERT` is called before `SessionManager::Start` (e.g. in a unit test). What happens? | `DIA_LOG_ERROR` writes to StdOut/Debug sinks (they work without a session). `reporter->SetFailing(reason)` flips the reporter's internal state. No `log.jsonl` is written (no session). No crash. Correct behaviour for unit tests that use the macros. |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `IHealthReporter` interface + `Health` struct + `HealthStatus` enum | AC1, AC16 | Planned | haiku | Plain headers |
| 2 | `HealthReporterBase` — atomic status, `SetFailing`/`SetDegraded`/`SetOK`, `IncrementErrors`/`Warnings`, `Report()` | AC11, AC15 | Planned | sonnet | Memory fence per OQ1 |
| 3 | `HealthRegistry` singleton — `Register`/`Unregister`, `Snapshot`, `PollTransitions`; mutex-protected list | AC1, AC2, AC7 | Planned | sonnet | |
| 4 | `DiaHealth.h` macro definitions — `DIA_OBSERVATION_ASSERT` + `DIA_OBSERVATION_FAIL` | AC10–AC14 | Planned | haiku | |
| 5 | `SessionManager::Tick` amended — 500ms health poll accumulator, `PollAndEmitHealthTransitions` | AC3, AC4, AC5 | Planned | sonnet | Amends Feature #2 files |
| 6 | `SessionManager::Stop` amended — final `HealthRegistry::Snapshot`, write `health.json`, populate `session.json` `modules` | AC6, AC7, AC9 | Planned | sonnet | Amends Feature #2 files |
| 7 | `SessionManager::EmergencyDump` amended — poll health, write `health.json` directly (no Logger) | AC8 | Planned | sonnet | Amends Feature #2 files; no DIA_LOG_* per AI-Q3 |
| 8 | Health transition `log.jsonl` record writer — emit `record_type: "health"` per AC17 schema | AC17 | Planned | haiku | Uses existing Logger path |
| 9 | Test utility `HealthFixture.h` in `Testing/` | Supporting ACs | Planned | haiku | |
| 10 | GoogleTests — all AC1–AC17 | All ACs | Planned | sonnet | |
| 11 | Update `DiaObservation.vcxproj` + `.vcxproj.filters` | AC18 | Planned | haiku | |
| 12 | Update `dia.dia.observation.architecture.module.md` — add `Health/` subsystem | — | Planned | haiku | |

---

## Status

`Approved` — 2026-05-17. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
