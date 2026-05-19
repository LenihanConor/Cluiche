# Implementation Plan: DiaMetrics Registry

**Spec:** [metrics-registry.md](metrics-registry.md)
**System Spec:** [diametrics.md](../../systems/dia/diametrics.md)
**Created:** 2026-05-18

---

## Session Notes

### Spec Decisions Summary

DiaMetrics is a standalone module (`Dia/DiaMetrics/`) depending **only** on DiaCore. Namespace: `Dia::Metric::`. All metric names are `StringCRC` (PD-001). No STL in public APIs — fixed arrays only (PD-004, SD-MET-005: `MetricEntry[128]`, `BucketEntry[17]`). `Counter` uses per-thread shards with lazy registration (SD-MET-002, SD-MET-006). `Gauge` uses `std::atomic<double>` (SD-MET-003). `Histogram` copies up to 16 bucket bounds (SD-MET-004). Snapshot is not atomic across primitives (SD-MET-008). `MetricRegistry` is available before any session — Meyer's singleton pattern (SD-MET-001). Internal `<atomic>`, `<mutex>`, `<thread>` permitted. `Directory.Build.props` owns OutDir/IntDir/toolchain (PD-008). Test utilities live in `Dia/DiaMetrics/Testing/` (SD-O20).

DiaObservation adds `MetricsFileSink` — stamps snapshots with `session_id` + epoch offset, writes `metric.jsonl` / `metrics-final.json`. `SessionManager::Start` registers the sink; `Tick` drives 100ms snapshots; `Stop` fires final. `MetricsCollectorModule` rewrite registers 4 gauges (fps, frame_time_ms, memory_bytes, uptime_s) in `DoInit`; module shape unchanged.

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `DiaMetrics.vcxproj` + `.vcxproj.filters` + add to `Cluiche.sln` | `dia pipeline --target diametrics` compiles (empty lib) | Done | sonnet | StaticLibrary, DiaCore project reference only. GUID: generate new. Include dirs: `./;./../;` |
| 2 | Create `dia.dia.metrics.architecture.module.md` | Inspect file | Done | haiku | YAML frontmatter per schema. Parent: none (top-level Dia module). Deps: `dia.core` |
| 3 | Implement `MetricSnapshot.h` + `IMetricSink.h` | Compiles in task 1 lib | Done | sonnet | Pure data structs + interface. `MetricEntry[128]`, `BucketEntry[17]`. `IMetricSink::OnSnapshot`/`OnFinal`. No .cpp needed. |
| 4 | Implement `Gauge.h/.cpp` | `dia run googletest --filter="MetricGauge*"` | Done | sonnet | `std::atomic<double>` store/load. TDD: RED test for Set/Value + concurrent Set, then GREEN. AC4. |
| 5 | Implement `Counter.h/.cpp` | `dia run googletest --filter="MetricCounter*"` | Done | opus | Per-thread shard via `thread_local` + mutex-guarded shard list. RAII unregister on thread exit. TDD: RED for Inc/Value + concurrent Inc from 4 threads (AC2, AC3), then GREEN. Most complex primitive. |
| 6 | Implement `Histogram.h/.cpp` | `dia run googletest --filter="MetricHistogram*"` | Done | sonnet | Fixed buckets (up to 16 + implicit +Inf). `Observe()` finds correct bucket. p50/p95/p99 via linear interpolation at read time. TDD: RED for Observe placing values in correct bucket (AC5) + percentile tolerance, then GREEN. |
| 7 | Implement `MetricRegistry.h/.cpp` | `dia run googletest --filter="MetricRegistry*"` | Done | sonnet | Meyer's singleton (`Instance()`). Register/Find for Counter/Gauge/Histogram. Idempotent re-register same kind (AC16). Assert on re-register different kind (AC17). `nullptr` for unregistered (AC15). `Snapshot()` fills `MetricSnapshot`. Sink register/unregister + dispatch. TDD. |
| 8 | Create `Testing/MetricFixture.h` | Used by tests in tasks 4-7 | Done | haiku | Test helper: `ResetRegistry()` for test isolation (or fresh instance per test). Follows `DiaStateMachine/Testing/` pattern. |
| 9 | Add DiaMetrics test files to `GoogleTests.vcxproj` + `.vcxproj.filters` | `dia pipeline --target googletest` compiles | Done | haiku | Add test .cpp files under `Cluiche/Tests/GoogleTests/DiaMetrics/` |
| 10 | Integration: `dia pipeline --target diametrics` + `dia run googletest --filter="Metric*"` | All pass, no warnings | Done | sonnet | Full build + test pass. AC18 (standalone build), AC2-AC5, AC15-AC17 all verified. |
| 11 | Implement `MetricsFileSink.h/.cpp` (in DiaObservation) | `dia run googletest --filter="MetricsFileSink*"` | Deferred | sonnet | Session-aware sink. `OnSnapshot` writes JSON-line to `metric.jsonl` (AC6-AC9, AC11). `OnFinal` writes `metrics-final.json` (AC12). Depends on DiaObservation existing — **DEFER if DiaObservation module does not yet exist on disk.** |
| 12 | Wire `SessionManager` to drive snapshots | `dia run googletest --filter="MetricsSession*"` | Deferred | sonnet | `Start` registers `MetricsFileSink` (AC14). `Tick` accumulates dt, fires at >=100ms (AC10). `Stop` fires final snapshot (AC12). `session.json` lists `metric.jsonl` (AC13). **DEFER if DiaObservation not yet built.** |
| 13 | Add `DiaMetrics` project reference to `DiaObservation.vcxproj` | Build | Deferred | haiku | **DEFER if DiaObservation not yet built.** |
| 14 | Rewrite `MetricsCollectorModule` internals | `dia pipeline --target cluichetest` green | Done | sonnet | Register 4 gauges (`dia.fps`, `dia.frame_time_ms`, `dia.memory_bytes`, `dia.uptime_s`) in `DoInit`. Update in `DoUpdate`. Delete hand-rolled `PUMetrics`/`MetricsSnapshot` internal storage. Module ID + `DoUpdate` signature unchanged. AC19. Add `DiaMetrics` project reference to `DiaApplicationFlow.vcxproj`. |
| 15 | Update module registry | Inspect file | Done | haiku | Add DiaMetrics entry to `docs/reference/registry/module-registry.md` |
| 16 | Final integration gate | `dia pipeline --target googletest` + `dia pipeline --target cluichetest` both green in Debug | Done | sonnet | AC18, AC19, AC20. Full verification of all ACs that don't require DiaObservation. |

---

## Dependency Graph

```
Task 1 (vcxproj) ──┬──► Task 2 (module doc)     [parallel with 3]
                    ├──► Task 3 (snapshot/sink interfaces)
                    │         │
                    │         ▼
                    ├──► Task 8 (test fixture)
                    │         │
                    │         ▼
                    ├──► Task 4 (Gauge)      ─┐
                    ├──► Task 5 (Counter)     ├──► Task 7 (Registry) ──► Task 9 (test vcxproj)
                    └──► Task 6 (Histogram)  ─┘                              │
                                                                             ▼
                                                                        Task 10 (integration)
                                                                             │
                                                    ┌────────────────────────┼────────────────┐
                                                    ▼                        ▼                ▼
                                              Task 11 (FileSink)       Task 14 (MCM rewrite) Task 15 (registry doc)
                                                    │
                                                    ▼
                                              Task 12 (SessionManager)
                                                    │
                                                    ▼
                                              Task 13 (DiaObs vcxproj ref)
                                                    │
                                                    ▼
                                              Task 16 (final gate)
```

---

## Parallelization Opportunities

- **Tasks 2, 3, 8** can run in parallel after Task 1 (different files, no shared headers)
- **Tasks 4, 5, 6** can run in parallel after Task 3 + 8 (each is a separate .h/.cpp + test file pair)
- **Tasks 11, 14, 15** can run in parallel after Task 10 (different modules entirely)

---

## Deferred Tasks (DiaObservation dependency)

Tasks 11, 12, 13 require the DiaObservation module to exist on disk. DiaObservation is blocked behind Feature #1 (Skeleton + DiaLogger Fold) and Feature #2 (Foundation) — neither is built yet.

**Strategy:** Complete Tasks 1–10, 14–16. The DiaMetrics module ships standalone with full test coverage. Tasks 11–13 execute later as part of DiaObservation Feature #5 implementation.

---

## Notes

- `MetricRegistry` uses Meyer's singleton (`static MetricRegistry& Instance()`) NOT `Dia::Core::Singleton<T>`. Reason: metrics must be available before any explicit `Create()` call — modules register in `DoInit` which runs before application-level setup. Meyer's singleton is lazy-initialized on first call and destroyed at static lifetime end.
- The old `MetricsCollectorModule` files were deleted on the current branch. Task 14 will recreate the module with the new implementation (register gauges via `MetricRegistry` instead of hand-rolling).
- `GetProcessMemoryInfo` via `<psapi.h>` + `WorkingSetSize` for memory gauge (Windows-only, consistent with PD-005).
- Counter shard pattern: each `Counter` owns a `std::mutex`-protected list of `thread_local` shard pointers. `Inc()` touches only the local shard (no lock). `Value()` locks the list and sums all shards.
