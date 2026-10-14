#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMessageBus/BusSubscriptionHandle.h>
#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <Shared/GameSpeedController.h>
#include "Modules/GameSpeedMessages.h"
#include "Modules/MessageBusHostModule.h"
#include "Modules/SaveLoadModule.h"
#include <DiaSaveGame/ISaveable.h>
#include <cstdint>
#include <memory>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/GameSpeedDebugDomain.h"
#endif

namespace CoW {

// Reports Degraded when the module runs on fallback preset config or the one-tick-per-frame
// cap is lagging, and Failing when a SimulationTick could not be published.
class GameSpeedHealthReporter : public Dia::Observation::Health::HealthReporterBase
{
public:
    GameSpeedHealthReporter() { SetOK(); }
    Dia::Core::StringCRC GetReporterName() const override { return Dia::Core::StringCRC("GameSpeedControllerModule"); }
};

class GameSpeedControllerModule;

// E1 save participant: persists the day counter only; speed, pause source and modal depth reset
// on load and the restore always resumes paused (A2 EC-03).
class GameSpeedSaveParticipant : public Dia::SaveGame::ISaveable
{
public:
    explicit GameSpeedSaveParticipant(GameSpeedControllerModule& module) : mModule(module) {}
    void Serialize(Dia::SaveGame::SaveContext& ctx) const override;
    void Deserialize(Dia::SaveGame::LoadContext& ctx) override;
    uint32_t GetVersion() const override { return 1; }

private:
    GameSpeedControllerModule& mModule;
};

// Steers the SimPU world SimTimeDomain (scale / pause / step), turns passing game time into
// discrete day ticks, and publishes SimulationTick. DiaSimTime owns the clock; this only steers it.
// Spec: docs/specs/applications/cow/systems/gamespeedcontroller/gamespeedcontroller.md
class GameSpeedControllerModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Game speed presets, pause rules and daily SimulationTick";

    explicit GameSpeedControllerModule(const Dia::Core::StringCRC& instanceId);
    ~GameSpeedControllerModule() override;

    // Read access for UI, debug and E1 only. Gameplay must subscribe to SimulationTick instead.
    const GameSpeedController& GetController() const { return mController; }
    const GameSpeedHealthReporter& GetHealthReporter() const { return mHealth; }
    bool IsUsingFallbackConfig() const { return mUsingFallbackConfig; }
    // Posts SpeedChangeRequested on the bus (the same path every other requester uses).
    bool RequestSpeed(SpeedTier tier);
    // Called by E1 on load. Always comes back paused.
    void RestoreState(const GameSpeedState& saved);
    // The ISaveable registered with SaveLoadModule when one exists (exposed for tests).
    Dia::SaveGame::ISaveable& GetSaveParticipant() { return mSaveParticipant; }

protected:
    void OnConfigure(const char* configJson) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void ApplyClockState();
    void PublishTick();
    void LogTransition(const char* cause, const GameSpeedState& before) const;
    void UpdateGauges();
    void UpdateHealth();
    void TryRegisterSaveParticipant();
    void UnregisterSaveParticipant();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::MessageBusHostModule> mBus{this};
    Dia::ApplicationFlow::ModuleRef<SaveLoadModule> mSaveLoad{this};
    GameSpeedSaveParticipant mSaveParticipant{*this};
    bool mSaveParticipantRegistered = false;

    SpeedPresetConfig   mPendingConfig = DefaultSpeedPresetConfig();
    bool                mConfigParseFailed = false;
    GameSpeedController mController{ DefaultSpeedPresetConfig() };
    DayClock            mDayClock;
    bool                mUsingFallbackConfig = false;

    Dia::MessageBus::BusSubscriptionHandle mSubscriptions[5];

    // Applied-to-domain cache so SetScale only runs on change.
    float mAppliedScale  = 0.0f;
    bool  mAppliedValid  = false;
    bool  mSubscriptionsFailed = false;
    bool  mStepPending   = false;
    bool  mClockResetPending = false;
    bool  mPublishFailed = false;
    uint32_t mBackwardsSeen = 0;

#ifdef DIA_DEBUG
    void TryRegisterDebugDomain();
    void UnregisterDebugDomain();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<GameSpeedDebugDomain> mDebugDomain;
    bool mDomainRegistered = false;
#endif

    GameSpeedHealthReporter mHealth;
    bool mHealthRegistered = false;

    Dia::Observation::Metric::Gauge*   mDayGauge = nullptr;
    Dia::Observation::Metric::Gauge*   mTierGauge = nullptr;
    Dia::Observation::Metric::Gauge*   mPausedGauge = nullptr;
    Dia::Observation::Metric::Gauge*   mModalDepthGauge = nullptr;
    Dia::Observation::Metric::Gauge*   mCarryGauge = nullptr;
    Dia::Observation::Metric::Counter* mTickCounter = nullptr;
    Dia::Observation::Metric::Counter* mStepCounter = nullptr;
    Dia::Observation::Metric::Counter* mStepRejectedCounter = nullptr;
    Dia::Observation::Metric::Counter* mSpeedChangeCounter = nullptr;
    Dia::Observation::Metric::Counter* mPublishDroppedCounter = nullptr;
    Dia::Observation::Metric::Counter* mClockBackwardsCounter = nullptr;
    Dia::Observation::Metric::Counter* mConfigFallbackCounter = nullptr;
};

} // namespace CoW
