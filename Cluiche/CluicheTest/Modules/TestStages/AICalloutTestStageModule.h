#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaAICallout/CalloutRegistry.h>
#include <DiaAICallout/CalloutHandle.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Observation::Metric { class Gauge; }

#ifdef DIA_DEBUG
#include <DiaApplicationFlow/ModuleRefV2.h>
#include "Modules/VisualDebuggerModule.h"
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <memory>
namespace Dia::AICalloutVisualDebugger { class CalloutRegistryDebugger; }
#endif

namespace CluicheTest {

struct EmitterAgent
{
    Dia::Maths::Vector2D         position;
    Dia::Maths::Vector2D         wanderTarget;
    Dia::Core::StringCRC         faction;
    float                        emitTimer    = 0.0f;
    Dia::AICallout::CalloutHandle activeHandle;
    uint8_t                      wanderDirIdx        = 0;
    bool                         handleValidPreUpdate = false;  // validity before this frame's Update()
};

struct RelayResponder
{
    enum class State { Idle, Traveling, Relaying };

    Dia::Maths::Vector2D         position;
    Dia::Maths::Vector2D         wanderTarget;
    Dia::Core::StringCRC         faction;
    Dia::Core::StringCRC         responderId;
    float                        relayTimer = 0.0f;
    State                        state      = State::Idle;
    Dia::AICallout::CalloutHandle claimedHandle;
    Dia::Maths::Vector2D         targetPosition;
    uint8_t                      wanderDirIdx = 0;
    bool                         releaseFlash = false;
};

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
    class AICalloutDebugLayer : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit AICalloutDebugLayer(const AICalloutTestStageModule* module) : mModule(module) {}
        Dia::Core::StringCRC GetLayerName() const override
        {
            return Dia::Core::StringCRC("CluicheTest.AICallout");
        }
        void Draw(Dia::Core::IDebugDraw& draw) override;
    private:
        const AICalloutTestStageModule* mModule = nullptr;
    };

    AICalloutDebugLayer                                                          mDebugLayer{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule>     mVisualDebuggerRef{this};
    std::unique_ptr<Dia::AICalloutVisualDebugger::CalloutRegistryDebugger>      mCalloutDebugger;
    bool mDomainRegistered = false;
#endif
};

} // namespace CluicheTest
