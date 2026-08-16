# Feature Spec: AICallout Test Stage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

## Problem Statement

DiaAICallout's unit tests exercise each API call in isolation with a synthetic registry and instant tick, but have never been run under real SimPU frame timing with concurrent emitters and claimers. This stage validates the full callout lifecycle — emit → query → claim → travel → release — across a two-faction scenario with wandering emitters, and exercises the `DiaAICalloutVisualDebugger` IDebugDomain in real time so the callout radius overlay and panel are visually confirmed.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine features exercised | `DiaAICallout`: `Emit`, `Query`, `Claim`, `Release`, `Update` (TTL tick), **faction filter** (blue vs red factions with guaranteed no cross-faction claims), test utilities. `DiaAICalloutVisualDebugger`: `CalloutRegistryDebugger` (IDebugDomain), `CalloutRadiiDrawer` (faction-coloured rings: pulsing=unclaimed, solid bright=claimed, white=expiring). |
| T2 | Scene setup in DoStart | 6 wandering emitters — 3 blue (E0–E2) starting on left arc, 3 red (E3–E5) starting on right arc; all wander within radius 10 at 1 m/s using a deterministic direction table (8 pre-computed angles per emitter, seeded by index). 6 relay responders — 3 blue (R0–R2) near origin left, 3 red (R3–R5) near origin right. Each responder queries only same-faction callouts. `CalloutRegistry` owned by module; `CalloutRegistryDebugger` registered with `DiaDebugDomainRegistry`. |
| T3 | Checkpoints and success conditions | `aicallout.first_emit` — first callout emitted. `aicallout.first_claim` — first claim succeeds. `aicallout.first_release` — first release after relay completes. `aicallout.first_ttl_expiry` — any callout expires via TTL (guaranteed by construction: when all 3 same-faction responders are mid-travel, the next emit expires before a free responder returns). `aicallout.ten_claims` — cumulative claims ≥ 10. Cross-faction violations tracked via metric (expected: 0). |
| T4 | Metrics emitted | `cluichetest.aicallout.total_emitted`, `cluichetest.aicallout.total_claimed`, `cluichetest.aicallout.total_expired`, `cluichetest.aicallout.total_released`, `cluichetest.aicallout.live_count_peak`, `cluichetest.aicallout.cross_faction_violations` |
| T5 | Processing Unit | SimPU — game/coordination logic on sim thread per platform rule |
| T6 | Assets needed | None — all positions and timings are constants in code |
| T7 | Gap vs unit tests | Unit tests call APIs directly with synthetic inputs and instant ticks; they never exercise: (a) concurrent emitters emitting while responders are mid-travel, (b) TTL expiry under real `dt` accumulation with emitters in motion, (c) multiple same-faction responders racing to claim the same callout (only one wins), (d) release returning a callout to the unclaimed pool mid-frame, (e) **faction filter rejecting cross-faction Query results** under real frame timing. |
| T8 | Determinism constraints | Fixed-timestep SimPU (30 Hz). Emitter wander uses a per-emitter pre-computed direction table (8 angles, deterministic by index). Responder movement speed constant (2 m/s). No randomness. TTL expiry guaranteed by construction: once all 3 same-faction responders are traveling/relaying simultaneously, the next emit expires within 6s. |
| T9 | Expected frame budget | ~900 frames (30s at 30Hz): first emit at ~5s (150f); first claim shortly after; first TTL expiry by ~20s; ten cumulative claims by ~25s. Budget: 1200 frames (40s). |
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
| AC-C4 | At least one callout expires via TTL — guaranteed by construction when all same-faction responders are occupied | `aicallout.first_ttl_expiry` checkpoint passes |
| AC-C5 | Total claims reach at least 10 over the run — proves sustained coordination cycle | `aicallout.ten_claims` checkpoint passes |
| AC-C6 | `CalloutRegistryDebugger` IDebugDomain is registered and toggling ~ shows the callout panel with a live callout table (kind, position, radius, TTL, faction, claimed) | Manual verification during `dia run cluichetest` |
| AC-C7 | `CalloutRadiiDrawer` draws faction-coloured rings: blue for blue callouts, red for red callouts; pulsing when unclaimed, solid bright when claimed, white when expiring (TTL<2s) | Visual inspection during manual run |
| AC-C8 | Stage completes within 1200 frames (40s at 30 Hz) | Orchestrator timeout |
| AC-C9 | Repeated run (Boot → AICalloutTestStage → Boot → AICalloutTestStage) produces identical `total_emitted` and `total_claimed` metrics | Determinism — AC-S7 |
| AC-C10 | `cross_faction_violations` metric is 0 — no responder ever successfully claims a callout of the wrong faction | Metric assertion in pytest |

## Design

### World Layout

```
World bounds: (−20, −20) → (20, 20)

Blue emitters (blue filled diamond, wander within r=10, start positions):
  E0: (−5.0,  4.0)
  E1: (−7.0, −2.0)
  E2: (−3.0, −6.0)

Red emitters (red filled diamond, wander within r=10, start positions):
  E3: ( 5.0,  4.0)
  E4: ( 7.0, −2.0)
  E5: ( 3.0, −6.0)

Blue responders (blue filled circle, start near origin left):
  R0: (−1.0,  1.0)
  R1: (−1.5, −0.5)
  R2: (−0.5, −1.5)

Red responders (red filled circle, start near origin right):
  R3: ( 1.0,  1.0)
  R4: ( 1.5, −0.5)
  R5: ( 0.5, −1.5)

Visual legend:
  Blue emitter diamond:        blue filled diamond
  Red emitter diamond:         red filled diamond
  Blue responder circle:       blue filled circle, directional arrow overlay
  Red responder circle:        red filled circle, directional arrow overlay
  Unclaimed callout ring:      faction-coloured pulsing ring (radius lerps 0.9→1.1× callout.radius, period 2s)
  Expiring ring (TTL<2s):      white ring (faction-neutral warning)
  Claimed callout ring:        faction-coloured bright solid ring
  Relay event flash:           1-frame white ring on release

ImGui sidebar:
  Live callout count (GetLiveCount())
  Blue: emitted / claimed / expired / released
  Red:  emitted / claimed / expired / released
  Cross-faction violations: 0
  Per-responder state (Idle | Traveling | Relaying) + target position
  Frame count
```

### Callout Parameters

```
kind:    StringCRC{"distress_signal"}
faction: emitter's own faction (StringCRC{"faction_blue"} or StringCRC{"faction_red"})
radius:  8.0f         (world units)
ttl:     6.0f         (seconds)
payload: empty

Emitter emit interval:  5.0s (float accumulator; emits only when no live callout from this emitter)
Emitter wander speed:   1.0 m/s (slower than responders — responders can catch up)
Emitter wander bound:   radius 10 from origin

Responder query faction: same as responder's own faction
Responder query radius:  12.0f (wide enough to see all same-faction emitters)
```

### Agent Structures

```cpp
struct EmitterAgent
{
    Dia::Maths::Vector2D position;
    Dia::Maths::Vector2D wanderTarget;
    Dia::Core::StringCRC faction;           // faction_blue or faction_red
    float                emitTimer = 0.0f;
    Dia::AICallout::CalloutHandle activeHandle;
};

struct RelayResponder
{
    enum class State { Idle, Traveling, Relaying };

    Dia::Maths::Vector2D position;
    Dia::Maths::Vector2D wanderTarget;      // Idle drift target
    Dia::Core::StringCRC faction;           // faction_blue or faction_red
    float                relayTimer = 0.0f;

    State state = State::Idle;
    Dia::Core::StringCRC responderId;       // e.g. StringCRC{"relay_0"}

    Dia::AICallout::CalloutHandle claimedHandle;
    Dia::Maths::Vector2D targetPosition;
};
```

### DoUpdate Frame Loop

```
Each frame:
  1. mRegistry.Update(dt)           — tick TTLs, expire stale callouts
  2. For each EmitterAgent:
     a. Move toward wanderTarget at 1 m/s; on arrival pick next angle from
        kEmitterWanderDirs[emitterIdx] (8 pre-computed angles, seeded by index;
        clamp new target to radius 10 from origin)
     b. emitTimer += dt
     c. If emitTimer >= 5.0s AND !activeHandle.IsValid():
          activeHandle = mRegistry.Emit({kind, position, radius, faction=emitter.faction, ttl})
          ++mTotalEmitted; emitTimer = 0
     d. If activeHandle was valid last frame but IsValid() returns false: ++mTotalExpired
     e. Update live count peak
  3. For each RelayResponder:
     a. Idle: query registry with {kind, origin=position, radius=12.0f, faction=responder.faction}
              find nearest unclaimed handle (by distance to callout.position)
              attempt Claim — if true → switch to Traveling; ++mTotalClaimed
              else: move toward wanderTarget at 2 m/s (kResponderWanderDirs, same pattern)
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

TTL expiry count (`mTotalExpired`) is derived by tracking `activeHandle.IsValid()` transitions per emitter each frame.

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
        "DiaAICallout e2e: two-faction wandering-emitter emit/claim/release/TTL lifecycle with visual debugger overlay";

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
    void DrawDebugOverlay();

    Dia::AICallout::CalloutRegistry mRegistry;

    static constexpr unsigned int kEmitterCount   = 6;
    static constexpr unsigned int kResponderCount = 6;

    EmitterAgent    mEmitters[kEmitterCount];
    RelayResponder  mResponders[kResponderCount];
    float           mPulseTimer = 0.0f;

    int          mTotalEmitted           = 0;
    int          mTotalClaimed           = 0;
    int          mTotalExpired           = 0;
    int          mTotalReleased          = 0;
    int          mLiveCountPeak          = 0;
    int          mCrossFactionViolations = 0;
    unsigned int mFrameCount             = 0;

    Dia::Observation::Metric::Gauge* mMetricEmitted      = nullptr;
    Dia::Observation::Metric::Gauge* mMetricClaimed      = nullptr;
    Dia::Observation::Metric::Gauge* mMetricExpired      = nullptr;
    Dia::Observation::Metric::Gauge* mMetricReleased     = nullptr;
    Dia::Observation::Metric::Gauge* mMetricPeak         = nullptr;
    Dia::Observation::Metric::Gauge* mMetricCrossFaction = nullptr;

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

    r = dia_client.poll_checkpoint("aicallout.first_release", timeout_s=15.0)
    assert r["passed"], f"No release completed: {r['message']}"

    r = dia_client.poll_checkpoint("aicallout.first_ttl_expiry", timeout_s=25.0)
    assert r["passed"], f"No callout expired via TTL: {r['message']}"

    r = dia_client.poll_checkpoint("aicallout.ten_claims", timeout_s=35.0)
    assert r["passed"], f"Did not reach 10 cumulative claims: {r['message']}"

    metrics = dia_client.query_metrics("cluichetest.aicallout.*")
    assert metrics["cluichetest.aicallout.total_emitted"] >= 10
    assert metrics["cluichetest.aicallout.total_claimed"] >= 10
    assert metrics["cluichetest.aicallout.total_expired"] >= 1
    assert metrics["cluichetest.aicallout.total_released"] >= 1
    assert metrics["cluichetest.aicallout.live_count_peak"] >= 1
    assert metrics["cluichetest.aicallout.cross_faction_violations"] == 0

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
| 2 | `EmitterAgent` struct + 6 start positions (3 blue left arc, 3 red right arc); wander movement at 1 m/s using `kEmitterWanderDirs[emitterIdx]` (8 pre-computed angles per emitter, seeded by index, clamped to r=10); emit every 5s when no live callout, callout faction = emitter.faction; derive `mTotalExpired` from handle validity transitions | `aicallout.first_emit` passes; `mTotalExpired` increments during run | Todo | sonnet | Blue starts: (−5,4),(−7,−2),(−3,−6). Red starts: (5,4),(7,−2),(3,−6). Wander bound r=10. |
| 3 | `RelayResponder` struct + 3-state loop; 6 responders (3 blue R0–R2, 3 red R3–R5); Query passes responder.faction (no cross-faction results); claim nearest same-faction callout by distance; wander when Idle using `kResponderWanderDirs`; `mTotalClaimed`, `mTotalReleased` counters | `aicallout.first_claim`, `aicallout.first_release` checkpoints pass; `cross_faction_violations` stays 0 | Todo | sonnet | Query radius 12; move speed 2 m/s |
| 4 | `OnUpdate` frame loop: `mRegistry.Update(dt)` → tick emitters (wander + emit) → tick responders → update live count peak → increment frame count | All 5 checkpoints pass; `aicallout.ten_claims` passes by ~900f | Todo | sonnet | |
| 5 | Register 5 checkpoints in `OnStart`; construct `CalloutRegistryDebugger` and register with `DiaDebugDomainRegistry` (`#ifdef DIA_DEBUG`) | Panel visible on `~` keypress; live callout table shows kind/faction/position/TTL/claimed | Todo | sonnet | |
| 6 | `DrawDebugOverlay()` via `IDebugDraw`: blue/red faction-coloured emitter diamonds + responder circles with direction arrows; faction-coloured pulsing rings (unclaimed), solid bright rings (claimed), white rings (TTL<2s), 1-frame white flash on release; ImGui sidebar (live count, per-faction totals, cross-faction violations, per-responder state) | Visually correct on manual run; faction colours consistent; ring state transitions visible | Todo | sonnet | Pulse: `radius * (0.9f + 0.1f * sinf(mPulseTimer * π))`, `mPulseTimer += dt / 2.0f` |
| 7 | Register 6 metrics in `OnStart` (add `cross_faction_violations`); emit updated values in `OnUpdate`; `OnStop`: clear registry, reset all counters + handles | Metrics readable from orchestrator; `cross_faction_violations == 0`; second run matches `total_emitted` | Todo | haiku | |
| 8 | Write pytest scenario `test_aicallout_e2e.py` + add to `default.json` | All 5 checkpoints pass; metric assertions hold including `cross_faction_violations == 0`; determinism second-pass passes | Todo | sonnet | |
| 9 | Verify: `dia run cluichetest` → navigate to AICalloutTestStage; observe faction-coloured emitters wandering, responders chasing, ring colour transitions; all checkpoints pass | E2E gate | Todo | sonnet | |

## Dependencies

- **DiaAICallout** ✅ — `CalloutRegistry`, `CalloutHandle`, `Callout`, test utilities
- **DiaAICalloutVisualDebugger** ✅ — `CalloutRegistryDebugger`, `CalloutRadiiDrawer`
- **Test Stage Infrastructure** — `TestStageModuleBase`, `AutomationService`, checkpoint contract
- **DiaDebugDomainRegistry** — confirm registration API (injected via module ref or static accessor) before Task 5

## Open Design Questions

1. **DiaDebugDomainRegistry access** — does the stage access `DiaDebugDomainRegistry` via a `ModuleRef`, a service singleton, or a different injection pattern? Confirm before Task 5 by looking at how `DebugGalleryTestStageModule` registers its domains.

2. **Wander direction tables** — `kEmitterWanderDirs` and `kResponderWanderDirs` need 8 angles per entity (deterministic, no runtime randomness). Confirm format (constexpr float[entityCount][8] in radians) and whether they live inline in the .cpp or in a separate constants header before Task 2.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | `kTypeId`, checkpoint names, responder IDs, callout kind, faction IDs all `StringCRC` |
| PD-004 | No STL in public APIs | Module's internal arrays are plain C arrays; `CalloutRegistry::Query` out-param is `DynamicArrayC` |
| PD-006 | VS project files source of truth | Task 1 scaffold + explicit vcxproj entries |
| PD-007 | C++20 required | `constexpr StringCRC` |
| PD-010 | `.diastage` for stages | Stage declared in `.diastage` |
| AD-001 (CT) | Three PUs | Module lives on SimPU |
| SD-TS-001 | One manifest stage per feature | One stage: AICalloutTestStage |
| SD-TS-002 | Checkpoints in OnStart, auto-clear on stop | `RegisterCheckpoints` called in `OnStart` |
| SD-TS-003 | Metrics for threshold assertions | 6 metrics registered; pytest asserts final values |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in manifest |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec |

## Status

**Status:** Approved
**Plan:** _(create alongside implementation)_
