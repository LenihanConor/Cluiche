# Feature Spec: Retry Mechanism

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md

**Gameplay Domains:** simulation

## Summary

The Retry Mechanism orchestrates world generation attempts when Feature 2 (Reachability Validator) detects that the three hard guarantees are not satisfied. It retries generation with deterministic sub-seeds (outer seed + attempt counter) up to a bounded attempt count, ensuring same outer seed always produces the same retry sequence and same final outcome (either success with specific placement, or fallthrough to Failure after exhausting retries).

This feature prevents unlucky seeds from producing broken worlds, while maintaining F-2's determinism contract — the retry sequence itself is deterministic, not random.

## Problem

Without retry:
- **High failure rate** — Feature 1's naive placeholder algorithm can easily produce worlds violating the three hard guarantees on first attempt; without retry, most seeds would fallthrough to Failure (unacceptable for iteration workflow)
- **Manual workarounds** — Designers would need to manually try different seeds until finding one that works, wasting iteration time
- **Non-deterministic outcomes** — If retry used random re-rolls, same seed could produce different worlds or different success/failure outcomes across runs, breaking F-2's determinism contract

With retry:
- **Higher success rate** — GDD Test 1a targets ≥95% success rate across random seeds (most seeds succeed within bounded retry attempts)
- **Deterministic** — Same outer seed → same retry sub-seed sequence → same final world or same Failure outcome every time
- **Transparent to caller** — Retry is internal to `WorldGenerationModule.GenerateWorld()`; caller doesn't orchestrate retries manually

## Goals

1. **Bounded retry attempts** — Retries up to `GenParams.maxRetryAttempts` before falling through to Failure (prevents infinite loops on unsatisfiable seeds)
2. **Deterministic sub-seeds** — Each retry attempt uses `outer_seed + attempt_counter` (or similar deterministic derivation) so same outer seed always produces same retry sequence
3. **Same outcome every time** — Same outer seed + same `GenParams` → same final world (if any attempt succeeds) or same Failure (if all attempts exhaust), every run
4. **High success rate** — Achieves ≥95% success rate across random seeds (GDD Test 1a target), meaning bounded retry count is tuned high enough to handle typical placement failures
5. **Clear failure diagnostics** — When retry exhausts, logs which guarantees failed on final attempt and how many retries were performed

## Non-Goals

1. **Infinite retry** — Bounded by `maxRetryAttempts`; unsatisfiable seeds must fallthrough to Failure (Feature 5 handles timeout/failure path)
2. **Guarantee-specific retry** — Placeholder retries full generation (re-rolls biomes, terrain, regions, sites); smarter retry (e.g., "only re-place Hearth Sites if reachability failed") is future optimization, not required for S-1
3. **Adaptive parameter tuning** — Retry uses same `GenParams` every attempt; smarter retry (e.g., "reduce Hearth count if placement keeps failing") is future optimization
4. **Parallel retry** — Sequential retry (attempt 1, validate, attempt 2, validate, ...); parallel generation is future optimization

## Architecture

### Retry Flow

```cpp
Result<WorldStructure> GenerateWorld(uint64_t outerSeed, const GenParams& params) {
    for (uint32_t attempt = 0; attempt < params.maxRetryAttempts; ++attempt) {
        uint64_t subSeed = DeriveSubSeed(outerSeed, attempt);
        
        WorldStructure world = PlaceholderGenerator::Generate(subSeed, params);
        ValidationResult validation = ReachabilityValidator::Validate(world, params.validation);
        
        if (validation.passed) {
            // Success: return world with retry count for telemetry
            DIA_LOG_INFO("WLD", "World generation succeeded on attempt %u", attempt + 1);
            return Result<WorldStructure>::Success(world);
        }
        
        // Log failure diagnostics for this attempt
        DIA_LOG_WARN("WLD", "Attempt %u failed: %s", attempt + 1, validation diagnostics);
    }
    
    // Exhausted retries: fallthrough to Failure path (Feature 5 handles Failure messaging)
    DIA_LOG_ERROR("WLD", "World generation failed after %u attempts", params.maxRetryAttempts);
    return Result<WorldStructure>::Failure("Retry exhausted: unsatisfiable seed or parameters");
}
```

**Deterministic sub-seed derivation:**

```cpp
uint64_t DeriveSubSeed(uint64_t outerSeed, uint32_t attemptCounter) {
    // Simple additive (ensures different seed per attempt, deterministic across runs)
    return outerSeed + attemptCounter;
    
    // Alternative: hash-based (better bit mixing if additive has patterns)
    // return Dia::Core::CRC64(outerSeed, attemptCounter);
}
```

**Rationale for additive:** Simplest deterministic derivation; guarantees different seed per attempt; if collision issues arise (e.g., `outerSeed = MAX_UINT64 - 1, attempt = 2` wraps), switch to hash-based.

### Integration with Features 1 and 2

- **Feature 1 (Placeholder Generation Algorithm):** Retry calls `PlaceholderGenerator::Generate(subSeed, params)` per attempt (Feature 1 is stateless, doesn't know about retry)
- **Feature 2 (Reachability Validator):** Retry calls `ReachabilityValidator::Validate(world, params.validation)` per attempt (Feature 2 is stateless, doesn't know about retry)
- **This feature (Retry Mechanism):** Orchestrates the loop (attempt counter, sub-seed derivation, success/failure decision)

### Telemetry

**On success:** Log attempt count (helps tune `maxRetryAttempts` — if most seeds succeed on attempt 1-2, default is well-tuned; if most succeed on attempt 10+, increase default or improve Feature 1 algorithm).

**On failure:** Log final validation diagnostics (which guarantees failed, why) and total attempt count (helps diagnose unsatisfiable seeds vs. too-low retry bound).

## Public Interfaces

### APIs

```cpp
// WorldGenerationModule.h (CoW/Shared/WorldGeneration/)
namespace CoW::WorldGeneration {

class WorldGenerationModule : public Dia::Core::IModule {
public:
    // Generate world from seed + parameters; blocks until complete or retry exhausted
    // Internally retries up to params.maxRetryAttempts if validation fails
    // Returns: success (with WorldStructure), or failure (retry exhausted)
    Dia::Core::Result<WorldStructure> GenerateWorld(uint64_t seed, const GenParams& params);
    
    // ... other Feature 4/5/6 APIs (Export, Import, GetProgress, Cancel)
};

} // namespace CoW::WorldGeneration
```

**Retry is internal:** `GenerateWorld()` returns final result (success or failure), not per-attempt results. Feature 6 (Debug Host Integration) displays retry telemetry via `GetProgress()` or event logs, not via retry-specific API.

### Events

```cpp
// Published on successful generation (GDD Phase 7)
struct WorldGenerationComplete {
    WorldStructure world;
    uint64_t seed;
    uint32_t retryAttempts;  // 0 if first attempt succeeded, >0 if retries were needed
};

// Published per retry attempt (optional, for F-1 debug host live telemetry)
struct WorldGenerationRetry {
    uint32_t attemptNumber;  // 1-indexed (attempt 1, 2, 3, ...)
    ValidationResult lastValidation;  // Why previous attempt failed
};
```

**Rationale for `WorldGenerationRetry` event:** Enables F-1 debug host to display live retry progress ("Attempt 2/10: Hearth↔Nexus reachability failed"). Optional — can be deferred to Feature 6 if not needed for S-1 MVP.

## Dependencies

### Upstream Dependencies

| System/Feature | Why needed | Status |
|----------------|-----------|---------|
| **Feature 1 (Placeholder Generation Algorithm)** | Generates world per retry attempt | Draft (being specced) |
| **Feature 2 (Reachability Validator)** | Validates world per retry attempt | Draft (being specced) |
| **F-2 Core Runtime** | RandomStreamManager for deterministic sub-seed RNG | Approved (backlog) |

### Dia Engine Dependencies

- **DiaCore** (`Result<T>` for success/failure return, logging via `DIA_LOG_*`)
- **DiaSimTime** (RandomStreamManager for sub-seed RNG, though sub-seed derivation itself is just arithmetic)

## Acceptance Criteria

### AC1 — Deterministic Retry Sequence
Generate world with outer seed that triggers retries (hand-crafted or found empirically). Record retry count and final world. Regenerate with same outer seed and `GenParams`. **Pass:** Same retry count, same final world (byte-for-byte identical, per Feature 1 AC1), every run. Test on ≥3 seeds that trigger retries.

### AC2 — Bounded Retry
Generate world with `maxRetryAttempts = 5` (arbitrary low bound) and outer seed engineered to never satisfy guarantees (e.g., impossible parameters like 1-cell grid with 10 Hearth Sites). **Pass:** Generation returns Failure after exactly 5 attempts, does not retry forever.

### AC3 — Success Rate (≥95%)
Generate worlds across 100 random outer seeds with reasonable `GenParams` (Standard 80×52 grid, typical site counts, `maxRetryAttempts = 10` or tuned default). Record how many succeed vs. fallthrough to Failure. **Pass:** ≥95 succeed (GDD Test 1a target). If <95%, either increase `maxRetryAttempts` default or improve Feature 1 algorithm.

### AC4 — Retry Telemetry
Generate world that triggers ≥1 retry. Capture logs. **Pass:** Log messages include attempt count and validation diagnostics per attempt (e.g., "Attempt 1 failed: Hearth↔Nexus reachability", "Attempt 2 succeeded"). Proves telemetry is present for tuning/debugging.

### AC5 — Sub-Seed Distinctness
Generate 10 sub-seeds from same outer seed (attempts 0-9). **Pass:** All 10 sub-seeds are distinct (no collisions). Proves sub-seed derivation doesn't trivially repeat.

### AC6 — Retry Doesn't Re-Validate on Success
Mock Feature 2 (Reachability Validator) to count validation calls. Generate world that succeeds on first attempt. **Pass:** Validator called exactly once (no redundant validation after success). Proves retry loop exits immediately on success.

## Inherited Binding Decisions

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-004 | No STL containers in public APIs | `Result<WorldStructure>` return type (Dia's `Result<T>` already compliant); `WorldGenerationRetry` event uses `ValidationResult` (which uses `DynamicArrayC` per Feature 2) |
| PD-007 | C++20 is required | Retry loop can use C++20 features (e.g., ranges for cleaner loop syntax if desired) |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-002 | Classify cross-cutting needs | Retry mechanism is **Feature-local** initially (lives in `WorldGenerationModule::GenerateWorld()`); if other CoW systems need deterministic retry, extract to **CoW-Shared** utility. Not Dia (too CoW-specific: uses CoW's three hard guarantees), not Baseline (no other game needs CoW's retry logic) |
| AD-003 | GDD owns design intent | GDD defines ≥95% success rate target (§7), deterministic retry sequence (decision 2, §4), and bounded attempt count (decision 5). This spec defines implementation (sub-seed derivation, loop structure, telemetry) |

### From System (WorldGenerationPipeline)

| ID | Decision | How it applies |
|----|----------|----------------|
| N/A | No system-level binding decisions in parent spec | System spec has Open Design Questions but no SD- binding decisions |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | What is the exact default value for `maxRetryAttempts`? | Affects AC3 success rate and Feature 5 timeout interaction; does not block implementation (use arbitrary default, tune during S-1 challenge pass) | GDD open question 1 defers this; start with placeholder default (e.g., 10 attempts), tune empirically via AC3 to hit ≥95% success rate |
| 2 | Should retry publish per-attempt events (`WorldGenerationRetry`) or only log? | Affects Feature 6 (Debug Host Integration) — events enable live UI updates ("Attempt 2/10 in progress"); logs are post-hoc only | Decide during Feature 6 spec (can add events later without breaking Feature 3 implementation) |
| 3 | Should sub-seed derivation use additive (`outer + attempt`) or hash-based (`CRC64(outer, attempt)`)? | Affects determinism guarantees if additive has edge-case collisions (e.g., integer overflow) | Start with additive (simpler, faster); switch to hash-based if AC5 shows collision issues or if better bit mixing needed for RNG quality |

## Status

`Approved` — 2026-10-11

---

## Implementation Notes

When implementation begins (after approval), create a plan file `retry-mechanism.plan.md` alongside this spec. The plan must include an **Implementation Patterns** section documenting:

- Retry loop structure (location in `WorldGenerationModule::GenerateWorld()`, state tracking)
- Sub-seed derivation formula (additive vs. hash-based, overflow handling)
- Telemetry strategy (log messages per attempt, success/failure paths, retry-count reporting)
- Early-exit optimization (how to skip remaining retries on success, ensure validator not called redundantly)
- Integration with Feature 5 (Timeout and Failure Handling) — how timeout interacts with retry loop (timeout can interrupt mid-retry)

These patterns are validated against actual implementation before tasks are marked Done.
