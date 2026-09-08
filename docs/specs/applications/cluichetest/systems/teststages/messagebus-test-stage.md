# Feature Spec: MessageBus Test Stage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

## Problem Statement

`DiaMessageBus` unit tests call `Bus::Post` / `Bus::Subscribe` in isolation with synthetic ticks. They never exercise BroadcastRouter fanning out to N real handlers at 30 Hz, EntityRouter delivering to a specific entity handle while others are skipped, Reaction-pass messages posted from a Primary handler, or an `IFlushAdapter` injecting events into the bus from outside. This stage validates all four paths simultaneously in a visually legible scene — and wires `MessageBusDebugDomain` so the routing graph and live ledger stats are inspectable in real time.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine features exercised | `DiaMessageBus`: `Bus::Post<T>`, `Bus::Broadcast<T>`, `Bus::Subscribe<T>`, `Bus::RegisterProducer<T>`, `BroadcastRouter` (fan-out), `EntityRouter` (targeted delivery), Reaction pass (chained post from Primary handler), `IFlushAdapter` (BurstAdapter injects `BurstEvent` at t=5s); `MessageBusDebugDomain`: Schema / Live / History tabs. |
| T2 | Scene setup in DoStart | 5 **Emitter** nodes (diamonds, wander at 0.8 m/s within r=12); 5 **Receiver** nodes (circles, wander at 0.6 m/s within r=12). Message types defined in stage: `NetworkPulseEvent` (broadcast, emitted every 2.0s by all Emitters), `DirectPingEvent` (entity-addressed, Emitter i pings Receiver i%5 every 3.0s; deterministic round-robin), `PongEvent` (Reaction pass — Receiver handler posts Pong back to a `BroadcastRouter` listener on Emitters), `BurstEvent` (BroadcastRouter, injected by `BurstAdapter` at frame 150 ≈ t=5s). `MessageBusDebugDomain` registered with `VisualDebuggerModule`. |
| T3 | Checkpoints and success conditions | `msgbus.first_pulse` — first `NetworkPulseEvent` delivered to ≥1 Receiver. `msgbus.first_ping` — first `DirectPingEvent` delivered to the correct Receiver. `msgbus.first_pong` — first `PongEvent` delivered via Reaction pass. `msgbus.twenty_pulses` — cumulative `NetworkPulseEvent` deliveries ≥ 20 (proves sustained broadcast). `msgbus.ten_pings` — cumulative `DirectPingEvent` deliveries ≥ 10 (proves sustained entity routing). |
| T4 | Metrics emitted | `cluichetest.msgbus.pulses_sent`, `cluichetest.msgbus.pings_sent`, `cluichetest.msgbus.pongs_received`, `cluichetest.msgbus.bursts`, `cluichetest.msgbus.dropped` |
| T5 | Processing Unit | SimPU — `MessageBusModule` lives on SimPU |
| T6 | Assets needed | None — positions, wander tables, and message types are all compile-time constants |
| T7 | Gap vs unit tests | Unit tests never exercise: (a) BroadcastRouter dispatching to 5 real handler callbacks at real frame rate, (b) EntityRouter skipping 4 Receivers and delivering to exactly 1 by handle, (c) Reaction-pass `PongEvent` posted from inside a Primary-pass `DirectPingEvent` handler (re-entrancy guard path), (d) `IFlushAdapter::Flush()` called in pre-Primary step injecting a `BurstEvent`, (e) `LedgerSnapshot` ring buffer filling over 30s with real tick data, (f) `MessageBusDebugDomain` reflecting live schema + stats in `GetJSONState()`. |
| T8 | Determinism constraints | Fixed-timestep SimPU (30 Hz). Emitter wander: `kEmitterWanderDirs[5][8]` (pre-computed, seeded by index), clamped to r=12. Receiver wander: `kReceiverWanderDirs[5][8]`. DirectPing target: `Receiver[emitterIdx % 5]` (round-robin). BurstEvent: `BurstAdapter` fires once at frame 150 (t=5.0s). No randomness. |
| T9 | Expected frame budget | ~900 frames (30s at 30Hz): `first_pulse` by ~2s (60f); `first_ping` by ~3s (90f); `first_pong` immediately after; `twenty_pulses` by ~10s (with 5 Emitters broadcasting every 2s → 2.5 pulses/s → 20 total by ~8s); `ten_pings` by ~9s. Budget: 1500 frames (50s). |
| T10 | Dependencies on other modules | `DiaMessageBus` (Bus, MessageBusModule), `diaentitytemplate` EntityRouter (registered at stage start), `MessageBusDebugDomain` (visual-debugger feature), `AutomationModule` (checkpoints), `VisualDebuggerModule` (domain registration), `DiaObservation::Metric`. |

## Acceptance Criteria

### Shared ACs (inherited from Infrastructure spec)

This stage satisfies AC-S1 through AC-S9 as defined in the [Infrastructure spec](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-C1 | First `NetworkPulseEvent` delivered to at least one Receiver within 3s | `msgbus.first_pulse` checkpoint passes |
| AC-C2 | First `DirectPingEvent` delivered to the correct Receiver (Emitter i → Receiver i%5) and no other Receiver | `msgbus.first_ping` checkpoint passes; `mPingsDeliveredToWrongReceiver` stays 0 |
| AC-C3 | First `PongEvent` arrives via Reaction pass after a `DirectPingEvent` is delivered | `msgbus.first_pong` checkpoint passes |
| AC-C4 | ≥ 20 cumulative `NetworkPulseEvent` deliveries — proves sustained broadcast routing | `msgbus.twenty_pulses` checkpoint passes |
| AC-C5 | ≥ 10 cumulative `DirectPingEvent` deliveries — proves sustained entity routing | `msgbus.ten_pings` checkpoint passes |
| AC-C6 | `BurstEvent` delivered to all 5 Receivers at frame 150 (all Receivers flash white simultaneously) | `cluichetest.msgbus.bursts == 1` metric; visual inspection — simultaneous white flash |
| AC-C7 | `cluichetest.msgbus.dropped == 0` throughout the run | Metric assertion in pytest |
| AC-C8 | `MessageBusDebugDomain` panel opens on `~`; Schema tab shows `NetworkPulseEvent`, `DirectPingEvent`, `PongEvent`, `BurstEvent` with correct producer/subscriber entries; Live tab updates each frame | Manual verification during `dia run cluichetest` |
| AC-C9 | Stage completes within 1500 frames (50s at 30Hz) | Orchestrator timeout |
| AC-C10 | Repeated run produces identical `pulses_sent` and `pings_sent` counts | Determinism — AC-S7 |

## Design

### World Layout

```
World bounds: (−15, −15) → (15, 15)

Emitters (coloured diamonds, wander r=12):
  E0: (−6.0,  5.0)  — cyan
  E1: (−8.0, −3.0)  — cyan
  E2: (−4.0, −7.0)  — cyan
  E3: ( 0.0,  7.0)  — cyan
  E4: ( 6.0,  0.0)  — cyan

Receivers (coloured circles, wander r=12):
  R0: ( 3.0,  5.0)  — magenta
  R1: ( 7.0, −2.0)  — magenta
  R2: ( 2.0, −6.0)  — magenta
  R3: (−2.0,  2.0)  — magenta
  R4: (−4.0, −4.0)  — magenta

Emitter Ei pings Receiver R[i%5] (round-robin, deterministic).

Flash colours:
  NetworkPulse received (Receiver):  blue ring, 0.3s
  DirectPing received (Receiver):    yellow ring, 0.3s
  PongEvent received (Emitter):      green ring, 0.3s
  BurstEvent received (Receiver):    white ring, 0.5s
  DirectPing in-flight:              thin line from Ei to R[i%5], 1 frame

ImGui sidebar:
  Pulses sent / Pings sent / Pongs received / Bursts
  Dropped: 0
  Per-Emitter: last ping target + time since last broadcast
  Per-Receiver: state (Idle | Pulse-flashing | Ping-flashing | Burst-flashing)
  Frame count / Bus tick index
```

### Message Types (stage-local)

```cpp
// MessageBusTestStageModule.cpp — not in a shared header

struct NetworkPulseEvent { uint32_t emitterIdx; };
struct DirectPingEvent   { uint32_t emitterIdx; uint32_t receiverIdx; };
struct PongEvent         { uint32_t receiverIdx; };
struct BurstEvent        { uint32_t frameNumber; };

// All tagged with kTypeId per Bus::Post<T> requirement
```

### BurstAdapter

```cpp
class BurstAdapter : public Dia::MessageBus::IFlushAdapter
{
public:
    explicit BurstAdapter(uint32_t triggerFrame) : mTriggerFrame(triggerFrame) {}
    void Flush(Dia::MessageBus::Bus& bus) override
    {
        if (!mFired && mCurrentFrame >= mTriggerFrame) {
            bus.Broadcast<BurstEvent>({ mCurrentFrame });
            mFired = true;
        }
        ++mCurrentFrame;
    }
private:
    uint32_t mTriggerFrame;
    uint32_t mCurrentFrame = 0;
    bool     mFired        = false;
};
```

### DoUpdate Frame Loop

```
Each frame:
  1. MessageBusModule::Update(dt) — runs pre-Primary (BurstAdapter::Flush), Primary pass, Reaction pass
  2. Tick each Emitter:
     a. Move toward wanderTarget at 0.8 m/s; on arrival pick next kEmitterWanderDirs[i] angle
     b. pulseTimer += dt; if >= 2.0s → Bus.Broadcast<NetworkPulseEvent>({i}); ++mPulsesSent; pulseTimer=0
     c. pingTimer  += dt; if >= 3.0s → Bus.Post<DirectPingEvent>({kEntityRouterId, receiverHandles[i%5]}, {i, i%5}); ++mPingsSent; pingTimer=0
  3. Update flash timers on all nodes
  4. Check checkpoint conditions, emit metrics
```

### Checkpoint Registration

```cpp
automation->RegisterCheckpoint(this, StringCRC("msgbus.first_pulse"),
    [this]() { return { mPulsesDelivered >= 1, ... }; });

automation->RegisterCheckpoint(this, StringCRC("msgbus.first_ping"),
    [this]() { return { mPingsDelivered >= 1, ... }; });

automation->RegisterCheckpoint(this, StringCRC("msgbus.first_pong"),
    [this]() { return { mPongsReceived >= 1, ... }; });

automation->RegisterCheckpoint(this, StringCRC("msgbus.twenty_pulses"),
    [this]() { return { mPulsesDelivered >= 20, ... }; });

automation->RegisterCheckpoint(this, StringCRC("msgbus.ten_pings"),
    [this]() { return { mPingsDelivered >= 10, ... }; });
```

### Subscribe / Register Setup (OnStart)

Each Receiver is spawned as a real entity via `EntityModule`. `DirectPingEvent` is subscribed per entity using its real `EntityHandle`, so only the addressed entity receives it via `EntityRouter`.

```cpp
// Spawn 5 Receiver entities; store handles
for (int i = 0; i < 5; ++i) {
    mReceiverHandles[i] = entityModule->Spawn(kReceiverIds[i]);
    mReceivers[i].position = kReceiverStartPositions[i];
}

// Broadcast subscribers (BroadcastRouter path — all 5 Receivers)
for (int i = 0; i < 5; ++i) {
    mSubs[i*2+0] = bus.Subscribe<NetworkPulseEvent>(kReceiverIds[i],
        [this, i](const NetworkPulseEvent&) { mReceivers[i].StartFlash(kFlashBlue); ++mPulsesDelivered; });
    mSubs[i*2+1] = bus.Subscribe<BurstEvent>(kReceiverIds[i],
        [this, i](const BurstEvent&) { mReceivers[i].StartFlash(kFlashWhite); });
}

// Entity-addressed subscriber — EntityRouter delivers only to the matching handle
for (int i = 0; i < 5; ++i) {
    mPingSubs[i] = bus.SubscribeEntity<DirectPingEvent>(mReceiverHandles[i], kReceiverIds[i],
        [this, i](const DirectPingEvent& e) {
            mReceivers[i].StartFlash(kFlashYellow); ++mPingsDelivered;
            bus.Post<PongEvent>({ Bus::kBroadcastRouterId, 0 }, { (uint32_t)i }); // Reaction pass
        }, Dia::MessageBus::Pass::Primary);
}

// PongEvent broadcast subscriber on all Emitters (Reaction pass)
for (int i = 0; i < 5; ++i) {
    mEmitterSubs[i] = bus.Subscribe<PongEvent>(kEmitterIds[i],
        [this, i](const PongEvent&) { mEmitters[i].StartFlash(kFlashGreen); ++mPongsReceived; },
        Dia::MessageBus::Pass::Reaction);
}
bus.RegisterFlushAdapter(&mBurstAdapter);
```

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheTest/Modules/TestStages/MessageBusTestStageModule.h` | New — module header + struct definitions |
| `Cluiche/CluicheTest/Modules/TestStages/MessageBusTestStageModule.cpp` | New — full implementation |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add ClCompile + ClInclude entries |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |
| `Cluiche/Assets/CluicheTest/Stages/MessageBusTestStage/messagebus_test_stage.diastage` | New — stage declaration |
| `Cluiche/Assets/CluicheTest/Stages/MessageBusTestStage/misc/ApplicationFlow/messagebus_test_stage.diaapp` | New — stage app manifest |
| `Cluiche/Assets/CluicheTest/Global/misc/ApplicationFlow/cluiche_main.diaapp` | Add MessageBusTestStage entry + Boot↔stage transitions |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import for messagebus_test_stage.diastage |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Add stage + manifest catalogue entries |
| `Tools/orchestrator/scenarios/cluichetest/msgbus/test_msgbus_e2e.py` | New — pytest scenario |
| `Tools/orchestrator/plans/cluichetest/default.json` | Add scenario to plan |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `dia scaffold stage MessageBusTestStage` — 4 touch points | Stage appears in Boot menu | Not Started | haiku | Generates stub module + manifests |
| 2 | Define message types + `BurstAdapter`; spawn 5 Receiver entities via `EntityModule` (store real `EntityHandle` per Receiver); register broadcast + entity-addressed + reaction-pass subscribers in `OnStart`; `BurstAdapter` registered as `IFlushAdapter`; 5 Emitters + 5 Receivers with wander tables | `msgbus.first_pulse` and `msgbus.first_ping` checkpoints pass | Not Started | sonnet | All wander dirs pre-computed in .cpp; Emitter i pings R[i%5] via real handle; `EntityRouter` registered by `EntityModule::OnStart()` |
| 3 | `OnUpdate` loop: tick emitters (wander + pulse + ping), update flash timers, check checkpoints, emit metrics; Reaction-pass `PongEvent` subscription on Emitters | All 5 checkpoints pass by frame ~600; `dropped == 0` | Not Started | sonnet | PongEvent subscribe with `Pass::Reaction` |
| 4 | `DrawDebugOverlay()`: coloured diamonds + circles, directional-arrow overlays, single-frame ping lines, flash rings, ImGui sidebar | Visually correct on manual run; simultaneous white burst flash on all Receivers at frame 150 | Not Started | sonnet | |
| 5 | Register `MessageBusDebugDomain` in `OnStart` via `VisualDebuggerModule`; 5 metrics registered; `OnStop` cleanup | `~` shows panel; Schema tab lists all 4 types; dropped metric == 0 | Not Started | haiku | |
| 6 | Write pytest scenario `test_msgbus_e2e.py` + add to `default.json` | All 5 checkpoints pass; `dropped == 0`; determinism second-pass passes | Not Started | sonnet | |
| 7 | Verify: `dia run cluichetest` → navigate to MessageBusTestStage; observe emitters broadcasting (blue flashes on all Receivers), ping lines + yellow flash on targeted Receiver, green pong flash on Emitter, white burst at t≈5s; debug panel shows live routing | E2E gate | Not Started | sonnet | |

## Dependencies

- **DiaMessageBus** ✅ — `Bus`, `MessageBusModule`, `BroadcastRouter`, `IFlushAdapter`
- **diaentitytemplate** ✅ — `EntityRouter` (registered at stage start); `EntityHandle` for Receiver addressing
- **MessageBusDebugDomain** — from visual-debugger feature (must ship first)
- **Test Stage Infrastructure** — `TestStageModuleBase`, `AutomationService`, checkpoint contract
- **VisualDebuggerModule** — `RegisterDomain` / `UnregisterDomain`

## Open Design Questions

All resolved before approval.

| # | Question | Resolution |
|---|----------|------------|
| 1 | EntityRouter registration timing | No special ordering needed. Standard SimPU lifecycle runs all `OnStart()` calls before any module's first `Update()`. The stage module accesses the bus via `ModuleRef<MessageBusModule>` and calls `bus.RegisterRouter(...)` (delegated through `EntityModule`) in `OnStart()`. First flush in `MessageBusModule::Update()` only runs after the full startup chain completes. |
| 2 | Receiver entity handles | Use real diaentitytemplate entities. Each of the 5 Receivers is spawned as an entity in `OnStart()`; `DirectPingEvent` is subscribed per entity via its real `EntityHandle`. The real `EntityRouter` from diaentitytemplate is registered at stage start (via `EntityModule::OnStart()`). Dependency on diaentitytemplate is confirmed. |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | `kTypeId`, checkpoint names, subscriber IDs, router constants all `StringCRC` |
| PD-004 | No STL in public APIs | Module header exposes no STL; internal wander tables are plain C arrays |
| PD-006 | VS project files source of truth | Task 1 scaffold + explicit vcxproj entries |
| PD-007 | C++20 required | `constexpr StringCRC` |
| PD-010 | `.diastage` for stages | Stage declared in `.diastage` |
| AD-001 (CT) | Three PUs | Module lives on SimPU |
| SD-TS-001 | One manifest stage per feature | One stage: MessageBusTestStage |
| SD-TS-002 | Checkpoints in OnStart, auto-clear on stop | `RegisterCheckpoints` called in `OnStart` |
| SD-TS-003 | Metrics for threshold assertions | 5 metrics registered |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in manifest |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec |

## Status

**Status:** Approved
