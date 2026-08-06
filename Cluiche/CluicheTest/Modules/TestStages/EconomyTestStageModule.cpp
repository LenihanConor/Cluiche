#include "Modules/TestStages/EconomyTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cmath>

#ifdef DIA_DEBUG
#include <DiaVisualDebugger/DebugLayerManager.h>
#endif

namespace CluicheTest {

const Dia::Core::StringCRC EconomyTestStageModule::kTypeId("EconomyTestStageModule");

static constexpr const char* kCarrySchemaJson = R"({
    "schema_name": "carry_schema",
    "resources": [
        { "resource_name": "gold", "minimum_value": 0.0, "maximum_value": 50.0, "starting_value": 0.0 }
    ],
    "income_rules": [],
    "modifiers": []
})";

static constexpr const char* kTreasurySchemaJson = R"({
    "schema_name": "treasury_schema",
    "resources": [
        { "resource_name": "gold", "minimum_value": 0.0, "maximum_value": 2000.0, "starting_value": 0.0 }
    ],
    "income_rules": [],
    "modifiers": []
})";

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------
EconomyTestStageModule::EconomyTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

EconomyTestStageModule::~EconomyTestStageModule() = default;

Dia::Core::StringCRC EconomyTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("EconomyTestStage");
}

const Dia::Core::StringCRC* EconomyTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("economy.first_transfer"),
        Dia::Core::StringCRC("economy.treasury_above_500"),
        Dia::Core::StringCRC("economy.consumer_spent"),
        Dia::Core::StringCRC("economy.modifier_activates"),
        Dia::Core::StringCRC("economy.event_count_nonzero"),
    };
    outCount = 5;
    return names;
}

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------
void EconomyTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    Json::Value carryRoot, treasuryRoot;
    Json::Reader reader;
    reader.parse(kCarrySchemaJson,    carryRoot);
    reader.parse(kTreasurySchemaJson, treasuryRoot);

    mCarrySchema    = Dia::Economy::EconomySchema::LoadFromJsonValue(carryRoot);
    mTreasurySchema = Dia::Economy::EconomySchema::LoadFromJsonValue(treasuryRoot);

    DIA_ASSERT(mCarrySchema.IsValid(),    "EconomyTestStageModule: carry schema failed");
    DIA_ASSERT(mTreasurySchema.IsValid(), "EconomyTestStageModule: treasury schema failed");

    mTreasury       = Dia::Economy::EconomyInstance::CreateFromSchema(mTreasurySchema);
    mConsumerWallet = Dia::Economy::EconomyInstance::CreateFromSchema(mCarrySchema);

    const Dia::Maths::Vector2D kMine(kMineX, kSceneY);
    for (int i = 0; i < kGathererCount; ++i)
    {
        mGatherers[i].carry      = Dia::Economy::EconomyInstance::CreateFromSchema(mCarrySchema);
        mGatherers[i].pos        = kMine;
        mGatherers[i].state      = GathererState::Mining;
        mGatherers[i].stateTimer = static_cast<float>(i) * 1.5f;
    }

    mCapture.Subscribe(mSystem);
    mSystem.SetConditionAdaptor(&mConditionAdaptor);
    mEconomyReady = true;

    RegisterCheckpoints(service);

    DIA_LOG_INFO("Economy", "EconomyTestStageModule: started -- gatherers, treasury, consumer");

#ifdef DIA_DEBUG
    SyncDrawData();
    auto* vd = mVisualDebuggerRef.Get();
    if (vd)
    {
        mDrawer = std::make_unique<EconomyDrawer>(
            mDrawData,
            mTreasury,
            mConsumerWallet,
            mConditionAdaptor.mMarketActive,
            mCapture.poolChangedCount,
            mFirstTransferDone,
            mTreasuryAbove500,
            mConsumerSpent,
            mModifierActivated,
            mEventCountNonZero,
            vd->GetLayerManager());
        vd->GetLayerManager().Register(mDrawer.get(), 30, Dia::Core::StringCRC("Economy"));
    }
#endif
}

void EconomyTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    auto* vd = mVisualDebuggerRef.Get();
    if (vd && mDrawer)
        vd->GetLayerManager().Unregister(mDrawer->GetLayerName());
    mDrawer.reset();
#endif

    mCapture.Unsubscribe(mSystem);
    mCapture.Reset();

    mEconomyReady      = false;
    mFirstTransferDone = false;
    mTreasuryAbove500  = false;
    mConsumerSpent     = false;
    mModifierActivated = false;
    mEventCountNonZero = false;
    mConsumerTimer     = 0.f;
    mConditionAdaptor.mMarketActive = false;
}

// -----------------------------------------------------------------------
// Update
// -----------------------------------------------------------------------
void EconomyTestStageModule::OnUpdate(float deltaTime)
{
    if (!mEconomyReady) return;

    const unsigned int frame = GetFrameCount();

    if (frame == 60 && !mConditionAdaptor.mMarketActive)
    {
        mConditionAdaptor.mMarketActive = true;
        mModifierActivated = true;
        DIA_LOG_INFO("Economy", "EconomyTestStageModule: market_bonus activated at frame 60");
    }

    for (int i = 0; i < kGathererCount; ++i)
        UpdateGatherer(mGatherers[i], deltaTime);

    UpdateConsumer(deltaTime);

    static const Dia::Core::StringCRC kGold("gold");

    if (!mEventCountNonZero && mCapture.poolChangedCount > 0)
        mEventCountNonZero = true;

    if (!mTreasuryAbove500 && mTreasury.GetValue(kGold) >= 500.f)
        mTreasuryAbove500 = true;

#ifdef DIA_DEBUG
    SyncDrawData();
#endif

    if (AllCheckpointsPassed() && !IsResolved() && frame >= kMinDisplayFrames)
        ReportPassed();
}

void EconomyTestStageModule::UpdateGatherer(Gatherer& g, float dt)
{
    static const Dia::Core::StringCRC kGold("gold");
    static const Dia::Maths::Vector2D kMine(kMineX, kSceneY);
    static const Dia::Maths::Vector2D kBase(kBaseX, kSceneY);

    switch (g.state)
    {
    case GathererState::MovingToMine:
    {
        const Dia::Maths::Vector2D dir = kMine + (g.pos * -1.f);
        const float dist = std::sqrtf(dir.X()*dir.X() + dir.Y()*dir.Y());
        if (dist < 2.f)
        {
            g.pos = kMine; g.state = GathererState::Mining; g.stateTimer = 0.f;
        }
        else
        {
            const float step = kGathererSpeed * dt;
            g.pos = g.pos + dir * (step / dist);
        }
        break;
    }
    case GathererState::Mining:
    {
        const float rate = mConditionAdaptor.mMarketActive ? 50.f : 25.f;
        (void)mSystem.Earn(g.carry, kGold, rate * dt);
        g.stateTimer += dt;
        if (g.carry.GetValue(kGold) >= kCarryCapacity - 0.1f)
        {
            g.state = GathererState::MovingToBase; g.stateTimer = 0.f;
        }
        break;
    }
    case GathererState::MovingToBase:
    {
        const Dia::Maths::Vector2D dir = kBase + (g.pos * -1.f);
        const float dist = std::sqrtf(dir.X()*dir.X() + dir.Y()*dir.Y());
        if (dist < 2.f)
        {
            g.pos = kBase; g.state = GathererState::Depositing; g.stateTimer = 0.f;
        }
        else
        {
            const float step = kGathererSpeed * dt;
            g.pos = g.pos + dir * (step / dist);
        }
        break;
    }
    case GathererState::Depositing:
    {
        const float carried = g.carry.GetValue(kGold);
        if (carried > 0.1f)
        {
            (void)mSystem.Transfer(g.carry, mTreasury, kGold, carried);
            if (!mFirstTransferDone)
            {
                mFirstTransferDone = true;
                DIA_LOG_INFO("Economy", "EconomyTestStageModule: first deposit -- treasury = %.1f",
                             mTreasury.GetValue(kGold));
            }
        }
        g.state = GathererState::MovingToMine; g.stateTimer = 0.f;
        break;
    }
    }
}

void EconomyTestStageModule::UpdateConsumer(float dt)
{
    static const Dia::Core::StringCRC kGold("gold");

    mConsumerTimer += dt;
    if (mConsumerTimer < 2.f)
        return;
    mConsumerTimer = 0.f;

    if (mTreasury.GetValue(kGold) < 30.f)
        return;

    (void)mSystem.Transfer(mTreasury, mConsumerWallet, kGold, 30.f);
    if (!mConsumerSpent)
    {
        mConsumerSpent = true;
        DIA_LOG_INFO("Economy", "EconomyTestStageModule: consumer first spend -- treasury now %.1f",
                     mTreasury.GetValue(kGold));
    }
}

bool EconomyTestStageModule::AllCheckpointsPassed() const
{
    return mFirstTransferDone && mTreasuryAbove500 && mConsumerSpent
        && mModifierActivated && mEventCountNonZero;
}

void EconomyTestStageModule::RegisterCheckpoints(Dia::Automation::AutomationService* service)
{
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("economy.first_transfer"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mFirstTransferDone, mFirstTransferDone ? "First deposit made" : "pending", 0.f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("economy.treasury_above_500"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mTreasuryAbove500, mTreasuryAbove500 ? "Treasury reached 500 gold" : "pending", 0.f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("economy.consumer_spent"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mConsumerSpent, mConsumerSpent ? "Consumer drained treasury" : "pending", 0.f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("economy.modifier_activates"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mModifierActivated, mModifierActivated ? "market_bonus active at frame 60" : "pending", 0.f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("economy.event_count_nonzero"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mEventCountNonZero, mEventCountNonZero ? "Observer events fired" : "pending", 0.f };
        });
}

// -----------------------------------------------------------------------
// Draw data sync (debug only)
// -----------------------------------------------------------------------
#ifdef DIA_DEBUG
void EconomyTestStageModule::SyncDrawData()
{
    static const Dia::Core::StringCRC kGold("gold");
    for (int i = 0; i < kGathererCount; ++i)
    {
        mDrawData[i].pos   = mGatherers[i].pos;
        mDrawData[i].carry = mGatherers[i].carry.GetValue(kGold);
        mDrawData[i].state = static_cast<int>(mGatherers[i].state);
    }
}
#else
void EconomyTestStageModule::SyncDrawData() {}
#endif

} // namespace CluicheTest

namespace { using EconomyTestStageModule_ = CluicheTest::EconomyTestStageModule; }
DIA_MODULE(EconomyTestStageModule_);
DIA_DESCRIBE(EconomyTestStageModule_::kTypeId, "Integration stage: DiaEconomy -- gatherers mine gold, deposit to treasury, consumer drains, market modifier");
