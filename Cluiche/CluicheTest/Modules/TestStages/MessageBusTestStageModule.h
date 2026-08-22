#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMessageBus/MessageBusModule.h>
#include <DiaMessageBus/BusSubscriptionHandle.h>
#include <DiaMessageBus/IFlushAdapter.h>
#include <cstdint>

namespace Dia { namespace Automation { class AutomationService; } }
namespace Dia::Observation::Metric { class Gauge; }

#ifdef DIA_DEBUG
#include <DiaApplicationFlow/ModuleRefV2.h>
#include "Modules/VisualDebuggerModule.h"
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <memory>
namespace Dia::MessageBus { class MessageBusDebugDomain; }
#endif

namespace CluicheTest {

// ---------------------------------------------------------------------------
// MessageBusTestStageModule
//
// DiaMessageBus E2E: 5 Emitter entities broadcast NetworkPulseEvent and post
// entity-addressed DirectPingEvent to 5 Receiver entities via the real
// EntityRouter (registered on this stage's own Bus — the first real stage to
// do so). A Receiver's Primary-pass DirectPingEvent handler posts PongEvent
// back, delivered to Emitters via the Reaction pass (same tick). A
// BurstAdapter (IFlushAdapter) injects one BurstEvent at frame 150. Also
// wires MessageBusDebugDomain into a real stage for the first time.
// ---------------------------------------------------------------------------
class MessageBusTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription =
        "MessageBus E2E: BroadcastRouter fan-out, EntityRouter targeted delivery, Reaction-pass chaining, IFlushAdapter injection";
    explicit MessageBusTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 1500; }
    static constexpr unsigned int kMinDisplayFrames = 300; // 10 s at 30 Hz — hold for visual watchability
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    // Thin subclass to expose the protected lifecycle methods of MessageBusModule.
    // MessageBusDebugDomain's History tab needs GetLedgerHistory(), which only
    // exists on MessageBusModule (not on a bare Bus) — see SpawnerTestStageModule
    // for the identical pattern applied to EntitySpawnerModule.
    class TestableMessageBusModule : public Dia::MessageBus::MessageBusModule
    {
    public:
        TestableMessageBusModule() = default;
        void Start()          { DoStart(); }
        void Update(float dt) { DoUpdate(dt); }
        void Stop()           { DoStop(); }
    };

    static constexpr unsigned int kNodeCount        = 5;
    static constexpr unsigned int kWanderDirCount   = 8;
    static constexpr float        kWanderRadius     = 6.0f;   // stays within world's r=12 wander bound
    static constexpr float        kWanderArriveDist = 0.15f;
    static constexpr float        kEmitterSpeed     = 0.8f;   // m/s
    static constexpr float        kReceiverSpeed    = 0.6f;   // m/s
    static constexpr float        kPulseIntervalSeconds = 2.0f;
    static constexpr float        kPingIntervalSeconds  = 3.0f;
    static constexpr uint32_t     kBurstTriggerFrame    = 150; // t ~= 5.0s at 30Hz

    enum class FlashKind : uint8_t { None, Pulse, Ping, Pong, Burst };

    struct WanderNode
    {
        Dia::Maths::Vector2D origin;
        Dia::Maths::Vector2D position;
        Dia::Maths::Vector2D target;
        unsigned int         dirIndex   = 0;
        FlashKind             flashKind  = FlashKind::None;
        float                 flashTimer = 0.0f;
    };

    // BurstAdapter — injected via Bus::RegisterFlushAdapter. Fires exactly once,
    // in the pre-Primary step, at mTriggerFrame. Reports back through
    // firedCounter so the stage can expose cluichetest.msgbus.bursts == 1.
    class BurstAdapter : public Dia::MessageBus::IFlushAdapter
    {
    public:
        void Configure(uint32_t triggerFrame, unsigned int* firedCounter)
        {
            mTriggerFrame  = triggerFrame;
            mCurrentFrame  = 0;
            mFired         = false;
            mFiredCounter  = firedCounter;
        }
        void Flush(Dia::MessageBus::Bus& bus) override;

    private:
        uint32_t      mTriggerFrame = kBurstTriggerFrame;
        uint32_t      mCurrentFrame = 0;
        bool          mFired        = false;
        unsigned int* mFiredCounter = nullptr;
    };

    void TickWander(WanderNode& node, const float* dirRowDegrees, float speed, float dt);
    static void StartFlash(WanderNode& node, FlashKind kind, float duration);
    void RegisterCheckpoints();

    Dia::Entity::Domain      mDomain;
    TestableMessageBusModule mBusModule;

    Dia::Entity::Entity mReceiverHandles[kNodeCount];

    WanderNode mEmitters[kNodeCount];
    WanderNode mReceivers[kNodeCount];

    float mEmitterPulseTimer[kNodeCount] = {};
    float mEmitterPingTimer[kNodeCount]  = {};

    // True for exactly one frame — the frame a DirectPingEvent was posted — so
    // the debug layer can draw the in-flight ping line.
    bool mPingLineActive[kNodeCount] = {};

    Dia::MessageBus::BusSubscriptionHandle mPulseSubs[kNodeCount];
    Dia::MessageBus::BusSubscriptionHandle mBurstSubs[kNodeCount];
    Dia::MessageBus::BusSubscriptionHandle mPingSubs[kNodeCount];
    Dia::MessageBus::BusSubscriptionHandle mPongSubs[kNodeCount];

    BurstAdapter mBurstAdapter;

    // --- Counters (drive checkpoints + metrics) ---
    unsigned int mPulsesSent                    = 0;
    unsigned int mPingsSent                     = 0;
    unsigned int mPulsesDelivered                = 0;
    unsigned int mPingsDelivered                 = 0;
    unsigned int mPongsReceived                  = 0;
    unsigned int mBurstDeliveryCount             = 0; // deliveries across all 5 Receivers
    unsigned int mBurstsFired                    = 0; // set by BurstAdapter — should end at exactly 1
    unsigned int mPingsDeliveredToWrongReceiver  = 0; // AC-C2 — must stay 0
    uint32_t     mDroppedTotal                   = 0;
    unsigned int mFrameCounter                   = 0;

    // --- Metrics ---
    Dia::Observation::Metric::Gauge* mMetricPulsesSent    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricPingsSent     = nullptr;
    Dia::Observation::Metric::Gauge* mMetricPongsReceived = nullptr;
    Dia::Observation::Metric::Gauge* mMetricBursts        = nullptr;
    Dia::Observation::Metric::Gauge* mMetricDropped       = nullptr;

#ifdef DIA_DEBUG
    friend class MessageBusDebugLayer;

    class MessageBusDebugLayer : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit MessageBusDebugLayer(const MessageBusTestStageModule* module) : mModule(module) {}
        Dia::Core::StringCRC GetLayerName() const override
        {
            return Dia::Core::StringCRC("CluicheTest.MessageBus");
        }
        void Draw(Dia::Core::IDebugDraw& draw) override;
    private:
        const MessageBusTestStageModule* mModule = nullptr;
    };

    MessageBusDebugLayer mDebugLayer{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<Dia::MessageBus::MessageBusDebugDomain> mDebugDomain;
    bool mDomainRegistered = false;
#endif
};

} // namespace CluicheTest
