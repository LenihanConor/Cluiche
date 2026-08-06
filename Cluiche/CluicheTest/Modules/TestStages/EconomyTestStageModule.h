#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEconomy/EconomySchema.h>
#include <DiaEconomy/EconomyInstance.h>
#include <DiaEconomy/EconomySystem.h>
#include <DiaEconomy/IEconomyConditionAdaptor.h>
#include <DiaEconomy/Testing/EconomyTestHelpers.h>
#include <DiaMaths/Vector/Vector2D.h>

#ifdef DIA_DEBUG
#include "Modules/TestStages/Drawers/EconomyDrawer.h"
#include "Modules/VisualDebuggerModule.h"
#include <memory>
#endif

namespace Dia { namespace Automation { class AutomationService; } }

namespace CluicheTest {

class EconomyTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Integration stage: DiaEconomy -- gatherers, treasury, consumer, modifier";
    static constexpr unsigned int kMinDisplayFrames = 300;

    explicit EconomyTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~EconomyTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 900; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    // -----------------------------------------------------------------------
    // Scene constants
    // -----------------------------------------------------------------------
    static constexpr int   kGathererCount = 2;
    static constexpr float kMineX         = -300.f;
    static constexpr float kBaseX         =    0.f;
    static constexpr float kConsumerX     =  250.f;
    static constexpr float kSceneY        =   50.f;
    static constexpr float kGathererSpeed =  120.f; // units/s
    static constexpr float kCarryCapacity =   50.f;
    static constexpr float kTreasuryMax   = 2000.f;

    // -----------------------------------------------------------------------
    // Condition adaptor
    // -----------------------------------------------------------------------
    struct MarketAdaptor : Dia::Economy::IEconomyConditionAdaptor
    {
        bool mMarketActive = false;
        bool Evaluate(const char*) const override { return mMarketActive; }
    };

    // -----------------------------------------------------------------------
    // Gatherer state
    // -----------------------------------------------------------------------
    enum class GathererState { MovingToMine = 0, Mining = 1, MovingToBase = 2, Depositing = 3 };

    struct Gatherer
    {
        Dia::Maths::Vector2D      pos;
        GathererState             state      = GathererState::MovingToMine;
        float                     stateTimer = 0.f;
        Dia::Economy::EconomyInstance carry;
    };

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------
    void RegisterCheckpoints(Dia::Automation::AutomationService* service);
    bool AllCheckpointsPassed() const;
    void UpdateGatherer(Gatherer& g, float dt);
    void UpdateConsumer(float dt);
    void SyncDrawData();

    // -----------------------------------------------------------------------
    // Economy objects
    // -----------------------------------------------------------------------
    Dia::Economy::EconomySystem   mSystem;
    Dia::Economy::EconomySchema   mCarrySchema;
    Dia::Economy::EconomySchema   mTreasurySchema;
    Dia::Economy::EconomyInstance mTreasury;
    Dia::Economy::EconomyInstance mConsumerWallet;
    Dia::Economy::Testing::EventCapture mCapture;
    MarketAdaptor                 mConditionAdaptor;

    Gatherer mGatherers[kGathererCount];
    float    mConsumerTimer = 0.f;

    // -----------------------------------------------------------------------
    // Checkpoint flags
    // -----------------------------------------------------------------------
    bool mFirstTransferDone = false;
    bool mTreasuryAbove500  = false;
    bool mConsumerSpent     = false;
    bool mModifierActivated = false;
    bool mEventCountNonZero = false;
    bool mEconomyReady      = false;

    // -----------------------------------------------------------------------
    // Visual debugger (debug builds only)
    // -----------------------------------------------------------------------
#ifdef DIA_DEBUG
    GathererDrawData mDrawData[kEconomyDrawerGathererCount];
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<EconomyDrawer> mDrawer;
#endif
};

} // namespace CluicheTest
