#include "Modules/TestStages/MessageBusTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaCore/CRC/CRC.h>
#include <DiaEntity/EntityAddress.h>
#include <DiaMessageBus/Bus.h>
#include <cmath>
#include <cstdio>

#ifdef DIA_DEBUG
#include <DiaMessageBus/MessageBusDebugDomain.h>
#endif

namespace {

// ---------------------------------------------------------------------------
// Stage-local message types — throwaway E2E fixtures, not codegen'd (per
// spec's own "not in a shared header" note). Each carries a kTypeId per
// Bus::RegisterType<T>/Post<T>'s requirement (T::kTypeId).
// ---------------------------------------------------------------------------
struct NetworkPulseEvent
{
    static inline const Dia::Core::StringCRC kTypeId{ "NetworkPulseEvent" };
    uint32_t emitterIdx = 0;
};

struct DirectPingEvent
{
    static inline const Dia::Core::StringCRC kTypeId{ "DirectPingEvent" };
    uint32_t emitterIdx  = 0;
    uint32_t receiverIdx = 0;
};

struct PongEvent
{
    static inline const Dia::Core::StringCRC kTypeId{ "PongEvent" };
    uint32_t receiverIdx = 0;
};

struct BurstEvent
{
    static inline const Dia::Core::StringCRC kTypeId{ "BurstEvent" };
    uint32_t frameNumber = 0;
};

// Builds a StringCRC whose Value() equals the exact numeric encoding
// MakeEntitySubscriberId() produces, so a Bus-side Subscribe<T> lands on the
// same Dia::Mailbox::SubscriberId that EntityRouter::ResolveEntity computes.
// Mirrors EntityRouterBusWiringTests.cpp's ToBusSubscriberId() helper exactly.
Dia::Core::StringCRC ToBusSubscriberId(Dia::Mailbox::SubscriberId subId)
{
    Dia::Core::StringCRC result;
    static_cast<Dia::Core::CRC&>(result) = static_cast<unsigned int>(subId.value);
    return result;
}

// World layout (see messagebus-test-stage.md "World Layout").
const Dia::Maths::Vector2D kEmitterStartPos[5] = {
    { -6.0f,  5.0f }, { -8.0f, -3.0f }, { -4.0f, -7.0f }, {  0.0f,  7.0f }, {  6.0f,  0.0f },
};
const Dia::Maths::Vector2D kReceiverStartPos[5] = {
    {  3.0f,  5.0f }, {  7.0f, -2.0f }, {  2.0f, -6.0f }, { -2.0f,  2.0f }, { -4.0f, -4.0f },
};

// Pre-computed deterministic wander direction tables (degrees), 8 entries
// each, distinct per index — no randomness (AC-C10 / T8).
const float kEmitterWanderDirs[5][8] = {
    {   0.0f,  45.0f,  90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f },
    {  20.0f,  65.0f, 110.0f, 155.0f, 200.0f, 245.0f, 290.0f, 335.0f },
    {  40.0f,  85.0f, 130.0f, 175.0f, 220.0f, 265.0f, 310.0f, 355.0f },
    {  60.0f, 105.0f, 150.0f, 195.0f, 240.0f, 285.0f, 330.0f,  15.0f },
    {  80.0f, 125.0f, 170.0f, 215.0f, 260.0f, 305.0f, 350.0f,  35.0f },
};
const float kReceiverWanderDirs[5][8] = {
    {  10.0f,  55.0f, 100.0f, 145.0f, 190.0f, 235.0f, 280.0f, 325.0f },
    {  30.0f,  75.0f, 120.0f, 165.0f, 210.0f, 255.0f, 300.0f, 345.0f },
    {  50.0f,  95.0f, 140.0f, 185.0f, 230.0f, 275.0f, 320.0f,   5.0f },
    {  70.0f, 115.0f, 160.0f, 205.0f, 250.0f, 295.0f, 340.0f,  25.0f },
    {  90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f,   0.0f,  45.0f },
};

constexpr float kPi = 3.14159265358979323846f;

} // anonymous namespace

namespace CluicheTest {

const Dia::Core::StringCRC MessageBusTestStageModule::kTypeId("MessageBusTestStageModule");

MessageBusTestStageModule::MessageBusTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

Dia::Core::StringCRC MessageBusTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("MessageBusTestStage");
}

const Dia::Core::StringCRC* MessageBusTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("msgbus.first_pulse"),
        Dia::Core::StringCRC("msgbus.first_ping"),
        Dia::Core::StringCRC("msgbus.first_pong"),
        Dia::Core::StringCRC("msgbus.twenty_pulses"),
        Dia::Core::StringCRC("msgbus.ten_pings"),
    };
    outCount = 5;
    return names;
}

// ---------------------------------------------------------------------------
// BurstAdapter
// ---------------------------------------------------------------------------
void MessageBusTestStageModule::BurstAdapter::Flush(Dia::MessageBus::Bus& bus)
{
    if (!mFired && mCurrentFrame >= mTriggerFrame)
    {
        bus.Broadcast<BurstEvent>({ mCurrentFrame });
        mFired = true;
        if (mFiredCounter)
            ++(*mFiredCounter);
    }
    ++mCurrentFrame;
}

// ---------------------------------------------------------------------------
// Wander helper
// ---------------------------------------------------------------------------
void MessageBusTestStageModule::TickWander(WanderNode& node, const float* dirRowDegrees, float speed, float dt)
{
    if (node.position.DistanceTo(node.target) < kWanderArriveDist)
    {
        node.dirIndex = (node.dirIndex + 1) % kWanderDirCount;
        const float rad = dirRowDegrees[node.dirIndex] * (kPi / 180.0f);
        node.target = node.origin + Dia::Maths::Vector2D(std::cos(rad), std::sin(rad)) * kWanderRadius;
    }
    node.position = Dia::Maths::Vector2D::MoveTowards(node.position, node.target, speed * dt);
}

void MessageBusTestStageModule::StartFlash(WanderNode& node, FlashKind kind, float duration)
{
    node.flashKind  = kind;
    node.flashTimer = duration;
}

// ---------------------------------------------------------------------------
// OnStart
// ---------------------------------------------------------------------------
void MessageBusTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    // Reset all counters for re-entry (determinism across repeated runs — AC-C10).
    mFrameCounter                  = 0;
    mPulsesSent                    = 0;
    mPingsSent                     = 0;
    mPulsesDelivered                = 0;
    mPingsDelivered                 = 0;
    mPongsReceived                  = 0;
    mBurstDeliveryCount             = 0;
    mBurstsFired                    = 0;
    mPingsDeliveredToWrongReceiver  = 0;
    mDroppedTotal                   = 0;

    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        mEmitterPulseTimer[i] = 0.0f;
        mEmitterPingTimer[i]  = 0.0f;
        mPingLineActive[i]    = false;
    }

    // Destroy any Receiver entities from a previous entry before spawning
    // fresh ones, so repeated navigation into this stage never leaks entities
    // in mDomain.
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        if (mReceiverHandles[i].IsValid() && mDomain.IsAlive(mReceiverHandles[i]))
            mDomain.QueueDestroy(mReceiverHandles[i]);
    }
    mDomain.EndOfFrame();

    // Spawn 5 Receiver entities — real diaentitytemplate entities, addressable
    // through the EntityRouter registered on the Bus below.
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        char name[32];
        std::snprintf(name, sizeof(name), "Receiver%u", i);
        mReceiverHandles[i] = mDomain.CreateEntity(name);
    }
    mDomain.EndOfFrame();

    // Init wander nodes at their world-layout start positions.
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        mEmitters[i].origin     = kEmitterStartPos[i];
        mEmitters[i].position   = kEmitterStartPos[i];
        mEmitters[i].target     = kEmitterStartPos[i];
        mEmitters[i].dirIndex    = 0;
        mEmitters[i].flashKind  = FlashKind::None;
        mEmitters[i].flashTimer = 0.0f;

        mReceivers[i].origin     = kReceiverStartPos[i];
        mReceivers[i].position   = kReceiverStartPos[i];
        mReceivers[i].target     = kReceiverStartPos[i];
        mReceivers[i].dirIndex    = 0;
        mReceivers[i].flashKind  = FlashKind::None;
        mReceivers[i].flashTimer = 0.0f;
    }

    // Bring up this stage's MessageBusModule (Bus::Initialize() + BroadcastRouter
    // registration happen inside DoStart()), then register the real EntityRouter
    // on the same Bus — the first real stage to do so (see
    // entity-router-bus-wiring.md).
    mBusModule.Start();
    Dia::MessageBus::Bus& bus = mBusModule.GetBus();
    bus.RegisterRouter(&mDomain.GetEntityRouter());

    bus.RegisterType<NetworkPulseEvent, 16>();
    bus.RegisterType<DirectPingEvent, 16>();
    bus.RegisterType<PongEvent, 16>();
    bus.RegisterType<BurstEvent, 8>();

    // Broadcast subscribers (BroadcastRouter path) — all 5 Receivers get every
    // NetworkPulseEvent and the single BurstEvent, regardless of subscriber id.
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        char idBuf[48];
        std::snprintf(idBuf, sizeof(idBuf), "MessageBusTestStage.Receiver%u.Pulse", i);
        mPulseSubs[i] = bus.Subscribe<NetworkPulseEvent>(Dia::Core::StringCRC(idBuf),
            [this, i](const NetworkPulseEvent&) {
                StartFlash(mReceivers[i], FlashKind::Pulse, 0.3f);
                ++mPulsesDelivered;
            });

        std::snprintf(idBuf, sizeof(idBuf), "MessageBusTestStage.Receiver%u.Burst", i);
        mBurstSubs[i] = bus.Subscribe<BurstEvent>(Dia::Core::StringCRC(idBuf),
            [this, i](const BurstEvent&) {
                StartFlash(mReceivers[i], FlashKind::Burst, 0.5f);
                ++mBurstDeliveryCount;
            });
    }

    // Entity-addressed subscriber — EntityRouter delivers only to the matching
    // Receiver's real entity handle (AC-C2/AC-C5).
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        const Dia::Mailbox::SubscriberId subId = Dia::Entity::MakeEntitySubscriberId(mReceiverHandles[i]);
        mPingSubs[i] = bus.Subscribe<DirectPingEvent>(ToBusSubscriberId(subId),
            [this, i](const DirectPingEvent& e) {
                if (e.receiverIdx != i)
                    ++mPingsDeliveredToWrongReceiver;
                StartFlash(mReceivers[i], FlashKind::Ping, 0.3f);
                ++mPingsDelivered;
                // Reaction-pass chaining — posted from inside this Primary-pass
                // handler, delivered later in the same tick (AC-C3).
                mBusModule.GetBus().Broadcast<PongEvent>({ e.receiverIdx });
            }, Dia::MessageBus::Pass::Primary);
    }

    // PongEvent broadcast subscriber on all Emitters — Reaction pass.
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        char idBuf[48];
        std::snprintf(idBuf, sizeof(idBuf), "MessageBusTestStage.Emitter%u.Pong", i);
        mPongSubs[i] = bus.Subscribe<PongEvent>(Dia::Core::StringCRC(idBuf),
            [this, i](const PongEvent& e) {
                if (e.receiverIdx != i)
                    return; // only the emitter that pinged this receiver reacts
                StartFlash(mEmitters[i], FlashKind::Pong, 0.3f);
                ++mPongsReceived;
            }, Dia::MessageBus::Pass::Reaction);
    }

    mBurstAdapter.Configure(kBurstTriggerFrame, &mBurstsFired);
    bus.RegisterFlushAdapter(&mBurstAdapter);

    RegisterCheckpoints();

    // Metrics
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    mMetricPulsesSent    = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.msgbus.pulses_sent"));
    mMetricPingsSent     = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.msgbus.pings_sent"));
    mMetricPongsReceived = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.msgbus.pongs_received"));
    mMetricBursts        = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.msgbus.bursts"));
    mMetricDropped       = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.msgbus.dropped"));

#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebuggerRef.Get())
        vd->GetLayerManager().Register(&mDebugLayer, 10);

    mDebugDomain      = std::make_unique<Dia::MessageBus::MessageBusDebugDomain>(mBusModule);
    mDomainRegistered = false;
#endif

    DIA_LOG_INFO("CluicheTest", "MessageBusTestStageModule::OnStart — 5 emitters + 5 receivers initialised");
}

// ---------------------------------------------------------------------------
// OnUpdate
// ---------------------------------------------------------------------------
void MessageBusTestStageModule::OnUpdate(float deltaTime)
{
    // 1. Drive the full bus flush first: pre-Primary (BurstAdapter::Flush),
    //    Primary pass, then Reaction pass.
    mBusModule.Update(deltaTime);

    mDroppedTotal += mBusModule.GetBus().GetLastTickLedger().droppedCount;

    // Clear last frame's one-shot ping lines before this frame may set new ones.
    for (unsigned int i = 0; i < kNodeCount; ++i)
        mPingLineActive[i] = false;

    // 2. Tick each Emitter: wander + pulse timer + ping timer.
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        TickWander(mEmitters[i], kEmitterWanderDirs[i], kEmitterSpeed, deltaTime);

        mEmitterPulseTimer[i] += deltaTime;
        if (mEmitterPulseTimer[i] >= kPulseIntervalSeconds)
        {
            mBusModule.GetBus().Broadcast<NetworkPulseEvent>({ i });
            ++mPulsesSent;
            mEmitterPulseTimer[i] = 0.0f;
        }

        mEmitterPingTimer[i] += deltaTime;
        if (mEmitterPingTimer[i] >= kPingIntervalSeconds)
        {
            const unsigned int targetReceiver = i % kNodeCount;
            DirectPingEvent msg{ i, targetReceiver };
            mBusModule.GetBus().Post<DirectPingEvent>(
                Dia::Entity::MakeEntityAddress(mReceiverHandles[targetReceiver]), msg);
            ++mPingsSent;
            mEmitterPingTimer[i] = 0.0f;
            mPingLineActive[i]   = true;
        }
    }

    // 3. Tick each Receiver: wander only.
    for (unsigned int i = 0; i < kNodeCount; ++i)
        TickWander(mReceivers[i], kReceiverWanderDirs[i], kReceiverSpeed, deltaTime);

    // 4. Update flash timers on all nodes.
    auto tickFlash = [deltaTime](WanderNode& node) {
        if (node.flashTimer > 0.0f)
        {
            node.flashTimer -= deltaTime;
            if (node.flashTimer <= 0.0f)
            {
                node.flashTimer = 0.0f;
                node.flashKind  = FlashKind::None;
            }
        }
    };
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        tickFlash(mEmitters[i]);
        tickFlash(mReceivers[i]);
    }

    ++mFrameCounter;

    // 5. Metrics.
    if (mMetricPulsesSent)    mMetricPulsesSent->Set(static_cast<double>(mPulsesSent));
    if (mMetricPingsSent)     mMetricPingsSent->Set(static_cast<double>(mPingsSent));
    if (mMetricPongsReceived) mMetricPongsReceived->Set(static_cast<double>(mPongsReceived));
    if (mMetricBursts)        mMetricBursts->Set(static_cast<double>(mBurstsFired));
    if (mMetricDropped)       mMetricDropped->Set(static_cast<double>(mDroppedTotal));

#ifdef DIA_DEBUG
    if (!mDomainRegistered && mDebugDomain)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
        {
            vd->RegisterDomain(*mDebugDomain);
            mDomainRegistered = true;
        }
    }
#endif

    // 6. Pass condition — all 5 checkpoints satisfied plus the burst delivered
    //    to every Receiver, held for kMinDisplayFrames so the visual is watchable.
    // NOTE: gated on mPulsesSent (not mPulsesDelivered) — NetworkPulseEvent is
    // fanned out to all 5 Receivers per broadcast, so mPulsesDelivered inflates
    // 5x per send and would satisfy ">=20" after a single broadcast round.
    // mPulsesSent is the counter cluichetest.msgbus.pulses_sent reports, and
    // matches the spec's own "20 total by ~8s" budget math (see
    // RegisterCheckpoints' msgbus.twenty_pulses for the matching fix).
    const bool allPassed =
        mPulsesSent      >= 20 &&
        mPingsDelivered  >= 10 &&
        mPongsReceived   >= 1  &&
        mBurstDeliveryCount >= kNodeCount;

    if (allPassed && !IsResolved() && GetFrameCount() >= kMinDisplayFrames)
        ReportPassed();
}

// ---------------------------------------------------------------------------
// OnStop
// ---------------------------------------------------------------------------
void MessageBusTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebuggerRef.Get())
        vd->GetLayerManager().Unregister(Dia::Core::StringCRC("CluicheTest.MessageBus"));

    if (mDomainRegistered && mDebugDomain)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->UnregisterDomain(*mDebugDomain);
        mDomainRegistered = false;
    }
    mDebugDomain.reset();
#endif

    if (auto* service = GetAutomationService())
        service->UnregisterCheckpoints(this);

    // Release Bus-side subscriptions before the next entry re-subscribes.
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        mPulseSubs[i] = Dia::MessageBus::BusSubscriptionHandle();
        mBurstSubs[i] = Dia::MessageBus::BusSubscriptionHandle();
        mPingSubs[i]  = Dia::MessageBus::BusSubscriptionHandle();
        mPongSubs[i]  = Dia::MessageBus::BusSubscriptionHandle();
    }

    mBusModule.Stop();

    DIA_LOG_INFO("CluicheTest", "MessageBusTestStageModule::OnStop");
}

// ---------------------------------------------------------------------------
// RegisterCheckpoints
// ---------------------------------------------------------------------------
void MessageBusTestStageModule::RegisterCheckpoints()
{
    auto* service = GetAutomationService();
    if (!service)
        return;

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("msgbus.first_pulse"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPulsesDelivered >= 1, mPulsesDelivered >= 1 ? "first pulse delivered" : "pending", 0.0f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("msgbus.first_ping"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPingsDelivered >= 1, mPingsDelivered >= 1 ? "first ping delivered" : "pending", 0.0f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("msgbus.first_pong"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPongsReceived >= 1, mPongsReceived >= 1 ? "first pong delivered" : "pending", 0.0f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("msgbus.twenty_pulses"),
        [this]() -> Dia::Automation::CheckpointResult {
            // Keyed on mPulsesSent, not mPulsesDelivered: NetworkPulseEvent
            // fans out to all 5 Receivers per broadcast, so mPulsesDelivered
            // reaches 20 after a single broadcast round (5 emitters x 5
            // receivers = 25 deliveries at t=2s) — long before
            // cluichetest.msgbus.pulses_sent (the metric this checkpoint is
            // meant to gate) actually reaches 20 sends (~t=8s, per spec T9).
            return { mPulsesSent >= 20, mPulsesSent >= 20 ? "20 pulses sent" : "pending", 0.0f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("msgbus.ten_pings"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPingsDelivered >= 10, mPingsDelivered >= 10 ? "10 pings delivered" : "pending", 0.0f };
        });
}

// ---------------------------------------------------------------------------
// MessageBusDebugLayer::Draw
// ---------------------------------------------------------------------------
#ifdef DIA_DEBUG
void MessageBusTestStageModule::MessageBusDebugLayer::Draw(Dia::Core::IDebugDraw& draw)
{
    using RGBA = Dia::Core::RGBA;
    using V2   = Dia::Maths::Vector2D;

    static constexpr float kS = 15.0f; // world units -> pixels

    auto s = [](const V2& v) { return V2(v.x * kS, v.y * kS); };
    auto flashColour = [](FlashKind kind) -> RGBA {
        switch (kind)
        {
            case FlashKind::Pulse: return RGBA( 80, 140, 255, 220); // blue
            case FlashKind::Ping:  return RGBA(255, 230,  60, 220); // yellow
            case FlashKind::Pong:  return RGBA( 80, 220, 100, 220); // green
            case FlashKind::Burst: return RGBA(255, 255, 255, 240); // white
            default:               return RGBA(  0,   0,   0,   0);
        }
    };

    // Ping lines — single frame, thin, from Emitter i to Receiver i%5.
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        if (mModule->mPingLineActive[i])
        {
            const unsigned int target = i % kNodeCount;
            draw.RequestDraw(s(mModule->mEmitters[i].position), s(mModule->mReceivers[target].position),
                RGBA(255, 230, 60, 160));
        }
    }

    // Emitters — cyan diamonds.
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        const WanderNode& node = mModule->mEmitters[i];
        const V2 c = s(node.position);
        const float r = 10.0f;
        const V2 top(c.x, c.y - r), bottom(c.x, c.y + r), left(c.x - r, c.y), right(c.x + r, c.y);
        const RGBA fill(60, 220, 220, 200); // cyan
        draw.RequestDraw(top, right, bottom, RGBA(255, 255, 255, 255), fill);
        draw.RequestDraw(top, bottom, left,  RGBA(255, 255, 255, 255), fill);

        if (node.flashTimer > 0.0f)
            draw.RequestDraw(c, r + 6.0f, flashColour(node.flashKind));

        char label[16];
        std::snprintf(label, sizeof(label), "E%u", i);
        draw.RequestDrawText(V2(c.x - 6.0f, c.y - r - 12.0f), label, 10.0f, RGBA(255, 255, 255, 220));
    }

    // Receivers — magenta circles.
    for (unsigned int i = 0; i < kNodeCount; ++i)
    {
        const WanderNode& node = mModule->mReceivers[i];
        const V2 c = s(node.position);
        draw.RequestDraw(c, 10.0f, RGBA(255, 255, 255, 255), RGBA(220, 60, 220, 200));

        if (node.flashTimer > 0.0f)
            draw.RequestDraw(c, 16.0f, flashColour(node.flashKind));

        char label[16];
        std::snprintf(label, sizeof(label), "R%u", i);
        draw.RequestDrawText(V2(c.x - 6.0f, c.y - 22.0f), label, 10.0f, RGBA(255, 255, 255, 220));
    }
}
#endif

} // namespace CluicheTest

namespace { using MessageBusTestStageModule_ = CluicheTest::MessageBusTestStageModule; }
DIA_MODULE(MessageBusTestStageModule_);
DIA_DESCRIBE(MessageBusTestStageModule_::kTypeId, "MessageBus E2E: BroadcastRouter fan-out, EntityRouter targeted delivery, Reaction-pass chaining, IFlushAdapter injection");
