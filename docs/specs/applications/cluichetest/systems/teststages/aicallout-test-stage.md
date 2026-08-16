# Feature Spec: AICallout Test Stage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

## Problem Statement

DiaAICallout's unit tests exercise each API call in isolation with a synthetic registry and instant tick, but have never been run under real SimPU frame timing with concurrent emitters and claimers. This stage validates the full callout lifecycle — emit → query → claim → travel → release — across a multi-entity scenario, and exercises the `DiaAICalloutVisualDebugger` IDebugDomain in real time so the callout radius overlay and panel are visually confirmed.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine features exercised | `DiaAICallout`: `Emit`, `Query`, `Claim`, `Release`, `Update` (TTL tick), faction filter, test utilities. `DiaAICalloutVisualDebugger`: `CalloutRegistryDebugger` (IDebugDomain), `CalloutRadiiDrawer` (green ring = unclaimed, red ring = claimed). |
| T2 | Scene setup in DoStart | 6 emitters — 5 arranged on an inner ring (radius 6, 72° apart) + 1 dead-zone emitter at (16, 0) far outside query range. 4 relay responders starting near origin. `CalloutRegistry` owned by module; `CalloutRegistryDebugger` registered with `DiaDebugDomainRegistry`. |
| T3 | Checkpoints and success conditions | `aicallout.first_emit` — first callout emitted. `aicallout.first_claim` — first claim succeeds. `aicallout.first_release` — first release after relay completes. `aicallout.first_ttl_expiry` — dead-zone emitter's callout expires (guaranteed: unreachable). `aicallout.ten_claims` — cumulative claims ≥ 10. |
| T4 | Metrics emitted | `cluichetest.aicallout.total_emitted`, `cluichetest.aicallout.total_claimed`, `cluichetest.aicallout.total_expired`, `cluichetest.aicallout.total_released`, `cluichetest.aicallout.live_count_peak` |
| T5 | Processing Unit | SimPU — game/coordination logic on sim thread per platform rule |
| T6 | Assets needed | None — all positions and timings are constants in code |
| T7 | Gap vs unit tests | Unit tests call APIs directly with synthetic inputs and instant ticks; they never exercise: (a) concurrent emitters emitting while responders are mid-travel, (b) TTL expiry under real `dt` accumulation, (c) multiple responders racing to claim the same callout (only one wins), (d) release returning a callout to the unclaimed pool mid-frame. |
| T8 | Determinism constraints | Fixed-timestep SimPU (30 Hz). Emitter timers are simple float accumulators; responder movement speed is constant (2 m/s). No randomness. Dead-zone emitter always expires because it is always out of query range. |
| T9 | Expected frame budget | ~900 frames (30s at 30Hz): first emit at ~5s (150f); first claim shortly after; first TTL expiry at ~13s (6s TTL, emits at ~7s); ten cumulative claims by ~20s. Budget: 1200 frames (40s). |
| T10 | Dependencies on other modules | `DiaAICallout` (CalloutRegistry), `DiaAICalloutVisualDebugger` (CalloutRegistryDebugger, IDebugDomain), `AutomationModule` (checkpoints), `DiaDebugDomainRegistry` (visual panel registration), `DiaObservation::Metric` (metrics). No DiaRenderModule dependency — visuals are canvas/ImGui only. |

## Acceptance Criteria

### Shared ACs (inherited from Infrastructure spec)

This stage satisfies AC-S1 through AC-S9 as defined in the [Infrastructure spec](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-C1 | First callout emitted within 6 seconds of stage start | `aicallout.first_emit` checkpoint passes |
| AC-C2 | At least one claim succeeds (Claim returns true) | `aicallout.first_claim` checkpoint passes |
| AC-C3 | At least one callout released back to unclaimed pool after relay completes; subsequent Query returns it as unclaimed | `aicallout.first_release` checkpoint passes; live count reflects recycle |
| AC-C4 | Dead-zone emitter's callout (at position (16,0), out of all query radii) expires via TTL — proves Update ticks correctly under real dt | `aicallout.first_ttl_expiry` checkpoint passes |
| AC-C5 | Total claims reach at least 10 over the run — proves sustained coordination cycle | `aicallout.ten_claims` checkpoint passes |
| AC-C6 | `CalloutRegistryDebugger` IDebugDomain is registered and toggling ~ shows the callout panel with a live callout table (kind, position, radius, TTL, claimed) | Manual verification during `dia run cluichetest` |
| AC-C7 | `CalloutRadiiDrawer` draws green rings for unclaimed callouts and red rings for claimed callouts, correctly updating each frame | Visual inspection during manual run |
| AC-C8 | Stage completes within 1200 frames (40s at 30 Hz) | Orchestrator timeout |
| AC-C9 | Repeated run (Boot → AICalloutTestStage → Boot → AICalloutTestStage) produces identical `total_emitted` and `total_claimed` metrics | Determinism — AC-S7 |

## Design

### World Layout

```
World bounds: (−20, −20) → (20, 20)

Inner emitters (cyan diamonds, radius 6 from origin, 72° apart):
  E0: ( 6.0,  0.0)
  E1: ( 1.9,  5.7)
  E2: (−4.9,  3.5)
  E3: (−4.9, −3.5)
  E4: ( 1.9, −5.7)

Dead-zone emitter (dark grey diamond, permanently out of query range):
  E5: (16.0,  0.0)

Relay responders (white circles, start near origin):
  R0: ( 0.5,  0.5)
  R1: (−0.5,  0.5)
  R2: ( 0.5, −0.5)
  R3: (−0.5, −0.5)

Visual legend:
  Emitters (inner):       cyan filled diamond
  Dead-zone emitter:      dark grey diamond
  Responders:             white filled circle, directional arrow overlay
  Unclaimed callout ring: pulsing green ring (radius lerps 0.9→1.1× callout.radius, period 2s)
  Expiring ring (TTL<2s): orange ring
  Claimed callout ring:   solid red ring
  Relay event flash:      1-frame yellow ring on release

ImGui sidebar:
  Live callout count (GetLiveCount())
  Cumulative: emitted / claimed / expired / released
  Per-responder state (Idle | Traveling | Relaying) + target position
  Frame count
```

### Callout Parameters

```
kind:    StringCRC{"distress_signal"}
faction: kInvalidCRC  (any responder may claim)
radius:  8.0f         (world units — query range for inner emitters covers the whole inner ring)
ttl:     6.0f         (seconds; dead-zone callout always expires before a responder can arrive)
payload: empty

Emitter emit interval:  5.0s (float accumulator; emits only when no live callout from this emitter)
```

### Agent Structures

```cpp
struct EmitterAgent
{
    Dia::Maths::Vector2D position;
    float                emitTimer = 0.0f;   // counts up to kEmitInterval
    Dia::AICallout::CalloutHandle activeHandle; // invalid when nothing live
    bool isDeadZone = false;
};

struct RelayResponder
{
    enum class State { Idle, Traveling, Relaying };

    Dia::Maths::Vector2D position;
    Dia::Maths::Vector2D wanderTarget;    // Idle drift target
    float                relayTimer = 0.0f;  // counts up in Relaying state

    State state = State::Idle;
    Dia::Core::StringCRC responderId;    // e.g. StringCRC{"relay_0"}

    Dia::AICallout::CalloutHandle claimedHandle;  // valid while Traveling/Relaying
    Dia::Maths::Vector2D targetPosition;          // callout position while Traveling
};
```

### DoUpdate Frame Loop

```
Each frame:
  1. mRegistry.Update(dt)           — tick TTLs, expire stale callouts
  2. For each EmitterAgent:
     a. emitTimer += dt
     b. If emitTimer >= 5.0s AND !activeHandle.IsValid():
          activeHandle = mRegistry.Emit({kind, position, radius, faction, ttl})
          ++mTotalEmitted; emitTimer = 0
     c. Update live count peak
  3. For each RelayResponder:
     a. Idle: query registry with {kind, origin=position, radius=10.0f, faction=kInvalidCRC}
              find nearest unclaimed handle (by distance to callout.position)
              attempt Claim — if true → switch to Traveling; log to mTotalClaimed
     b. Traveling: move toward targetPosition at 2 m/s
              on arrival (distance < 0.5): switch to Relaying, relayTimer=0
     c. Relaying: relayTimer += dt
              on relayTimer >= 2.0s: mRegistry.Release(claimedHandle, responderId)
              ++mTotalReleased; switch to Idle
  4. Check checkpoint conditions, emit metric snapshots
  5. ++mFrameCount
```

### Checkpoint Registration

```cpp
automation->RegisterCheckpoint(this, StringCRC("aicallout.first_emit"),
    [this]() { return { mTotalEmitted >= 1, ... }; });

automation->RegisterCheckpoint(this, StringCRC("aicallout.first_claim"),
    [this]() { return { mTotalClaimed >= 1, ... }; });

automation->RegisterCheckpoint(this, StringCRC("aicallout.first_release"),
    [this]() { return { mTotalReleased >= 1, ... }; });

automation->RegisterCheckpoint(this, StringCRC("aicallout.first_ttl_expiry"),
    [this]() { return { mTotalExpired >= 1, ... }; });

automation->RegisterCheckpoint(this, StringCRC("aicallout.ten_claims"),
    [this]() { return { mTotalClaimed >= 10, ... }; });
```

TTL expiry count (`mTotalExpired`) is derived by checking `GetLiveCount()` drop alongside a `mLastLiveCount` snapshot — or by wrapping `CalloutHandle::IsValid()` state transitions on emitters each frame.

### Visual Debugger Panel Wiring

`CalloutRegistryDebugger` (from `DiaAICalloutVisualDebugger`) is constructed with a `const CalloutRegistry&` reference and registered with `DiaDebugDomainRegistry` in `OnStart`. Pressing `~` during the run opens the debug panel showing the live callout table and the radii overlay.

```cpp
// OnStart
mCalloutDebugger = std::make_unique<Dia::AICallout::CalloutRegistryDebugger>(mRegistry);
if (auto* registry = GetDebugDomainRegistry())
    registry->Register(*mCalloutDebugger);
```

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/AICalloutTestStageModule.h
namespace CluicheTest {

class AICalloutTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription =
        "DiaAICallout e2e: emit/claim/release/TTL lifecycle with visual debugger overlay";

    explicit AICalloutTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int         GetBudgetFrames() const override { return 1200; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;

    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void TickEmitters(float dt);
    void TickResponders(float dt);
    void UpdateExpiredCount();
    void DrawDebugOverlay();

    Dia::AICallout::CalloutRegistry mRegistry;

    static constexpr unsigned int kEmitterCount  = 6;
    static constexpr unsigned int kResponderCount = 4;

    EmitterAgent    mEmitters[kEmitterCount];
    RelayResponder  mResponders[kResponderCount];
    float           mPulseTimer = 0.0f;  // drives ring animation

    int          mTotalEmitted  = 0;
    int          mTotalClaimed  = 0;
    int          mTotalExpired  = 0;
    int          mTotalReleased = 0;
    int          mLiveCountPeak = 0;
    unsigned int mFrameCount    = 0;

    Dia::Observation::Metric::Gauge* mMetricEmitted  = nullptr;
    Dia::Observation::Metric::Gauge* mMetricClaimed  = nullptr;
    Dia::Observation::Metric::Gauge* mMetricExpired  = nullptr;
    Dia::Observation::Metric::Gauge* mMetricReleased = nullptr;
    Dia::Observation::Metric::Gauge* mMetricPeak     = nullptr;

#ifdef DIA_DEBUG
    std::unique_ptr<Dia::AICallout::CalloutRegistryDebugger> mCalloutDebugger;
#endif
};

} // namespace CluicheTest
DIA_MODULE(AICalloutTestStageModule);
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/aicallout/test_aicallout_e2e.py

def test_aicallout_lifecycle(dia_client):
    dia_client.navigate_to("AICalloutTestStage")

    r = dia_client.poll_checkpoint("aicallout.first_emit", timeout_s=8.0)
    assert r["passed"], f"No callout emitted: {r['message']}"

    r = dia_client.poll_checkpoint("aicallout.first_claim", timeout_s=10.0)
    assert r["passed"], f"No claim succeeded: {r['message']}"

    r = dia_client.poll_checkpoint("aicallout.first_ttl_expiry", timeout_s=15.0)
    assert r["passed"], f"Dead-zone callout did not expire: {r['message']}"

    r = dia_client.poll_checkpoint("aicallout.first_release", timeout_s=15.0)
    assert r["passed"], f"No release completed: {r['message']}"

    r = dia_client.poll_checkpoint("aicallout.ten_claims", timeout_s=35.0)
    assert r["passed"], f"Did not reach 10 cumulative claims: {r['message']}"

    metrics = dia_client.query_metrics("cluichetest.aicallout.*")
    assert metrics["cluichetest.aicallout.total_emitted"] >= 10
    assert metrics["cluichetest.aicallout.total_claimed"] >= 10
    assert metrics["cluichetest.aicallout.total_expired"] >= 1
    assert metrics["cluichetest.aicallout.total_released"] >= 1
    assert metrics["cluichetest.aicallout.live_count_peak"] >= 1

    # Determinism: second run
    first_emitted = metrics["cluichetest.aicallout.total_emitted"]
    dia_client.navigate_to("Boot")
    dia_client.navigate_to("AICalloutTestStage")
    dia_client.poll_checkpoint("aicallout.ten_claims", timeout_s=40.0)
    metrics2 = dia_client.query_metrics("cluichetest.aicallout.*")
    assert metrics2["cluichetest.aicallout.total_emitted"] == first_emitted

    dia_client.navigate_to("Boot")
```

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheTest/Modules/TestStages/AICalloutTestStageModule.h` | New — module header + EmitterAgent + RelayResponder structs |
| `Cluiche/CluicheTest/Modules/TestStages/AICalloutTestStageModule.cpp` | New — full implementation |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add ClCompile + ClInclude entries |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |
| `Cluiche/Assets/CluicheTest/Stages/AICalloutTestStage/aicallout_test_stage.diastage` | New — stage declaration |
| `Cluiche/Assets/CluicheTest/Stages/AICalloutTestStage/misc/ApplicationFlow/aicallout_test_stage.diaapp` | New — stage app manifest |
| `Cluiche/Assets/CluicheTest/Global/misc/ApplicationFlow/cluiche_main.diaapp` | Add AICalloutTestStage entry + Boot↔stage transitions |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import for aicallout_test_stage.diastage |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Add stage + manifest catalogue entries |
| `Tools/orchestrator/scenarios/cluichetest/aicallout/test_aicallout_e2e.py` | New — pytest scenario |
| `Tools/orchestrator/plans/cluichetest/default.json` | Add scenario to plan |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `dia scaffold stage AICalloutTestStage` — 4 touch points | Stage appears in Boot menu | Todo | haiku | Exact command; generates stub module + manifests |
| 2 | `EmitterAgent` struct + 6 positions (5 inner ring + dead-zone at (16,0)); emitter tick loop: emit every 5s when no live callout, increment `mTotalEmitted`; derive `mTotalExpired` from handle validity transitions | `aicallout.first_emit` passes; dead-zone callout expires and `mTotalExpired` increments | Todo | sonnet | Inner ring positions: (6,0), (1.86,5.73), (−4.85,3.53), (−4.85,−3.53), (1.86,−5.73) |
| 3 | `RelayResponder` struct + 3-state loop (Idle: wander+query, Traveling: move toward target, Relaying: wait 2s then release); 4 responders; `mTotalClaimed`, `mTotalReleased` counters | `aicallout.first_claim`, `aicallout.first_release` checkpoints pass | Todo | sonnet | Query radius 10; move speed 2 m/s; claim nearest by distance; wander target = random point on unit circle × 4 (seeded, deterministic) |
| 4 | `OnUpdate` frame loop: `mRegistry.Update(dt)` → tick emitters → tick responders → update live count peak → increment frame count | All 5 checkpoints pass; `aicallout.ten_claims` passes by ~900f | Todo | sonnet | |
| 5 | Register 5 checkpoints in `OnStart`; construct `CalloutRegistryDebugger` and register with `DiaDebugDomainRegistry` (`#ifdef DIA_DEBUG`) | Panel visible on `~` keypress; live callout table shows kind/position/TTL/claimed | Todo | sonnet | |
| 6 | `DrawDebugOverlay()` via `IDebugDraw`: emitter diamonds (cyan/grey), responder circles with direction arrow, pulsing green/orange/red rings per callout, ImGui sidebar (live count, totals, per-responder state) | Visually correct on manual run; unclaimed=green, expiring=orange, claimed=red | Todo | sonnet | Pulse: `radius * (0.9f + 0.1f * sinf(mPulseTimer * π))`, `mPulseTimer += dt / 2.0f` |
| 7 | Register 5 metrics in `OnStart`; emit updated values in `OnUpdate`; `OnStop`: clear registry, reset all counters + handles | Metrics readable from orchestrator; second run matches `total_emitted` | Todo | haiku | |
| 8 | Write pytest scenario `test_aicallout_e2e.py` + add to `default.json` | All 5 checkpoints pass; metric assertions hold; determinism second-pass passes | Todo | sonnet | |
| 9 | Verify: `dia run cluichetest` → navigate to AICalloutTestStage; observe visual debugger panel + overlays; all checkpoints pass | E2E gate | Todo | sonnet | |

## Dependencies

- **DiaAICallout** ✅ — `CalloutRegistry`, `CalloutHandle`, `Callout`, test utilities
- **DiaAICalloutVisualDebugger** ✅ — `CalloutRegistryDebugger`, `CalloutRadiiDrawer`
- **Test Stage Infrastructure** — `TestStageModuleBase`, `AutomationService`, checkpoint contract
- **DiaDebugDomainRegistry** — confirm registration API (injected via module ref or static accessor) before Task 5

## Open Design Questions

1. **DiaDebugDomainRegistry access** — does the stage access `DiaDebugDomainRegistry` via a `ModuleRef`, a service singleton, or a different injection pattern? Confirm before Task 5 by looking at how `DebugGalleryTestStageModule` registers its domains.

2. **Wander target determinism** — `RelayResponder` uses a fixed-seed wander sequence to maintain determinism. If the wander direction is computed per-responder from a deterministic index sequence (e.g., `directions[responderIndex * frameCount % N]`), there is no randomness. Confirm the exact wander strategy before Task 3.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | `kTypeId`, checkpoint names, responder IDs, callout kind all `StringCRC` |
| PD-004 | No STL in public APIs | Module's internal `EmitterAgent[]` and `RelayResponder[]` are plain arrays; `CalloutRegistry::Query` out-param is `DynamicArrayC` |
| PD-006 | VS project files source of truth | Task 1 scaffold + explicit vcxproj entries |
| PD-007 | C++20 required | `constexpr StringCRC` |
| PD-010 | `.diastage` for stages | Stage declared in `.diastage` |
| AD-001 (CT) | Three PUs | Module lives on SimPU |
| SD-TS-001 | One manifest stage per feature | One stage: AICalloutTestStage |
| SD-TS-002 | Checkpoints in OnStart, auto-clear on stop | `RegisterCheckpoints` called in `OnStart` |
| SD-TS-003 | Metrics for threshold assertions | 5 metrics registered; pytest asserts final values |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in manifest |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec |

## Status

**Status:** Approved
**Plan:** _(create alongside implementation)_
