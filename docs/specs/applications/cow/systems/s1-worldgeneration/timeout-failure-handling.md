# Feature Spec: Timeout and Failure Handling

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md

**Gameplay Domains:** simulation

## Summary

Timeout and Failure Handling prevents infinite hangs on unlucky seeds or unsatisfiable parameters by monitoring total generation time (including all retry attempts) and routing timeouts, unsatisfiable seeds, and actual generation errors through CoW's Cancel path (Phase 6's existing discard-and-return mechanism). Each failure mode has a distinguishing assert/message so they're never conflated — timeout wins over retry (prevents infinite hangs while preserving determinism), and failure fallthrough cleanly discards partial state.

This feature resolves PARK-P6-001 (worldgen timeout/failure behavior, parked since Phase 6 pending a working prototype).

## Problem

Without timeout/failure handling:
- **Infinite hangs** — Unlucky seeds or unsatisfiable parameters (e.g., too many Hearth Sites for small grid) could retry forever if Feature 3's `maxRetryAttempts` is set too high or validation is slow
- **Silent failures** — Actual generation errors (e.g., out-of-memory, file I/O error loading the placeholder terrain table) would crash the game or return corrupted worlds without clear diagnostics
- **Non-distinguishable failures** — Timeout, retry-exhaustion, manual Cancel, and generation errors would look identical in logs, making debugging impossible

With timeout/failure handling:
- **Hard timeout prevents hangs** — Total generation time monitored; if exceeded, timeout path triggers immediately (even mid-retry), preventing infinite loops
- **Clean failure path** — Unsatisfiable seeds (retry exhaustion) and generation errors (exceptions, I/O failures) route through Cancel's discard-and-return, discarding partial state and returning control to the S-1 panel (the stage stays open)
- **Distinguishing diagnostics** — Timeout, Failure, and manual Cancel have distinct assert/log messages, enabling root-cause debugging

## Goals

1. **Hard timeout enforcement** — Monitors total generation time (wall-clock) from `GenerateWorld()` call to return; if exceeds `GenParams.timeoutSeconds`, auto-cancels immediately
2. **Timeout wins over retry** — If retry loop pushes past timeout, timeout path triggers (prevents infinite hangs on unlucky seeds while preserving determinism — same seed → same retry sequence up to timeout)
3. **Three distinguishable failure modes** — Timeout ("TIMEOUT"), retry exhaustion ("FAILURE: unsatisfiable seed"), generation error ("FAILURE: error message"); each has unique assert/log text
4. **Cancel path integration** — Reuses Phase 6's existing discard-and-return mechanism (no new exit path invented); Cancel button, timeout, and failure all route through same path with distinguishing source
5. **Partial state cleanup** — No half-constructed worlds returned; partial `WorldStructure` discarded on Cancel/timeout/failure
5a. **Previous world survives** *(clarified 2026-10-15)* — A failed, timed-out or cancelled generation does not touch the last accepted world: `GetCurrentWorld()` and `GetWorldRevision()` are unchanged, and the stage stays open on it. "Discard-and-return" discards the attempt, not the stage or the previous world. Feature 3's `GenerationReport` records the failure
6. **Non-deterministic timeout** — Timeout is wall-clock based (not tick-based), so same seed may timeout on slow machine or succeed on fast machine; this is acceptable (timeout is a safety net, not a design constraint)

## Non-Goals

1. **Per-operation timeout** — Hard timeout applies to full generation (all retries); not per-attempt or per-validation timeout (future optimization if needed)
2. **Automatic parameter adjustment** — Timeout/failure does not auto-tune `GenParams` (e.g., reduce site count, increase retry attempts); designer must manually adjust via Feature 6 debug host
3. **Timeout as design constraint** — Timeout prevents hangs, not a perf budget (5-second generation target is Goal in Feature 1, not enforced by timeout); timeout value should be much higher than expected generation time
4. **Retry after timeout** — Once timeout triggers, generation stops immediately (no "extend timeout" or "retry with longer timeout")

## Architecture

### Per-Frame State Machine *(added 2026-10-15)*

`WorldGenerationModule` runs generation **one attempt per SimPU frame**, so Cancel can be handled and the frame never stalls for the full timeout:

```
Idle --WorldRegenerationRequested--> Attempting(n) --each DoUpdate--> Attempting(n+1) | Idle
```

Each `DoUpdate` while `Attempting(n)`:
1. Drain requests. A `WorldGenerationCancelled` finishes the run as MANUAL_CANCEL.
2. If total wall-clock time exceeds `timeoutSeconds`, finish as TIMEOUT.
3. Otherwise run attempt `n` (Feature 3). Passed: accept the world (revision +1), publish `WorldGenerationComplete`, go `Idle`. Failed: record the attempt, publish `WorldGenerationRetry`, then continue, or finish as UNSATISFIABLE when `maxRetryAttempts` is reached.
4. An exception thrown by an attempt finishes the run as ERROR.

Finishing for any failure reason publishes `WorldGenerationFailed`, leaves the last accepted world and revision untouched (Goal 5a), and goes `Idle`. The blocking `GenerateWorld(seed, params)` stays for tests and headless use: it loops the same step with an injectable clock and cancel flag. The code below is the blocking form of the same checks.

### Timeout Monitoring

```cpp
Result<WorldStructure> GenerateWorld(uint64_t outerSeed, const GenParams& params) {
    auto startTime = std::chrono::steady_clock::now();

    // Terrain (Feature 1) + biomes (Feature 8): built once from outerSeed + params.shape (SD-S1-008).
    WorldStructure terrain = BuildTerrainAndBiomes(outerSeed, params);
    
    for (uint32_t attempt = 0; attempt < params.maxRetryAttempts; ++attempt) {
        // Check timeout before each retry attempt
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > std::chrono::seconds(static_cast<long long>(params.timeoutSeconds))) {
            DIA_LOG_ERROR("WLD", "TIMEOUT: World generation exceeded %.1f seconds", params.timeoutSeconds);
            PublishEvent(WorldGenerationFailed{FailureReason::TIMEOUT, "TIMEOUT"});
            return Result<WorldStructure>::Failure("TIMEOUT");
        }
        
        // Re-roll Regions and Sites only (Feature 9), then validate (Feature 2)
        uint64_t subSeed = DeriveSubSeed(outerSeed, attempt);
        WorldStructure world = terrain;
        RegionsAndSites::Place(world, subSeed, params);
        ValidationResult validation = ReachabilityValidator::Validate(world, params.validation);
        
        if (validation.passed) {
            AcceptWorld(world);  // bumps revision; publishes WorldGenerationComplete{outerSeed, attempt, revision}
            return Result<WorldStructure>::Success(world);
        }
    }
    
    // Exhausted retries: fallthrough to Failure
    DIA_LOG_ERROR("WLD", "FAILURE: World generation failed after %u attempts (unsatisfiable seed or parameters)", params.maxRetryAttempts);
    PublishEvent(WorldGenerationFailed{FailureReason::UNSATISFIABLE, "FAILURE: unsatisfiable seed"});
    return Result<WorldStructure>::Failure("FAILURE: unsatisfiable seed");
}
```

**Timeout granularity:** Checked once per retry attempt (not per-cell or per-site placement); sufficient for typical generation times (<5 seconds) and retry counts (<10 attempts). If timeout needs finer granularity (e.g., kill generation mid-attempt), add checks inside Feature 1's generation loop.

### Generation Error Handling

```cpp
Result<WorldStructure> GenerateWorld(uint64_t outerSeed, const GenParams& params) {
    try {
        // ... retry loop with timeout checks (above)
    } catch (const std::exception& e) {
        DIA_LOG_ERROR("WLD", "FAILURE: World generation error: %s", e.what());
        PublishEvent(WorldGenerationFailed{FailureReason::ERROR, std::string("FAILURE: ") + e.what()});
        return Result<WorldStructure>::Failure(std::string("FAILURE: ") + e.what());
    }
}
```

**Error sources:**
- Feature 1 (Terrain Generator) or Feature 9 (Regions and Sites): out-of-bounds array access, out-of-memory, bad RNG state
- Feature 2 (Reachability Validator): failed to load placeholder terrain table, pathfinding stack overflow
- Feature 4 (JSON Export/Import): file I/O error (disk full, permission denied)

**Partial state cleanup:** `Result<WorldStructure>::Failure()` does not return a world; caller receives failure status only. Partial `WorldStructure` (allocated in `Generate()` but not validated) is discarded (goes out of scope, cleaned up by destructors).

### Manual Cancel Integration

**Triggered by:** Feature 6 (Debug Host Integration) "Cancel" button publishes `WorldGenerationCancelled` event.

**Handling:** `GenerateWorld()` checks cancellation flag per retry attempt (same granularity as timeout check):

```cpp
if (mCancelRequested) {
    DIA_LOG_INFO("WLD", "CANCEL: World generation cancelled by user");
    PublishEvent(WorldGenerationFailed{FailureReason::MANUAL_CANCEL, "CANCEL"});
    return Result<WorldStructure>::Failure("CANCEL");
}
```

**Thread-safe flag:** `std::atomic<bool> mCancelRequested` set by event handler, checked by generation thread.

### Distinguishing Messages

| Failure Mode | Assert/Log Message | Event `FailureReason` | How to Identify |
|--------------|-------------------|----------------------|-----------------|
| **Timeout** | "TIMEOUT: World generation exceeded X seconds" | `TIMEOUT` | Elapsed time > `timeoutSeconds` |
| **Unsatisfiable Seed** | "FAILURE: World generation failed after N attempts (unsatisfiable seed or parameters)" | `UNSATISFIABLE` | Retry exhausted, no attempt passed validation |
| **Generation Error** | "FAILURE: World generation error: <exception message>" | `ERROR` | Exception caught during generation/validation |
| **Manual Cancel** | "CANCEL: World generation cancelled by user" | `MANUAL_CANCEL` | User clicked Cancel button (Feature 6) |

**Rationale:** Distinct prefixes ("TIMEOUT", "FAILURE", "CANCEL") ensure grep/log-search can distinguish failure modes. Event `FailureReason` enum enables programmatic handling (Feature 6 debug host can display different UI feedback per mode).

## Public Interfaces

### APIs

```cpp
// WorldGenerationModule.h (CoW/Shared/WorldGeneration/)
namespace CoW::WorldGeneration {

enum class FailureReason {
    TIMEOUT,          // Hard timeout exceeded
    UNSATISFIABLE,    // Retry exhausted without success
    ERROR,            // Exception or generation error
    MANUAL_CANCEL     // User clicked Cancel button
};

class WorldGenerationModule : public Dia::Core::IModule {
public:
    // Generate world from seed + parameters; blocks until complete, timeout, or cancel
    // Returns: success (with WorldStructure), or failure (timeout/unsatisfiable/error/cancel)
    Dia::Core::Result<WorldStructure> GenerateWorld(uint64_t seed, const GenParams& params);
    
    // Cancel ongoing generation (thread-safe, callable from UI thread)
    void CancelGeneration();
    
    // ... other Feature 4/6 APIs (Export, Import, GetProgress)
};

} // namespace CoW::WorldGeneration
```

**`CancelGeneration()` thread-safety:** Sets atomic flag; generation thread checks flag per retry attempt and exits cleanly.

### Events

```cpp
// Published on timeout/failure/cancel
struct WorldGenerationFailed {
    FailureReason reason;  // TIMEOUT / UNSATISFIABLE / ERROR / MANUAL_CANCEL
    char message[128];    // Fixed buffer (truncated): distinguishing assert text (includes prefix per table above)
};

// Subscribed by WorldGenerationModule (from F-1 debug host)
struct WorldGenerationCancelled {
    // Triggered by F-1's Cancel button; handled by setting mCancelRequested flag
};
```

**Rationale for event:** Feature 6 (Debug Host Integration) subscribes to `WorldGenerationFailed` to display failure feedback ("Generation timed out after 10 seconds", "Unsatisfiable seed, try different parameters").

## Dependencies

### Upstream Dependencies

| System/Feature | Why needed | Status |
|----------------|-----------|---------|
| **Feature 1 (Terrain Generator, @terrain-generator.md)** | Generates world per retry attempt; may throw exceptions | Draft (re-spec 2026-10-15) |
| **Feature 2 (Reachability Validator)** | Validates world per retry attempt; may throw exceptions | Approved |
| **Feature 3 (Retry Mechanism)** | Retry loop is where timeout/failure checks are inserted | Approved |
| **Phase 6 Cancel path** | Reuses existing discard-and-return mechanism | Assumed implemented (GDD decision 5 references it) |

### Dia Engine Dependencies

- **DiaCore** (`Result<T>` for failure return, logging via `DIA_LOG_*`, `std::atomic<bool>` for cancel flag)
- **DiaMessageBus** (`WorldGenerationFailed` event, `WorldGenerationCancelled` subscription)
- **C++ Standard Library** (`<chrono>` for timeout monitoring, `<exception>` for error handling)

## Acceptance Criteria

### AC1 — Timeout Enforcement
Force generation past hard timeout (inject artificial delay in Feature 1 generation or Feature 2 validation, e.g., `std::this_thread::sleep_for(timeout + 1 second)`). **Pass:** Generation returns Failure with `reason == TIMEOUT` and message contains "TIMEOUT" prefix. Proves timeout monitoring works.

### AC2 — Timeout Wins Over Retry
Generate world with `maxRetryAttempts = 100` and `timeoutSeconds = 5`. Force every validation to fail (hand-edit test world to violate guarantees). **Pass:** Generation returns Failure with `reason == TIMEOUT` within ~5 seconds, does not attempt all 100 retries. Proves timeout interrupts retry loop.

### AC3 — Unsatisfiable Seed Failure
Generate world with `maxRetryAttempts = 5` and engineered-to-never-succeed parameters (e.g., impossible placement constraints). **Pass:** Generation returns Failure with `reason == UNSATISFIABLE` and message contains "FAILURE" prefix after exactly 5 attempts. Proves retry exhaustion path works.

### AC4 — Generation Error Handling
Force generation error (e.g., mock Feature 2 to throw `std::runtime_error("Test error")`). **Pass:** Generation returns Failure with `reason == ERROR` and message contains "FAILURE: Test error". Proves exception handling and error propagation.

### AC5 — Manual Cancel
Start generation, trigger `CancelGeneration()` mid-generation (e.g., after first retry attempt). **Pass:** Generation returns Failure with `reason == MANUAL_CANCEL` and message contains "CANCEL" prefix. Proves cancel flag checked and handled.

### AC6 — Distinguishing Messages
Run AC1 (timeout), AC3 (unsatisfiable), AC4 (error), AC5 (cancel). Capture logs for each. **Pass:** Each log message has distinct prefix ("TIMEOUT", "FAILURE", "CANCEL") and distinct `FailureReason` enum value. Proves all four failure modes distinguishable.

### AC7 — No Partial State Returned
Force generation failure (any mode). **Pass:** `GenerateWorld()` returns `Result<WorldStructure>::Failure()`, not a partial `WorldStructure`. Caller cannot access half-constructed world (proves cleanup works).

### AC8 — Non-Deterministic Timeout (Informational)
Generate world on fast machine (should succeed within 5 seconds), then artificially slow Feature 1 (inject delay) and regenerate with same seed. **Expected:** First run succeeds, second run times out. **Observation only (not pass/fail):** Proves timeout is wall-clock based, not deterministic. Acceptable per Non-Goals.

## Inherited Binding Decisions

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | `WorldGenerationModule` runs on SimPU (generation is sim-time work, but timeout monitoring is wall-clock); `CancelGeneration()` callable from MainPU (UI thread, cross-thread communication via atomic flag) |
| PD-004 | No STL containers in public APIs | `Result<WorldStructure>` return type (Dia's `Result<T>` already compliant); `FailureReason` enum and `std::string` message are acceptable in events (not arrays) |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-002 | Classify cross-cutting needs | Timeout/failure handling is **Feature-local** (lives in `WorldGenerationModule::GenerateWorld()`); if other CoW systems need timeout monitoring, extract to **CoW-Shared** utility. Not Dia (too CoW-specific: uses CoW's Cancel path and WLD events), not Baseline |
| AD-003 | GDD owns design intent | GDD defines "timeout and failure route through Cancel's existing path" (decision 5), distinguishing messages (§6 Tests 4/5), and PARK-P6-001 resolution. This spec defines implementation (timeout monitoring, exception handling, cancel flag, failure diagnostics) |

### From System (WorldGenerationPipeline)

| ID | Decision | How it applies |
|----|----------|----------------|
| N/A | No system-level binding decisions in parent spec | System spec has Open Design Questions but no SD- binding decisions |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | What is the exact default value for `timeoutSeconds`? | Affects AC1/AC2 test data; does not block implementation (use arbitrary default, tune during S-1 challenge pass) | GDD open question 1 defers this; start with placeholder default (e.g., 30 seconds — much higher than 5-second generation target, provides safety net without false positives) |
| 2 | Should timeout check be per-retry-attempt (coarse) or per-generation-phase (fine)? | Affects timeout granularity (coarse: may overshoot timeout by one full attempt; fine: more accurate but adds checks to Feature 1 generation loop) | Start with per-retry-attempt (simpler, matches cancel check granularity); add finer checks if AC2 shows timeout overshoots are too large |
| 3 | Should generation errors (exceptions) be fatal (crash app) or recoverable (return Failure)? | Current design: recoverable (return Failure, log error); alternative: fatal (assert/crash for debugging) | Recoverable for S-1 (designers can retry with different parameters); add `DIA_ASSERT` in Debug builds if exceptions indicate logic errors, not just bad parameters |

## Status

`Approved` — 2026-10-11. Clarified 2026-10-15: failure keeps the previous world and the stage open (Goal 5a); success publishes the notification-shaped `WorldGenerationComplete`.

---

## Implementation Notes

When implementation begins (after approval), create a plan file `timeout-failure-handling.plan.md` alongside this spec. The plan must include an **Implementation Patterns** section documenting:

- Timeout monitoring implementation (`std::chrono::steady_clock`, check granularity, elapsed time calculation)
- Cancel flag thread-safety (`std::atomic<bool>`, where checked, how set by event handler)
- Exception handling strategy (which exceptions caught, how propagated, how logged)
- Failure message format (prefix conventions, error message composition, log level per failure mode)
- Integration with Feature 3 retry loop (where timeout/cancel checks inserted, how to exit loop cleanly)

These patterns are validated against actual implementation before tasks are marked Done.
