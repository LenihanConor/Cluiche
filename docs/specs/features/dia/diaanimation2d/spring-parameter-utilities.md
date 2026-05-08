# Feature Spec: Spring Parameter Utilities

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaanimation2d.md | **spring-parameter-utilities** |

**Status:** `Approved`

---

## Problem Statement

Artists and designers think about spring behavior in terms of oscillation frequency (Hz) and damping ratio (0=undamped, 1=critical), not raw stiffness constant k and damping coefficient d. Every team that uses spring chains will need to convert between these representations. Without an engine-provided utility, each caller reinvents the same math.

---

## Solution Overview

One free function in `Dia::Animation2D`:

```cpp
namespace Dia::Animation2D {
    // Convert artist-friendly spring parameters to internal constants.
    // frequency: oscillation speed in Hz (e.g. 2.0 = 2 cycles/sec)
    // dampingRatio: 0 = undamped, 1 = critically damped, >1 = overdamped
    // mass: virtual mass of the node (default 1.0)
    // Returns SpringNodeDef with computed stiffness, damping, default maxAngularVelocity.
    //
    // Formulas:
    //   k = (2pi * frequency)^2 * mass
    //   d = 2 * dampingRatio * (2pi * frequency) * mass
    //   maxAngularVelocity = SpringNodeDef default (20.0f)
    SpringNodeDef SpringParamsFromFrequency(float frequency, float dampingRatio, float mass = 1.0f);
}
```

### Key Behaviours

1. **Pure function** — no state, no side effects.
2. **DIA_ASSERT on invalid inputs**: frequency <= 0, dampingRatio < 0, mass <= 0 (all must be positive; dampingRatio must be non-negative).
3. **Returns a fully populated SpringNodeDef** with computed stiffness, damping, and maxAngularVelocity set to the SpringNodeDef default (20.0f).
4. **Common artist presets** (documented, not encoded):
   - Loose tail: ~1 Hz, 0.3 damping ratio — bouncy, slow settle
   - Stiff frill: ~4 Hz, 0.8 damping ratio — quick, minimal overshoot
   - Critical damp: any Hz, 1.0 damping ratio — returns to rest without oscillation

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaAnimation2D/SpringParamUtils.h` | Free function declaration |
| `Dia/DiaAnimation2D/SpringParamUtils.cpp` | Implementation |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | Correct stiffness (k) computation for known frequency/mass: k = (2pi * frequency)^2 * mass | Unit test: frequency=1 Hz, mass=1 yields k = 4pi^2 ~= 39.4784 |
| 2 | Correct damping (d) computation for known dampingRatio/frequency/mass: d = 2 * dampingRatio * (2pi * frequency) * mass | Unit test: frequency=1 Hz, dampingRatio=0.5, mass=1 yields d = 2pi ~= 6.2832 |
| 3 | mass=1 default works — calling with two arguments produces same result as explicit mass=1 | Unit test: compare SpringParamsFromFrequency(2.0f, 0.5f) with SpringParamsFromFrequency(2.0f, 0.5f, 1.0f) |
| 4 | DIA_ASSERT on zero/negative frequency | Unit test: frequency=0 and frequency=-1 trigger assertion |
| 5 | DIA_ASSERT on negative dampingRatio | Unit test: dampingRatio=-0.1 triggers assertion |
| 6 | DIA_ASSERT on zero/negative mass | Unit test: mass=0 and mass=-1 trigger assertion |
| 7 | maxAngularVelocity set to default 20.0f in returned SpringNodeDef | Unit test: verify result.maxAngularVelocity == 20.0f for any valid input |
| 8 | Critically damped (ratio=1.0) produces exact expected k and d values | Unit test: frequency=3 Hz, dampingRatio=1.0, mass=2 yields k = (6pi)^2 * 2 = 710.6118, d = 2 * 1.0 * 6pi * 2 = 75.3982 |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Implement `SpringParamUtils.h` / `SpringParamUtils.cpp` — free function, DIA_ASSERT guards, formula computation | - | Add to DiaAnimation2D.vcxproj when that project exists |
| 2 | Unit tests in `Cluiche/Tests/GoogleTests/Animation2D/` — all 8 acceptance criteria | 1 | Tests verify formula accuracy within floating-point tolerance (1e-4) |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-007 | Platform | C++20 required | Compliant — compiled under /std:c++20; no pre-C++20 features relied upon |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — function declared in Dia::Animation2D:: namespace |
| AND-010 | DiaAnimation2D | No STL in public APIs | Compliant — function signature uses only float primitives and returns SpringNodeDef (a Dia struct); no STL types |
| AND-029 | DiaAnimation2D | SpringParamsFromFrequency converts Hz/damping-ratio to k/d | Compliant — this feature directly implements AND-029; formulas match exactly |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | API | Should the function support computing frequency/dampingRatio back from k/d (inverse conversion)? | Not in v1. The inverse formulas are straightforward (frequency = sqrt(k/mass) / (2pi), dampingRatio = d / (2 * sqrt(k * mass))) but there is no use case yet — artists author in Hz/ratio space, the engine consumes k/d. If a debug inspector wants to display Hz/ratio from runtime k/d, add an inverse function as a separate feature spec. |
| 2 | Validation | Should overdamped (dampingRatio > 1) be warned about or just allowed? | Allowed silently. Overdamped springs are mathematically valid and physically meaningful — they return to rest without any oscillation, just slower than critically damped. Some use cases want this (heavy curtain, thick tentacle). No DIA_LOG_WARNING; the DIA_ASSERT only fires on negative dampingRatio. |
| 3 | API | Should there be named presets (e.g. SpringPreset::kLooseTail, SpringPreset::kStiffFrill)? | Not in this feature. Presets are game-specific tuning values, not engine math. Documenting common recipes (loose tail ~1 Hz/0.3, stiff frill ~4 Hz/0.8) in the header comment is sufficient. If preset demand grows, a separate preset feature spec can define them as constexpr SpringNodeDefs. |
| 4 | Numerical | What happens with very high frequency values — is there a practical maximum? | No hard clamp in the utility. Very high frequencies (e.g. 100 Hz) produce very high stiffness (k = (200pi)^2 ~= 394784), which is valid mathematically but may cause instability in SpringChain's semi-implicit Euler integrator even with sub-stepping. This is a SpringChain concern (AND-019 maxAngularVelocity clamp provides the safety net), not a SpringParamUtils concern. The utility is a pure math converter — it does not enforce simulation-stability limits. |
| 5 | Testing | Should golden-value tests use hardcoded expected values or compute expected values from the same formula? | Hardcoded expected values. Computing expected values from the formula in the test would be a tautology — it tests that the code matches itself, not that it is correct. Hardcoded values are computed independently (e.g. by hand or in a separate tool) and serve as a true oracle. Tolerance of 1e-4 accounts for float precision. |

---

## Open Questions

None — all resolved above.
