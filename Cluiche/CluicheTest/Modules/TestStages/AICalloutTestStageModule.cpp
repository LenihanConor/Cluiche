#include "Modules/TestStages/AICalloutTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaAICallout/Callout.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <cmath>
#include <cstring>

#ifdef DIA_DEBUG
#include <DiaAICalloutVisualDebugger/CalloutRegistryDebugger.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Colour/RGBA.h>
#endif

namespace CluicheTest {

// ---------------------------------------------------------------------------
// Wander direction tables (8 angles per emitter/responder, seeded by index)
// Angles in radians, spread around a circle. Each entity starts at a different
// offset so wanderers fan out and provide good spatial coverage.
// ---------------------------------------------------------------------------
static constexpr float kPi = 3.14159265358979f;

static constexpr float kEmitterWanderDirs[6][8] = {
    // E0 — blue
    { 0.0f,       kPi*0.25f, kPi*0.5f,  kPi*0.75f,
      kPi,        kPi*1.25f, kPi*1.5f,  kPi*1.75f },
    // E1 — blue
    { kPi*0.125f, kPi*0.375f, kPi*0.625f, kPi*0.875f,
      kPi*1.125f, kPi*1.375f, kPi*1.625f, kPi*1.875f },
    // E2 — blue
    { kPi*0.25f,  kPi*0.5f,  kPi*0.75f,  kPi,
      kPi*1.25f,  kPi*1.5f,  kPi*1.75f,  0.0f },
    // E3 — red
    { kPi*0.5f,   kPi*0.75f, kPi,        kPi*1.25f,
      kPi*1.5f,   kPi*1.75f, 0.0f,       kPi*0.25f },
    // E4 — red
    { kPi*0.625f, kPi*0.875f, kPi*1.125f, kPi*1.375f,
      kPi*1.625f, kPi*1.875f, kPi*0.125f, kPi*0.375f },
    // E5 — red
    { kPi*0.75f,  kPi,       kPi*1.25f,  kPi*1.5f,
      kPi*1.75f,  0.0f,       kPi*0.25f,  kPi*0.5f },
};

static constexpr float kResponderWanderDirs[6][8] = {
    // R0 — blue
    { kPi*0.125f, kPi*0.375f, kPi*0.625f, kPi*0.875f,
      kPi*1.125f, kPi*1.375f, kPi*1.625f, kPi*1.875f },
    // R1 — blue
    { kPi*0.25f,  kPi*0.5f,   kPi*0.75f,  kPi,
      kPi*1.25f,  kPi*1.5f,   kPi*1.75f,  0.0f },
    // R2 — blue
    { kPi*0.375f, kPi*0.625f, kPi*0.875f, kPi*1.125f,
      kPi*1.375f, kPi*1.625f, kPi*1.875f, kPi*0.125f },
    // R3 — red
    { kPi*0.625f, kPi*0.875f, kPi*1.125f, kPi*1.375f,
      kPi*1.625f, kPi*1.875f, kPi*0.125f, kPi*0.375f },
    // R4 — red
    { kPi*0.75f,  kPi,        kPi*1.25f,  kPi*1.5f,
      kPi*1.75f,  0.0f,        kPi*0.25f,  kPi*0.5f },
    // R5 — red
    { kPi*0.875f, kPi*1.125f, kPi*1.375f, kPi*1.625f,
      kPi*1.875f, kPi*0.125f,  kPi*0.375f, kPi*0.625f },
};

// ---------------------------------------------------------------------------
// Start positions
// ---------------------------------------------------------------------------
static const Dia::Maths::Vector2D kEmitterStartPos[6] = {
    { -5.0f,  4.0f },  // E0 blue
    { -7.0f, -2.0f },  // E1 blue
    { -3.0f, -6.0f },  // E2 blue
    {  5.0f,  4.0f },  // E3 red
    {  7.0f, -2.0f },  // E4 red
    {  3.0f, -6.0f },  // E5 red
};

static const Dia::Maths::Vector2D kResponderStartPos[6] = {
    { -1.0f,  1.0f },  // R0 blue
    { -1.5f, -0.5f },  // R1 blue
    { -0.5f, -1.5f },  // R2 blue
    {  1.0f,  1.0f },  // R3 red
    {  1.5f, -0.5f },  // R4 red
    {  0.5f, -1.5f },  // R5 red
};

static constexpr float kWanderBound     = 10.0f;
static constexpr float kEmitterSpeed    =  1.0f;
static constexpr float kResponderSpeed  =  2.0f;
static constexpr float kArrivalDist     =  0.5f;
static constexpr float kRelayDuration   =  2.0f;
static constexpr float kEmitInterval    =  5.0f;
static constexpr float kCalloutRadius   =  8.0f;
static constexpr float kCalloutTTL      =  6.0f;
static constexpr float kQueryRadius     = 12.0f;

static const Dia::Core::StringCRC kFactionBlue("faction_blue");
static const Dia::Core::StringCRC kFactionRed("faction_red");
static const Dia::Core::StringCRC kCalloutKind("distress_signal");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

Dia::Maths::Vector2D ClampToWanderBound(const Dia::Maths::Vector2D& pos)
{
    Dia::Maths::Vector2D clamped = pos;
    const float dist = std::sqrt(pos.x * pos.x + pos.y * pos.y);
    if (dist > kWanderBound && dist > 0.0001f)
    {
        clamped.x = pos.x / dist * kWanderBound;
        clamped.y = pos.y / dist * kWanderBound;
    }
    return clamped;
}

Dia::Maths::Vector2D WanderTargetFrom(const Dia::Maths::Vector2D& current,
                                       float angleRad)
{
    const float stepDist = 4.0f;
    Dia::Maths::Vector2D target;
    target.x = current.x + std::cos(angleRad) * stepDist;
    target.y = current.y + std::sin(angleRad) * stepDist;
    return ClampToWanderBound(target);
}

void MoveToward(Dia::Maths::Vector2D& pos, const Dia::Maths::Vector2D& target,
                float speed, float dt)
{
    const float dx  = target.x - pos.x;
    const float dy  = target.y - pos.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.0001f) return;
    const float move = speed * dt;
    if (move >= len)
    {
        pos = target;
    }
    else
    {
        pos.x += (dx / len) * move;
        pos.y += (dy / len) * move;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Static data
// ---------------------------------------------------------------------------
const Dia::Core::StringCRC AICalloutTestStageModule::kTypeId("AICalloutTestStageModule");

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
AICalloutTestStageModule::AICalloutTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

Dia::Core::StringCRC AICalloutTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("AICalloutTestStage");
}

const Dia::Core::StringCRC* AICalloutTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("aicallout.first_emit"),
        Dia::Core::StringCRC("aicallout.first_claim"),
        Dia::Core::StringCRC("aicallout.first_release"),
        Dia::Core::StringCRC("aicallout.first_ttl_expiry"),
        Dia::Core::StringCRC("aicallout.ten_claims"),
    };
    outCount = 5;
    return names;
}

// ---------------------------------------------------------------------------
// OnStart
// ---------------------------------------------------------------------------
void AICalloutTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    // Reset counters
    mTotalEmitted           = 0;
    mTotalClaimed           = 0;
    mTotalExpired           = 0;
    mTotalReleased          = 0;
    mLiveCountPeak          = 0;
    mCrossFactionViolations = 0;
    mFrameCount             = 0;
    mPulseTimer             = 0.0f;

    // Emitters
    for (unsigned int i = 0; i < kEmitterCount; ++i)
    {
        EmitterAgent& e  = mEmitters[i];
        e.position       = kEmitterStartPos[i];
        e.faction        = (i < 3) ? kFactionBlue : kFactionRed;
        e.emitTimer      = 0.0f;
        e.wanderDirIdx   = 0;
        e.activeHandle   = Dia::AICallout::CalloutHandle{};
        e.wanderTarget   = WanderTargetFrom(e.position, kEmitterWanderDirs[i][0]);
    }

    // Responders
    static const char* responderIds[6] = {
        "relay_0", "relay_1", "relay_2",
        "relay_3", "relay_4", "relay_5"
    };
    for (unsigned int i = 0; i < kResponderCount; ++i)
    {
        RelayResponder& r = mResponders[i];
        r.position        = kResponderStartPos[i];
        r.faction         = (i < 3) ? kFactionBlue : kFactionRed;
        r.responderId     = Dia::Core::StringCRC(responderIds[i]);
        r.state           = RelayResponder::State::Idle;
        r.relayTimer      = 0.0f;
        r.wanderDirIdx    = 0;
        r.claimedHandle   = Dia::AICallout::CalloutHandle{};
        r.releaseFlash    = false;
        r.wanderTarget    = WanderTargetFrom(r.position, kResponderWanderDirs[i][0]);
    }

    // Checkpoints
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("aicallout.first_emit"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mTotalEmitted >= 1,
                     mTotalEmitted >= 1 ? "first callout emitted" : "no emit yet", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("aicallout.first_claim"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mTotalClaimed >= 1,
                     mTotalClaimed >= 1 ? "first claim succeeded" : "no claim yet", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("aicallout.first_release"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mTotalReleased >= 1,
                     mTotalReleased >= 1 ? "first release done" : "no release yet", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("aicallout.first_ttl_expiry"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mTotalExpired >= 1,
                     mTotalExpired >= 1 ? "first TTL expiry observed" : "no expiry yet", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("aicallout.ten_claims"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mTotalClaimed >= 10,
                     mTotalClaimed >= 10 ? "ten cumulative claims reached" : "pending", 0.0f };
        });

    // Metrics
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    mMetricEmitted      = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.aicallout.total_emitted"));
    mMetricClaimed      = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.aicallout.total_claimed"));
    mMetricExpired      = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.aicallout.total_expired"));
    mMetricReleased     = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.aicallout.total_released"));
    mMetricPeak         = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.aicallout.live_count_peak"));
    mMetricCrossFaction = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.aicallout.cross_faction_violations"));

#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebuggerRef.Get())
        vd->GetLayerManager().Register(&mDebugLayer, 10);

    mCalloutDebugger  = std::make_unique<Dia::AICalloutVisualDebugger::CalloutRegistryDebugger>(mRegistry);
    mDomainRegistered = false;
#endif

    DIA_LOG_INFO("CluicheTest", "AICalloutTestStageModule::OnStart — 6 emitters, 6 responders");
}

// ---------------------------------------------------------------------------
// TickEmitters
// ---------------------------------------------------------------------------
void AICalloutTestStageModule::TickEmitters(float dt)
{
    for (unsigned int i = 0; i < kEmitterCount; ++i)
    {
        EmitterAgent& e = mEmitters[i];

        // Wander
        const float dx  = e.wanderTarget.x - e.position.x;
        const float dy  = e.wanderTarget.y - e.position.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < kArrivalDist)
        {
            e.wanderDirIdx = static_cast<uint8_t>((e.wanderDirIdx + 1u) % 8u);
            e.wanderTarget = WanderTargetFrom(e.position, kEmitterWanderDirs[i][e.wanderDirIdx]);
        }
        MoveToward(e.position, e.wanderTarget, kEmitterSpeed, dt);

        // Detect TTL expiry: handle was valid before Update() ran this frame, now invalid
        if (e.handleValidPreUpdate && !e.activeHandle.IsValid())
            ++mTotalExpired;

        // Emit
        e.emitTimer += dt;
        if (e.emitTimer >= kEmitInterval && !e.activeHandle.IsValid())
        {
            Dia::AICallout::Callout callout;
            callout.kind     = kCalloutKind;
            callout.position = e.position;
            callout.radius   = kCalloutRadius;
            callout.faction  = e.faction;
            callout.ttl      = kCalloutTTL;
            e.activeHandle = mRegistry.Emit(callout);
            e.emitTimer    = 0.0f;
            ++mTotalEmitted;
        }
    }
}

// ---------------------------------------------------------------------------
// TickResponders
// ---------------------------------------------------------------------------
void AICalloutTestStageModule::TickResponders(float dt)
{
    for (unsigned int i = 0; i < kResponderCount; ++i)
    {
        RelayResponder& r = mResponders[i];
        r.releaseFlash = false;

        switch (r.state)
        {
        case RelayResponder::State::Idle:
        {
            // Query same-faction callouts
            Dia::Core::Containers::DynamicArrayC<Dia::AICallout::CalloutHandle, 32> results;
            Dia::AICallout::QueryFilter filter;
            filter.kind    = kCalloutKind;
            filter.origin  = r.position;
            filter.radius  = kQueryRadius;
            filter.faction = r.faction;
            mRegistry.Query(filter, results);

            // Pick nearest
            float bestDistSq = 1e30f;
            Dia::AICallout::CalloutHandle bestHandle;
            for (unsigned int j = 0; j < static_cast<unsigned int>(results.Size()); ++j)
            {
                const Dia::AICallout::Callout* c = results[j].Get();
                if (!c) continue;
                const float dx = c->position.x - r.position.x;
                const float dy = c->position.y - r.position.y;
                const float dSq = dx * dx + dy * dy;
                if (dSq < bestDistSq)
                {
                    bestDistSq  = dSq;
                    bestHandle  = results[j];
                }
            }

            if (bestHandle.IsValid())
            {
                if (mRegistry.Claim(bestHandle, r.responderId))
                {
                    r.claimedHandle  = bestHandle;
                    r.targetPosition = bestHandle.Get()->position;
                    r.state          = RelayResponder::State::Traveling;
                    ++mTotalClaimed;
                }
                // else another responder won the race — idle wander
            }

            // Idle wander
            {
                const float dx  = r.wanderTarget.x - r.position.x;
                const float dy  = r.wanderTarget.y - r.position.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len < kArrivalDist)
                {
                    r.wanderDirIdx = static_cast<uint8_t>((r.wanderDirIdx + 1u) % 8u);
                    r.wanderTarget = WanderTargetFrom(r.position, kResponderWanderDirs[i][r.wanderDirIdx]);
                }
                MoveToward(r.position, r.wanderTarget, kResponderSpeed, dt);
            }
            break;
        }

        case RelayResponder::State::Traveling:
        {
            // Handle invalidated claim (TTL expired while we were traveling)
            if (!r.claimedHandle.IsValid())
            {
                r.state = RelayResponder::State::Idle;
                break;
            }

            // Update target in case the emitter moved (callout position is fixed at emit time,
            // so target is stable — but we cache it to avoid dereferencing a potentially
            // later-expired handle in the draw layer)
            const Dia::AICallout::Callout* c = r.claimedHandle.Get();
            if (c) r.targetPosition = c->position;

            MoveToward(r.position, r.targetPosition, kResponderSpeed, dt);

            const float dx  = r.targetPosition.x - r.position.x;
            const float dy  = r.targetPosition.y - r.position.y;
            if (std::sqrt(dx * dx + dy * dy) < kArrivalDist)
            {
                r.state      = RelayResponder::State::Relaying;
                r.relayTimer = 0.0f;
            }
            break;
        }

        case RelayResponder::State::Relaying:
        {
            r.relayTimer += dt;
            if (!r.claimedHandle.IsValid() || r.relayTimer >= kRelayDuration)
            {
                if (r.claimedHandle.IsValid())
                    mRegistry.Release(r.claimedHandle, r.responderId);
                r.releaseFlash  = true;
                r.claimedHandle = Dia::AICallout::CalloutHandle{};
                r.state         = RelayResponder::State::Idle;
                ++mTotalReleased;
            }
            break;
        }
        }
    }
}

// ---------------------------------------------------------------------------
// OnUpdate
// ---------------------------------------------------------------------------
void AICalloutTestStageModule::OnUpdate(float deltaTime)
{
    mPulseTimer += deltaTime * 0.5f;   // full cycle period = 2π / 0.5 ≈ 12.6s (visible pulse)

    // Snapshot handle validity before Update() expires them so TickEmitters can detect TTL events
    for (unsigned int i = 0; i < kEmitterCount; ++i)
        mEmitters[i].handleValidPreUpdate = mEmitters[i].activeHandle.IsValid();

    mRegistry.Update(deltaTime);
    TickEmitters(deltaTime);
    TickResponders(deltaTime);

    // Live count peak
    const int live = mRegistry.GetLiveCount();
    if (live > mLiveCountPeak) mLiveCountPeak = live;

    ++mFrameCount;

    // Metrics
    if (mMetricEmitted)      mMetricEmitted->Set(static_cast<double>(mTotalEmitted));
    if (mMetricClaimed)      mMetricClaimed->Set(static_cast<double>(mTotalClaimed));
    if (mMetricExpired)      mMetricExpired->Set(static_cast<double>(mTotalExpired));
    if (mMetricReleased)     mMetricReleased->Set(static_cast<double>(mTotalReleased));
    if (mMetricPeak)         mMetricPeak->Set(static_cast<double>(mLiveCountPeak));
    if (mMetricCrossFaction) mMetricCrossFaction->Set(static_cast<double>(mCrossFactionViolations));

    // All-checkpoints trigger stage pass
    if (!IsResolved()
        && mTotalEmitted >= 1
        && mTotalClaimed >= 10
        && mTotalExpired >= 1
        && mTotalReleased >= 1
        && mCrossFactionViolations == 0)
    {
        ReportPassed();
    }

#ifdef DIA_DEBUG
    if (!mDomainRegistered && mCalloutDebugger)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
        {
            vd->RegisterDomain(*mCalloutDebugger);
            mDomainRegistered = true;
        }
    }
#endif
}

// ---------------------------------------------------------------------------
// OnStop
// ---------------------------------------------------------------------------
void AICalloutTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebuggerRef.Get())
    {
        vd->GetLayerManager().Unregister(Dia::Core::StringCRC("CluicheTest.AICallout"));

        if (mDomainRegistered && mCalloutDebugger)
        {
            vd->UnregisterDomain(*mCalloutDebugger);
            mDomainRegistered = false;
        }
    }
    mCalloutDebugger.reset();
#endif

    if (auto* service = GetAutomationService())
        service->UnregisterCheckpoints(this);

    // Reset handles before clearing registry (handles must not dangle)
    for (unsigned int i = 0; i < kResponderCount; ++i)
    {
        mResponders[i].claimedHandle = Dia::AICallout::CalloutHandle{};
        mResponders[i].state         = RelayResponder::State::Idle;
    }
    for (unsigned int i = 0; i < kEmitterCount; ++i)
        mEmitters[i].activeHandle = Dia::AICallout::CalloutHandle{};

    // Clear registry so a re-entry starts from a clean state (determinism requirement)
    mRegistry.Reset();

    DIA_LOG_INFO("CluicheTest", "AICalloutTestStageModule::OnStop — emitted=%d claimed=%d expired=%d released=%d",
        mTotalEmitted, mTotalClaimed, mTotalExpired, mTotalReleased);
}

// ---------------------------------------------------------------------------
// Debug draw
// ---------------------------------------------------------------------------
#ifdef DIA_DEBUG
void AICalloutTestStageModule::AICalloutDebugLayer::Draw(Dia::Core::IDebugDraw& draw)
{
    using RGBA = Dia::Core::RGBA;
    using V2   = Dia::Maths::Vector2D;

    // World-to-screen scale: 1 world unit → 40 pixels
    static constexpr float kS = 40.0f;
    auto s = [](const V2& v) { return V2(v.x * kS, v.y * kS); };

    const RGBA kBlueEmitter  (  60, 120, 255, 230);
    const RGBA kRedEmitter   ( 220,  50,  50, 230);
    const RGBA kBlueResp     (  80, 160, 255, 210);
    const RGBA kRedResp      ( 255,  80,  80, 210);
    const RGBA kBlueRing     (  60, 120, 255, 160);
    const RGBA kRedRing      ( 220,  50,  50, 160);
    const RGBA kClaimedBright( 255, 255,  50, 200);
    const RGBA kExpiringRing ( 255, 255, 255, 200);
    const RGBA kFlashRing    ( 255, 255, 255, 255);

    // Draw callout rings
    for (uint32_t i = 0u; i < Dia::AICallout::CalloutRegistryData::kMaxCallouts; ++i)
    {
        const Dia::AICallout::CalloutSlot& slot = mModule->mRegistry.mSlots[i];
        if (!slot.live) continue;

        const V2 pos    = s(slot.callout.position);
        const float ttl = slot.callout.ttl;
        const bool blue = (slot.callout.faction == kFactionBlue);

        if (slot.claimed)
        {
            // Solid bright claimed ring
            draw.RequestDraw(pos, slot.callout.radius * kS, kClaimedBright);
        }
        else if (ttl < 2.0f)
        {
            // Expiring: white ring
            draw.RequestDraw(pos, slot.callout.radius * kS, kExpiringRing);
        }
        else
        {
            // Unclaimed pulsing ring: radius oscillates ±10%
            const float pulse = slot.callout.radius * kS
                * (0.9f + 0.1f * std::sin(mModule->mPulseTimer * kPi));
            const RGBA colour = blue ? kBlueRing : kRedRing;
            draw.RequestDraw(pos, pulse, colour);
        }
    }

    // Draw responders (circles)
    for (unsigned int i = 0; i < 6u; ++i)
    {
        const RelayResponder& r = mModule->mResponders[i];
        const V2 pos = s(r.position);
        const bool blue = (r.faction == kFactionBlue);

        // Release flash
        if (r.releaseFlash)
        {
            draw.RequestDraw(pos, 24.0f, kFlashRing);
        }

        // Body
        const RGBA fill  = blue ? kBlueResp : kRedResp;
        const RGBA out   = blue ? RGBA(140, 200, 255, 255) : RGBA(255, 140, 140, 255);
        draw.RequestDraw(pos, 10.0f, out, fill);

        // Direction arrow when traveling
        if (r.state == RelayResponder::State::Traveling)
        {
            const float dx  = r.targetPosition.x - r.position.x;
            const float dy  = r.targetPosition.y - r.position.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.001f)
            {
                V2 dir{ dx / len, dy / len };
                draw.RequestDrawRay(pos, dir, 16.0f, RGBA(255, 255, 200, 200));
            }
        }

        // State label
        const char* stateStr = (r.state == RelayResponder::State::Idle)      ? "I"
                             : (r.state == RelayResponder::State::Traveling)  ? "T"
                             :                                                  "R";
        draw.RequestDrawText(V2(pos.x + 12.0f, pos.y - 6.0f), stateStr, 9.0f, RGBA(220, 220, 220, 200));
    }

    // Draw emitters (diamonds approximated as 4-point stars via triangles)
    for (unsigned int i = 0; i < 6u; ++i)
    {
        const EmitterAgent& e = mModule->mEmitters[i];
        const V2 pos  = s(e.position);
        const bool blue = (e.faction == kFactionBlue);
        const RGBA fill = blue ? kBlueEmitter : kRedEmitter;
        const RGBA out  = blue ? RGBA(180, 220, 255, 255) : RGBA(255, 180, 180, 255);

        constexpr float kR = 8.0f;
        // Draw diamond (4 triangles)
        const V2 t(pos.x,       pos.y - kR);
        const V2 r(pos.x + kR,  pos.y);
        const V2 b(pos.x,       pos.y + kR);
        const V2 l(pos.x - kR,  pos.y);
        draw.RequestDraw(t, r, pos, out, fill);
        draw.RequestDraw(r, b, pos, out, fill);
        draw.RequestDraw(b, l, pos, out, fill);
        draw.RequestDraw(l, t, pos, out, fill);
    }
}
#endif

} // namespace CluicheTest

namespace { using AICalloutTestStageModule_ = CluicheTest::AICalloutTestStageModule; }
DIA_MODULE(AICalloutTestStageModule_);
